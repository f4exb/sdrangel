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
 * \file rtpcollisionlist.h
 */

#ifndef RTPCOLLISIONLIST_H

#define RTPCOLLISIONLIST_H

#include "rtpconfig.h"
#include "rtpaddress.h"
#include "rtptimeutilities.h"
#include <list>

#include "export.h"

namespace qrtplib
{

class RTPAddress;

/** This class represents a list of addresses from which SSRC collisions were detected. */
class QRTPLIB_API RTPCollisionList
{
public:
    /** Constructs an instance, optionally installing a memory manager. */
    RTPCollisionList();
    ~RTPCollisionList()
    {
        Clear();
    }

    /** Clears the list of addresses. */
    void Clear();

    /** Updates the entry for address \c addr to indicate that a collision was detected at time \c receivetime.
     *  Updates the entry for address \c addr to indicate that a collision was detected at time \c receivetime.
     *  If the entry did not exist yet, the flag \c created is set to \c true, otherwise it is set to \c false.
     */
    int UpdateAddress(const RTPAddress *addr, const RTPTime &receivetime, bool *created);

    /** Returns \c true} if the address \c addr appears in the list. */
    bool HasAddress(const RTPAddress *addr) const;

    /** Assuming that the current time is given by \c currenttime, this function times out entries which
     *  haven't been updated in the previous time interval specified by \c timeoutdelay.
     */
    void Timeout(const RTPTime &currenttime, const RTPTime &timeoutdelay);

private:
    class AddressAndTime
    {
    public:
        AddressAndTime(RTPAddress *a, const RTPTime &t) :
                addr(a), recvtime(t)
        {
        }

        RTPAddress *addr;
        RTPTime recvtime;
    };

    std::list<AddressAndTime> addresslist;
};

} // end namespace

#endif // RTPCOLLISIONLIST_H

