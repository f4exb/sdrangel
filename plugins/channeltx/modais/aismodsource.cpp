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

#include <QDebug>

#include "dsp/basebandsamplesink.h"
#include "dsp/datafifo.h"
#include "dsp/scopevis.h"
#include "aismodsource.h"
#include "util/crc.h"
#include "util/messagequeue.h"
#include "maincore.h"
#include "channel/channelapi.h"

AISModSource::AISModSource() :
    m_channelSampleRate(AISModSettings::AISMOD_SAMPLE_RATE),
    m_channelFrequencyOffset(0),
    m_fmPhase(0.0),
    m_spectrumSink(nullptr),
    m_scopeSink(nullptr),
    m_magsq(0.0),
    m_levelCalcCount(0),
    m_peakLevel(0.0f),
    m_levelSum(0.0f),
    m_state(idle),
    m_byteIdx(0),
    m_bitIdx(0),
    m_last5Bits(0),
    m_bitCount(0),
    m_scopeSampleBufferIndex(0),
    m_specSampleBufferIndex(0)
 {
    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;
    m_scopeSampleBuffer.resize(m_scopeSampleBufferSize);
    m_specSampleBuffer.resize(m_specSampleBufferSize);

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

AISModSource::~AISModSource()
{
}

void AISModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void AISModSource::pullOne(Sample& sample)
{
    if (m_settings.m_channelMute)
    {
        sample.m_real = 0.0f;
        sample.m_imag = 0.0f;
        return;
    }

	Complex ci;

    if (m_interpolatorDistance > 1.0f)
    {
        modulateSample();

        while (!m_interpolator.decimate(&m_interpolatorDistanceRemain, m_modSample, &ci))
        {
            modulateSample();
        }
    }
    else
    {
        if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, m_modSample, &ci))
        {
            modulateSample();
        }
    }

    m_interpolatorDistanceRemain += m_interpolatorDistance;

    ci *= m_carrierNco.nextIQ(); // shift to carrier frequency

    // Calculate power
    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();

    // Convert from float to fixed point
    sample.m_real = (FixReal) (ci.real() * SDR_TX_SCALEF);
    sample.m_imag = (FixReal) (ci.imag() * SDR_TX_SCALEF);
}

void AISModSource::sampleToSpectrum(Complex sample)
{
    if (m_spectrumSink)
    {
        Real r = std::real(sample) * SDR_TX_SCALEF;
        Real i = std::imag(sample) * SDR_TX_SCALEF;
        m_specSampleBuffer[m_specSampleBufferIndex++] = Sample(r, i);

        if (m_specSampleBufferIndex == m_specSampleBufferSize)
        {
            m_spectrumSink->feed(m_specSampleBuffer.begin(), m_specSampleBuffer.end(), false);
            m_specSampleBufferIndex = 0;
        }
    }
}

void AISModSource::sampleToScope(Complex sample)
{
    if (m_scopeSink)
    {
        Real r = std::real(sample) * SDR_RX_SCALEF;
        Real i = std::imag(sample) * SDR_RX_SCALEF;
        m_scopeSampleBuffer[m_scopeSampleBufferIndex++] = Sample(r, i);

        if (m_scopeSampleBufferIndex == m_scopeSampleBufferSize)
        {
            std::vector<SampleVector::const_iterator> vbegin;
            vbegin.push_back(m_scopeSampleBuffer.begin());
            m_scopeSink->feed(vbegin, m_scopeSampleBufferSize);
            m_scopeSampleBufferIndex = 0;
        }
    }
}

void AISModSource::modulateSample()
{
    Real mod;
    Real linearRampGain;

    if ((m_state == idle) || (m_state == wait))
    {
        m_modSample.real(0.0f);
        m_modSample.imag(0.0f);
        sampleToSpectrum(m_modSample);
        sampleToScope(m_modSample);
        Real s = std::abs(m_modSample);
        calculateLevel(s);
        if (m_state == wait)
        {
            m_waitCounter--;
            if (m_waitCounter == 0) {
                initTX();
            }
        }
    }
    else
    {
        if (m_sampleIdx == 0)
        {
            if (bitsValid())
            {
                // NRZI encoding - encode 0 as change of freq, 1 no change
                if (getBit() == 0) {
                    m_nrziBit = m_nrziBit == 1 ? 0 : 1;
                }
            }
            // Should we start ramping down power?
            if ((m_bitCount < m_settings.m_rampDownBits) || ((m_bitCount == 0) && !m_settings.m_rampDownBits))
            {
                m_state = ramp_down;
                if (m_settings.m_rampDownBits > 0) {
                    m_powRamp = -m_settings.m_rampRange/(m_settings.m_rampDownBits * (Real)m_samplesPerSymbol);
                }
            }
        }
        m_sampleIdx++;
        if (m_sampleIdx >= m_samplesPerSymbol) {
            m_sampleIdx = 0;
        }

        // Apply Gaussian pulse shaping filter
        mod = m_pulseShape.filter(m_nrziBit ? 1.0f : -1.0f);

        // FM
        m_fmPhase += m_phaseSensitivity * mod;
        // Keep phase in range -pi,pi
        if (m_fmPhase > M_PI) {
            m_fmPhase -= 2.0f * M_PI;
        } else if (m_fmPhase < -M_PI) {
            m_fmPhase += 2.0f * M_PI;
        }

        linearRampGain = powf(10.0f, m_pow/20.0f);

        m_modSample.real(m_linearGain * linearRampGain * cos(m_fmPhase));
        m_modSample.imag(m_linearGain * linearRampGain * sin(m_fmPhase));

        if (m_iqFile.is_open()) {
            m_iqFile << mod << "," << m_modSample.real() << "," << m_modSample.imag() << "\n";
        }

        if (m_settings.m_rfNoise)
        {
            // Noise to test filter frequency response
            m_modSample.real(m_linearGain * ((Real)rand()/((Real)RAND_MAX)-0.5f));
            m_modSample.imag(m_linearGain * ((Real)rand()/((Real)RAND_MAX)-0.5f));
        }

        // Display baseband in spectrum analyser and scope
        sampleToSpectrum(m_modSample);
        sampleToScope(m_modSample);

        // Ramp up/down power at start/end of packet
        if ((m_state == ramp_up) || (m_state == ramp_down))
        {
            m_pow += m_powRamp;
            if ((m_state == ramp_up) && (m_pow >= 0.0f))
            {
                // Finished ramp up, transmit at full gain
                m_state = tx;
                m_pow = 0.0f;
            }
            else if ((m_state == ramp_down) && (   (m_settings.m_rampRange == 0)
                                                || (m_settings.m_rampDownBits == 0)
                                                || (m_pow <= -(Real)m_settings.m_rampRange)
                                               ))
            {
                m_state = idle;
                // Do we need to retransmit the packet?
                if (m_settings.m_repeat)
                {
                    if (m_packetRepeatCount > 0)
                        m_packetRepeatCount--;
                    if ((m_packetRepeatCount == AISModSettings::infinitePackets) || (m_packetRepeatCount > 0))
                    {
                        if (m_settings.m_repeatDelay > 0.0f)
                        {
                            // Wait before retransmitting
                            m_state = wait;
                            m_waitCounter = m_settings.m_repeatDelay * AISModSettings::AISMOD_SAMPLE_RATE;
                        }
                        else
                        {
                            // Retransmit immediately
                            initTX();
                        }
                    }
                }
            }
        }

        Real s = std::abs(m_modSample);
        calculateLevel(s);
    }

    // Send Gaussian filter output to mod analyzer
    m_demodBuffer[m_demodBufferFill] = std::real(mod) * std::numeric_limits<int16_t>::max();
    ++m_demodBufferFill;

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

void AISModSource::calculateLevel(Real& sample)
{
    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), sample);
        m_levelSum += sample * sample;
        m_levelCalcCount++;
    }
    else
    {
        m_rmsLevel = sqrt(m_levelSum / m_levelNbSamples);
        m_peakLevelOut = m_peakLevel;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void AISModSource::applySettings(const AISModSettings& settings, bool force)
{
    if ((settings.m_bt != m_settings.m_bt)
     || (settings.m_symbolSpan != m_settings.m_symbolSpan)
     || (settings.m_baud != m_settings.m_baud) || force)
    {
        qDebug() << "AISModSource::applySettings: Recreating pulse shaping filter: "
                << " SampleRate:" << AISModSettings::AISMOD_SAMPLE_RATE
                << " bt: " << settings.m_bt
                << " symbolSpan: " << settings.m_symbolSpan
                << " baud:" << settings.m_baud
                << " data:" << settings.m_data;
        m_pulseShape.create(settings.m_bt, settings.m_symbolSpan, AISModSettings::AISMOD_SAMPLE_RATE/settings.m_baud);
    }

    if ((settings.m_data != m_settings.m_data) || force)
    {
        qDebug() << "AISModSource::applySettings: new data: " << settings.m_data;
        addTXPacket(settings.m_data);
    }

    m_settings = settings;

    // Precalculate FM sensensity and linear gain to save doing it in the loop
    m_samplesPerSymbol = AISModSettings::AISMOD_SAMPLE_RATE / m_settings.m_baud;
    Real modIndex = m_settings.m_fmDeviation / (Real)m_settings.m_baud;
    m_phaseSensitivity = 2.0f * M_PI * modIndex / (Real)m_samplesPerSymbol;
    m_linearGain = powf(10.0f,  m_settings.m_gain/20.0f);
}

void AISModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "AISModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset
            << " rfBandwidth: " << m_settings.m_rfBandwidth;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
     || (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) AISModSettings::AISMOD_SAMPLE_RATE / (Real) channelSampleRate;
        m_interpolator.create(48, AISModSettings::AISMOD_SAMPLE_RATE, m_settings.m_rfBandwidth / 2.2, 3.0);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue* messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(m_channel, m_channelSampleRate);
            messageQueue->push(msg);
        }
    }
}

bool AISModSource::bitsValid()
{
    return m_bitCount > 0;
}

int AISModSource::getBit()
{
    int bit;

    if (m_bitCount > 0)
    {
        bit = (m_bits[m_byteIdx] >> m_bitIdx) & 1;
        m_bitIdx++;
        m_bitCount--;
        if (m_bitIdx == 8)
        {
            m_byteIdx++;
            m_bitIdx = 0;
        }
    }
    else
        bit = 0;
    return bit;
}

void AISModSource::addBit(int bit)
{
    // Transmit LSB first
    m_bits[m_byteIdx] |= bit << m_bitIdx;
    m_bitIdx++;
    m_bitCount++;
    m_bitCountTotal++;
    if (m_bitIdx == 8)
    {
        m_byteIdx++;
        m_bits[m_byteIdx] = 0;
        m_bitIdx = 0;
    }
    m_last5Bits = ((m_last5Bits << 1) | bit) & 0x1f;
}

void AISModSource::initTX()
{
    m_byteIdx = 0;
    m_bitIdx = 0;
    m_bitCount = m_bitCountTotal; // Reset to allow retransmission
    m_nrziBit = 1;

    if (m_settings.m_rampUpBits == 0)
    {
        m_state = tx;
        m_pow = 0.0f;
    }
    else
    {
        m_state = ramp_up;
        m_pow = -(Real)m_settings.m_rampRange;
        m_powRamp = m_settings.m_rampRange/(m_settings.m_rampUpBits * (Real)m_samplesPerSymbol);
    }
}

void AISModSource::addTXPacket(const QString& data)
{
    QByteArray ba = QByteArray::fromHex(data.toUtf8());
    addTXPacket(ba);
}

void AISModSource::addTXPacket(QByteArray data)
{
    uint8_t packet[AIS_MAX_BYTES];
    uint8_t *crc_start;
    uint8_t *packet_end;
    uint8_t *p;
    crc16x25 crc;
    uint16_t crcValue;
    int packet_length;

    // Create AIS message
    p = packet;
    // Training
    *p++ = AIS_TRAIN;
    *p++ = AIS_TRAIN;
    *p++ = AIS_TRAIN;
    // Flag
    *p++ = AIS_FLAG;
    crc_start = p;

    // Copy packet payload
    for (int i = 0; i < data.size(); i++) {
        *p++ = data[i];
    }

    // CRC (do not include flags)
    crc.calculate(crc_start, p-crc_start);
    crcValue = crc.get();
    *p++ = crcValue & 0xff;
    *p++ = (crcValue >> 8);
    packet_end = p;
    // Flag
    *p++ = AIS_FLAG;
    // Buffer
    *p++ = 0;

    packet_length = p-&packet[0];

    encodePacket(packet, packet_length, crc_start, packet_end);
}

void AISModSource::encodePacket(uint8_t *packet, int packet_length, uint8_t *crc_start, uint8_t *packet_end)
{
    // HDLC bit stuffing
    m_byteIdx = 0;
    m_bitIdx = 0;
    m_last5Bits = 0;
    m_bitCount = 0;
    m_bitCountTotal = 0;
    for (int i = 0; i < packet_length; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            int tx_bit = (packet[i] >> j) & 1;
            // Stuff 0 if last 5 bits are 1s, unless transmitting flag
            // Except for special case of when last 5 bits of CRC are 1s
            if (   (   (packet[i] != AIS_FLAG)
                    || (   (&packet[i] >= crc_start)
                        && (   (&packet[i] < packet_end)
                            || ((&packet[i] == packet_end) && (j == 0))
                           )
                       )
                   )
                && (m_last5Bits == 0x1f)
               )
                addBit(0);
            addBit(tx_bit);
        }
    }
    //m_samplesPerSymbol = AISMOD_SAMPLE_RATE / m_settings.m_baud;
    m_packetRepeatCount = m_settings.m_repeatCount;
}

void AISModSource::transmit()
{
    initTX();
    // Only reset phases at start of new packet TX, not in initTX(), so that
    // there isn't a discontinuity in phase when repeatedly transmitting a
    // single tone
    m_sampleIdx = 0;
    m_fmPhase = 0.0;

    if (m_settings.m_writeToFile)
        m_iqFile.open("aismod.csv", std::ofstream::out);
    else if (m_iqFile.is_open())
        m_iqFile.close();
}
