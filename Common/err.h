/*
 * Based on MIM UW code (Concurrent Programming lab sample code)
 * */

#ifndef _ERR_
#define _ERR_

#include <cerrno>

/* print system call error message and terminate */
extern void syserr(int bl, const char *fmt, ...);

/* print error message and terminate */
extern void fatal(const char *fmt, ...);

/* Prettifying macro */
#define verify(action, message) \
do { \
    if ((action) == -1) \
        syserr(errno, message); \
} while (0)

#endif
