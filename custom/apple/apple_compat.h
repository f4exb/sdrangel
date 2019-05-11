/*
 * APPLE Compatibility
 */

#ifdef __APPLE__

/**
 *  Missing POSIX Thread Barriers implementation
 */
#ifndef PTHREAD_BARRIER_H_
#define PTHREAD_BARRIER_H_

#include <pthread.h>
#include <errno.h>

typedef int pthread_barrierattr_t;
typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int tripCount;
} pthread_barrier_t;


int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count);

int pthread_barrier_destroy(pthread_barrier_t *barrier);

int pthread_barrier_wait(pthread_barrier_t *barrier);

#endif // PTHREAD_BARRIER_H_


// macOS < 10.12 doesn't have clock_gettime()
#include <time.h>
#if !defined(CLOCK_REALTIME) && !defined(CLOCK_MONOTONIC)

#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 6
typedef int clockid_t;

#include <sys/time.h>
#include <mach/mach_time.h>

// here to avoid problem on linking of qrtplib
inline int clock_gettime( clockid_t clk_id, struct timespec *ts )
{
  int ret = -1;
  if ( ts )
  {
    if      ( CLOCK_REALTIME == clk_id )
    {
      struct timeval tv;
      ret = gettimeofday(&tv, NULL);
      ts->tv_sec  = tv.tv_sec;
      ts->tv_nsec = tv.tv_usec * 1000;
    }
    else if ( CLOCK_MONOTONIC == clk_id )
    {
      const uint64_t t = mach_absolute_time();
      mach_timebase_info_data_t timebase;
      mach_timebase_info(&timebase);
      const uint64_t tdiff = t * timebase.numer / timebase.denom;
      ts->tv_sec  = tdiff / 1000000000;
      ts->tv_nsec = tdiff % 1000000000;
      ret = 0;
    }
  }
  return ret;
}

#endif // CLOCK_REALTIME and CLOCK_MONOTONIC

#endif // __APPLE__
