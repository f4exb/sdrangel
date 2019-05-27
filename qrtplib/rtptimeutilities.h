/*

 This file is a part of JRTPLIB
 Copyright (c) 1999-2017 Jori Liesenborgs

 Contact: jori.liesenborgs@gmail.com

 This library was developed at the Expertise Centre for Digital Media
 (http://www.edm.uhasselt.be), a research center of the Hasselt University
 (http://www.uhasselt.be). The library is based upon work done for
 my thesis at the School for Knowledge Technology (Belgium/The Netherlands).

 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 IN THE SOFTWARE.

 */

/**
 * \file rtptimeutilities.h
 */

#ifndef RTPTIMEUTILITIES_H

#define RTPTIMEUTILITIES_H

#include "rtpconfig.h"
#include "rtptypes.h"
#ifndef RTP_HAVE_QUERYPERFORMANCECOUNTER
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#else
#include "Windows.h"
#endif // RTP_HAVE_QUERYPERFORMANCECOUNTER

#include "export.h"

#define RTP_NTPTIMEOFFSET									2208988800UL

#ifdef RTP_HAVE_VSUINT64SUFFIX
#define C1000000 1000000ui64
#define CEPOCH 11644473600000000ui64
#else
#define C1000000 1000000ULL
#define CEPOCH 11644473600000000ULL
#endif // RTP_HAVE_VSUINT64SUFFIX

#ifdef __APPLE__
#include "../custom/apple/apple_compat.h"
#endif

namespace qrtplib
{

/**
 * This is a simple wrapper for the most significant word (MSW) and least
 * significant word (LSW) of an NTP timestamp.
 */
class QRTPLIB_API RTPNTPTime
{
public:
    /** This constructor creates and instance with MSW \c m and LSW \c l. */
    RTPNTPTime(uint32_t m, uint32_t l)
    {
        msw = m;
        lsw = l;
    }

    /** Returns the most significant word. */
    uint32_t GetMSW() const
    {
        return msw;
    }

    /** Returns the least significant word. */
    uint32_t GetLSW() const
    {
        return lsw;
    }
private:
    uint32_t msw, lsw;
};

/** This class is used to specify wallclock time, delay intervals etc.
 *  This class is used to specify wallclock time, delay intervals etc.
 *  It stores a number of seconds and a number of microseconds.
 */
class QRTPLIB_API RTPTime
{
public:
    /** Returns an RTPTime instance representing the current wallclock time.
     *  Returns an RTPTime instance representing the current wallclock time. This is expressed
     *  as a number of seconds since 00:00:00 UTC, January 1, 1970.
     */
    static RTPTime CurrentTime();

    /** This function waits the amount of time specified in \c delay. */
    static void Wait(const RTPTime &delay);

    /** Creates an RTPTime instance representing \c t, which is expressed in units of seconds. */
    RTPTime(double t);

    /** Creates an instance that corresponds to \c ntptime.
     *  Creates an instance that corresponds to \c ntptime.  If
     *  the conversion cannot be made, both the seconds and the
     *  microseconds are set to zero.
     */
    RTPTime(RTPNTPTime ntptime);

    /** Creates an instance corresponding to \c seconds and \c microseconds. */
    RTPTime(int64_t seconds, uint32_t microseconds);

    /** Returns the number of seconds stored in this instance. */
    int64_t GetSeconds() const;

    /** Returns the number of microseconds stored in this instance. */
    uint32_t GetMicroSeconds() const;

    /** Returns the time stored in this instance, expressed in units of seconds. */
    double GetDouble() const
    {
        return m_t;
    }

    /** Returns the NTP time corresponding to the time stored in this instance. */
    RTPNTPTime GetNTPTime() const;

    RTPTime &operator-=(const RTPTime &t);
    RTPTime &operator+=(const RTPTime &t);
    bool operator<(const RTPTime &t) const;
    bool operator>(const RTPTime &t) const;
    bool operator<=(const RTPTime &t) const;
    bool operator>=(const RTPTime &t) const;

    bool IsZero() const
    {
        return m_t == 0.0;
    }
private:
#ifdef RTP_HAVE_QUERYPERFORMANCECOUNTER
    static inline uint64_t CalculateMicroseconds(uint64_t performancecount,uint64_t performancefrequency);
#endif // RTP_HAVE_QUERYPERFORMANCECOUNTER

    double m_t;
};

inline RTPTime::RTPTime(double t)
{
    m_t = t;
}

inline RTPTime::RTPTime(int64_t seconds, uint32_t microseconds)
{
    if (seconds >= 0)
    {
        m_t = (double) seconds + 1e-6 * (double) microseconds;
    }
    else
    {
        int64_t possec = -seconds;

        m_t = (double) possec + 1e-6 * (double) microseconds;
        m_t = -m_t;
    }
}

inline RTPTime::RTPTime(RTPNTPTime ntptime)
{
    if (ntptime.GetMSW() < RTP_NTPTIMEOFFSET)
    {
        m_t = 0;
    }
    else
    {
        uint32_t sec = ntptime.GetMSW() - RTP_NTPTIMEOFFSET;

        double x = (double) ntptime.GetLSW();
        x /= (65536.0 * 65536.0);
        x *= 1000000.0;
        uint32_t microsec = (uint32_t) x;

        m_t = (double) sec + 1e-6 * (double) microsec;
    }
}

inline int64_t RTPTime::GetSeconds() const
{
    return (int64_t) m_t;
}

inline uint32_t RTPTime::GetMicroSeconds() const
{
    uint32_t microsec;

    if (m_t >= 0)
    {
        int64_t sec = (int64_t) m_t;
        microsec = (uint32_t) (1e6 * (m_t - (double) sec) + 0.5);
    }
    else // m_t < 0
    {
        int64_t sec = (int64_t) (-m_t);
        microsec = (uint32_t) (1e6 * ((-m_t) - (double) sec) + 0.5);
    }

    if (microsec >= 1000000)
        return 999999;
    // Unsigned, it can never be less than 0
    // if (microsec < 0)
    // 	return 0;
    return microsec;
}

#ifdef RTP_HAVE_QUERYPERFORMANCECOUNTER

inline uint64_t RTPTime::CalculateMicroseconds(uint64_t performancecount,uint64_t performancefrequency)
{
    uint64_t f = performancefrequency;
    uint64_t a = performancecount;
    uint64_t b = a/f;
    uint64_t c = a%f; // a = b*f+c => (a*1000000)/f = b*1000000+(c*1000000)/f

    return b*C1000000+(c*C1000000)/f;
}

inline RTPTime RTPTime::CurrentTime()
{
    static int inited = 0;
    static uint64_t microseconds, initmicroseconds;
    static LARGE_INTEGER performancefrequency;

    uint64_t emulate_microseconds, microdiff;
    SYSTEMTIME systemtime;
    FILETIME filetime;

    LARGE_INTEGER performancecount;

    QueryPerformanceCounter(&performancecount);

    if(!inited)
    {
        inited = 1;
        QueryPerformanceFrequency(&performancefrequency);
        GetSystemTime(&systemtime);
        SystemTimeToFileTime(&systemtime,&filetime);
        microseconds = ( ((uint64_t)(filetime.dwHighDateTime) << 32) + (uint64_t)(filetime.dwLowDateTime) ) / (uint64_t)10;
        microseconds-= CEPOCH; // EPOCH
        initmicroseconds = CalculateMicroseconds(performancecount.QuadPart, performancefrequency.QuadPart);
    }

    emulate_microseconds = CalculateMicroseconds(performancecount.QuadPart, performancefrequency.QuadPart);

    microdiff = emulate_microseconds - initmicroseconds;

    double t = 1e-6*(double)(microseconds + microdiff);
    return RTPTime(t);
}

inline void RTPTime::Wait(const RTPTime &delay)
{
    if (delay.m_t <= 0)
    return;

    uint64_t sec = (uint64_t)delay.m_t;
    uint32_t microsec = (uint32_t)(1e6*(delay.m_t-(double)sec));
    DWORD t = ((DWORD)sec)*1000+(((DWORD)microsec)/1000);
    Sleep(t);
}

#else // unix style

#ifdef RTP_HAVE_CLOCK_GETTIME
inline double RTPTime_timespecToDouble(struct timespec &ts)
{
    return (double) ts.tv_sec + 1e-9 * (double) ts.tv_nsec;
}

inline RTPTime RTPTime::CurrentTime()
{
    static bool s_initialized = false;
    static double s_startOffet = 0;

    if (!s_initialized)
    {
        s_initialized = true;

        // Get the corresponding times in system time and monotonic time
        struct timespec tpSys, tpMono;

        clock_gettime(CLOCK_REALTIME, &tpSys);
        clock_gettime(CLOCK_MONOTONIC, &tpMono);

        double tSys = RTPTime_timespecToDouble(tpSys);
        double tMono = RTPTime_timespecToDouble(tpMono);

        s_startOffet = tSys - tMono;
        return tSys;
    }

    struct timespec tpMono;
    clock_gettime(CLOCK_MONOTONIC, &tpMono);

    double tMono0 = RTPTime_timespecToDouble(tpMono);
    return tMono0 + s_startOffet;
}

#else // gettimeofday fallback

inline RTPTime RTPTime::CurrentTime()
{
    struct timeval tv;

    gettimeofday(&tv,0);
    return RTPTime((uint64_t)tv.tv_sec,(uint32_t)tv.tv_usec);
}
#endif // RTP_HAVE_CLOCK_GETTIME

inline void RTPTime::Wait(const RTPTime &delay)
{
    if (delay.m_t <= 0)
        return;

    uint64_t sec = (uint64_t) delay.m_t;
    uint64_t nanosec = (uint32_t) (1e9 * (delay.m_t - (double) sec));

    struct timespec req, rem;
    int ret;

    req.tv_sec = (time_t) sec;
    req.tv_nsec = ((long) nanosec);
    do
    {
        ret = nanosleep(&req, &rem);
        req = rem;
    } while (ret == -1 && errno == EINTR);
}

#endif // RTP_HAVE_QUERYPERFORMANCECOUNTER

inline RTPTime &RTPTime::operator-=(const RTPTime &t)
{
    m_t -= t.m_t;
    return *this;
}

inline RTPTime &RTPTime::operator+=(const RTPTime &t)
{
    m_t += t.m_t;
    return *this;
}

inline RTPNTPTime RTPTime::GetNTPTime() const
{
    uint32_t sec = (uint32_t) m_t;
    uint32_t microsec = (uint32_t) ((m_t - (double) sec) * 1e6);

    uint32_t msw = sec + RTP_NTPTIMEOFFSET;
    uint32_t lsw;
    double x;

    x = microsec / 1000000.0;
    x *= (65536.0 * 65536.0);
    lsw = (uint32_t) x;

    return RTPNTPTime(msw, lsw);
}

inline bool RTPTime::operator<(const RTPTime &t) const
{
    return m_t < t.m_t;
}

inline bool RTPTime::operator>(const RTPTime &t) const
{
    return m_t > t.m_t;
}

inline bool RTPTime::operator<=(const RTPTime &t) const
{
    return m_t <= t.m_t;
}

inline bool RTPTime::operator>=(const RTPTime &t) const
{
    return m_t >= t.m_t;
}

class QRTPLIB_API RTPTimeInitializerObject
{
public:
    RTPTimeInitializerObject();
    void Dummy()
    {
        dummy++;
    }
private:
    int dummy;
};

extern QRTPLIB_API RTPTimeInitializerObject timeinit;

} // end namespace

#endif // RTPTIMEUTILITIES_H

