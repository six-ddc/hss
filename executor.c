#include <sys/wait.h>
#include "common.h"
#include "sstring.h"
#include "executor.h"
#include "slot.h"
#include "command.h"

#define MAXFD 1024

#define BLOCK_SIGCHLD                           \
    sigemptyset(&sigmask);                      \
    sigaddset(&sigmask, SIGCHLD);               \
    sigprocmask(SIG_BLOCK, &sigmask, &osigmask)
#define UNBLOCK_SIGCHLD                         \
    sigprocmask(SIG_SETMASK, &osigmask, NULL)

sigset_t sigmask;
sigset_t osigmask;

#define UPLOAD "0"
#define DOWNLOAD "1"

volatile int alive_children = 0;

static void
print_line(struct slot *pslot, int io_type, sstring buf, void *data) {
    FILE *output = (FILE *) data;
    if (output == stdout) {
        if (io_type == STDOUT_FILENO) {
            printf(ANSI_COLOR_GREEN "[O] %s -> " ANSI_COLOR_RESET, pslot->host);
        } else {
            printf(ANSI_COLOR_RED "[E] %s -> " ANSI_COLOR_RESET, pslot->host);
        }
    }
    fwrite(buf, 1, string_length(buf), output);
    fflush(output);
}

static void
save_string(struct slot *pslot, int io_type, sstring buf, void *data) {
    sstring str;
    if (!data) {
        return;
    }
    str = *((sstring *) data);
    str = string_append(str, buf, string_length(buf));
    *(sstring *) data = str;
}

static struct slot *
fork_command(struct slot *pslot, void (*fn)(struct slot *, int, char **), int argc, char **argv) {
    int pid;
    slot_reinit(pslot);
    switch (pid = fork()) {
        case 0:
            // child
            fn(pslot, argc, argv);
            break;
        case -1:
            // error
            eprintf("unable to fork: %s\n", strerror(errno));
            return NULL;
        default:
            pslot->pid = pid;
            pslot->alive = true;
            close(pslot->io.out[PIPE_WRITE_END]);
            close(pslot->io.err[PIPE_WRITE_END]);
            alive_children++;
            if (pconfig->verbose) {
                printf("pid \"%d\" run in main thread, host: %s\n", pslot->pid, pslot->host);
            }
            break;
    }
    return pslot;
}

static void
fdset_alive_slots(struct slot *pslot, fd_set *readfds) {
    while (pslot) {
        if (!pslot->alive) {
            pslot = pslot->next;
            continue;
        }
        FD_SET(pslot->io.out[PIPE_READ_END], readfds);
        FD_SET(pslot->io.err[PIPE_READ_END], readfds);
        pslot = pslot->next;
    }
}

static void
read_alive_slots(struct slot *pslot, fd_set *readfds, FILE *output) {
    while (pslot) {
        if (!pslot->alive) {
            pslot = pslot->next;
            continue;
        }
        if (FD_ISSET(pslot->io.out[PIPE_READ_END], readfds)) {
            slot_read_line(pslot, STDOUT_FILENO, print_line, output);
        }
        if (FD_ISSET(pslot->io.err[PIPE_READ_END], readfds)) {
            slot_read_line(pslot, STDERR_FILENO, print_line, output);
        }
        pslot = pslot->next;
    }
}

static void
read_dead_slots(struct slot *pslot, struct slot *pslot_end, FILE *output) {
    while (pslot && pslot != pslot_end) {
        if (!pslot->alive) {
            slot_read_remains(pslot, STDOUT_FILENO, print_line, output);
            slot_read_remains(pslot, STDERR_FILENO, print_line, output);
        }
        pslot = pslot->next;
    }
}

int
exec_local_cmd(char *cmd) {
    return system(cmd);
}

static struct command *
find_inner_command(char *name) {
    struct command *p = inner_commands;
    while ((p = p->next) != NULL) {
        if (strcmp(name, p->name) == 0) {
            return p;
        }
    }
    return NULL;
}

int
exec_inner_cmd(char *line) {
    char word[32];
    char *pword = word;
    int i = 0;
    struct command *cmd;
    char *argv;

    while (line[i] && whitespace(line[i]))
        i++;
    while (line[i] && !whitespace(line[i]) && i < 31) {
        *pword++ = line[i++];
    }
    *pword = '\0';
    if (i >= 31) {
        eprintf("%s: command is too long\n", word);
        return -1;
    }

    cmd = find_inner_command(word);
    if (!cmd) {
        printf("%s: no such command\n", word);
        return -1;
    }

    while (whitespace(line[i]))
        i++;

    argv = line + i;

    return (*cmd->func)(argv);
}

void
reap_child_handler(int sig) {
    int exit_code;
    int pid;
    int ret;
    struct slot *pslot;
    SAVE_ERRNO;
    while ((pid = waitpid(-1, &ret, WNOHANG)) > 0) {
        if (WIFEXITED(ret))
            exit_code = WEXITSTATUS(ret);
        else
            exit_code = 255;
        pslot = slot_find_by_pid(slots, pid);
        if (pslot) {
            slot_close(pslot, exit_code);
        }
        if (pconfig->verbose) {
            printf("wait pid %d, status code %d\n", pslot->pid, pslot->exit_code);
        }
        alive_children--;
    }

    RESTORE_ERRNO;
}

static void
exec_ssh_cmd(struct slot *pslot, int argc, char **argv) {
    char host_argv[256];
    char *ssh_argv[128];
    int idx = 0;
    int i;
    int ret;
    char *cmd = argv[0];

    close(STDIN_FILENO);

    // printf("cmd: %s\n", cmd);

    close(pslot->io.out[PIPE_READ_END]);
    close(pslot->io.err[PIPE_READ_END]);

    if (dup2(pslot->io.out[PIPE_WRITE_END], STDOUT_FILENO) == -1) {
        eprintf("failed to dup stdout: %s\n", strerror(errno));
    }
    if (dup2(pslot->io.err[PIPE_WRITE_END], STDERR_FILENO) == -1) {
        eprintf("failed to dup stderr: %s\n", strerror(errno));
    }

    ssh_argv[idx++] = "-q";
    ssh_argv[idx++] = "-oNumberOfPasswordPrompts=0";
    ssh_argv[idx++] = "-oStrictHostKeyChecking=no";

    for (i = 0; i < pconfig->common_options_argc; ++i) {
        ssh_argv[idx++] = (char *) pconfig->common_options_argv[i];
    }

    for (i = 0; i < pslot->ssh_argc; ++i) {
        if (i == pslot->ssh_argc - 1 && pconfig->user) {
            if (!strchr(pslot->ssh_argv[i], '@')) {
                snprintf(host_argv, sizeof host_argv, "%s@%s", pconfig->user, pslot->ssh_argv[i]);
                ssh_argv[idx++] = host_argv;
            } else {
                ssh_argv[idx++] = pslot->ssh_argv[i];
            }
        } else {
            ssh_argv[idx++] = pslot->ssh_argv[i];
        }
    }
    ssh_argv[idx++] = cmd;
    ssh_argv[idx++] = NULL;

    ret = execvp("ssh", ssh_argv);

    eprintf("failed to exec the ssh binary: (%d) %s\n", ret, strerror(errno));
    exit(1);
}

static void
exec_scp_cmd(struct slot *pslot, int argc, char **argv) {
    char host_argv[256];
    char local_argv[256];
    char *scp_argv[128];
    int idx = 0;
    int i;
    int ret;
    char *mode = argv[0];
    char *local_filename, *remote_filename;
    if (strcmp(mode, UPLOAD) == 0) {
        local_filename = argv[1];
        remote_filename = argv[2];
    } else {
        local_filename = argv[2];
        remote_filename = argv[1];
    }

    close(STDIN_FILENO);

    close(pslot->io.out[PIPE_READ_END]);
    close(pslot->io.err[PIPE_READ_END]);

    if (dup2(pslot->io.out[PIPE_WRITE_END], STDOUT_FILENO) == -1) {
        eprintf("failed to dup stdout: %s\n", strerror(errno));
    }
    if (dup2(pslot->io.err[PIPE_WRITE_END], STDERR_FILENO) == -1) {
        eprintf("failed to dup stderr: %s\n", strerror(errno));
    }

    scp_argv[idx++] = "-r";
    scp_argv[idx++] = "-oNumberOfPasswordPrompts=0";
    scp_argv[idx++] = "-oStrictHostKeyChecking=no";

    for (i = 0; i < pconfig->common_options_argc; ++i) {
        if (strcmp(pconfig->common_options_argv[i], "-p") == 0) {
            scp_argv[idx++] = "-P";
        } else {
            scp_argv[idx++] = (char *) pconfig->common_options_argv[i];
        }
    }

    for (i = 0; i < pslot->ssh_argc - 1; ++i) {
        if (strcmp(pslot->ssh_argv[i], "-p") == 0) {
            scp_argv[idx++] = "-P";
        } else {
            scp_argv[idx++] = pslot->ssh_argv[i];
        }
    }

    if (pconfig->user && !strchr(pslot->ssh_argv[i], '@')) {
        snprintf(host_argv, sizeof host_argv, "%s@%s:%s", pconfig->user, pslot->ssh_argv[i], remote_filename);
    } else {
        snprintf(host_argv, sizeof host_argv, "%s:%s", pslot->ssh_argv[i], remote_filename);
    }
    if (strcmp(mode, UPLOAD) == 0) {
        scp_argv[idx++] = local_filename;
        scp_argv[idx++] = host_argv;
    } else {
        scp_argv[idx++] = host_argv;
        snprintf(local_argv, sizeof local_argv, "%s-%s", pslot->ssh_argv[i], local_filename);
        scp_argv[idx++] = local_argv;
    }
    scp_argv[idx++] = NULL;

    ret = execvp("scp", scp_argv);

    eprintf("failed to exec the scp binary: (%d) %s\n", ret, strerror(errno));
    exit(1);
}

static int
exec_command_foreach(struct slot *pslot_list, void (*fn_fork)(struct slot *, int, char **), int argc, char **argv) {
    fd_set readfds;
    struct slot *pslot = pslot_list->next;
    struct slot *pslot_head = pslot;
    struct timeval no_timeout;
    struct timeval *timeout;
    FILE *output;

    if (!pslot) {
        return 0;
    }

    memset(&no_timeout, 0, sizeof(struct timeval));

    if (pconfig->output_file) {
        output = fopen(pconfig->output_file, "a");
        if (!output) {
            eprintf("can not open file %s (%s)\n", pconfig->output_file, strerror(errno));
            return -1;
        }
    } else {
        output = stdout;
    }

    alive_children = 0;

    while (pslot || alive_children) {
        BLOCK_SIGCHLD;
        if (pslot) {
            fork_command(pslot, fn_fork, argc, argv);
            pslot = pslot->next;
            usleep(10 * 1000);
        }

        read_dead_slots(pslot_head, pslot, output);

        FD_ZERO(&readfds);
        fdset_alive_slots(pslot_head, &readfds);

        timeout = pslot ? &no_timeout : NULL;

        if (select(MAXFD, &readfds, NULL, NULL, timeout) > 0) {
            read_alive_slots(pslot_head, &readfds, output);
        }
        UNBLOCK_SIGCHLD;
    }

    read_dead_slots(pslot_head, pslot, output);

    if (output != stdout) {
        fclose(output);
    }

    return 0;
}

int
exec_remote_cmd(struct slot *pslot_list, char *cmd) {
    const int argc = 1;
    char *argv[] = {cmd};
    return exec_command_foreach(pslot_list, exec_ssh_cmd, argc, argv);
}

int
upload_file(struct slot *pslot_list, char *local_filename, char *remote_filename) {
    const int argc = 3;
    char *argv[] = {UPLOAD, local_filename, remote_filename};
    return exec_command_foreach(pslot_list, exec_scp_cmd, argc, argv);
}

int
download_file(struct slot *pslot_list, char *remote_filename, char *local_filename) {
    const int argc = 3;
    char *argv[] = {DOWNLOAD, remote_filename, local_filename};
    return exec_command_foreach(pslot_list, exec_scp_cmd, argc, argv);
}

int
sync_exec_remote_cmd(struct slot *pslot_list, char *cmd, sstring *out, sstring *err) {

    fd_set readfds;
    char *argv[1] = {cmd};
    struct slot *pslot = pslot_list->next;

    if (!pslot) {
        return 0;
    }

    FD_ZERO(&readfds);

    BLOCK_SIGCHLD;
    fork_command(pslot, exec_ssh_cmd, 1, argv);
    FD_SET(pslot->io.out[PIPE_READ_END], &readfds);
    FD_SET(pslot->io.err[PIPE_READ_END], &readfds);
    UNBLOCK_SIGCHLD;

    while (alive_children) {
        BLOCK_SIGCHLD;
        if (select(MAXFD, &readfds, NULL, NULL, NULL) > 0) {
            if (FD_ISSET(pslot->io.out[PIPE_READ_END], &readfds)) {
                slot_read_line(pslot, STDOUT_FILENO, save_string, out);
            }
            if (FD_ISSET(pslot->io.err[PIPE_READ_END], &readfds)) {
                slot_read_line(pslot, STDERR_FILENO, save_string, err);
            }
        }
        UNBLOCK_SIGCHLD;
    }
    return 0;
}

