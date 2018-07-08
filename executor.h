#ifndef _HSS_EXECUTOR_H_
#define _HSS_EXECUTOR_H_

#include "sstring.h"

struct slot;

void
reap_child_handler(int sig);

int
exec_remote_cmd(struct slot *pslot_list, char *cmd);

int
sync_exec_remote_cmd(struct slot *pslot_list, char *cmd, sstring *out, sstring *err);

#endif //_HSS_EXECUTOR_H_
