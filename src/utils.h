#ifndef _MR_UTILS_H_
#define _MR_UTILS_H_

#include <stdbool.h>

void die(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
bool is_sep(char c);

#endif
