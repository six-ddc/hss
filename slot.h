#ifndef _HSS_SLOT_H_
#define _HSS_SLOT_H_

#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "sstring.h"

#define PIPE_READ_END 0
#define PIPE_WRITE_END 1

struct
stdio_pipe {
    int out[2];
    int err[2];
};

struct
slot {
    char host[1024];
    int ssh_argc;
    char **ssh_argv;
    int pid;
    int exit_code;
    bool alive;

    sstring out_buff;
    sstring err_buff;
    struct stdio_pipe io;

    struct slot *next;
};

typedef
void (*fn_getline)(struct slot *pslot, int io_type, sstring str, void *data);

void
slot_read_line(struct slot *pslot, int io_type, fn_getline cb, void *cb_data);

void
slot_read_remains(struct slot *pslot, int io_type, fn_getline cb, void *cb_data);

struct slot *
new_slot(const char *args);

void
slot_reinit(struct slot *pslot);

void
slot_append(struct slot *pslot, struct slot *next);

void
slot_close(struct slot *pslot, int exit_code);

void
slot_free(struct slot *pslot);

void
print_slot_args(struct slot *pslot);

struct slot *
slot_find_by_pid(struct slot *pslot, int pid);

struct slot *
slot_del_by_host(struct slot *pslot, const char *host);

#endif //_HSS_SLOT_H_
