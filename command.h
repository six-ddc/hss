#ifndef _HSS_COMMAND_H
#define _HSS_COMMAND_H

#include "common.h"

struct command {
    char *name;
    rl_icpfunc_t *func;
    const char *desc;

    struct command *next;
};

#endif //_HSS_COMMAND_H
