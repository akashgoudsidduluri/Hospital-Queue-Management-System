#include "time_util.h"
#include <time.h>
#include <stdio.h>

void get_now_iso(char *buf, size_t buflen) {
    if (!buf || buflen == 0) return;

    time_t t = time(NULL);
    if (t == (time_t)-1) { buf[0] = '\0'; return; }

    struct tm tm;
#if defined(_MSC_VER)
    if (localtime_s(&tm, &t) != 0) { buf[0] = '\0'; return; }
#elif defined(_POSIX_VERSION) || defined(__linux__) || defined(__APPLE__)
    if (localtime_r(&t, &tm) == NULL) { buf[0] = '\0'; return; }
#elif defined(_WIN32)
    /* MinGW: localtime_s may not be present — copy from localtime() */
    {
        struct tm *tmp = localtime(&t);
        if (!tmp) { buf[0] = '\0'; return; }
        tm = *tmp;
    }
#else
    {
        struct tm *tmp = localtime(&t);
        if (!tmp) { buf[0] = '\0'; return; }
        tm = *tmp;
    }
#endif

    if (strftime(buf, buflen, "%Y-%m-%d %H:%M:%S", &tm) == 0) {
        if (buflen > 0) buf[0] = '\0';
    }
}