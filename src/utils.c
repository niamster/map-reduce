#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

void die(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    exit(1);
}

bool is_sep(char c) {
    switch (c) {
        case ' ':
        case '\t':
        case '\n':
        case '!':
        case ',':
        case '-':
        case '.':
        case ':':
        case ';':
        case '?':
            return true;
    }

    return false;
}
