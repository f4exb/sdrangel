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
 * \file rtpsecuresession.h
 */

#ifndef RTPSECURESESSION_H

#define RTPSECURESESSION_H

#include "rtpconfig.h"

#ifdef RTP_SUPPORT_SRTP

#include "rtpsession.h"

#ifdef RTP_SUPPORT_THREAD
	#include <jthread/jthread.h>
#endif // RTP_SUPPORT_THREAD

struct srtp_ctx_t;

namespace qrtplib
{

class RTPCrypt;

// SRTP library needs to be initialized already!

/** RTPSession derived class that serves as a base class for an SRTP implementation.
 *
 *  This is an RTPSession derived class that serves as a base class for an SRTP implementation.
 *  The class sets the RTPSession::SetChangeIncomingData and RTPSession::SetChangeOutgoingData
 *  flags, and implements RTPSession::OnChangeIncomingData, RTPSession::OnChangeRTPOrRTCPData
 *  and RTPSession::OnSentRTPOrRTCPData so that encryption and decryption is applied to packets.
 *  The encryption and decryption will be done using [libsrtp](https://github.com/cisco/libsrtp),
 *  which must be available at compile time.
 *
 *  Your derived class should call RTPSecureSession::InitializeSRTPContext to initialize a context
 *  struct of `libsrtp`. When this succeeds, the context can be obtained and used with the
 *  RTPSecureSession::LockSRTPContext function, which also locks a mutex if thread support was
 *  available. After you're done using the context yourself (to set encryption parameters for
 *  SSRCs), you **must** release it again using RTPSecureSession::UnlockSRTPContext.
 *
 *  See `example7.cpp` for an example of how to use this class.
 */
class JRTPLIB_IMPORTEXPORT RTPSecureSession : public RTPSession
{
public:
	/** Constructs an RTPSecureSession instance, see RTPSession::RTPSession
	 *  for more information about the parameters. */
	RTPSecureSession(RTPRandom *rnd = 0, RTPMemoryManager *mgr = 0);
	~RTPSecureSession();
protected:
	/** Initializes the SRTP context, in case of an error it may be useful to inspect
	 *  RTPSecureSession::GetLastLibSRTPError. */
	int InitializeSRTPContext();

	/** This function locks a mutex and returns the `libsrtp` context that was
	 *  created in RTPSecureSession::InitializeSRTPContext, so that you can further
	 *  use it to specify encryption parameters for various sources; note that you
	 *  **must** release the context again after use with the
	 *  RTPSecureSession::UnlockSRTPContext function. */
	srtp_ctx_t *LockSRTPContext();

	/** Releases the lock on the SRTP context that was obtained in
	 *  RTPSecureSession::LockSRTPContext. */
	int UnlockSRTPContext();

	/** Returns (and clears) the last error that was encountered when using a
	 *  `libsrtp` based function. */
	int GetLastLibSRTPError();

	void SetLastLibSRTPError(int err);

	/** In case the reimplementation of OnChangeIncomingData (which may take place
	 *  in a background thread) encounters an error, this member function will be
	 *  called; implement it in a derived class to receive notification of this. */
	virtual void OnErrorChangeIncomingData(int errcode, int libsrtperrorcode);

	int OnChangeRTPOrRTCPData(const void *origdata, size_t origlen, bool isrtp, void **senddata, size_t *sendlen);
	bool OnChangeIncomingData(RTPRawPacket *rawpack);
	void OnSentRTPOrRTCPData(void *senddata, size_t sendlen, bool isrtp);
private:
	int encryptData(uint8_t *pData, int &dataLength, bool rtp);
	int decryptRawPacket(RTPRawPacket *rawpack, int *srtpError);

	srtp_ctx_t *m_pSRTPContext;
	int m_lastSRTPError;
#ifdef RTP_SUPPORT_THREAD
	jthread::JMutex m_srtpLock;
#endif // RTP_SUPPORT_THREAD
};

inline void RTPSecureSession::OnErrorChangeIncomingData(int, int) { }

} // end namespace

#endif // RTP_SUPPORT_SRTP

#endif // RTPSECURESESSION_H

