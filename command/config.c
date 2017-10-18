#include "../common.h"
#include "../command.h"
#include "../sstring.h"
#include "config.h"

static void
print_config_usage() {
    eprintf("Usage: config <command>\n"
            "\n"
            "Commands:\n"
            "  get    all|<config>       : get config\n"
            "  set    <config> [value]   : set config\n"
            "\n"
            "Config:\n"
            "  output <filename>         : redirect output to a file. stdout is used if filename is '-'\n"
            "  conn-timeout <filename>   : ssh connect timeout\n"
    );
}

static int
config_get(int argc, const char **argv) {
    static int name_width = 15;
    if (argc == 1 || strcmp(argv[1], "all") == 0) {
        printf("%-*s %s\n", name_width, "output", pconfig->output_file ? pconfig->output_file : "-");
        printf("%-*s %d\n", name_width, "conn-timeout", pconfig->conn_timeout);
    } else {
        if (strcmp(argv[1], "output") == 0) {
            printf("%-*s %s\n", name_width, "output", pconfig->output_file ? pconfig->output_file : "-");
        } else if (strcmp(argv[1], "conn-timeout") == 0) {
            printf("%-*s %d\n", name_width, "conn-timeout", pconfig->conn_timeout);
        } else {
            eprintf("invalid config \"%s\"\n", argv[1]);
        }
    }
    return 0;
}

static int
config_set(int argc, const char **argv) {
    if (argc < 2) {
        printf("\"config set\" requires at least 1 argument\n\n");
        print_config_usage();
        return -1;
    }
    if (strcmp(argv[1], "output") == 0) {
        if (argc < 3) {
            goto err;
        }
        if (pconfig->output_file) {
            string_free(pconfig->output_file);
        }
        pconfig->output_file = new_string(argv[2]);
    } else if (strcmp(argv[1], "conn-timeout") == 0) {
        pconfig->conn_timeout = (int) strtol(argv[2], NULL, 10);
    } else {
        eprintf("invalid config \"%s\"\n", argv[1]);
    }
    return 0;
    err:
    printf("\"config set %s\" requires at least 1 argument\n\n", argv[1]);
    print_config_usage();
    return 1;
}

static int
inner_command_config(char *args) {
    int argc;
    const char **argv;
    int ret;
    if (!*args) {
        print_config_usage();
        return 0;
    }
    ret = parse_argv_string(args, &argc, &argv);
    if (ret != 0) {
        eprintf("unable parse command \"%s\"\n", args);
        return -1;
    }
    if (strcmp(argv[0], "get") == 0) {
        config_get(argc, argv);
    } else if (strcmp(argv[0], "set") == 0) {
        config_set(argc, argv);
    } else {
        eprintf("invalid child command \"%s\"\n", argv[0]);
    }
    free(argv);
    return 0;
}

struct command *
register_config() {
    struct command *pcmd = malloc(sizeof(struct command));
    pcmd->name = "config";
    pcmd->func = inner_command_config;
    pcmd->desc = "configuration manager";
    pcmd->next = inner_commands;
    return pcmd;
}
