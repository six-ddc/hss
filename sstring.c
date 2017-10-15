#include "common.h"
#include "sstring.h"

#define STRING_MAX_PREALLOC (1024*1024)
#define TO_INNER_STRING(s) (void *) ((s) - sizeof(struct inner_string))

struct inner_string {
    size_t len;
    size_t alloc;
    char data[];
};

sstring
new_stringlen(const char *buf, size_t len) {
    // +1 for trailing \0 byte
    struct inner_string *str = calloc(1, sizeof(struct inner_string) + len + 1);
    str->len = len;
    str->alloc = len;
    if (len && buf) {
        memcpy(str->data, buf, len);
    }
    str->data[len] = '\0';
    return str->data;
}

sstring
new_string(const char *buf) {
    return new_stringlen(buf, strlen(buf));
}

sstring
new_emptystring() {
    return new_stringlen("", 0);
}

void
string_free(sstring s) {
    struct inner_string *str;
    if (!s) return;
    str = TO_INNER_STRING(s);
    free(str);
}

sstring
string_enlarge(sstring s, size_t addlen) {
    struct inner_string *str = TO_INNER_STRING(s);
    size_t avail = str->alloc - str->len;
    struct inner_string *newstr;
    size_t newlen;
    if (avail >= addlen) {
        return s;
    }
    newlen = str->len + addlen;
    if (newlen < STRING_MAX_PREALLOC) {
        newlen *= 2;
    } else {
        newlen += STRING_MAX_PREALLOC;
    }
    newstr = malloc(sizeof(struct inner_string) + newlen + 1);
    if (!newstr)
        return NULL;
    memcpy(newstr->data, s, str->len + 1);
    newstr->len = str->len;
    newstr->alloc = newlen;
    string_free(s);
    return newstr->data;
}

size_t
string_length(sstring s) {
    struct inner_string *str = TO_INNER_STRING(s);
    return str->len;
}

sstring
string_append(sstring s, const char *buf, size_t len) {
    size_t curlen = string_length(s);
    struct inner_string *str;
    s = string_enlarge(s, len);
    if (!s)
        return NULL;
    str = TO_INNER_STRING(s);
    memcpy(str->data + curlen, buf, len);
    str->len = curlen + len;
    s[str->len] = '\0';
    return s;
}

sstring
string_append_char(sstring s, char c) {
    size_t curlen = string_length(s);
    struct inner_string *str;
    s = string_enlarge(s, 1);
    if (!s)
        return NULL;
    str = TO_INNER_STRING(s);
    str->data[curlen] = c;
    str->len = curlen + 1;
    s[str->len] = '\0';
    return s;
}

sstring
string_clear(sstring s) {
    struct inner_string *str = TO_INNER_STRING(s);
    memset(str->data, 0, str->len);
    str->len = 0;
    return s;
}

sstring
string_dup(sstring s) {
    return new_stringlen(s, string_length(s));
}

char **
string_split(char *str, char delim, size_t *ret_count) {
    char **result = 0;
    size_t count = 0;
    const char *last_delim = 0;
    char sep[] = {delim, '\0'};
    size_t idx = 0;
    char *token;

    /* Count how many elements will be extracted. */
    const char *tmp = str;
    while (*tmp) {
        if (delim == *tmp) {
            count++;
            last_delim = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    if (last_delim < (str + strlen(str) - 1)) {
        count++;
    }

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char *) * count);

    if (result) {
        token = strtok(str, sep);
        while (token) {
            *(result + idx++) = strdup(token);
            token = strtok(0, sep);
        }
        *(result + idx) = NULL;
        if (ret_count) {
            *ret_count = count - 1;
        }
    }

    return result;
}

#define ARGV_ARRAY_GROW_DELTA 5

int dup_argv(int argc, const char **argv,
             int *argc_ptr, const char ***argv_ptr) {
    size_t nb = (argc + 1) * sizeof(*argv);
    const char **argv2;
    char *dst;
    int i;

    if (argc <= 0 || argv == NULL)
        return -1;
    for (i = 0; i < argc; i++) {
        if (argv[i] == NULL)
            return -1;
        nb += strlen(argv[i]) + 1;
    }

    dst = malloc(nb);
    if (dst == NULL)
        return -1;
    argv2 = (void *) dst;
    dst += (argc + 1) * sizeof(*argv);
    *dst = '\0';

    for (i = 0; i < argc; i++) {
        argv2[i] = dst;
        dst = stpcpy(dst, argv[i]);
        dst++;    /* trailing NUL */
    }
    argv2[argc] = NULL;

    if (argv_ptr) {
        *argv_ptr = argv2;
    } else {
        free(argv2);
        argv2 = NULL;
    }
    if (argc_ptr)
        *argc_ptr = argc;
    return 0;
}

/* Code based on poptParseArgvString() from libpopt */
int
parse_argv_string(const char *s, int *argc_ptr, const char ***argv_ptr) {
    const char *src;
    char quote = '\0';
    int argv_alloced = ARGV_ARRAY_GROW_DELTA;
    const char **argv = malloc(sizeof(*argv) * argv_alloced);
    int argc = 0;
    size_t buflen = strlen(s) + 1;
    char *buf, *buf_orig = NULL;
    int rc = -1;

    if (argv == NULL)
        return rc;
    buf = buf_orig = calloc((size_t) 1, buflen);
    if (buf == NULL) {
        free(argv);
        return rc;
    }
    argv[argc] = buf;
    for (src = s; *src != '\0'; src++) {
        if (quote == *src) {
            quote = '\0';
        } else if (quote != '\0') {
            if (*src == '\\') {
                src++;
                if (!*src) {
                    rc = -1;
                    goto exit;
                }
                if (*src != quote) *buf++ = '\\';
            }
            *buf++ = *src;
        } else if (isspace(*src)) {
            if (*argv[argc] != '\0') {
                buf++, argc++;
                if (argc == argv_alloced) {
                    argv_alloced += ARGV_ARRAY_GROW_DELTA;
                    argv = realloc(argv, sizeof(*argv) * argv_alloced);
                    if (argv == NULL) goto exit;
                }
                argv[argc] = buf;
            }
        } else
            switch (*src) {
                case '"':
                case '\'':
                    quote = *src;
                    break;
                case '\\':
                    src++;
                    if (!*src) {
                        rc = -1;
                        goto exit;
                    }
                default:
                    *buf++ = *src;
                    break;
            }
    }

    if (strlen(argv[argc])) {
        argc++, buf++;
    }

    rc = dup_argv(argc, argv, argc_ptr, argv_ptr);

    exit:
    if (buf_orig) free(buf_orig);
    if (argv) free(argv);
    return rc;
}
