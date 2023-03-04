///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>

#include <complex.h>

#include "dsp/dspengine.h"
#include "dsp/datafifo.h"
#include "util/db.h"
#include "util/stepfunctions.h"
#include "maincore.h"

#include "packetdemod.h"
#include "packetdemodsink.h"

PacketDemodSink::PacketDemodSink(PacketDemod *packetDemod) :
        m_packetDemod(packetDemod),
        m_channelSampleRate(PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_f1(nullptr),
        m_f0(nullptr),
        m_corrBuf(nullptr),
        m_corrIdx(0),
        m_corrCnt(0)
{
    m_magsq = 0.0;

    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

PacketDemodSink::~PacketDemodSink()
{
    delete[] m_f1;
    delete[] m_f0;
    delete[] m_corrBuf;
}

void PacketDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

void PacketDemodSink::processOneSample(Complex &ci)
{
    Complex ca;

    // FM demodulation
    double magsqRaw;
    Real deviation;
    Real fmDemod = m_phaseDiscri.phaseDiscriminatorDelta(ci, magsqRaw, deviation);

    // Calculate average and peak levels for level meter
    Real magsq = magsqRaw / (SDR_RX_SCALED*SDR_RX_SCALED);
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak)
    {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;

    m_corrBuf[m_corrIdx] = fmDemod;
    if (m_corrCnt >= m_correlationLength)
    {
        // Correlate with 1200 + 2200 baud complex exponentials
        Complex corrF0 = 0.0f;
        Complex corrF1 = 0.0f;
        for (int i = 0; i < m_correlationLength; i++)
        {
            int j = m_corrIdx - i;
            if (j < 0)
                j += m_correlationLength;
            corrF0 += m_f0[i] * m_corrBuf[j];
            corrF1 += m_f1[i] * m_corrBuf[j];
        }
        m_corrCnt--; // Avoid overflow in increment below

        // Low pass filter, to minimize changes above the baud rate
        Real f0Filt = m_lowpassF0.filter(std::abs(corrF0));
        Real f1Filt = m_lowpassF1.filter(std::abs(corrF1));

        // Determine which is the closest match and then quantise to 1 or -1
        // FIXME: We should try to account for the fact that higher frequencies can have preemphasis
        float diff = f1Filt - f0Filt;
        int sample = diff >= 0.0f ? 1 : 0;

        // Look for edge
        if (sample != m_samplePrev)
        {
            m_syncCount = PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE/m_settings.getBaudRate()/2;
        }
        else
        {
            m_syncCount--;
            if (m_syncCount <= 0)
            {
                // HDLC deframing

                // Should be in the middle of the symbol
                // NRZI decoding
                int bit;
                if (sample != m_symbolPrev)
                    bit = 0;
                else
                    bit = 1;
                m_symbolPrev = sample;

                // Store in shift reg
                m_bits |= bit << m_bitCount;
                m_bitCount++;

                if (bit == 1)
                {
                    m_onesCount++;
                    // Shouldn't ever get 7 1s in a row
                    if ((m_onesCount == 7) && m_gotSOP)
                    {
                        m_gotSOP = false;
                        m_byteCount = 0;
                    }
                }
                else if (bit == 0)
                {
                    if (m_onesCount == 5)
                    {
                        // Remove bit-stuffing (5 1s followed by a 0)
                        m_bitCount--;
                    }
                    else if (m_onesCount == 6)
                    {
                        // Start/end of packet
                        if ((m_bitCount == 8) && (m_bits == 0x7e) && (m_byteCount > 0))
                        {
                            // End of packet
                            // Check CRC is valid
                            m_crc.init();
                            m_crc.calculate(m_bytes, m_byteCount - 2);
                            uint16_t calcCrc = m_crc.get();
                            uint16_t rxCrc = m_bytes[m_byteCount-2] | (m_bytes[m_byteCount-1] << 8);
                            if (calcCrc == rxCrc)
                            {
                                QByteArray rxPacket((char *)m_bytes, m_byteCount);
                                qDebug() << "RX: " << rxPacket.toHex();
                                if (getMessageQueueToChannel())
                                {
                                    MainCore::MsgPacket *msg = MainCore::MsgPacket::create(m_packetDemod, rxPacket, QDateTime::currentDateTime()); // FIXME pointer
                                    getMessageQueueToChannel()->push(msg);
                                }
                            }
                            else
                                qDebug() << QString("PacketDemodSink::processOneSample: CRC mismatch: %1 %2")
                                    .arg(calcCrc, 4, 16,  QLatin1Char('0'))
                                    .arg(rxCrc, 4, 16, QLatin1Char('0'));
                            // Reset state to start receiving next packet
                            m_gotSOP = false;
                            m_bits = 0;
                            m_bitCount = 0;
                            m_byteCount = 0;
                        }
                        else
                        {
                            // Start of packet
                            m_gotSOP = true;
                            m_bits = 0;
                            m_bitCount = 0;
                            m_byteCount = 0;
                        }
                    }
                    m_onesCount = 0;
                }

                if (m_gotSOP)
                {
                    if (m_bitCount == 8)
                    {
                        if (m_byteCount >= 512)
                        {
                            // Too many bytes
                            m_gotSOP = false;
                            m_byteCount = 0;
                        }
                        else
                        {
                            m_bytes[m_byteCount] = m_bits;
                            m_byteCount++;
                        }
                        m_bits = 0;
                        m_bitCount = 0;
                    }
                }
                m_syncCount = PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE/m_settings.getBaudRate();
            }
        }
        m_samplePrev = sample;
    }
    m_corrIdx = (m_corrIdx + 1) % m_correlationLength;
    m_corrCnt++;

    m_demodBuffer[m_demodBufferFill++] = fmDemod * std::numeric_limits<int16_t>::max();

    if (m_demodBufferFill >= m_demodBuffer.size())
    {
        QList<ObjectPipe*> dataPipes;
        MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", dataPipes);

        if (dataPipes.size() > 0)
        {
            QList<ObjectPipe*>::iterator it = dataPipes.begin();

            for (; it != dataPipes.end(); ++it)
            {
                DataFifo *fifo = qobject_cast<DataFifo*>((*it)->m_element);

                if (fifo) {
                    fifo->write((quint8*) &m_demodBuffer[0], m_demodBuffer.size() * sizeof(qint16), DataFifo::DataTypeI16);
                }
            }
        }

        m_demodBufferFill = 0;
    }
}

void PacketDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "PacketDemodSink::applyChannelSettings:"
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void PacketDemodSink::applySettings(const PacketDemodSettings& settings, bool force)
{
    qDebug() << "PacketDemodSink::applySettings:"
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }
    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force)
    {
        m_phaseDiscri.setFMScaling(PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE / (2.0f * settings.m_fmDeviation));
    }

    if (force)
    {
        delete[] m_f1;
        delete[] m_f0;
        delete[] m_corrBuf;
        m_correlationLength = PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE/settings.getBaudRate();
        m_f1 = new Complex[m_correlationLength]();
        m_f0 = new Complex[m_correlationLength]();
        m_corrBuf = new Complex[m_correlationLength]();
        m_corrIdx = 0;
        m_corrCnt = 0;
        Real f0 = 0.0f;
        Real f1 = 0.0f;
        for (int i = 0; i < m_correlationLength; i++)
        {
            m_f0[i] = Complex(cos(f0), sin(f0));
            m_f1[i] = Complex(cos(f1), sin(f1));
            f0 += 2.0f*(Real)M_PI*2200.0f/PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE;
            f1 += 2.0f*(Real)M_PI*1200.0f/PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE;
        }

        m_lowpassF1.create(301, PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE, settings.getBaudRate() * 1.1f);
        m_lowpassF0.create(301, PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE, settings.getBaudRate() * 1.1f);
        m_samplePrev = 0;
        m_syncCount = 0;
        m_symbolPrev = 0;
        m_bits = 0;
        m_bitCount = 0;
        m_onesCount = 0;
        m_gotSOP = false;
        m_byteCount = 0;
    }

    m_settings = settings;
}
