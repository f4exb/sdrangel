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

#include "rtpconfig.h"
#ifdef RTP_HAVE_RAND_S
#define _CRT_RAND_S
#endif // RTP_HAVE_RAND_S

#include "rtprandomrands.h"
#include "rtperrors.h"
#include <stdlib.h>
#include <math.h>

// If compiling on VC 8 or later for full Windows, we'll attempt to use rand_s,
// which generates better random numbers.  However, its only supported on Windows XP,
// Windows Server 2003, and later, so we'll do a run-time check to see if we can
// use it (it won't work on Windows 2000 SP4 for example).

namespace qrtplib
{

#ifndef RTP_HAVE_RAND_S

RTPRandomRandS::RTPRandomRandS()
{
    initialized = false;
}

RTPRandomRandS::~RTPRandomRandS()
{
}

int RTPRandomRandS::Init()
{
    return ERR_RTP_RTPRANDOMRANDS_NOTSUPPORTED;
}

uint8_t RTPRandomRandS::GetRandom8()
{
    return 0;
}

uint16_t RTPRandomRandS::GetRandom16()
{
    return 0;
}

uint32_t RTPRandomRandS::GetRandom32()
{
    return 0;
}

double RTPRandomRandS::GetRandomDouble()
{
    return 0;
}

#else

RTPRandomRandS::RTPRandomRandS()
{
    initialized = false;
}

RTPRandomRandS::~RTPRandomRandS()
{
}

int RTPRandomRandS::Init()
{
    if (initialized) // doesn't matter then
    return 0;

    HMODULE hAdvApi32 = LoadLibrary(TEXT("ADVAPI32.DLL"));
    if(hAdvApi32 != NULL)
    {
        if(NULL != GetProcAddress( hAdvApi32, "SystemFunction036" )) // RtlGenRandom
        {
            initialized = true;
        }
        FreeLibrary(hAdvApi32);
        hAdvApi32 = NULL;
    }

    if (!initialized)
    return ERR_RTP_RTPRANDOMRANDS_NOTSUPPORTED;
    return 0;
}

// rand_s gives a number between 0 and UINT_MAX. We'll assume that UINT_MAX is at least 8 bits

uint8_t RTPRandomRandS::GetRandom8()
{
    if (!initialized)
    return 0;

    unsigned int r;

    rand_s(&r);

    return (uint8_t)(r&0xff);
}

uint16_t RTPRandomRandS::GetRandom16()
{
    if (!initialized)
    return 0;

    unsigned int r;
    int shift = 0;
    uint16_t x = 0;

    for (int i = 0; i < 2; i++, shift += 8)
    {
        rand_s(&r);

        x ^= ((uint16_t)r << shift);
    }

    return x;
}

uint32_t RTPRandomRandS::GetRandom32()
{
    if (!initialized)
    return 0;

    unsigned int r;
    int shift = 0;
    uint32_t x = 0;

    for (int i = 0; i < 4; i++, shift += 8)
    {
        rand_s(&r);

        x ^= ((uint32_t)r << shift);
    }

    return x;
}

double RTPRandomRandS::GetRandomDouble()
{
    if (!initialized)
    return 0;

    unsigned int r;
    int shift = 0;
    uint64_t x = 0;

    for (int i = 0; i < 8; i++, shift += 8)
    {
        rand_s(&r);

        x ^= ((uint64_t)r << shift);
    }

    x &= 0x7fffffffffffffffULL;

    int64_t x2 = (int64_t)x;
    return RTPRANDOM_2POWMIN63*(double)x2;
}

#endif // RTP_HAVE_RAND_S

}
 // end namespace

