#include "../common.h"
#include "../command.h"
#include "../executor.h"
#include "upload.h"

static void
print_upload_usage() {
    eprintf("Usage: upload <local_path> <remote_path>\n");
}

static int
inner_command_upload(char *args) {
    int argc;
    const char **argv;
    int ret;
    if (!*args) {
        print_upload_usage();
        return 0;
    }
    ret = parse_argv_string(args, &argc, &argv);
    if (ret != 0) {
        eprintf("unable parse command \"%s\"\n", args);
        return -1;
    }
    if (argc < 2) {
        printf("\"upload\" requires at least 2 argument\n\n");
        print_upload_usage();
        return -1;
    }
    upload_file(slots, (char *) argv[0], (char *) argv[1]);
    free(argv);
    return 0;
}

struct command *
register_upload() {
    struct command *pcmd = malloc(sizeof(struct command));
    pcmd->name = "upload";
    pcmd->func = inner_command_upload;
    pcmd->desc = "upload file";
    pcmd->next = NULL;
    return pcmd;
}
