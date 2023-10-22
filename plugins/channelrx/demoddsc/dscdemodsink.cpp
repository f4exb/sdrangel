///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include <complex.h>

#include "dsp/dspengine.h"
#include "dsp/scopevis.h"
#include "util/db.h"
#include "util/popcount.h"
#include "maincore.h"

#include "dscdemod.h"
#include "dscdemodsink.h"

DSCDemodSink::DSCDemodSink(DSCDemod *dscDemod) :
        m_scopeSink(nullptr),
        m_dscDemod(dscDemod),
        m_channelSampleRate(DSCDemodSettings::DSCDEMOD_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_exp(nullptr),
        m_sampleBufferIndex(0)
{
    m_magsq = 0.0;

    for (int i = 0; i < DSCDemodSettings::m_scopeStreams; i++) {
        m_sampleBuffer[i].resize(m_sampleBufferSize);
    }

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);

    m_lowpassComplex1.create(301, DSCDemodSettings::DSCDEMOD_CHANNEL_SAMPLE_RATE, DSCDemodSettings::DSCDEMOD_BAUD_RATE * 1.1);
    m_lowpassComplex2.create(301, DSCDemodSettings::DSCDEMOD_CHANNEL_SAMPLE_RATE, DSCDemodSettings::DSCDEMOD_BAUD_RATE * 1.1);
}

DSCDemodSink::~DSCDemodSink()
{
    delete[] m_exp;
}

void DSCDemodSink::sampleToScope(Complex sample, Real abs1Filt, Real abs2Filt, Real unbiasedData, Real biasedData)
{
    if (m_scopeSink)
    {
        m_sampleBuffer[0][m_sampleBufferIndex] = sample;
        m_sampleBuffer[1][m_sampleBufferIndex] = Complex(m_magsq, 0.0f);
        m_sampleBuffer[2][m_sampleBufferIndex] = Complex(abs1Filt, 0.0f);
        m_sampleBuffer[3][m_sampleBufferIndex] = Complex(abs2Filt, 0.0f);
        m_sampleBuffer[4][m_sampleBufferIndex] = Complex(unbiasedData, 0.0f);
        m_sampleBuffer[5][m_sampleBufferIndex] = Complex(biasedData, 0.0f);
        m_sampleBuffer[6][m_sampleBufferIndex] = Complex(m_data, 0.0f);
        m_sampleBuffer[7][m_sampleBufferIndex] = Complex(m_clock, 0.0f);
        m_sampleBuffer[8][m_sampleBufferIndex] = Complex(m_bit, 0.0f);
        m_sampleBuffer[9][m_sampleBufferIndex] = Complex(m_gotSOP, 0.0f);
        m_sampleBufferIndex++;

        if (m_sampleBufferIndex == m_sampleBufferSize)
        {
            std::vector<ComplexVector::const_iterator> vbegin;

            for (int i = 0; i < DSCDemodSettings::m_scopeStreams; i++) {
                vbegin.push_back(m_sampleBuffer[i].begin());
            }

            m_scopeSink->feed(vbegin, m_sampleBufferSize);
            m_sampleBufferIndex = 0;
        }
    }
}

void DSCDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    Complex ci;

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex c(it->real(), it->imag());
        c *= m_nco.nextIQ();

        if (m_interpolatorDistance < 1.0f) // interpolate
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

void DSCDemodSink::processOneSample(Complex &ci)
{
    // Calculate average and peak levels for level meter
    double magsqRaw = ci.real()*ci.real() + ci.imag()*ci.imag();;
    Real magsq = magsqRaw / (SDR_RX_SCALED*SDR_RX_SCALED);
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak)
    {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;

    // Sum power while data is being received
    if (m_gotSOP)
    {
        m_rssiMagSqSum += magsq;
        m_rssiMagSqCount++;
    }

    ci /= SDR_RX_SCALEF;

    // Correlate with expected frequencies
    Complex exp = m_exp[m_expIdx];
    m_expIdx = (m_expIdx + 1) % m_expLength;
    Complex corr1 = ci * exp;
    Complex corr2 = ci * std::conj(exp);

    // Low pass filter
    Real abs1Filt = std::abs(m_lowpassComplex1.filter(corr1));
    Real abs2Filt = std::abs(m_lowpassComplex2.filter(corr2));

    // Envelope calculation
    m_movMax1(abs1Filt);
    m_movMax2(abs2Filt);
    Real env1 = m_movMax1.getMaximum();
    Real env2 = m_movMax2.getMaximum();

    // Automatic threshold correction to compensate for frequency selective fading
    // http://www.w7ay.net/site/Technical/ATC/index.html
    Real bias1 = abs1Filt - 0.5 * env1;
    Real bias2 = abs2Filt - 0.5 * env2;
    Real unbiasedData = abs1Filt - abs2Filt;
    Real biasedData = bias1 - bias2;

    // Save current data for edge detection
    m_dataPrev = m_data;
    // Set data according to stongest correlation
    m_data = biasedData > 0;

    // Calculate timing error (we expect clockCount to be 0 when data changes), and add a proportion of it
    if (m_data && !m_dataPrev) {
        m_clockCount -= m_clockCount * 0.25;
    }

    m_clockCount += 1.0;
    if (m_clockCount >= m_samplesPerBit/2.0-1.0)
    {
        // Sample in middle of symbol
        receiveBit(m_data);
        m_clock = 1;
        // Wrap clock counter
        m_clockCount -= m_samplesPerBit;
    }
    else
    {
        m_clock = 0;
    }

    sampleToScope(ci, abs1Filt, abs2Filt, unbiasedData, biasedData);
}

const QList<DSCDemodSink::PhasingPattern> DSCDemodSink::m_phasingPatterns = {
    {0b1011111001'1111011001'1011111001, 9},      // 125 111 125
    {0b1111011001'1011111001'0111011010, 8},      // 111 125 110
    {0b1011111001'0111011010'1011111001, 7},      // 125 110 125
    {0b0111011010'1011111001'1011011010, 6},      // 110 125 109
    {0b1011111001'1011011010'1011111001, 5},      // 125 109 125
    {0b1011011010'1011111001'0011011011, 4},      // 109 125 108
    {0b1011111001'0011011011'1011111001, 3},      // 125 108 125
    {0b0011011011'1011111001'1101011010, 2},      // 108 125 107
    {0b1011111001'1101011010'1011111001, 1},      // 125 107 125
    {0b1101011010'1011111001'0101011011, 0},      // 107 125 106
};

void DSCDemodSink::receiveBit(bool bit)
{
    m_bit = bit;

    // Store in shift reg
    m_bits = (m_bits << 1) | m_bit;
    m_bitCount++;

    if (!m_gotSOP)
    {
        // Dot pattern - 200 1/0s or 20 1/0s
        // Phasing pattern - 6 DX=125 RX=111 110 109 108 107 106 105 104
        // Phasing is considered to be achieved when two DXs and one RX, or two RXs and one DX, or three RXs in the appropriate DX or RX positions, respectively, are successfully received.
        if (m_bitCount == 10*3)
        {
            m_bitCount--;

            unsigned int pat = m_bits & 0x3fffffff;
            for (int i = 0; i < m_phasingPatterns.size(); i++)
            {
                if (pat == m_phasingPatterns[i].m_pattern)
                {
                    m_dscDecoder.init(m_phasingPatterns[i].m_offset);
                    m_gotSOP = true;
                    m_bitCount = 0;
                    m_rssiMagSqSum = 0.0;
                    m_rssiMagSqCount = 0;
                    break;
                }
            }
        }
    }
    else
    {
        if (m_bitCount == 10)
        {
            if (m_dscDecoder.decodeBits(m_bits & 0x3ff))
            {
                QByteArray bytes = m_dscDecoder.getMessage();
                DSCMessage message(bytes, QDateTime::currentDateTime());
                //qDebug() << "RX Bytes: " << bytes.toHex();
                //qDebug() << "DSC Message: " << message.toString();

                if (getMessageQueueToChannel())
                {
                    float rssi = CalcDb::dbPower(m_rssiMagSqSum / m_rssiMagSqCount);
                    DSCDemod::MsgMessage *msg = DSCDemod::MsgMessage::create(message, m_dscDecoder.getErrors(), rssi);
                    getMessageQueueToChannel()->push(msg);
                }

                // Reset demod
                init();
            }
            m_bitCount = 0;
        }
    }
}

void DSCDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "DSCDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, m_settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) channelSampleRate / (Real) DSCDemodSettings::DSCDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void DSCDemodSink::init()
{
    m_expIdx = 0;
    m_bit = 0;
    m_bits = 0;
    m_bitCount = 0;
    m_gotSOP = false;
    m_errorCount = 0;
    m_clockCount = -m_samplesPerBit/2.0;
    m_clock = 0;
    m_int = 0.0;
    m_rssiMagSqSum = 0.0;
    m_rssiMagSqCount = 0;
    m_consecutiveErrors = 0;
    m_messageBuffer = "";
}

void DSCDemodSink::applySettings(const DSCDemodSettings& settings, bool force)
{
    qDebug() << "DSCDemodSink::applySettings:"
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) DSCDemodSettings::DSCDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    if (force)
    {
        delete[] m_exp;
        m_exp = new Complex[m_expLength];
        Real f0 = 0.0f;
        for (int i = 0; i < m_expLength; i++)
        {
            m_exp[i] = Complex(cos(f0), sin(f0));
            f0 += 2.0f * (Real)M_PI * (DSCDemodSettings::DSCDEMOD_FREQUENCY_SHIFT/2.0f) / DSCDemodSettings::DSCDEMOD_CHANNEL_SAMPLE_RATE;
        }
        init();

        m_movMax1.setSize(m_samplesPerBit * 8);
        m_movMax2.setSize(m_samplesPerBit * 8);
    }

    m_settings = settings;
}
