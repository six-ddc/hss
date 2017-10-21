#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <setjmp.h>
#include <getopt.h>

#include "common.h"
#include "executor.h"
#include "completion.h"
#include "slot.h"
#include "command.h"
#include "command/help.h"
#include "command/host.h"
#include "command/config.h"

const char *HSS_VERSION = "1.0-beta";

char history_file[512];

volatile bool sigint_interrupt_enabled = false;
sigjmp_buf sigint_interrupt_jmp;

struct slot *slots = NULL;

const char *host_file = NULL;

struct hss_config *pconfig = NULL;

int default_conn_timeout = 30;

enum state {
    REMOTE,
    INNER,
    LOCAL,
} running_state = REMOTE;

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
    switch (running_state) {
        case REMOTE:
            return ANSI_COLOR_BOLD "[remote] >>> " ANSI_COLOR_RESET;
        case INNER:
            return ANSI_COLOR_MAGENTA_BOLD "[inner] >>> " ANSI_COLOR_RESET;
        case LOCAL:
            return ANSI_COLOR_CYAN_BOLD "[local] >>> " ANSI_COLOR_RESET;
        default:
            return "[unknown] >>> ";
    }
}

static void
update_completion_function() {
    switch (running_state) {
        case REMOTE:
            rl_attempted_completion_function = remote_filepath_completion_func;
            break;
        case INNER:
            rl_attempted_completion_function = inner_completion_func;
            break;
        case LOCAL:
            rl_attempted_completion_function = NULL;
            break;
        default:
            break;
    }

}

static int
key_esc_handler(int count, int key) {
    running_state = (running_state + 1) % LOCAL;
    update_completion_function();

    rl_on_new_line_with_prompt();
    rl_set_prompt(get_prompt());
    rl_redisplay();
    return 0;
}

/*
 * Gets a line of interactive input by GNU readline.
 * The result is a malloc'd string.
 * Caller *must* have set up sigint_interrupt_jmp before calling.
 */
static char *
gets_interactive(const char *prompt) {
    char *line;
    rl_reset_screen_size();

    sigint_interrupt_enabled = true;
    line = readline(get_prompt());
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
    FILE *f;
    char line[1024];
    char *p;
    if (strcmp(fname, "-") == 0) {
        f = stdin;
    } else {
        f = fopen(fname, "r");
        if (!f) {
            eprintf("Can not open file %s (%s)\n", fname, strerror(errno));
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

struct command *inner_commands = NULL;

void
init_inner_commands() {
    struct command* phelp = register_help();
    struct command* phost =register_host();
    struct command* pconfig =register_config();
    inner_commands->next = phelp;
    inner_commands->next->next = phost;
    inner_commands->next->next->next = pconfig;
}

void usage(const char *msg) {
    if (!msg) {
        eprintf("\n"
                "An interactive parallel ssh client.\n"
                "\n"
                "Usage: hss [-f hostfile] [-o file] [command]\n\n"
                "Options:\n"
                "  -f, --file=FILE           file with the list of hosts or - for stdin\n"
                "  -H, --host                specifies a host option\n"
                "  -i, --identity-file=FILE  specifies a identity (private key) authentication file\n"
                "  -t, --conn-timeout        ssh connect timeout (default %d sec)\n"
                "  -o, --output=FILE         write remote command output to a file\n"
                "  -v, --verbose             be more verbose (i.e. show ssh command)\n"
                "  -V, --version             show program version\n"
                "  -h, --help                display this message\n"
                "\n"
                "For more information, see https://github.com/six-ddc/hss"
                "\n", default_conn_timeout
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
    int opt;

    static struct option long_opts[] = {
            {"help",          no_argument,       NULL, 'h'},
            {"file",          required_argument, NULL, 'f'},
            {"host",          required_argument, NULL, 'H'},
            {"identity-file", required_argument, NULL, 'i'},
            {"conn-timeout",  required_argument, NULL, 't'},
            {"output",        required_argument, NULL, 'o'},
            {"verbose",       no_argument,       NULL, 'v'},
            {"version",       no_argument,       NULL, 'V'},
            {NULL,            0,                 NULL, 0}
    };
    const char *short_opts = "hf:H:i:t:o:Vv";

    pconfig = calloc(1, sizeof(struct hss_config));
    pconfig->conn_timeout = default_conn_timeout;

    while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (opt) {
            case 'h':
                usage(NULL);
                break;
            case 'f':
                add_hostfile(optarg);
                break;
            case 'H':
                add_host(optarg);
                break;
            case 'i':
                pconfig->identity_file = optarg;
                break;
            case 't':
                pconfig->conn_timeout = (int) strtol(optarg, NULL, 10);
                break;
            case 'o':
                pconfig->output_file = new_string(optarg);
                break;
            case 'v':
                pconfig->verbose = true;
                break;
            case 'V':
                print_version();
                break;
            default:
                usage(NULL);
                break;
        }
    }
    argc -= optind;
    argv += optind;

    if (slots->next == NULL) {
        eprintf("ssh slots is empty\n");
        running_state = INNER;
    }

    if (argc == 0) {
        if (!isatty(STDIN_FILENO)) {
            usage("missing command parameter");
        }
        return;
    }
    exec_remote_cmd(slots, argv[0]);
    exit(0);
}

int
main(int argc, char **argv) {

    int ret = 0;
    char *line;

    slots = calloc(1, sizeof(struct slot));
    inner_commands = calloc(1, sizeof(struct command));

    /* Set the default locale values according to environment variables. */
    setlocale(LC_ALL, "");
    signal(SIGCHLD, reap_child_handler);

    parse_opts(argc, argv);

    signal(SIGINT, sigint_handler);
    init_inner_commands();

    /* initialize readline */
    rl_readline_name = "hss";
    rl_bind_key(27, key_esc_handler);
    rl_initialize();

    using_history();
    sprintf(history_file, "%s/%s", getenv("HOME"), ".hss_history");
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

        line = gets_interactive(">>> ");

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
            switch (running_state) {
                case REMOTE:
                    exec_remote_cmd(slots, line);
                    break;
                case INNER:
                    exec_inner_cmd(line);
                    break;
                case LOCAL:
                    exec_local_cmd(line);
                    break;
                default:
                    break;
            }
        }
        rl_free(line);
    }

    return 0;
}
