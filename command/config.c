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
            "  common-options <filename> : common ssh options\n"
    );
}

static sstring
get_common_options() {
    sstring args = new_emptystring();
    int i = 0;
    for (i = 0; i < pconfig->common_options_argc; ++i) {
        args = string_append(args, pconfig->common_options_argv[i], strlen(pconfig->common_options_argv[i]));
        if (i != pconfig->common_options_argc - 1) {
            args = string_append_char(args, ' ');
        }
    }
    return args;
}

static int
config_get(int argc, const char **argv) {
    static int name_width = 15;
    sstring args = NULL;
    if (argc == 1 || strcmp(argv[1], "all") == 0) {
        if (pconfig->output_file) {
            printf("%-*s \"%s\"\n", name_width, "output", pconfig->output_file);
        } else {
            printf("%-*s -\n", name_width, "output");
        }
        args = get_common_options();
        printf("%-*s \"%s\"\n", name_width, "common-options", args);
    } else {
        if (strcmp(argv[1], "output") == 0) {
            if (pconfig->output_file) {
                printf("%-*s \"%s\"\n", name_width, "output", pconfig->output_file);
            } else {
                printf("%-*s -\n", name_width, "output");
            }
        } else if (strcmp(argv[1], "common-options") == 0) {
            args = get_common_options();
            printf("%-*s \"%s\"\n", name_width, "common-options", args);
        } else {
            eprintf("invalid config \"%s\"\n", argv[1]);
        }
    }
    string_free(args);
    return 0;
}

static int
config_set(int argc, const char **argv) {
    int ret;
    if (argc < 2) {
        printf("\"config set\" requires at least 1 argument\n\n");
        print_config_usage();
        return -1;
    }
    if (strcmp(argv[1], "output") == 0) {
        if (pconfig->output_file) {
            string_free(pconfig->output_file);
        }
        if (argc < 3 || strcmp(argv[2], "-") == 0) {
            pconfig->output_file = NULL;
        } else {
            pconfig->output_file = new_string(argv[2]);
        }
    } else if (strcmp(argv[1], "common-options") == 0) {
        if (argc < 3) {
            pconfig->common_options_argc = 0;
            free(pconfig->common_options_argv);
            pconfig->common_options_argv = NULL;
        } else {
            ret = parse_argv_string(argv[2], &pconfig->common_options_argc, &pconfig->common_options_argv);
            if (ret != 0) {
                eprintf("unable parse options \"%s\"\n", argv[2]);
                return 1;
            }
        }
    } else {
        eprintf("invalid config \"%s\"\n", argv[1]);
        return 1;
    }
    return 0;
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
    pcmd->next = NULL;
    return pcmd;
}
