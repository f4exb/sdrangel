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
 * \file rtpaddress.h
 */

#ifndef RTPADDRESS_H

#define RTPADDRESS_H

#include "rtpconfig.h"
#include <string>

namespace qrtplib
{

/** This class is an abstract class which is used to specify destinations, multicast groups etc. */
class RTPAddress
{
public:
    /** Identifies the actual implementation being used. */
    enum AddressType
    {
        IPv4Address, /**< Used by the UDP over IPv4 transmitter. */
        IPv6Address, /**< Used by the UDP over IPv6 transmitter. */
        ByteAddress, /**< A very general type of address, consisting of a port number and a number of bytes representing the host address. */
        UserDefinedAddress, /**< Can be useful for a user-defined transmitter. */
        TCPAddress /**< Used by the TCP transmitter. */
    };

    /** Returns the type of address the actual implementation represents. */
    AddressType GetAddressType() const
    {
        return addresstype;
    }

    /** Creates a copy of the RTPAddress instance.
     *  Creates a copy of the RTPAddress instance. If \c mgr is not NULL, the
     *  corresponding memory manager will be used to allocate the memory for the address
     *  copy.
     */
    virtual RTPAddress *CreateCopy() const = 0;

    /** Checks if the address \c addr is the same address as the one this instance represents.
     *  Checks if the address \c addr is the same address as the one this instance represents.
     *  Implementations must be able to handle a NULL argument.
     */
    virtual bool IsSameAddress(const RTPAddress *addr) const = 0;

    /** Checks if the address \c addr represents the same host as this instance.
     *  Checks if the address \c addr represents the same host as this instance. Implementations
     *  must be able to handle a NULL argument.
     */
    virtual bool IsFromSameHost(const RTPAddress *addr) const = 0;

    virtual ~RTPAddress()
    {
    }
protected:
    // only allow subclasses to be created
    RTPAddress(const AddressType t) :
            addresstype(t)
    {
    }
private:
    const AddressType addresstype;
};

} // end namespace

#endif // RTPADDRESS_H

