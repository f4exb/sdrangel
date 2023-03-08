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
#include <QRegularExpression>

#include <complex.h>

#include "dsp/dspengine.h"
#include "dsp/scopevis.h"
#include "util/db.h"
#include "maincore.h"

#include "navtexdemod.h"
#include "navtexdemodsink.h"

NavtexDemodSink::NavtexDemodSink(NavtexDemod *packetDemod) :
        m_navtexDemod(packetDemod),
        m_channelSampleRate(NavtexDemodSettings::NAVTEXDEMOD_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_exp(nullptr),
        m_sampleBufferIndex(0)
{
    m_magsq = 0.0;

    m_sampleBuffer.resize(m_sampleBufferSize);

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);

    m_lowpassComplex1.create(301, NavtexDemodSettings::NAVTEXDEMOD_CHANNEL_SAMPLE_RATE, NavtexDemodSettings::NAVTEXDEMOD_BAUD_RATE * 1.1);
    m_lowpassComplex2.create(301, NavtexDemodSettings::NAVTEXDEMOD_CHANNEL_SAMPLE_RATE, NavtexDemodSettings::NAVTEXDEMOD_BAUD_RATE * 1.1);
}

NavtexDemodSink::~NavtexDemodSink()
{
    delete[] m_exp;
}

void NavtexDemodSink::sampleToScope(Complex sample)
{
    if (m_scopeSink)
    {
        Real r = std::real(sample) * SDR_RX_SCALEF;
        Real i = std::imag(sample) * SDR_RX_SCALEF;
        m_sampleBuffer[m_sampleBufferIndex++] = Sample(r, i);

        if (m_sampleBufferIndex == m_sampleBufferSize)
        {
            std::vector<SampleVector::const_iterator> vbegin;
            vbegin.push_back(m_sampleBuffer.begin());
            m_scopeSink->feed(vbegin, m_sampleBufferSize);
            m_sampleBufferIndex = 0;
        }
    }
}

void NavtexDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

void NavtexDemodSink::eraseChars(int n)
{
    if (getMessageQueueToChannel())
    {
        QString msg = QString("%1").arg(QChar(0x8)); // Backspace
        for (int i = 0; i < n; i++)
        {
            NavtexDemod::MsgCharacter *msg = NavtexDemod::MsgCharacter::create(QChar(0x8));
            getMessageQueueToChannel()->push(msg);
        }
    }
}

void NavtexDemodSink::processOneSample(Complex &ci)
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
    m_data = biasedData < 0;

    // Generate sampling clock by aligning to correlator zero-crossing
    if (m_data && !m_dataPrev)
    {
        if ((m_clockCount > 2) && (m_clockCount < m_samplesPerBit*3/4) && m_gotSOP)
        {
            //qDebug() << "Clock toggle ignored at " << m_clockCount;
        }
        else
        {
            m_clockCount = 0;
            m_clock = false;
        }
    }
    else
    {
        // Sample in middle of symbol
        if (m_clockCount == m_samplesPerBit/2)
        {
            receiveBit(m_data);
            m_clock = true;
        }
        m_clockCount = (m_clockCount + 1) % m_samplesPerBit;
        if (m_clockCount == 0) {
            m_clock = false;
        }
    }

    // Select signals to feed to scope
    Complex scopeSample;
    switch (m_settings.m_scopeCh1)
    {
    case 0:
        scopeSample.real(ci.real());
        break;
    case 1:
        scopeSample.real(ci.imag());
        break;
    case 2:
        scopeSample.real(real(exp));
        break;
    case 3:
        scopeSample.real(imag(exp));
        break;
    case 4:
        scopeSample.real(real(corr1));
        break;
    case 5:
        scopeSample.real(imag(corr1));
        break;
    case 6:
        scopeSample.real(real(corr2));
        break;
    case 7:
        scopeSample.real(imag(corr2));
        break;
    case 8:
        scopeSample.real(abs1Filt);
        break;
    case 9:
        scopeSample.real(abs2Filt);
        break;
    case 10:
        scopeSample.real(env1);
        break;
    case 11:
        scopeSample.real(env2);
        break;
    case 12:
        scopeSample.real(unbiasedData);
        break;
    case 13:
        scopeSample.real(biasedData);
        break;
    case 14:
        scopeSample.real(m_data);
        break;
    case 15:
        scopeSample.real(m_clock);
        break;
    case 16:
        scopeSample.real(m_bit);
        break;
    case 17:
        scopeSample.real(m_gotSOP);
        break;
    }
    switch (m_settings.m_scopeCh2)
    {
    case 0:
        scopeSample.imag(ci.real());
        break;
    case 1:
        scopeSample.imag(ci.imag());
        break;
    case 2:
        scopeSample.imag(real(exp));
        break;
    case 3:
        scopeSample.imag(imag(exp));
        break;
    case 4:
        scopeSample.imag(real(corr1));
        break;
    case 5:
        scopeSample.imag(imag(corr1));
        break;
    case 6:
        scopeSample.imag(real(corr2));
        break;
    case 7:
        scopeSample.imag(imag(corr2));
        break;
    case 8:
        scopeSample.imag(abs1Filt);
        break;
    case 9:
        scopeSample.imag(abs2Filt);
        break;
    case 10:
        scopeSample.imag(env1);
        break;
    case 11:
        scopeSample.imag(env2);
        break;
    case 12:
        scopeSample.imag(unbiasedData);
        break;
    case 13:
        scopeSample.imag(biasedData);
        break;
    case 14:
        scopeSample.imag(m_data);
        break;
    case 15:
        scopeSample.imag(m_clock);
        break;
    case 16:
        scopeSample.imag(m_bit);
        break;
    case 17:
        scopeSample.imag(m_gotSOP);
        break;
    }
    sampleToScope(scopeSample);
}

void NavtexDemodSink::receiveBit(bool bit)
{
    m_bit = bit;

    // Store in shift reg
    m_bits = (m_bits << 1) | m_bit;
    m_bitCount++;

    if (!m_gotSOP)
    {
        if (m_bitCount == 14)
        {
            if ((m_bits & 0x3fff) == 0x19f8) // phase 2 followed by phase 1
            {
                m_gotSOP = true;
                m_bitCount = 0;
                m_sitorBDecoder.init();
            }
            else
            {
                m_bitCount--;
            }
        }
    }
    else
    {
        if (m_bitCount == 7)
        {
            signed char c = m_sitorBDecoder.decode(m_bits & 0x7f);
            if (c != -1)
            {
                //qDebug() << "Out: " << SitorBDecoder::printable(c);
                m_consecutiveErrors = 0;

                if ((c != '<') && (c != '>') && (c != 0x2))
                {
                    // 7 bytes per second, so may as well send individually to be displayed
                    if (getMessageQueueToChannel())
                    {
                        NavtexDemod::MsgCharacter *msg = NavtexDemod::MsgCharacter::create(SitorBDecoder::printable(c));
                        getMessageQueueToChannel()->push(msg);
                    }
                    // Add character to message buffer
                    m_messageBuffer.append(c);
                }
                else
                {
                    if (m_messageBuffer.size() > 0)
                    {
                        QRegularExpression re("[Z*][C*][Z*][C*](.|\n|\r)*[N*][N*][N*][N*]");
                        QRegularExpressionMatch match = re.match(m_messageBuffer);
                        if (match.hasMatch())
                        {
                            if (getMessageQueueToChannel())
                            {
                                NavtexMessage navtexMsg = NavtexMessage(match.captured(0));

                                float rssi = CalcDb::dbPower(m_rssiMagSqSum / m_rssiMagSqCount);
                                NavtexDemod::MsgMessage *msg = NavtexDemod::MsgMessage::create(navtexMsg, m_sitorBDecoder.getErrors(), rssi);
                                getMessageQueueToChannel()->push(msg);
                            }
                            // Navtex messages can span multiple blocks?
                            m_messageBuffer = "";
                        }
                    }
                    if (c == 0x2) // End of text
                    {
                        // Reset demod
                        init();
                    }
                }

            }
            if (c == '*')
            {
                m_errorCount++;
                m_consecutiveErrors++;
                // ITU 476-5 just says return to standby after the percentage of
                // mutilated signals received has reached a predetermined value
                // without saying what that value is
                if (m_messageBuffer.size() >= 12)
                {
                    float errorPC = m_errorCount / (float)(m_messageBuffer.size() + m_errorCount);
                    if (errorPC >= 0.2f)
                    {
                        //qDebug() << "Too many errors" << m_errorCount << m_messageBuffer.size();
                        init();
                    }
                }
                else if (m_errorCount >= 3)
                {
                    //qDebug() << "Too many errors" << m_errorCount << m_messageBuffer.size();
                    eraseChars(m_messageBuffer.size());
                    init();
                }
                if (m_consecutiveErrors >= 5)
                {
                    //qDebug() << "Too many consequtive errors";
                    init();
                }
            }
            m_bitCount = 0;
        }
    }
}

void NavtexDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "NavtexDemodSink::applyChannelSettings:"
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) NavtexDemodSettings::NAVTEXDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void NavtexDemodSink::init()
{
    m_expIdx = 0;
    m_bit = 0;
    m_bits = 0;
    m_bitCount = 0;
    m_gotSOP = false;
    m_errorCount = 0;
    m_clockCount = 0;
    m_clock = 0;
    m_rssiMagSqSum = 0.0;
    m_rssiMagSqCount = 0;
    m_consecutiveErrors = 0;
    m_sitorBDecoder.init();
    m_messageBuffer = "";
}

void NavtexDemodSink::applySettings(const NavtexDemodSettings& settings, bool force)
{
    qDebug() << "NavtexDemodSink::applySettings:"
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) NavtexDemodSettings::NAVTEXDEMOD_CHANNEL_SAMPLE_RATE;
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
            f0 += 2.0f * (Real)M_PI * (NavtexDemodSettings::NAVTEXDEMOD_FREQUENCY_SHIFT/2.0f) / NavtexDemodSettings::NAVTEXDEMOD_CHANNEL_SAMPLE_RATE;
        }
        init();
        // Due to start and stop bits, we should get mark and space at least every 8 bits
        // while something is being transmitted
        m_movMax1.setSize(m_samplesPerBit * 8);
        m_movMax2.setSize(m_samplesPerBit * 8);
    }

    m_settings = settings;
}

