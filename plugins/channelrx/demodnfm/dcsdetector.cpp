//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                                              //
//                                                                                                          //
// This program is free software; you can redistribute it and/or modify                                     //
// it under the terms of the GNU General Public License as published by                                     //
// the Free Software Foundation as version 3 of the License, or                                             //
// (at your option) any later version.                                                                      //
//                                                                                                          //
// This program is distributed in the hope that it will be useful,                                          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                                           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                                             //
// GNU General Public License V3 for more details.                                                          //
//                                                                                                          //
// You should have received a copy of the GNU General Public License                                        //
// along with this program. If not, see <http://www.gnu.org/licenses/>.                                     //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <QMutexLocker>

#include "dcsdetector.h"

DCSDetector::DCSDetector() :
    m_bitIndex(0.0),
    m_sampleRate(48000),
    m_eqSamples(nullptr),
    m_high(0.0f),
    m_low(0.0f),
    m_mid(0.0f),
    m_prevSample(0.0f),
    m_dcsWord(0),
    m_mutex(QMutex::Recursive)
{
    setBitrate(134.3);
    setEqWindow(23);
}

bool DCSDetector::analyze(Real *sample, unsigned int& dcsCode)
{
    QMutexLocker mlock(&m_mutex);
    bool codeAvailable = false;

    if (!m_eqSamples) {
        return false;
    }
    // Equalizer
    m_eqSamples[m_eqIndex++] = *sample;

    if (m_eqIndex == m_eqSize)
    {
        m_high = *std::max_element(m_eqSamples, m_eqSamples + m_eqSize);
        m_low  = *std::min_element(m_eqSamples, m_eqSamples + m_eqSize);
        m_mid = (m_high + m_low) / 2.0f;
        // qDebug("DCSDetector::analyze: %f %f %f", m_low, m_mid, m_high);
        m_eqIndex = 0;
    }

    // Edge detection
    if (((m_prevSample < m_mid) && (*sample >= m_mid)) || ((m_prevSample > m_mid) && (*sample <= m_mid))) {
        m_bitIndex = 0.0;
    }

    // Symbol detection
    m_prevSample = *sample;
    float fprev = m_bitIndex;
    m_bitIndex += m_bitsPerSample;

    if ((fprev < 0.5f) && (m_bitIndex >= 0.5f)) // mid point detection
    {
        unsigned int bit = *sample > m_mid ? 1 : 0; // always work in positive mode
        m_dcsWord = (bit << 23) + (m_dcsWord >> 1);

        if (((m_dcsWord >> 9) & 0x07) == 4) // magic signature
        {
            codeAvailable = m_golay2312.decodeParityFirst(&m_dcsWord);

            if (codeAvailable) {
                dcsCode = m_dcsWord & 0x1FF;
            }
        }
    }

    if (m_bitIndex > 1.0f) {
        m_bitIndex -= 1.0f;
    }

    return codeAvailable;
}

void DCSDetector::setBitrate(float bitrate)
{
    m_bitrate = bitrate;
    m_bitsPerSample = m_bitrate / m_sampleRate;
    m_samplesPerBit = m_sampleRate / m_bitrate;
}

void DCSDetector::setSampleRate(int sampleRate)
{
    m_sampleRate  = sampleRate;
    m_bitsPerSample = m_bitrate / m_sampleRate;
    m_samplesPerBit = m_sampleRate / m_bitrate;
}

void DCSDetector::setEqWindow(int nbBits)
{
    QMutexLocker mlock(&m_mutex);

    m_eqBits = nbBits;
    m_eqSize = (int) m_samplesPerBit * m_eqBits;

    if (m_eqSamples) {
        delete[] m_eqSamples;
    }

    m_eqSamples = new float[m_eqSize];
    m_eqIndex = 0;
}

DCSDetector::~DCSDetector()
{
    if (m_eqSamples) {
        delete[] m_eqSamples;
    }
}
