#include "common.h"
#include "slot.h"

void
slot_read_line(struct slot *pslot, int io_type, fn_getline cb, void *cb_data) {
    int fd;
    sstring *buff;
    char c;
    ssize_t i;
    switch (io_type) {
        case STDOUT_FILENO:
            fd = pslot->io.out[PIPE_READ_END];
            buff = &pslot->out_buff;
            break;
        case STDERR_FILENO:
            fd = pslot->io.err[PIPE_READ_END];
            buff = &pslot->err_buff;
            break;
        default:
            return;
    }
    while (1) {
        i = read(fd, &c, 1);
        if (i == 0) break;
        if (i < 0) {
            if (errno == EINTR) continue;
            break;
        }
        *buff = string_append_char(*buff, c);
        if (c == '\n') {
            cb(pslot, io_type, *buff, cb_data);
            *buff = string_clear(*buff);
        }
    }
}

void
slot_read_remains(struct slot *pslot, int io_type, fn_getline cb, void *cb_data) {
    sstring *buff;
    switch (io_type) {
        case STDOUT_FILENO:
            buff = &pslot->out_buff;
            break;
        case STDERR_FILENO:
            buff = &pslot->err_buff;
            break;
        default:
            return;
    }
    if (!(*buff) || !string_length(*buff)) {
        return;
    }
    cb(pslot, io_type, *buff, cb_data);
    string_free(*buff);
    *buff = NULL;
}

struct slot *
new_slot(const char *args) {
    int ret;
    struct slot *pslot = calloc(1, sizeof(struct slot));
    if (!pslot) {
        eprintf("failed to alloc slot: %s\n", strerror(errno));
        exit(1);
    }
    ret = parse_argv_string(args, &pslot->ssh_argc, (const char ***) &pslot->ssh_argv);
    if (ret != 0 || pslot->ssh_argc == 0) {
        eprintf("failed to parse ssh options \"%s\" into args array", args);
        free(pslot);
        return NULL;
    }
    strcpy(pslot->host, pslot->ssh_argv[pslot->ssh_argc - 1]);
    pslot->out_buff = new_emptystring();
    pslot->err_buff = new_emptystring();
    pslot->exit_code = -1;
    pslot->alive = false;
    pslot->pid = -1;
    return pslot;
}

void
slot_reinit(struct slot *pslot) {
    if (pipe(pslot->io.out) == -1) {
        eprintf("pipe error: %s\n", strerror(errno));
    }
    if (pipe(pslot->io.err) == -1) {
        eprintf("pipe error: %s\n", strerror(errno));
    }
    fcntl(pslot->io.out[PIPE_READ_END], F_SETFL, O_NONBLOCK);
    fcntl(pslot->io.err[PIPE_READ_END], F_SETFL, O_NONBLOCK);
    pslot->pid = -1;
    string_free(pslot->out_buff);
    pslot->out_buff = new_emptystring();
    string_free(pslot->err_buff);
    pslot->err_buff = new_emptystring();
}

void
slot_append(struct slot *pslot, struct slot *next) {
    while (pslot->next) {
        pslot = pslot->next;
    }
    pslot->next = next;
}

void
slot_close(struct slot *pslot, int exit_code) {
    pslot->exit_code = exit_code;
    pslot->alive = false;
}

void
slot_free(struct slot *pslot) {
    if (pslot->ssh_argv) {
        free(pslot->ssh_argv);
    }
    string_free(pslot->out_buff);
    string_free(pslot->err_buff);
    free(pslot);
}

struct slot *
slot_find_by_pid(struct slot *pslot, int pid) {
    while (pslot) {
        if (pslot->pid == pid) {
            return pslot;
        }
        pslot = pslot->next;
    }
    return NULL;
}

void
print_slot_args(struct slot *pslot) {
    int i;
    for (i = 0; i < pslot->ssh_argc; ++i) {
        printf("%s", pslot->ssh_argv[i]);
        if (i != pslot->ssh_argc - 1) {
            printf(" ");
        }
    }
}

struct slot *
slot_del_by_host(struct slot *pslot, const char *host) {
    struct slot *ret = pslot;
    struct slot *prev_pslot = NULL;
    while (pslot) {
        if (strcmp(pslot->host, host) == 0) {
            if (prev_pslot == NULL) {
                ret = pslot->next;
            } else {
                prev_pslot->next = pslot->next;
            }
            printf("deleted: ");
            printf("%s [", pslot->host);
            print_slot_args(pslot);
            printf("]\n");
            slot_free(pslot);
        }
        prev_pslot = pslot;
        pslot = pslot->next;
    }
    return ret;
}
