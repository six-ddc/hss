#include "../common.h"
#include "../command.h"
#include "help.h"

static int
inner_command_help(char *arg) {
    struct command *pcmd = inner_commands;
    printf("Command:\n");
    while (pcmd) {
        printf("  %s\t: %s\n", pcmd->name, pcmd->desc);
        pcmd = pcmd->next;
    }
    return 0;
}

struct command *
register_help() {
    struct command *pcmd = malloc(sizeof(struct command));
    pcmd->name = "help";
    pcmd->func = inner_command_help;
    pcmd->desc = "display this message";
    pcmd->next = inner_commands;
    return pcmd;
}
