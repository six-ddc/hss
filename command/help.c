#include "../common.h"
#include "../command.h"
#include "help.h"

static int
inner_command_help(char *arg) {
    static int name_width = 10;
    struct command *pcmd = inner_commands;
    printf("Command:\n");
    while ((pcmd = pcmd->next) != NULL) {
        printf("%-*s : %s\n", name_width, pcmd->name, pcmd->desc);
    }
    return 0;
}

struct command *
register_help() {
    struct command *pcmd = malloc(sizeof(struct command));
    pcmd->name = "help";
    pcmd->func = inner_command_help;
    pcmd->desc = "display this message";
    pcmd->next = NULL;
    return pcmd;
}
