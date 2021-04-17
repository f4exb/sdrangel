///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2010, 2011, 2012 Jan van Katwijk (J.vanKatwijk@gmail.com)
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "dabdemoddevice.h"

// Implementation of default device, as not included in library

deviceHandler::deviceHandler(void)
{
    lastFrequency = 100000;
}

deviceHandler::~deviceHandler(void)
{
}

bool deviceHandler::restartReader(int32_t)
{
    return true;
}

void deviceHandler::stopReader(void)
{
}

void deviceHandler::run(void)
{
}

int32_t deviceHandler::getSamples(std::complex<float> *v, int32_t amount)
{
    (void)v;
    (void)amount;
    return 0;
}

int32_t deviceHandler::Samples(void)
{
    return 0;
}

int32_t deviceHandler::defaultFrequency(void)
{
    return 220000000;
}

void deviceHandler::resetBuffer(void)
{
}

void deviceHandler::setGain(int32_t x)
{
    (void)x;
}

bool deviceHandler::has_autogain(void)
{
    return false;
}

void deviceHandler::set_autogain(bool b)
{
    (void)b;
}

void deviceHandler::set_ifgainReduction(int x)
{
    (void)x;
}

void deviceHandler::set_lnaState(int x)
{
    (void)x;
}

// Device with source data from SDRangel
// Other methods not needed for DAB library

DABDemodDevice::DABDemodDevice() :
    m_iqBuffer (4 * 1024 * 1024)
{
}

void DABDemodDevice::putSample(std::complex<float> s)
{
    std::complex<float> localBuf[1];
    localBuf[0] = s;
    if (m_iqBuffer.GetRingBufferWriteAvailable() > 0)
        m_iqBuffer.putDataIntoBuffer(localBuf, 1);
}

int32_t DABDemodDevice::getSamples(std::complex<float> *buf, int32_t size)
{
    return m_iqBuffer.getDataFromBuffer(buf, size);
}

int32_t DABDemodDevice::Samples(void)
{
    return m_iqBuffer.GetRingBufferReadAvailable();
}

void DABDemodDevice::resetBuffer(void)
{
    m_iqBuffer.FlushRingBuffer();
}
