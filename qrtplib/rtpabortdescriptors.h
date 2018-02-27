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
 * \file rtpabortdescriptors.h
 */

#ifndef RTPABORTDESCRIPTORS_H

#define RTPABORTDESCRIPTORS_H

#include "rtpconfig.h"
#include "rtpsocketutil.h"

namespace qrtplib
{

/**
 * Helper class for several RTPTransmitter instances, to be able to cancel a
 * call to 'select', 'poll' or 'WSAPoll'.
 *
 * This is a helper class for several RTPTransmitter instances. Typically a
 * call to 'select' (or 'poll' or 'WSAPoll', depending on the platform) is used
 * to wait for incoming data for a certain time. To be able to cancel this wait
 * from another thread, this class provides a socket descriptor that's compatible
 * with e.g. the 'select' call, and to which data can be sent using
 * RTPAbortDescriptors::SendAbortSignal. If the descriptor is included in the
 * 'select' call, the function will detect incoming data and the function stops
 * waiting for incoming data.
 *
 * The class can be useful in case you'd like to create an implementation which
 * uses a single poll thread for several RTPSession and RTPTransmitter instances.
 * This idea is further illustrated in `example8.cpp`.
 */
class RTPAbortDescriptors
{
public:
    RTPAbortDescriptors();
    ~RTPAbortDescriptors();

    /** Initializes this instance. */
    int Init();

    /** Returns the socket descriptor that can be included in a call to
     *  'select' (for example).*/
    SocketType GetAbortSocket() const
    {
        return m_descriptors[0];
    }

    /** Returns a flag indicating if this instance was initialized. */
    bool IsInitialized() const
    {
        return m_init;
    }

    /** De-initializes this instance. */
    void Destroy();

    /** Send a signal to the socket that's returned by RTPAbortDescriptors::GetAbortSocket,
     *  causing the 'select' call to detect that data is available, making the call
     *  end. */
    int SendAbortSignal();

    /** For each RTPAbortDescriptors::SendAbortSignal function that's called, a call
     *  to this function can be made to clear the state again. */
    int ReadSignallingByte();

    /** Similar to ReadSignallingByte::ReadSignallingByte, this function clears the signalling
     *  state, but this also works independently from the amount of times that
     *  RTPAbortDescriptors::SendAbortSignal was called. */
    int ClearAbortSignal();
private:
    SocketType m_descriptors[2];
    bool m_init;
};

} // end namespace

#endif // RTPABORTDESCRIPTORS_H
