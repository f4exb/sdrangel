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

#include "rtprandomurandom.h"
#include "rtperrors.h"

namespace qrtplib
{

RTPRandomURandom::RTPRandomURandom()
{
    device = 0;
}

RTPRandomURandom::~RTPRandomURandom()
{
    if (device)
        fclose(device);
}

int RTPRandomURandom::Init()
{
    if (device)
        return ERR_RTP_RTPRANDOMURANDOM_ALREADYOPEN;

    device = fopen("/dev/urandom", "rb");
    if (device == 0)
        return ERR_RTP_RTPRANDOMURANDOM_CANTOPEN;

    return 0;
}

uint8_t RTPRandomURandom::GetRandom8()
{
    if (!device)
        return 0;

    uint8_t value;

    if (fread(&value, sizeof(uint8_t), 1, device) != sizeof(uint8_t))
    {
        return 0;
    }

    return value;
}

uint16_t RTPRandomURandom::GetRandom16()
{
    if (!device)
        return 0;

    uint16_t value;

    if (fread(&value, sizeof(uint16_t), 1, device) != sizeof(uint16_t))
    {
        return 0;
    }

    return value;
}

uint32_t RTPRandomURandom::GetRandom32()
{
    if (!device)
        return 0;

    uint32_t value;

    if (fread(&value, sizeof(uint32_t), 1, device) != sizeof(uint32_t))
    {
        return 0;
    }

    return value;
}

double RTPRandomURandom::GetRandomDouble()
{
    if (!device)
        return 0;

    uint64_t value;

    if (fread(&value, sizeof(uint64_t), 1, device) != sizeof(uint64_t))
    {
        return 0;
    }

    value &= 0x7fffffffffffffffULL;
    int64_t value2 = (int64_t) value;
    double x = RTPRANDOM_2POWMIN63 * (double) value2;

    return x;
}

} // end namespace

