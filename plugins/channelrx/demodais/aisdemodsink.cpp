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
#include "dsp/scopevis.h"
#include "util/db.h"
#include "util/stepfunctions.h"
#include "maincore.h"

#include "aisdemod.h"
#include "aisdemodsink.h"

AISDemodSink::AISDemodSink(AISDemod *aisDemod) :
        m_scopeSink(nullptr),
        m_aisDemod(aisDemod),
        m_channelSampleRate(AISDemodSettings::AISDEMOD_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_rxBuf(nullptr),
        m_train(nullptr),
        m_sampleBufferIndex(0)
{
    m_magsq = 0.0;

    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;
    for (int i = 0; i < AISDemodSettings::m_scopeStreams; i++) {
        m_sampleBuffer[i].resize(m_sampleBufferSize);
    }

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

AISDemodSink::~AISDemodSink()
{
    delete[] m_rxBuf;
    delete[] m_train;
}

void AISDemodSink::sampleToScope(Complex sample, Real magsq, Real fmDemod, Real filt, Real rxBuf, Real corr, Real thresholdMet, Real dcOffset, Real crcValid)
{
    if (m_scopeSink)
    {
        m_sampleBuffer[0][m_sampleBufferIndex] = sample;
        m_sampleBuffer[1][m_sampleBufferIndex] = Complex(magsq, 0.0f);
        m_sampleBuffer[2][m_sampleBufferIndex] = Complex(fmDemod, 0.0f);
        m_sampleBuffer[3][m_sampleBufferIndex] = Complex(filt, 0.0f);
        m_sampleBuffer[4][m_sampleBufferIndex] = Complex(rxBuf, 0.0f);
        m_sampleBuffer[5][m_sampleBufferIndex] = Complex(corr, 0.0f);
        m_sampleBuffer[6][m_sampleBufferIndex] = Complex(thresholdMet, 0.0f);
        m_sampleBuffer[7][m_sampleBufferIndex] = Complex(dcOffset, 0.0f);
        m_sampleBuffer[8][m_sampleBufferIndex] = Complex(crcValid, 0.0f);
        m_sampleBufferIndex++;
        if (m_sampleBufferIndex == m_sampleBufferSize)
        {
            std::vector<ComplexVector::const_iterator> vbegin;

            for (int i = 0; i < AISDemodSettings::m_scopeStreams; i++) {
                vbegin.push_back(m_sampleBuffer[i].begin());
            }

            m_scopeSink->feed(vbegin, m_sampleBufferSize);
            m_sampleBufferIndex = 0;
        }
    }
}

void AISDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

void AISDemodSink::processOneSample(Complex &ci)
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

    // Gaussian filter
    Real filt = m_pulseShape.filter(fmDemod);

    // An input frequency offset corresponds to a DC offset after FM demodulation
    // AIS spec allows up to +-1kHz offset
    // We need to remove this, otherwise it may effect the sampling
    // To calculate what it is, we sum the training sequence, which should be zero

    // Clip, as large noise can result in high correlation
    // Don't clip to 1.0 - as there may be some DC offset (1k/4.8k max dev=0.2)
    Real filtClipped;
    filtClipped = std::fmax(-1.4, std::fmin(1.4, filt));

    // Buffer filtered samples. We buffer enough samples for a max length message
    // before trying to demod, so false triggering can't make us miss anything
    m_rxBuf[m_rxBufIdx] = filtClipped;
    m_rxBufIdx = (m_rxBufIdx + 1) % m_rxBufLength;
    m_rxBufCnt = std::min(m_rxBufCnt + 1, m_rxBufLength);

    Real corr = 0.0f;
    bool scopeCRCValid = false;
    bool scopeCRCInvalid = false;
    Real dcOffset = 0.0f;
    bool thresholdMet = false;
    if (m_rxBufCnt >= m_rxBufLength)
    {
        Real trainingSum = 0.0f;

        // Correlate with training sequence
        // Note that DC offset doesn't matter for this
        // Calculate sum to estimate DC offset
        for (int i = 0; i < m_correlationLength; i++)
        {
            int j = (m_rxBufIdx + i) % m_rxBufLength;
            corr += m_train[i] * m_rxBuf[j];
            trainingSum += m_rxBuf[j];
        }

        // If we meet threshold, try to demod
        // Take abs value, to account for both initial phases
        thresholdMet = fabs(corr) >= m_settings.m_correlationThreshold;
        if (thresholdMet)
        {
            // Use mean of preamble as DC offset
            dcOffset = trainingSum/m_correlationLength;

            // Start demod after (most of) preamble
            int x = (m_rxBufIdx + m_correlationLength*3/4 + 4) % m_rxBufLength;

            // Attempt to demodulate
            bool gotSOP = false;
            int bits = 0;
            int bitCount = 0;
            int onesCount = 0;
            int byteCount = 0;
            int symbolPrev = 0;
            int totalBitCount = 0; // Count of bits after start flag, before bit stuffing removal, including stop flag
            for (int sampleIdx = 0; sampleIdx < m_rxBufLength; sampleIdx += m_samplesPerSymbol)
            {
                // Sum and slice
                // Summing 3 samples seems to give a very small improvement vs just using 1
                int sampleCnt = 3;
                int sampleOffset = -1;
                Real sampleSum = 0.0f;
                for (int i = 0; i < sampleCnt; i++) {
                    sampleSum += m_rxBuf[(x + sampleOffset + i) % m_rxBufLength] - dcOffset;
                }
                int symbol = sampleSum >= 0.0f ? 1 : 0;

                // Move to next symbol
                x = (x + m_samplesPerSymbol) % m_rxBufLength;

                // HDLC deframing

                // NRZI decoding
                int bit;
                if (symbol != symbolPrev) {
                    bit = 0;
                } else {
                    bit = 1;
                }
                symbolPrev = symbol;

                // Store in shift reg
                bits |= bit << bitCount;
                bitCount++;

                if (bit == 1)
                {
                    onesCount++;
                    // Shouldn't ever get 7 1s in a row
                    if ((onesCount == 7) && gotSOP)
                    {
                        gotSOP = false;
                        byteCount = 0;
                        break;
                    }
                }
                else if (bit == 0)
                {
                    if (onesCount == 5)
                    {
                        // Remove bit-stuffing (5 1s followed by a 0)
                        bitCount--;
                    }
                    else if (onesCount == 6)
                    {
                        // Start/end of packet
                        if (gotSOP && (bitCount == 8) && (bits == 0x7e) && (byteCount > 0))
                        {
                            // End of packet
                            // Check CRC is valid
                            m_crc.init();
                            m_crc.calculate(m_bytes, byteCount - 2);
                            uint16_t calcCrc = m_crc.get();
                            uint16_t rxCrc = m_bytes[byteCount-2] | (m_bytes[byteCount-1] << 8);
                            if (calcCrc == rxCrc)
                            {
                                scopeCRCValid = true;
                                QByteArray rxPacket((char *)m_bytes, byteCount - 2); // Don't include CRC
                                //qDebug() << "RX: " << rxPacket.toHex();
                                if (getMessageQueueToChannel())
                                {
                                    // Calculate slot number based on time of start of transmission
                                    // This is unlikely to be accurate in absolute terms, given we don't know latency from SDR or buffering within SDRangel
                                    // But can be used to get an idea of congestion
                                    QDateTime currentTime = QDateTime::currentDateTime();
                                    int txTimeMs = (totalBitCount + 8 + 24 + 8) * (1000.0 / m_settings.m_baud); // Add ramp up, preamble and start-flag
                                    QDateTime startDateTime = currentTime.addMSecs(-txTimeMs);
                                    int ms = startDateTime.time().second() * 1000 + startDateTime.time().msec();
                                    float slotTime = 60.0f * 1000.0f / 2250.0f; // 2250 slots per minute, 26.6ms per slot
                                    int slot = ms / slotTime;
                                    int totalSlots = std::ceil(txTimeMs / slotTime);
                                    AISDemod::MsgMessage *msg = AISDemod::MsgMessage::create(rxPacket, currentTime, slot, totalSlots);
                                    getMessageQueueToChannel()->push(msg);
                                }

                                // Skip over received packet, so we don't try to re-demodulate it
                                m_rxBufCnt -= sampleIdx;
                            }
                            else
                            {
                                //qDebug() << QString("CRC mismatch: %1 %2").arg(calcCrc, 4, 16, QLatin1Char('0')).arg(rxCrc, 4, 16, QLatin1Char('0'));
                                scopeCRCInvalid = true;
                            }
                            break;
                        }
                        else if (gotSOP)
                        {
                            // Repeated start flag without data or misalignment, something not right
                            break;
                        }
                        else
                        {
                            // Start of packet
                            gotSOP = true;
                            bits = 0;
                            bitCount = 0;
                            byteCount = 0;
                            totalBitCount = 0;
                        }
                    }
                    onesCount = 0;
                }

                if (gotSOP)
                {
                    totalBitCount++;
                    if (bitCount == 8)
                    {
                        // Could also check count according to message ID as that varies
                        if (byteCount >= AISDEMOD_MAX_BYTES)
                        {
                            // Too many bytes
                            break;
                        }
                        else
                        {
                            // Got a complete byte
                            m_bytes[byteCount] = bits;
                            byteCount++;
                        }
                        bits = 0;
                        bitCount = 0;
                    }
                }

                // Abort demod if we haven't found start flag within a couple of bytes of presumed preamble
                if (!gotSOP && (sampleIdx >= 16 * m_samplesPerSymbol)) {
                    break;
                }
            }
        }
    }

    // Select signals to feed to scope
    sampleToScope(ci / SDR_RX_SCALEF, magsq, fmDemod, filt, m_rxBuf[m_rxBufIdx], corr / 100.0, thresholdMet, dcOffset, scopeCRCValid ? 1.0 : (scopeCRCInvalid ? -1.0 : 0));

    // Send demod signal to Demod Analzyer feature
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

void AISDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "AISDemodSink::applyChannelSettings:"
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) AISDemodSettings::AISDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
    m_samplesPerSymbol = AISDemodSettings::AISDEMOD_CHANNEL_SAMPLE_RATE / m_settings.m_baud;
    qDebug() << "AISDemodSink::applyChannelSettings: m_samplesPerSymbol: " << m_samplesPerSymbol;
}

void AISDemodSink::applySettings(const AISDemodSettings& settings, bool force)
{
    qDebug() << "AISDemodSink::applySettings:"
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) AISDemodSettings::AISDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
        m_lowpass.create(301, AISDemodSettings::AISDEMOD_CHANNEL_SAMPLE_RATE, settings.m_rfBandwidth / 2.0f);
    }
    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force)
    {
        m_phaseDiscri.setFMScaling(AISDemodSettings::AISDEMOD_CHANNEL_SAMPLE_RATE / (2.0f * settings.m_fmDeviation));
    }

    if ((settings.m_baud != m_settings.m_baud) || force)
    {
        m_samplesPerSymbol = AISDemodSettings::AISDEMOD_CHANNEL_SAMPLE_RATE / settings.m_baud;
        qDebug() << "ISDemodSink::applySettings: m_samplesPerSymbol: " << m_samplesPerSymbol << " baud " << settings.m_baud;
        m_pulseShape.create(0.5, 3, m_samplesPerSymbol);

        // Recieve buffer, long enough for one max length message
        delete[] m_rxBuf;
        m_rxBufLength = AISDEMOD_MAX_BYTES*8*m_samplesPerSymbol;
        m_rxBuf = new Real[m_rxBufLength];
        m_rxBufIdx = 0;
        m_rxBufCnt = 0;

        // Create 24-bit training sequence for correlation
        delete[] m_train;
        m_correlationLength = 24*m_samplesPerSymbol;
        m_train = new Real[m_correlationLength]();
        const int trainNRZ[24] = {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1};

        // Pulse shape filter takes a few symbols before outputting expected shape
        for (int j = 0; j < m_samplesPerSymbol; j++)
            m_pulseShape.filter(-1.0f);
        for (int j = 0; j < m_samplesPerSymbol; j++)
            m_pulseShape.filter(1.0f);
        for (int i = 0; i < 24; i++)
        {
            for (int j = 0; j < m_samplesPerSymbol; j++)
            {
                m_train[i*m_samplesPerSymbol+j] = m_pulseShape.filter(trainNRZ[i] * 2.0f - 1.0f);
            }
        }
    }

    m_settings = settings;
}
