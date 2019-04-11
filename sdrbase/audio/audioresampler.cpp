///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "audioresampler.h"

AudioResampler::AudioResampler() :
    m_decimation(1),
    m_decimationCount(0)
{}

AudioResampler::~AudioResampler()
{}

void AudioResampler::setDecimation(uint32_t decimation)
{
    m_decimation = decimation == 0 ? 1 : decimation;
}

void AudioResampler::setAudioFilters(int srHigh, int srLow, int fcLow, int fcHigh, float gain)
{
    srHigh = (srHigh <= 100 ? 100 : srHigh);
    srLow = (srLow <= 0 ? 1 : srLow);
    srLow = srLow > srHigh - 50 ? srHigh - 50 : srLow;

    fcLow = fcLow < 0 ? 0 : fcLow;
    fcHigh = fcHigh < 100 ? 100 : fcHigh;
    fcLow = fcLow > fcHigh - 100 ? fcHigh - 100 : fcLow;

    m_audioFilter.setDecimFilters(srHigh, srLow, fcHigh, fcLow, gain);
}

bool AudioResampler::downSample(qint16 sampleIn, qint16& sampleOut)
{
    if (m_decimation == 1)
    {
        sampleOut = sampleIn;
        return true;
    }

    if (m_decimationCount >= m_decimation - 1)
    {
        float lpSample = m_audioFilter.run(sampleIn / 32768.0f);
        sampleOut = lpSample * 32768.0f;
        m_decimationCount = 0;
        return true;
    }
    else
    {
        m_decimationCount++;
        return false;
    }
}

bool AudioResampler::upSample(qint16 sampleIn, qint16& sampleOut)
{
    float lpSample;

    if (m_decimation == 1)
    {
        sampleOut = sampleIn;
        return true;
    }

    if (m_decimationCount >= m_decimation - 1)
    {
        m_decimationCount = 0;
        lpSample = m_audioFilter.run(sampleIn / 32768.0f);
        sampleOut = lpSample * 32768.0f;
        return true;
    }
    else
    {
        m_decimationCount++;
        lpSample = m_audioFilter.run(0.0f);
        sampleOut = lpSample * 32768.0f;
        return false;
    }
}


