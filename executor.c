#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <time.h>
#include "common.h"
#include "sstring.h"
#include "executor.h"
#include "slot.h"

#define MAXFD 1024

#define BLOCK_SIGCHLD                           \
    sigemptyset(&sigmask);                      \
    sigaddset(&sigmask, SIGCHLD);               \
    sigprocmask(SIG_BLOCK, &sigmask, &osigmask)
#define UNBLOCK_SIGCHLD                         \
    sigprocmask(SIG_SETMASK, &osigmask, NULL)

sigset_t sigmask;
sigset_t osigmask;

typedef void (*executor)(struct slot *pslot, int argc, char **argv);

#define UPLOAD "0"
#define DOWNLOAD "1"

volatile int alive_children = 0;

static void
print_line(struct slot *pslot, int io_type, sstring buf, void *data) {
    FILE *output = (FILE *) data;
    if (stdout_isatty) {
        if (io_type == STDOUT_FILENO) {
            printf(ANSI_COLOR_GREEN "[O] %s -> " ANSI_COLOR_RESET, pslot->host);
        } else {
            printf(ANSI_COLOR_GREEN "[E] %s -> " ANSI_COLOR_RESET, pslot->host);
        }
    }
    if (output != stdout) {
        fwrite(buf, 1, string_length(buf), output);
        fflush(output);
    }
    fwrite(buf, 1, string_length(buf), stdout);
    fflush(stdout);
}

static char *
server_specific_directory(const char *path, const struct slot *pslot) {
    char nameBuf[1024];
    char *end = nameBuf;
    size_t chars;
    size_t remain = 1023;

    // find text after last /
    char *name = strrchr(path, '/') + 1;

    chars = name - path;
    if (chars > remain) {
        return NULL;
    }
    memcpy(end, path, chars);
    remain -= chars;
    end += chars;

    // append host
    chars = strlen(pslot->host);
    if (chars > remain) {
        return NULL;
    }
    strncpy(end, pslot->host, remain);
    remain -= chars;
    end += chars;
    
    mkdir(nameBuf, S_IRWXU | S_IRWXG);

    // Append filename
    name--; // Take into account /
    
    chars = strlen(name);
    if (chars > remain) {
        return NULL;
    }

    strncpy(end, name, remain);
    remain -= chars;
    end += chars;

    char *dirName = malloc(end - nameBuf + 1);
    strcpy(dirName, nameBuf);
    return dirName;
}

//return success flag
int
insure_folder_exists(char *subFolder) {

    char nameBuf[1024];
    size_t chars;
    const char *homeDir = getenv("HOME");
    if (!homeDir) {
        eprintf("no home directory set\n");
        return false;
    }

	//creates the parent logs folder
    chars = snprintf(nameBuf, 1024, "%s/%s", homeDir,subFolder);
    if (chars >= 1024) {
        eprintf("file name too long for %s\n", subFolder);
        return false;
    } else if (chars < 0) {
        eprintf("failed to encode file name for %s\n", subFolder);
        return false;
    }
    
    //return zero on success, or -1 if an error occurred
    return mkdir(nameBuf, S_IRWXU | S_IRWXG)==0;
}

static FILE *
server_log_file(const struct slot *pslot) {
    char nameBuf[1024];
    size_t chars;
    FILE *output;
    time_t now = time(NULL);

    const char *homeDir = getenv("HOME");
    if (!homeDir) {
        eprintf("no home directory set\n");
        return NULL;
    }

    insure_folder_exists(".hss");
	//creates the parent logs folder
    insure_folder_exists(".hss/logs");


    chars = strftime(nameBuf + chars, 1024 - chars, "%F.log", localtime(&now));
    if (chars == 0) {
        eprintf("file name too long for %s\n", pslot->host);
        return NULL;
    }

    char *fileName = server_specific_directory(nameBuf, pslot);
    if (fileName == NULL) {
        eprintf("file name too long for %s\n", pslot->host);
        return NULL;
    }

    output = fopen(fileName, "a");
    if (!output) {
        eprintf("can not open file %s (%s)\n", fileName, strerror(errno));
        free(fileName);
        return NULL;
    }
    free(fileName);

    return output;
}

static char *
scp_remote_filepath(const char *filename, struct slot *pslot) {
    char host[256];

    if (!strchr(pslot->host, '@')) {
        int err = snprintf(host, sizeof(host), "%s@%s", pconfig->user, pslot->host);
        if (err == 0) {
            eprintf("host too long for %s\n", pslot->host);
            return NULL;
        }
    } else {
        if (strlen(pslot->host) >= 256) {
            eprintf("host too long for %s\n", pslot->host);
            return NULL;
        }
        strcpy(host, pslot->host);
    }

    size_t filepathLen = strlen(filename) + strlen(host) + 2;
    char *filepath = malloc(filepathLen);
    snprintf(filepath, filepathLen, "%s:%s", host, filename);
    return filepath;
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
poll_alive_slots(struct slot *pslot, struct pollfd *pfd, size_t *cnt) {
    *cnt = 0;
    while (pslot) {
        if (!pslot->alive) {
            pslot->poll_index = -1;
            pslot = pslot->next;
            continue;
        }
        pslot->poll_index = (int) *cnt;

        pfd[*cnt].fd = pslot->io.out[PIPE_READ_END];
        pfd[*cnt].events = POLLIN;
        *cnt = *cnt + 1;

        pfd[*cnt].fd = pslot->io.err[PIPE_READ_END];
        pfd[*cnt].events = POLLIN;
        *cnt = *cnt + 1;

        pslot = pslot->next;
    }
}

static void
read_alive_slots(struct slot *pslot, struct pollfd *pfd) {
    while (pslot) {
        if (!pslot->alive || pslot->poll_index < 0) {
            pslot = pslot->next;
            continue;
        }
        if (pfd[pslot->poll_index].revents & POLLIN) {
            slot_read_line(pslot, STDOUT_FILENO, print_line, pslot->output);
        }
        if (pfd[pslot->poll_index + 1].revents & POLLIN) {
            slot_read_line(pslot, STDERR_FILENO, print_line, pslot->output);
        }
        pslot = pslot->next;
    }
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
        if (!pslot) {
            continue;
        }
        slot_read_remains(pslot, print_line, pslot->output);
        slot_close(pslot, exit_code);
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

    close(STDIN_FILENO);

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

    ssh_argv[idx++] = "TERM=xterm-mono";

    for (i = 0; i < argc; ++i) {
        ssh_argv[idx++] = argv[i];
    }
    ssh_argv[idx++] = NULL;


    if (pslot->output != stdout) {
        printf("$");
        for (int i = 0; i < argc; i++) {
            printf(" %s", argv[i]);
        }
        printf("\n");
        fflush(stdout);
    }

    ret = execvp("ssh", ssh_argv);

    eprintf("failed to exec the ssh binary: (%d) %s\n", ret, strerror(errno));
    exit(1);
}

static void
exec_scp_cmd(struct slot *pslot, int argc, char **argv) {
    char *scp_argv[128];
    int idx = 0;
    int i;
    int ret;

    close(STDIN_FILENO);

    close(pslot->io.out[PIPE_READ_END]);
    close(pslot->io.err[PIPE_READ_END]);

    if (dup2(pslot->io.out[PIPE_WRITE_END], STDOUT_FILENO) == -1) {
        eprintf("failed to dup stdout: %s\n", strerror(errno));
    }
    if (dup2(pslot->io.err[PIPE_WRITE_END], STDERR_FILENO) == -1) {
        eprintf("failed to dup stderr: %s\n", strerror(errno));
    }

    scp_argv[idx++] = "-q";
    scp_argv[idx++] = "-oNumberOfPasswordPrompts=0";
    scp_argv[idx++] = "-oStrictHostKeyChecking=no";

    for (i = 0; i < pconfig->common_options_argc; ++i) {
        scp_argv[idx++] = (char *) pconfig->common_options_argv[i];
    }

    for (i = 0; i < pslot->ssh_argc - 1; ++i) {
        scp_argv[idx++] = pslot->ssh_argv[i];
    }
    if (argc < 1) {
        eprintf("scp requires at least one argument\n");
        exit(1);
    } else if (argc == 1) {
        // Transfer file to /tmp
        scp_argv[idx++] = argv[0];
        scp_argv[idx++] = scp_remote_filepath("/tmp/", pslot);
    } else if (argc == 2) {
        if (strncmp(argv[0], "remote:", 7) == 0) {
            char dstBuffer[1024];
            if (strlen(argv[1]) > 1022) {
                eprintf("destination filename too long for %s\n", pslot->host);
            }
            if (argv[1][strlen(argv[1]) - 1] != '/') {
                strncpy(dstBuffer, argv[1], 1022);
                strncat(dstBuffer, "/", 1023);
            } else {
                strncpy(dstBuffer, argv[1], 1023);
            }
            mkdir(dstBuffer, S_IRWXU | S_IRWXG);

            scp_argv[idx++] = scp_remote_filepath(argv[0] + 7, pslot);
            scp_argv[idx++] = server_specific_directory(dstBuffer, pslot);
            if (scp_argv[idx - 1] == NULL) {
                eprintf("destination filename too long for %s\n", pslot->host);
            }
        } else if (strncmp(argv[1], "remote:", 7) == 0) {
            scp_argv[idx++] = argv[0];
            scp_argv[idx++] = scp_remote_filepath(argv[1] + 7, pslot);
        } else {
            eprintf("no remote file specified (\"%s\", \"%s\")\n", argv[0], argv[1]);
            exit(1);
        }
    } else {
        eprintf("scp requires at most two arguments, %i provided\n", argc);
        exit(1);
    }
    scp_argv[idx++] = NULL;

    ret = execvp("scp", scp_argv);

    eprintf("failed to exec the scp binary: (%d) %s\n", ret, strerror(errno));
    exit(1);
}

executor executors[] = {
    [MODE_SSH] = exec_ssh_cmd,
    [MODE_SCP] = exec_scp_cmd,
};

static int
exec_command_foreach(struct slot *pslot_list, executor fn_fork, int argc, char **argv,
                     int concurrency) {
    struct pollfd *pfd;
    struct slot *pslot_head = pslot_list->next;
    struct slot *pslot = pslot_head;
    int timeout;
    FILE *output = NULL;
    size_t cnt = 0;

    if (!pslot) {
        return 0;
    }

    if (pconfig->output_file && !pconfig->split_server_logs) {
        output = fopen(pconfig->output_file, "a");
        if (!output) {
            eprintf("can not open file %s (%s)\n", pconfig->output_file, strerror(errno));
            return -1;
        }
    } else {
        output = stdout;
    }

    while (pslot) {
        // poll stdout & stderr
        cnt += 2;
        if (pconfig->split_server_logs) {
            pslot->output = server_log_file(pslot);
            if (!pslot->output) {
                return -1;
            }
        } else {
            pslot->output = output;
        }
        pslot = pslot->next;
    }
    pfd = calloc(cnt, sizeof(struct pollfd));
    pslot = pslot_head;

    alive_children = 0;

    while (pslot || alive_children) {
        BLOCK_SIGCHLD;
        if (pslot) {
            if (concurrency <= 0 || alive_children < concurrency) {
                fork_command(pslot, fn_fork, argc, argv);
                pslot = pslot->next;
                usleep(10 * 1000);
            }
        }

        poll_alive_slots(pslot_head, pfd, &cnt);

        timeout = pslot ? 0 : -1;

        if (poll(pfd, (nfds_t) cnt, timeout) > 0) {
            read_alive_slots(pslot_head, pfd);
        }
        UNBLOCK_SIGCHLD;
    }

    if (pconfig->split_server_logs) {
        pslot = pslot_head;
        while (pslot) {
            fclose(pslot->output);
            pslot = pslot->next;
        }
    } else if (output != stdout) {
        fclose(output);
    }

    free(pfd);
    return 0;
}

int
exec_remote_cmd(struct slot *pslot_list, char *cmd, int concurrency) {
    const int argc = 1;
    char *argv[] = {cmd};
    return exec_remote_cmd_args(pslot_list, argc, argv, concurrency);
}

int
exec_remote_cmd_args(struct slot *pslot_list, int argc, char **argv, int concurrency) {
    return exec_command_foreach(pslot_list, executors[pconfig->mode], argc, argv, concurrency);
}

int
sync_exec_remote_cmd(struct slot *pslot_list, char *cmd, sstring *out, sstring *err) {
    struct pollfd pfd[2];
    char *argv[1] = {cmd};
    struct slot *pslot = pslot_list->next;

    if (!pslot) {
        return 0;
    }

    memset(&pfd[0], 0, sizeof(struct pollfd));
    memset(&pfd[1], 0, sizeof(struct pollfd));

    alive_children = 0;

    BLOCK_SIGCHLD;
    fork_command(pslot, exec_ssh_cmd, 1, argv);
    pfd[0].fd = pslot->io.out[PIPE_READ_END];
    pfd[0].events = POLLIN;
    pfd[1].fd = pslot->io.err[PIPE_READ_END];
    pfd[1].events = POLLIN;
    UNBLOCK_SIGCHLD;

    while (alive_children) {
        BLOCK_SIGCHLD;
        if (poll(pfd, 2, 0) > 0) {
            if (pfd[0].revents & POLLIN) {
                slot_read_line(pslot, STDOUT_FILENO, save_string, out);
            }
            if (pfd[1].revents & POLLIN) {
                slot_read_line(pslot, STDERR_FILENO, save_string, err);
            }
        }
        UNBLOCK_SIGCHLD;
    }
    return 0;
}

