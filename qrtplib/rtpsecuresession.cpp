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

#include "rtpsecuresession.h"

#ifdef RTP_SUPPORT_SRTP

#include "rtprawpacket.h"
#include <jthread/jmutexautolock.h>
#include <srtp/srtp.h>
#include <iostream>
#include <vector>

using namespace std;
using namespace jthread;

namespace qrtplib
{

// SRTP library needs to be initialized already!

RTPSecureSession::RTPSecureSession(RTPRandom *rnd, RTPMemoryManager *mgr) : RTPSession(rnd, mgr)
{
	// Make sure the OnChange... functions will be called
	SetChangeIncomingData(true);
	SetChangeOutgoingData(true);
	m_pSRTPContext = 0;
	m_lastSRTPError = 0;
}

RTPSecureSession::~RTPSecureSession()
{
	if (m_pSRTPContext)
		srtp_dealloc(m_pSRTPContext);
}

int RTPSecureSession::InitializeSRTPContext()
{
#ifdef RTP_SUPPORT_THREAD
	if (!m_srtpLock.IsInitialized())
	{
		if (m_srtpLock.Init() < 0)
			return ERR_RTP_SECURESESSION_CANTINITMUTEX;
	}

	JMutexAutoLock l(m_srtpLock);
#endif // RTP_SUPPORT_THREAD

	if (m_pSRTPContext)
		return ERR_RTP_SECURESESSION_CONTEXTALREADYINITIALIZED;

	err_status_t result = srtp_create(&m_pSRTPContext, NULL);
	if (result != err_status_ok)
	{
		m_pSRTPContext = 0;
		m_lastSRTPError = (int)result;
		return ERR_RTP_SECURESESSION_CANTINITIALIZE_SRTPCONTEXT;
	}

	return 0;
}

int RTPSecureSession::GetLastLibSRTPError()
{
#ifdef RTP_SUPPORT_THREAD
	JMutexAutoLock l(m_srtpLock);
#endif // RTP_SUPPORT_THREAD

	int err = m_lastSRTPError;
	m_lastSRTPError = 0; // clear it

	return err;
}

void RTPSecureSession::SetLastLibSRTPError(int err)
{
#ifdef RTP_SUPPORT_THREAD
	JMutexAutoLock l(m_srtpLock);
#endif // RTP_SUPPORT_THREAD

	m_lastSRTPError = err;
}

srtp_ctx_t *RTPSecureSession::LockSRTPContext()
{
#ifdef RTP_SUPPORT_THREAD
	m_srtpLock.Lock();
#endif // RTP_SUPPORT_THREAD

	if (!m_pSRTPContext)
	{
#ifdef RTP_SUPPORT_THREAD
		m_srtpLock.Unlock(); // Make sure the mutex is not locked on error
#endif // RTP_SUPPORT_THREAD
		return 0;
	}

	return m_pSRTPContext;
}

int RTPSecureSession::UnlockSRTPContext()
{
	if (!m_pSRTPContext)
		return ERR_RTP_SECURESESSION_CONTEXTNOTINITIALIZED;

#ifdef RTP_SUPPORT_THREAD
	m_srtpLock.Unlock();
#endif // RTP_SUPPORT_THREAD
	return 0;
}

int RTPSecureSession::encryptData(uint8_t *pData, int &dataLength, bool rtp)
{
	int length = dataLength;

	if (rtp)
	{
		if (length < (int)sizeof(uint32_t)*3)
			return ERR_RTP_SECURESESSION_NOTENOUGHDATATOENCRYPT ;

		err_status_t result = srtp_protect(m_pSRTPContext, (void *)pData, &length);
		if (result != err_status_ok)
		{
			m_lastSRTPError = (int)result;
			return ERR_RTP_SECURESESSION_CANTENCRYPTRTPDATA;
		}
	}
	else // rtcp
	{
		if (length < (int)sizeof(uint32_t)*2)
			return ERR_RTP_SECURESESSION_NOTENOUGHDATATOENCRYPT;

		err_status_t result = srtp_protect_rtcp(m_pSRTPContext, (void *)pData, &length);
		if (result != err_status_ok)
		{
			m_lastSRTPError = (int)result;
			return ERR_RTP_SECURESESSION_CANTENCRYPTRTCPDATA;
		}
	}

	dataLength = length;

	return 0;
}

int RTPSecureSession::OnChangeRTPOrRTCPData(const void *origdata, size_t origlen, bool isrtp, void **senddata, size_t *sendlen)
{
	*senddata = 0;

	if (!origdata || origlen == 0) // Nothing to do in this case, packet can be ignored
		return 0;

	srtp_ctx_t *pCtx = LockSRTPContext();
	if (pCtx == 0)
		return ERR_RTP_SECURESESSION_CONTEXTNOTINITIALIZED;

	// Need to add some extra bytes, and we'll add a few more to be really safe
	uint8_t *pDataCopy = RTPNew(GetMemoryManager(), RTPMEM_TYPE_BUFFER_SRTPDATA) uint8_t [origlen + SRTP_MAX_TRAILER_LEN + 32];
	memcpy(pDataCopy, origdata, origlen);

	int status = 0;
	int dataLength = (int)origlen;

	if ((status = encryptData(pDataCopy, dataLength, isrtp)) < 0)
	{
		UnlockSRTPContext();
		RTPDeleteByteArray(pDataCopy, GetMemoryManager());
		return status;
	}

	UnlockSRTPContext();

	*senddata = pDataCopy;
	*sendlen = dataLength;

	return 0;
}

int RTPSecureSession::decryptRawPacket(RTPRawPacket *rawpack, int *srtpError)
{
	*srtpError = 0;

	uint8_t *pData = rawpack->GetData();
	int dataLength = (int)rawpack->GetDataLength();

	if (rawpack->IsRTP())
	{
		if (dataLength < (int)sizeof(uint32_t)*3)
			return ERR_RTP_SECURESESSION_NOTENOUGHDATATODECRYPT;

		err_status_t result = srtp_unprotect(m_pSRTPContext, (void*)pData, &dataLength);
		if (result != err_status_ok)
		{
			*srtpError = result;
			return ERR_RTP_SECURESESSION_CANTDECRYPTRTPDATA;
		}
	}
	else // RTCP
	{
		if (dataLength < (int)sizeof(uint32_t)*2)
			return ERR_RTP_SECURESESSION_NOTENOUGHDATATODECRYPT;

		err_status_t result = srtp_unprotect_rtcp(m_pSRTPContext, (void *)pData, &dataLength);
		if (result != err_status_ok)
		{
			*srtpError = result;
			return ERR_RTP_SECURESESSION_CANTDECRYPTRTCPDATA;
		}
	}

	rawpack->ZeroData(); // make sure we don't delete the data we're going to store
	rawpack->SetData(pData, (size_t)dataLength);

	return 0;
}

bool RTPSecureSession::OnChangeIncomingData(RTPRawPacket *rawpack)
{
	if (!rawpack)
		return false;

	srtp_ctx_t *pCtx = LockSRTPContext();
	if (pCtx == 0)
	{
		OnErrorChangeIncomingData(ERR_RTP_SECURESESSION_CONTEXTNOTINITIALIZED, 0);
		return false;
	}

	int srtpErr = 0;
	int status = decryptRawPacket(rawpack, &srtpErr);
	UnlockSRTPContext();

	if (status < 0)
	{
		OnErrorChangeIncomingData(status, srtpErr);
		return false;
	}

	return true;
}

void RTPSecureSession::OnSentRTPOrRTCPData(void *senddata, size_t sendlen, bool isrtp)
{
	JRTPLIB_UNUSED(sendlen);
	JRTPLIB_UNUSED(isrtp);

	if (senddata)
		RTPDeleteByteArray((uint8_t *)senddata, GetMemoryManager());
}

} // end namespace

#endif // RTP_SUPPORT_SRTP

