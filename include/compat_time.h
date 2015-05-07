/*--------------------------------------------------------------------
 * MAC OS/X is incredibly moronic when it comes to time and such...
 */

#include <time.h>
#include <sys/time.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 0
#define CLOCK_REALTIME 0

static int clock_gettime(int foo, struct timespec *ts)
{
        struct timeval tv;

        gettimeofday(&tv, NULL);
        ts->tv_sec = tv.tv_sec;
        ts->tv_nsec = tv.tv_usec * 1000;
        return (0);
}

static int pthread_condattr_setclock(pthread_condattr_t *attr, int foo)
{
  (void)attr;
  (void)foo;
  return (0);
}

#endif /* !CLOCK_MONOTONIC */

