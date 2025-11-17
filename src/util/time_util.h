#ifndef TIME_UTIL_H
#define TIME_UTIL_H

#include <stddef.h>

/* Buffer size for "YYYY-MM-DD HH:MM:SS" (19) + NUL + margin */
#define TIME_LEN 25

void get_now_iso(char *buf, size_t buflen);

#endif /* TIME_UTIL_H */