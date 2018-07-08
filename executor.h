#ifndef _HSS_EXECUTOR_H_
#define _HSS_EXECUTOR_H_

#include "sstring.h"

struct slot;

void
reap_child_handler(int sig);

int
upload_file(struct slot* pslot_list, char* local_filename, char* remote_filename);

int
download_file(struct slot* pslot_list, char* remote_filename, char* local_filename);

int
exec_remote_cmd(struct slot *pslot_list, char *cmd);

int
sync_exec_remote_cmd(struct slot *pslot_list, char *cmd, sstring *out, sstring *err);

#endif //_HSS_EXECUTOR_H_
