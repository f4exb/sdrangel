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
 * \file rtpselect.h
 */

#ifndef RTPSELECT_H

#define RTPSELECT_H

#include "rtpconfig.h"
#include "rtptypes.h"
#include "rtperrors.h"
#include "rtptimeutilities.h"
#include "rtpsocketutil.h"

#if defined(RTP_HAVE_WSAPOLL) || defined(RTP_HAVE_POLL)

#ifndef RTP_HAVE_WSAPOLL
#include <poll.h>
#include <errno.h>
#endif // !RTP_HAVE_WSAPOLL

#include <vector>
#include <limits>

namespace qrtplib
{

inline int RTPSelect(const SocketType *sockets, int8_t *readflags, std::size_t numsocks, RTPTime timeout)
{
    using namespace std;

    vector<struct pollfd> fds(numsocks);

    for (std::size_t i = 0; i < numsocks; i++)
    {
        fds[i].fd = sockets[i];
        fds[i].events = POLLIN;
        fds[i].revents = 0;
        readflags[i] = 0;
    }

    int timeoutmsec = -1;
    if (timeout.GetDouble() >= 0)
    {
        double dtimeoutmsec = timeout.GetDouble() * 1000.0;
        if (dtimeoutmsec > (numeric_limits<int>::max)()) // parentheses to prevent windows 'max' macro expansion
            dtimeoutmsec = (numeric_limits<int>::max)();

        timeoutmsec = (int) dtimeoutmsec;
    }

#ifdef RTP_HAVE_WSAPOLL
    int status = WSAPoll(&(fds[0]), (ULONG)numsocks, timeoutmsec);
    if (status < 0)
    return ERR_RTP_SELECT_ERRORINPOLL;
#else
    int status = poll(&(fds[0]), numsocks, timeoutmsec);
    if (status < 0)
    {
        // We're just going to ignore an EINTR
        if (errno == EINTR)
            return 0;
        return ERR_RTP_SELECT_ERRORINPOLL;
    }
#endif // RTP_HAVE_WSAPOLL

    if (status > 0)
    {
        for (std::size_t i = 0; i < numsocks; i++)
        {
            if (fds[i].revents)
                readflags[i] = 1;
        }
    }
    return status;
}

} // end namespace

#else

#ifndef RTP_SOCKETTYPE_WINSOCK
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#endif // !RTP_SOCKETTYPE_WINSOCK

namespace qrtplib
{

    /** Wrapper function around 'select', 'poll' or 'WSAPoll', depending on the
     *  availability on your platform.
     *
     *  Wrapper function around 'select', 'poll' or 'WSAPoll', depending on the
     *  availability on your platform. The function will check the specified
     *  `sockets` for incoming data and sets the flags in `readflags` if so.
     *  A maximum time `timeout` will be waited for data to arrive, which is
     *  indefinitely if set to a negative value. The function returns the number
     *  of sockets that have data incoming.
     */
    inline int RTPSelect(const SocketType *sockets, int8_t *readflags, std::size_t numsocks, RTPTime timeout)
    {
        struct timeval tv;
        struct timeval *pTv = 0;

        if (timeout.GetDouble() >= 0)
        {
            tv.tv_sec = (long)timeout.GetSeconds();
            tv.tv_usec = timeout.GetMicroSeconds();
            pTv = &tv;
        }

        fd_set fdset;
        FD_ZERO(&fdset);
        for (std::size_t i = 0; i < numsocks; i++)
        {
#ifndef RTP_SOCKETTYPE_WINSOCK
            const int setsize = FD_SETSIZE;
            // On windows it seems that comparing the socket value to FD_SETSIZE does
            // not make sense
            if (sockets[i] >= setsize)
            return ERR_RTP_SELECT_SOCKETDESCRIPTORTOOLARGE;
#endif // RTP_SOCKETTYPE_WINSOCK
            FD_SET(sockets[i], &fdset);
            readflags[i] = 0;
        }

        int status = select(FD_SETSIZE, &fdset, 0, 0, pTv);
#ifdef RTP_SOCKETTYPE_WINSOCK
        if (status < 0)
        return ERR_RTP_SELECT_ERRORINSELECT;
#else
        if (status < 0)
        {
            // We're just going to ignore an EINTR
            if (errno == EINTR)
            return 0;
            return ERR_RTP_SELECT_ERRORINSELECT;
        }
#endif // RTP_HAVE_WSAPOLL

        if (status > 0) // some descriptors were set, check them
        {
            for (std::size_t i = 0; i < numsocks; i++)
            {
                if (FD_ISSET(sockets[i], &fdset))
                readflags[i] = 1;
            }
        }
        return status;
    }

} // end namespace

#endif // RTP_HAVE_POLL || RTP_HAVE_WSAPOLL

#endif // RTPSELECT_H
