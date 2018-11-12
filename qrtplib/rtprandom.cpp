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

#if defined(WIN32) && !defined(_WIN32_WCE)
#define _CRT_RAND_S
#endif // WIN32 || _WIN32_WCE

#include "rtprandom.h"
#include "rtprandomrands.h"
#include "rtprandomurandom.h"
#include "rtprandomrand48.h"

#include <time.h>
#ifdef _MSC_VER
#include "Windows.h"
#else
#include <unistd.h>
#endif

#include <QDateTime>

namespace qrtplib
{

uint32_t RTPRandom::PickSeed()
{
    uint32_t x;
#ifdef _MSC_VER
    x = GetCurrentProcessId();
#else
    x = (uint32_t) getpid();
#endif
    QDateTime currentDateTime = QDateTime::currentDateTime();
    x += currentDateTime.toTime_t();
#if defined(WIN32)
    x += QDateTime::currentMSecsSinceEpoch() % 1000;
#else
    x += (uint32_t)clock();
#endif
    x ^= (uint32_t)((uint8_t *)this - (uint8_t *)0);
    return x;
}

RTPRandom *RTPRandom::CreateDefaultRandomNumberGenerator()
{
    RTPRandomURandom *r = new RTPRandomURandom();
    RTPRandom *rRet = r;

    if (r->Init() < 0) // fall back to rand48
    {
        delete r;
        rRet = new RTPRandomRand48();
    }

    return rRet;
}

} // end namespace

