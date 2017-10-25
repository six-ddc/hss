#include "../common.h"
#include "../command.h"
#include "../executor.h"
#include "download.h"

static void
print_download_usage() {
    eprintf("Usage: download <remote_path> <local_path>\n");
}

static int
inner_command_download(char *args) {
    int argc;
    const char **argv;
    int ret;
    if (!*args) {
        print_download_usage();
        return 0;
    }
    ret = parse_argv_string(args, &argc, &argv);
    if (ret != 0) {
        eprintf("unable parse command \"%s\"\n", args);
        return -1;
    }
    if (argc < 2) {
        printf("\"download\" requires at least 2 argument\n\n");
        print_download_usage();
        return -1;
    }
    download_file(slots, (char *) argv[0], (char *) argv[1]);
    free(argv);
    return 0;
}

struct command *
register_download() {
    struct command *pcmd = malloc(sizeof(struct command));
    pcmd->name = "download";
    pcmd->func = inner_command_download;
    pcmd->desc = "download file";
    pcmd->next = NULL;
    return pcmd;
}
