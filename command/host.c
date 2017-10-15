#include "../common.h"
#include "../command.h"
#include "../slot.h"

static void
print_host_usage() {
    eprintf("Usage: host <command>\n"
                    "Commands:\n"
                    "  list               : list all ssh slots\n"
                    "  add <ssh_options>  : add a ssh slot\n"
                    "  del <ssh_host>     : delete special ssh slot\n"
    );
}

static int
host_list(int argc, const char **argv) {
    int max_len = 0;
    struct slot *pslot = slots;
    int len;
    while (pslot) {
        len = (int) strlen(pslot->host);
        max_len = len > max_len ? len : max_len;
        pslot = pslot->next;
    }
    printf("%-*s exit_code options\n", max_len, "host");
    pslot = slots;
    while (pslot) {
        printf("%-*s %-*d \"", max_len, pslot->host,
               (int) strlen("exit_code"), pslot->exit_code);
        print_slot_args(pslot);
        printf("\"\n");
        pslot = pslot->next;
    }
    return 0;
}

static int
host_del(int argc, const char **argv) {
    if (argc < 2) {
        printf("\"host del\" requires at least 1 argument\n\n");
        print_host_usage();
        return -1;
    }
    slots = slot_del_by_host(slots, argv[1]);
    return 0;
}

static int
host_add(int argc, const char **argv) {
    struct slot *pslot;
    if (argc < 2) {
        printf("\"host add\" requires at least 1 argument\n\n");
        print_host_usage();
        return -1;
    }
    pslot = new_slot(argv[1]);
    if (!pslot) {
        return -1;
    }
    if (!slots) {
        slots = pslot;
    } else {
        slot_append(slots, pslot);
    }
    return 0;
}

static int
inner_command_host(char *args) {
    int argc;
    const char **argv;
    int ret;
    if (!*args) {
        print_host_usage();
        return 0;
    }
    ret = parse_argv_string(args, &argc, &argv);
    if (ret != 0) {
        eprintf("unable parse command [%s]\n", args);
        return -1;
    }
    if (strcmp(argv[0], "list") == 0) {
        host_list(argc, argv);
    } else if (strcmp(argv[0], "help") == 0 || strcmp(argv[0], "-h") == 0) {
        print_host_usage();
    } else if (strcmp(argv[0], "add") == 0) {
        host_add(argc, argv);
    } else if (strcmp(argv[0], "del") == 0) {
        host_del(argc, argv);
    } else {
        eprintf("invalid child command [%s]\n", argv[0]);
    }
    free(argv);
    return 0;
}

struct command *
register_host() {
    struct command *pcmd = malloc(sizeof(struct command));
    pcmd->name = "host";
    pcmd->func = inner_command_host;
    pcmd->desc = "operate host";
    pcmd->next = inner_commands;
    return pcmd;
}
