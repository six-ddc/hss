#ifndef _HSS_COMMON_H_
#define _HSS_COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>

#include <readline/readline.h>
#include <readline/history.h>


#ifndef NOT_USER_COLOR
// RL_PROMPT_START_IGNORE RL_PROMPT_END_IGNORE
#define ANSI_COLOR_RED     "\033[0;31m"
#define ANSI_COLOR_GREEN   "\033[0;32m"
#define ANSI_COLOR_YELLOW  "\033[0;33m"
#define ANSI_COLOR_BLUE    "\033[0;34m"
#define ANSI_COLOR_MAGENTA "\033[0;35m"
#define ANSI_COLOR_CYAN    "\033[0;36m"

#define ANSI_COLOR_BOLD         "\033[1m"
#define ANSI_COLOR_RED_BOLD     "\033[1;31m"
#define ANSI_COLOR_GREEN_BOLD   "\033[1;31m"
#define ANSI_COLOR_YELLOW_BOLD  "\033[1;31m"
#define ANSI_COLOR_BLUE_BOLD    "\033[1;31m"
#define ANSI_COLOR_MAGENTA_BOLD "\033[1;31m"
#define ANSI_COLOR_CYAN_BOLD    "\033[1;31m"

#define ANSI_COLOR_RESET   "\033[0m"

/* Color array for per-host output cycling */
static const char *HOST_COLORS[] = {
    ANSI_COLOR_GREEN,
    ANSI_COLOR_YELLOW,
    ANSI_COLOR_BLUE,
    ANSI_COLOR_MAGENTA,
    ANSI_COLOR_CYAN
};
#define HOST_COLORS_COUNT 5

#else
#define ANSI_COLOR_RED
#define ANSI_COLOR_GREEN
#define ANSI_COLOR_YELLOW
#define ANSI_COLOR_BLUE
#define ANSI_COLOR_MAGENTA
#define ANSI_COLOR_CYAN
#define ANSI_COLOR_RESET
#endif

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

#define SAVE_ERRNO int saved_errno = errno
#define RESTORE_ERRNO errno = saved_errno

extern struct slot *slots;

extern struct hss_config {
    bool verbose;
    char *user;
    int common_options_argc;
    const char **common_options_argv;
    char *output_file;
    int concurrency;
} *pconfig;

extern int stdout_isatty;

#endif //_HSS_COMMON_H_
