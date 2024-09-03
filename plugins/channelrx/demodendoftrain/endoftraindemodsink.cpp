///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include "dsp/datafifo.h"
#include "device/deviceapi.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"

#include "endoftraindemod.h"
#include "endoftraindemodsink.h"

EndOfTrainDemodSink::EndOfTrainDemodSink(EndOfTrainDemod *endoftrainDemod) :
        m_scopeSink(nullptr),
        m_endoftrainDemod(endoftrainDemod),
        m_channelSampleRate(EndOfTrainDemodSettings::CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_f1(nullptr),
        m_f0(nullptr),
        m_corrBuf(nullptr),
        m_corrIdx(0),
        m_corrCnt(0),
        m_sampleBufferIndex(0)
{
    m_magsq = 0.0;

    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;
    for (int i = 0; i < EndOfTrainDemodSettings::m_scopeStreams; i++) {
        m_sampleBuffer[i].resize(m_sampleBufferSize);
    }

    applySettings(m_settings, QStringList(), true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

EndOfTrainDemodSink::~EndOfTrainDemodSink()
{
    delete[] m_f1;
    delete[] m_f0;
    delete[] m_corrBuf;
}

void EndOfTrainDemodSink::sampleToScope(Complex sample, Real s1, Real s2, Real s3, Real s4, Real s5, Real s6, Real s7, Real s8)
{
    if (m_scopeSink)
    {
        m_sampleBuffer[0][m_sampleBufferIndex] = sample;
        m_sampleBuffer[1][m_sampleBufferIndex] = Complex(s1, 0.0f);
        m_sampleBuffer[2][m_sampleBufferIndex] = Complex(s2, 0.0f);
        m_sampleBuffer[3][m_sampleBufferIndex] = Complex(s3, 0.0f);
        m_sampleBuffer[4][m_sampleBufferIndex] = Complex(s4, 0.0f);
        m_sampleBuffer[5][m_sampleBufferIndex] = Complex(s5, 0.0f);
        m_sampleBuffer[6][m_sampleBufferIndex] = Complex(s6, 0.0f);
        m_sampleBuffer[7][m_sampleBufferIndex] = Complex(s7, 0.0f);
        m_sampleBuffer[8][m_sampleBufferIndex] = Complex(s8, 0.0f);
        m_sampleBufferIndex++;
        if (m_sampleBufferIndex == m_sampleBufferSize)
        {
            std::vector<ComplexVector::const_iterator> vbegin;

            for (int i = 0; i < EndOfTrainDemodSettings::m_scopeStreams; i++) {
                vbegin.push_back(m_sampleBuffer[i].begin());
            }

            m_scopeSink->feed(vbegin, m_sampleBufferSize);
            m_sampleBufferIndex = 0;
        }
    }
}

void EndOfTrainDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

void EndOfTrainDemodSink::processOneSample(Complex &ci)
{
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

    Real f0Filt = 0.0f;
    Real f1Filt = 0.0f;
    float diff = 0.0;
    int sample = 0;
    int bit = 0;

    m_corrBuf[m_corrIdx] = fmDemod;
    if (m_corrCnt >= m_correlationLength)
    {
        // Correlate with 1200 + 1800 baud complex exponentials
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
        f0Filt = m_lowpassF0.filter(std::abs(corrF0));
        f1Filt = m_lowpassF1.filter(std::abs(corrF1));

        // Determine which is the closest match and then quantise to 1 or -1
        diff = f1Filt - f0Filt;
        sample = diff >= 0.0f ? 1 : 0;

        // Look for edge
        if (sample != m_samplePrev)
        {
            m_syncCount = EndOfTrainDemodSettings::CHANNEL_SAMPLE_RATE/EndOfTrainDemodSettings::BAUD_RATE/2;
        }
        else
        {
            m_syncCount--;
            if (m_syncCount <= 0)
            {
                bit = sample;

                // Store in shift reg - LSB first
                m_bits |= bit << m_bitCount;

                m_bitCount++;

                if (!m_gotSOP)
                {
                    if (m_bitCount >= 17)
                    {
                        // Look for frame sync
                        if ((m_bits & 0x1ffff) == 0x91D5)
                        {
                            // Start of packet
                            m_gotSOP = true;
                            m_bits = 0;
                            m_bitCount = 0;
                            m_byteCount = 0;
                        }
                        else
                        {
                            m_bitCount--;
                            m_bits >>= 1;
                        }
                    }
                }
                else
                {
                    if (m_bitCount == 8)
                    {
                        if (m_byteCount == 8)
                        {
                            QByteArray rxPacket((char *)m_bytes, m_byteCount);
                            //qDebug() << "RX: " << rxPacket.toHex();
                            if (getMessageQueueToChannel())
                            {
                                QDateTime dateTime = QDateTime::currentDateTime();
                                if (m_settings.m_useFileTime)
                                {
                                    QString hardwareId = m_endoftrainDemod->getDeviceAPI()->getHardwareId();

                                    if ((hardwareId == "FileInput") || (hardwareId == "SigMFFileInput"))
                                    {
                                        QString dateTimeStr;
                                        int deviceIdx = m_endoftrainDemod->getDeviceSetIndex();

                                        if (ChannelWebAPIUtils::getDeviceReportValue(deviceIdx, "absoluteTime", dateTimeStr)) {
                                            dateTime = QDateTime::fromString(dateTimeStr, Qt::ISODateWithMs);
                                        }
                                    }
                                }

                                MainCore::MsgPacket *msg = MainCore::MsgPacket::create(m_endoftrainDemod, rxPacket, dateTime);
                                getMessageQueueToChannel()->push(msg);
                            }
                            // Reset state to start receiving next packet
                            m_gotSOP = false;
                            m_bits = 0;
                            m_bitCount = 0;
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
                m_syncCount = EndOfTrainDemodSettings::CHANNEL_SAMPLE_RATE/EndOfTrainDemodSettings::BAUD_RATE;
            }
        }
        m_samplePrev = sample;
    }
    m_corrIdx = (m_corrIdx + 1) % m_correlationLength;
    m_corrCnt++;

    // Select signals to feed to scope
    sampleToScope(ci / SDR_RX_SCALEF, magsq, fmDemod, f0Filt, f1Filt, diff, sample, bit, m_gotSOP);

    // Send demod signal to Demod Analyzer feature
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

void EndOfTrainDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "EndOfTrainDemodSink::applyChannelSettings:"
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) EndOfTrainDemodSettings::CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void EndOfTrainDemodSink::applySettings(const EndOfTrainDemodSettings& settings, const QStringList& settingsKeys, bool force)
{
    qDebug() << "EndOfTrainDemodSink::applySettings:"
             << settings.getDebugString(settingsKeys, force)
             << " force: " << force;

    if (settingsKeys.contains("rfBandwidth") || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) EndOfTrainDemodSettings::CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }
    if (settingsKeys.contains("fmDeviation") || force)
    {
        m_phaseDiscri.setFMScaling(EndOfTrainDemodSettings::CHANNEL_SAMPLE_RATE / (2.0f * settings.m_fmDeviation));
    }

    if (force)
    {
        delete[] m_f1;
        delete[] m_f0;
        delete[] m_corrBuf;
        m_correlationLength = EndOfTrainDemodSettings::CHANNEL_SAMPLE_RATE/EndOfTrainDemodSettings::BAUD_RATE;
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
            f0 += 2.0f*(Real)M_PI*1800.0f/EndOfTrainDemodSettings::CHANNEL_SAMPLE_RATE;
            f1 += 2.0f*(Real)M_PI*1200.0f/EndOfTrainDemodSettings::CHANNEL_SAMPLE_RATE;
        }

        m_lowpassF1.create(301, EndOfTrainDemodSettings::CHANNEL_SAMPLE_RATE, EndOfTrainDemodSettings::BAUD_RATE * 1.1f);
        m_lowpassF0.create(301, EndOfTrainDemodSettings::CHANNEL_SAMPLE_RATE, EndOfTrainDemodSettings::BAUD_RATE * 1.1f);
        m_samplePrev = 0;
        m_syncCount = 0;
        m_bits = 0;
        m_bitCount = 0;
        m_gotSOP = false;
        m_byteCount = 0;
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}
