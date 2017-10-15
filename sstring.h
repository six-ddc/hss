#ifndef _HSS_SSTRING_H_
#define _HSS_SSTRING_H_

#include <string.h>

typedef char *sstring;

sstring
new_stringlen(const char *buf, size_t len);

sstring
new_string(const char *buf);

sstring
new_emptystring();

void
string_free(sstring s);

sstring
string_enlarge(sstring s, size_t addlen);

size_t
string_length(sstring s);

sstring
string_append(sstring s, const char *buf, size_t len);

sstring
string_append_char(sstring s, char c);

sstring
string_clear(sstring s);

sstring
string_dup(sstring s);

char **
string_split(char *str, char delim, size_t *ret_count);

int
parse_argv_string(const char *str, int *argc_ptr, const char ***argv_ptr);

#endif //_HSS_SSTRING_H_
