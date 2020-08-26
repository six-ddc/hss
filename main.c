#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <setjmp.h>
#include <getopt.h>

#include "common.h"
#include "executor.h"
#include "completion.h"
#include "slot.h"

const char *HSS_VERSION = "1.7";

char history_file[512];

volatile bool sigint_interrupt_enabled = false;
sigjmp_buf sigint_interrupt_jmp;

struct slot *slots = NULL;

struct hss_config *pconfig = NULL;

int stdout_isatty = -1;

/* Forward declarations. */

static void
sigint_handler(int sig) {
    SAVE_ERRNO;

    /* if we are waiting for input, longjmp out of it */
    if (sigint_interrupt_enabled) {
        sigint_interrupt_enabled = true;
        siglongjmp(sigint_interrupt_jmp, 1);
    }

    /* just in case the write changed it */
    RESTORE_ERRNO;
}

static const char *
get_prompt() {
    if (stdout_isatty) {
        return "$ " ANSI_COLOR_BOLD;
    } else {
        return "$ ";
    }
}

static void
reset_prompt_color() {
    if (stdout_isatty) {
        printf(ANSI_COLOR_RESET);
    }
}

static void
update_completion_function() {
    rl_attempted_completion_function = remote_filepath_completion_func;
}

/*
 * Gets a line of interactive input by GNU readline.
 * The result is a malloc'd string.
 * Caller *must* have set up sigint_interrupt_jmp before calling.
 */
static char *
gets_interactive() {
    char *line;
    rl_reset_screen_size();

    sigint_interrupt_enabled = true;
    line = readline(get_prompt());
    reset_prompt_color();
    sigint_interrupt_enabled = false;

    return line;
}

static void
add_host(const char *args) {
    struct slot *pslot;
    pslot = new_slot(args);
    if (!pslot) {
        return;
    }
    slot_append(slots, pslot);
}

bool
isspace_string(char *str) {
    char c;
    while ((c = *str++) != '\0') {
        if (!isspace(c)) {
            return false;
        }
    }
    return true;
}

static int
add_hostfile(const char *fname) {
    char line[1024];
    char *p;
    FILE *f = fopen(fname, "r");
    if (!f) {
        if (errno == ENOENT) {
            // No file? Try ~/.hss directory
            char nameBuf[1024];

            char *homeDir = getenv("HOME");
            if (!homeDir) {
                eprintf("no home directory set\n");
                exit(1);
            }
            
            int chars = snprintf(nameBuf, 1024, "%s/.hss/%s", homeDir, fname);
            if (chars >= 1024) {
                eprintf("hostfile name is too long\n");
                exit(1);
            } else if (chars < 0) {
                eprintf("encoding error while locating hostfile %s\n", fname);
                exit(1);
            }

            f = fopen(nameBuf, "r");
        }
        if (!f) {
            eprintf("can not open file %s (%s)\n", fname, strerror(errno));
            exit(1);
        }
    }

    while (fgets(line, sizeof(line), f)) {
        p = line;
        while (isspace(*p)) p++;
        if (*p == '\0') continue;
        add_host(p);
    }

    fclose(f);
    return 0;
}

void usage(const char *msg) {
    if (!msg) {
        eprintf("An interactive parallel ssh client.\n"
                "\n"
                "Usage: hss [-f hostfile] [-o file] [-u username] [command]...\n\n"
                "Options:\n"
                "  -f file        file with the list of hosts\n"
                "  -H host        specifies a host option, support the same options as the ssh command\n"
                "  -l limit       number of multiple ssh to perform at a time (default: unlimited)\n"
                "  -u user        the default user name to use when connecting to the remote server\n"
                "  -c opts        specify the common ssh options (i.e. '-p 22 -i identity_file')\n"
                "  -o file        write remote command output to a file\n"
                "  -O             write remote command output to log file per server\n"
                "  -i             force use a vi-style line editing interface\n"
                "  -s             transfer files over scp\n"
                "  -v             be more verbose\n"
                "  -V             show program version\n"
                "  -h             display this message\n"
                "\n"
                "For more information, see https://github.com/six-ddc/hss"
                "\n"
        );
        exit(0);
    } else {
        eprintf("%s\n", msg);
        exit(1);
    }
}

void print_version() {
    printf("hss - %s\n", HSS_VERSION);
    printf("libreadline - %s\n", rl_library_version);
    exit(0);
}

void
parse_opts(int argc, char **argv) {
    int ret;
    int opt;

    const char *short_opts = "hif:H:c:u:o:Ol:svV";

    pconfig = calloc(1, sizeof(struct hss_config));

    while ((opt = getopt(argc, argv, short_opts)) != -1) {
        switch (opt) {
            case 'h':
                usage(NULL);
                break;
            case 'i':
                rl_editing_mode = 0;
                break;
            case 'f':
                add_hostfile(optarg);
                break;
            case 'H':
                add_host(optarg);
                break;
            case 'l':
                pconfig->concurrency = atoi(optarg);
                break;
            case 'c':
                ret = parse_argv_string(optarg, &pconfig->common_options_argc, &pconfig->common_options_argv);
                if (ret != 0) {
                    eprintf("unable parse options \"%s\"\n", optarg);
                    exit(1);
                }
                break;
            case 'u':
                pconfig->user = optarg;
                break;
            case 'o':
                pconfig->output_file = new_string(optarg);
                break;
            case 'O':
                pconfig->split_server_logs = true;
                break;
            case 's':
                pconfig->mode = MODE_SCP;
                break;
            case 'v':
                pconfig->verbose = true;
                break;
            case 'V':
                print_version();
                break;
            default:
                usage(NULL);
        }
    }
    argc -= optind;
    argv += optind;

    if (slots->next == NULL) {
        eprintf("ssh slots is empty\n");
    }

    if (argc == 0) {
        if (!stdout_isatty && isatty(STDIN_FILENO)) {
            usage("missing command parameter");
        }
        return;
    }
    exec_remote_cmd_args(slots, argc, argv, pconfig->concurrency);
    exit(0);
}

int
startup_hook() {
    reset_prompt_color();
    return 0;
}

int
main(int argc, char **argv) {

    int ret = 0;
    char *line;

    slots = calloc(1, sizeof(struct slot));
    stdout_isatty = isatty(STDOUT_FILENO);

    /* Set the default locale values according to environment variables. */
    setlocale(LC_ALL, "");
    signal(SIGCHLD, reap_child_handler);

    parse_opts(argc, argv);

    signal(SIGINT, sigint_handler);

    /* initialize readline */
    rl_startup_hook = startup_hook;
    rl_readline_name = "hss";
    rl_initialize();

    using_history();
    insure_folder_exists(".hss");
    insure_folder_exists(".hss/logs");    
    sprintf(history_file, "%s/.hss/logs/%s", getenv("HOME"), "history.log");
    read_history(history_file);

    update_completion_function();

    while (1) {
        /* Establish longjmp destination for exiting from wait-for-input. We
           must re-do this each time through the loop for safety, since the
           jmpbuf might get changed during command execution. */
        if (sigsetjmp(sigint_interrupt_jmp, 1) != 0) {
            /* got here with longjmp */
            putc('\n', stdout);
        }
        fflush(stdout);

        line = gets_interactive();

        if (line == NULL || strcmp(line, "exit") == 0) {
            if (line == NULL)
                putc('\n', stdout);
            printf("exit\n");
            break;
        }
        if (*line) {
            add_history(line);
            ret = write_history(history_file);
            if (ret != 0) {
                eprintf("failed to write history: %s\n", strerror(errno));
            }
            exec_remote_cmd(slots, line, pconfig->concurrency);
        }
        rl_free(line);
    }

    return 0;
}
