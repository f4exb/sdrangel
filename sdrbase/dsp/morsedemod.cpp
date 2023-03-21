///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include <QDebug>

#include "util/morse.h"

#include "morsedemod.h"

MESSAGE_CLASS_DEFINITION(MorseDemod::MsgReportIdent, Message)

MorseDemod::MorseDemod() :
    m_movingAverageIdent(5000),
    m_prevBit(0),
    m_bitTime(0)
{
}

void MorseDemod::reset()
{
    m_binSampleCnt = 0;
    m_binCnt = 0;
    m_identNoise = 0.0001f;
    for (int i = 0; i < m_identBins; i++)
    {
        m_identMaxs[i] = 0.0f;
    }
    m_ident = "";
}

void MorseDemod::applyChannelSettings(int channelSampleRate)
{
    if (channelSampleRate <= 0) {
        return;
    }
    m_samplesPerDot7wpm = channelSampleRate*60/(50*7);
    m_samplesPerDot10wpm = channelSampleRate*60/(50*10);

    m_ncoIdent.setFreq(-1020, channelSampleRate);  // +-50Hz source offset allowed
    m_bandpassIdent.create(1001, channelSampleRate, 970.0f, 1070.0f); // Ident at 1020

    m_lowpassIdent.create(301, channelSampleRate, 100.0f);
    m_movingAverageIdent.resize(m_samplesPerDot10wpm/5);  // Needs to be short enough for noise floor calculation

    reset();
}

void MorseDemod::applySettings(int identThreshold)
{
    m_identThreshold = identThreshold;
    reset();
}

void MorseDemod::processOneSample(const Complex &magc)
{
    // Filter to remove voice
    Complex c1 = m_bandpassIdent.filter(magc);
    // Remove ident sub-carrier offset
    c1 *= m_ncoIdent.nextIQ();
    // Filter other signals
    Complex c2 = std::abs(m_lowpassIdent.filter(c1));

    // Filter noise with moving average (moving average preserves edges)
    m_movingAverageIdent(c2.real());
    Real mav = m_movingAverageIdent.asFloat();

    // Caclulate noise floor
    if (mav > m_identMaxs[m_binCnt])
        m_identMaxs[m_binCnt] = mav;
    m_binSampleCnt++;
    if (m_binSampleCnt >= m_samplesPerDot10wpm/4)
    {
        // Calc minimum of maximums
        m_identNoise = 1.0f;
        for (int i = 0; i < m_identBins; i++)
        {
            m_identNoise = std::min(m_identNoise, m_identMaxs[i]);
        }
        m_binSampleCnt = 0;
        m_binCnt++;
        if (m_binCnt == m_identBins)
            m_binCnt = 0;
        m_identMaxs[m_binCnt] = 0.0f;

        // Prevent divide by zero
        if (m_identNoise == 0.0f)
            m_identNoise = 1e-20f;
    }

    // CW demod
    int bit = (mav / m_identNoise) >= m_identThreshold;
    //m_stream << mav << "," << m_identNoise << "," << bit << "," << (mav / m_identNoise) << "\n";
    if ((m_prevBit == 0) && (bit == 1))
    {
        if (m_bitTime > 7*m_samplesPerDot10wpm)
        {
            if (m_ident.trimmed().size() > 2) // Filter out noise that may appear as one or two characters
            {
                qDebug() << "MorseDemod::processOneSample:" << m_ident << " " << Morse::toString(m_ident);

                if (getMessageQueueToChannel())
                {
                    MorseDemod::MsgReportIdent *msg = MorseDemod::MsgReportIdent::create(m_ident);
                    getMessageQueueToChannel()->push(msg);
                }
            }
            m_ident = "";
        }
        else if (m_bitTime > 2.5*m_samplesPerDot10wpm)
        {
            m_ident.append(" ");
        }
        m_bitTime = 0;
    }
    else if (bit == 1)
    {
        m_bitTime++;
    }
    else if ((m_prevBit == 1) && (bit == 0))
    {
        if (m_bitTime > 2*m_samplesPerDot10wpm)
        {
            m_ident.append("-");
        }
        else if (m_bitTime > 0.2*m_samplesPerDot10wpm)
        {
            m_ident.append(".");
        }
        m_bitTime = 0;
    }
    else
    {
        m_bitTime++;
        if (m_bitTime > 10*m_samplesPerDot7wpm)
        {
            m_ident = m_ident.simplified();
            if (m_ident.trimmed().size() > 2) // Filter out noise that may appear as one or two characters
                {
                qDebug() << "MorseDemod::processOneSample:" << m_ident << " " << Morse::toString(m_ident);

                if (getMessageQueueToChannel())
                {
                    MorseDemod::MsgReportIdent *msg = MorseDemod::MsgReportIdent::create(m_ident);
                    getMessageQueueToChannel()->push(msg);
                }

            }
            m_ident = "";
            m_bitTime = 0;
        }
    }
    m_prevBit = bit;
}

