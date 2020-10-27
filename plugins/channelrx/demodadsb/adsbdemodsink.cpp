///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <stdio.h>
#include <complex.h>
#include <cmath>

#include <QTime>
#include <QDebug>

#include "util/stepfunctions.h"
#include "util/db.h"
#include "util/crc.h"
#include "audio/audiooutput.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"

#include "adsbdemodreport.h"
#include "adsbdemodsink.h"
#include "adsb.h"

ADSBDemodSink::ADSBDemodSink() :
        m_channelSampleRate(6000000),
        m_channelFrequencyOffset(0),
        m_sampleIdx(0),
        m_sampleCount(0),
        m_skipCount(0),
        m_magsq(0.0f),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToGUI(nullptr),
        m_sampleBuffer(nullptr),
        m_preamble(nullptr)
{
    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

ADSBDemodSink::~ADSBDemodSink()
{
    delete m_sampleBuffer;
    delete m_preamble;
}

void ADSBDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    Complex ci;

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex c(it->real(), it->imag());
        c *= m_nco.nextIQ();

        if (m_interpolatorDistance == 1.0f)
        {
            processOneSample(c);
        }
        else if (m_interpolatorDistance < 1.0f) // interpolate
        {
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else // decimate
        {
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
    }

}

void ADSBDemodSink::processOneSample(Complex &ci)
{
    Real sample;

    double magsqRaw = ci.real()*ci.real() + ci.imag()*ci.imag();
    Real magsq = magsqRaw / (SDR_RX_SCALED*SDR_RX_SCALED);
    m_movingAverage(magsq);
    m_magsqSum += magsq;

    if (magsq > m_magsqPeak)
    {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;

    sample = sqrtf(magsq);
    m_sampleBuffer[m_sampleCount] = sample;
    m_sampleCount++;

    // Do we have enough data for a frame
    if ((m_sampleCount >= m_totalSamples) && (m_skipCount == 0))
    {
        int startIdx = m_sampleCount - m_totalSamples;

        // Correlate received signal with expected preamble
        Real preambleCorrelation = 0.0f;
        for (int i = 0; i < ADS_B_PREAMBLE_CHIPS*m_samplesPerChip; i++)
            preambleCorrelation += m_preamble[i] * m_sampleBuffer[startIdx+i];

        // If the correlation is exactly 0, it's probably no signal
        if ((preambleCorrelation > m_settings.m_correlationThreshold) && (preambleCorrelation != 0.0f))
        {
            // Skip over preamble
            startIdx += m_settings.m_samplesPerBit*ADS_B_PREAMBLE_BITS;

            // Demodulate waveform to bytes
            unsigned char data[ADS_B_ES_BYTES];
            int byteIdx = 0;
            int currentBit;
            unsigned char currentByte = 0;
            bool adsbOnly = true;
            int df;

            for (int bit = 0; bit < ADS_B_ES_BITS; bit++)
            {
                // PPM (Pulse position modulation) - Each bit spreads to two chips, 1->10, 0->01
                // Determine if bit is 1 or 0, by seeing which chip has largest combined energy over the sampling period
                Real oneSum = 0.0f;
                Real zeroSum = 0.0f;
                for (int i = 0; i < m_samplesPerChip; i++)
                {
                    oneSum += m_sampleBuffer[startIdx+i];
                    zeroSum += m_sampleBuffer[startIdx+m_samplesPerChip+i];
                }
                currentBit = oneSum > zeroSum;
                startIdx += m_settings.m_samplesPerBit;
                // Convert bit to bytes - MSB first
                currentByte |= currentBit << (7-(bit & 0x7));
                if ((bit & 0x7) == 0x7)
                {
                    data[byteIdx++] = currentByte;
                    currentByte = 0;
                    // Don't try to demodulate any further, if this isn't an ADS-B frame
                    // to help reduce processing overhead
                    if (adsbOnly && (bit == 7))
                    {
                        df = ((data[0] >> 3) & ADS_B_DF_MASK);
                        if ((df != 17) && (df != 18))
                            break;
                    }
                }
            }

            // Is ADS-B?
            df = ((data[0] >> 3) & ADS_B_DF_MASK);
            if ((df == 17) || (df == 18))
            {
                crcadsb crc;
                //int icao = (data[1] << 16) | (data[2] << 8) | data[3]; // ICAO aircraft address
                int parity = (data[11] << 16) | (data[12] << 8) | data[13]; // Parity / CRC

                crc.calculate(data, ADS_B_ES_BYTES-3);
                if (parity == crc.get())
                {
                    // Got a valid frame
                    // Don't try to re-demodulate the same frame
                    m_skipCount = (ADS_B_ES_BITS+ADS_B_PREAMBLE_BITS)*ADS_B_CHIPS_PER_BIT*m_samplesPerChip;
                    // Pass to GUI
                    if (getMessageQueueToGUI())
                    {
                        ADSBDemodReport::MsgReportADSB *msg = ADSBDemodReport::MsgReportADSB::create(QByteArray((char*)data, sizeof(data)), preambleCorrelation);
                        getMessageQueueToGUI()->push(msg);
                    }
                    // Pass to worker
                    if (getMessageQueueToWorker())
                    {
                        ADSBDemodReport::MsgReportADSB *msg = ADSBDemodReport::MsgReportADSB::create(QByteArray((char*)data, sizeof(data)), preambleCorrelation);
                        getMessageQueueToWorker()->push(msg);
                    }
                }
            }
        }
    }
    if (m_skipCount > 0)
        m_skipCount--;
    if (m_sampleCount >= 2*m_totalSamples)
    {
        // Copy second half of buffer to first
        memcpy(&m_sampleBuffer[0], &m_sampleBuffer[m_totalSamples], m_totalSamples*sizeof(Real));
        m_sampleCount = m_totalSamples;
    }
    m_sampleIdx++;

}

void ADSBDemodSink::init(int samplesPerBit)
{
    if (m_sampleBuffer)
        delete m_sampleBuffer;
    if (m_preamble)
        delete m_preamble;

    m_totalSamples = samplesPerBit*(ADS_B_PREAMBLE_BITS+ADS_B_ES_BITS);
    m_samplesPerChip = samplesPerBit/ADS_B_CHIPS_PER_BIT;

    m_sampleBuffer = new Real[2*m_totalSamples];
    m_preamble = new Real[ADS_B_PREAMBLE_CHIPS*m_samplesPerChip];
    // Possibly don't look for first chip
    const int preambleChips[] = {1, -1, 1, -1, -1, -1, -1, 1, -1, 1, -1, -1, -1, -1, -1, -1};
    for (int i = 0; i < ADS_B_PREAMBLE_CHIPS; i++)
    {
        for (int j = 0; j < m_samplesPerChip; j++)
        {
            m_preamble[i*m_samplesPerChip+j] = preambleChips[i];
        }
    }
}

void ADSBDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "ADSBDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, m_settings.m_rfBandwidth / 2.2);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) channelSampleRate / (Real) (ADS_B_BITS_PER_SECOND * m_settings.m_samplesPerBit);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void ADSBDemodSink::applySettings(const ADSBDemodSettings& settings, bool force)
{
    qDebug() << "ADSBDemodSink::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_correlationThreshold: " << settings.m_correlationThreshold
            << " m_samplesPerBit: " << settings.m_samplesPerBit
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth)
        || (settings.m_samplesPerBit != m_settings.m_samplesPerBit) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance =  (Real) m_channelSampleRate / (Real) (ADS_B_BITS_PER_SECOND * settings.m_samplesPerBit);
    }
    if ((settings.m_samplesPerBit != m_settings.m_samplesPerBit) || force)
    {
        init(settings.m_samplesPerBit);
    }

    m_settings = settings;
}
