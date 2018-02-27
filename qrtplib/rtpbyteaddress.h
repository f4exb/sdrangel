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
 * \file rtpbyteaddress.h
 */

#ifndef RTPBYTEADDRESS_H

#define RTPBYTEADDRESS_H

#include "rtpconfig.h"
#include "rtpaddress.h"
#include "rtptypes.h"
#include <string.h>

#define RTPBYTEADDRESS_MAXLENGTH					128

namespace qrtplib
{

class RTPMemoryManager;

/** A very general kind of address consisting of a port number and a number of bytes describing the host address.
 *  A very general kind of address, consisting of a port number and a number of bytes describing the host address.
 */
class JRTPLIB_IMPORTEXPORT RTPByteAddress : public RTPAddress
{
public:
	/** Creates an instance of the class using \c addrlen bytes of \c hostaddress as host identification,
	 *  and using \c port as the port number. */
	RTPByteAddress(const uint8_t hostaddress[RTPBYTEADDRESS_MAXLENGTH], size_t addrlen, uint16_t port = 0) : RTPAddress(ByteAddress) 	{ if (addrlen > RTPBYTEADDRESS_MAXLENGTH) addrlen = RTPBYTEADDRESS_MAXLENGTH; memcpy(RTPByteAddress::hostaddress, hostaddress, addrlen); RTPByteAddress::addresslength = addrlen; RTPByteAddress::port = port; }

	/** Sets the host address to the first \c addrlen bytes of \c hostaddress. */
	void SetHostAddress(const uint8_t hostaddress[RTPBYTEADDRESS_MAXLENGTH], size_t addrlen)						{ if (addrlen > RTPBYTEADDRESS_MAXLENGTH) addrlen = RTPBYTEADDRESS_MAXLENGTH; memcpy(RTPByteAddress::hostaddress, hostaddress, addrlen); RTPByteAddress::addresslength = addrlen; }

	/** Sets the port number to \c port. */
	void SetPort(uint16_t port)													{ RTPByteAddress::port = port; }

	/** Returns a pointer to the stored host address. */
	const uint8_t *GetHostAddress() const												{ return hostaddress; }

	/** Returns the length in bytes of the stored host address. */
	size_t GetHostAddressLength() const												{ return addresslength; }

	/** Returns the port number stored in this instance. */
	uint16_t GetPort() const													{ return port; }

	RTPAddress *CreateCopy(RTPMemoryManager *mgr) const;
	bool IsSameAddress(const RTPAddress *addr) const;
	bool IsFromSameHost(const RTPAddress *addr) const;

private:
	uint8_t hostaddress[RTPBYTEADDRESS_MAXLENGTH];
	size_t addresslength;
	uint16_t port;
};

} // end namespace

#endif // RTPBYTEADDRESS_H

