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

#include <cctype>
#include <QDebug>

#include "dsp/basebandsamplesink.h"
#include "packetmodsource.h"
#include "util/crc.h"

PacketModSource::PacketModSource() :
    m_channelSampleRate(48000),
    m_preemphasisFilter(48000, FMPREEMPHASIS_TAU_US),
    m_channelFrequencyOffset(0),
    m_magsq(0.0),
    m_audioPhase(0.0f),
    m_fmPhase(0.0f),
    m_levelCalcCount(0),
    m_peakLevel(0.0f),
    m_levelSum(0.0f),
    m_bitCount(0),
    m_byteIdx(0),
    m_bitIdx(0),
    m_last5Bits(0),
    m_state(idle)
{
    m_lowpass.create(301, m_channelSampleRate, 22000.0 / 2.0);
    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

PacketModSource::~PacketModSource()
{
}

void PacketModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void PacketModSource::pullOne(Sample& sample)
{
    if (m_settings.m_channelMute)
    {
        sample.m_real = 0.0f;
        sample.m_imag = 0.0f;
        return;
    }

    // Calculate next sample
    modulateSample();

    // Shift to carrier frequency
    Complex ci = m_modSample;
    ci *= m_carrierNco.nextIQ();

    // Calculate power
    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();

    // Convert from float to fixed point
    sample.m_real = (FixReal) (ci.real() * SDR_TX_SCALEF);
    sample.m_imag = (FixReal) (ci.imag() * SDR_TX_SCALEF);
}

void PacketModSource::prefetch(unsigned int nbSamples)
{
}

void PacketModSource::sampleToSpectrum(Real sample)
{
    if (m_spectrumSink)
    {
        Complex out;
        Complex in;
        in.real(sample);
        in.imag(0.0f);
        if (m_interpolator.decimate(&m_interpolatorDistanceRemain, in, &out))
        {
            sample = std::real(out);
            m_sampleBuffer.push_back(Sample(sample * 0.891235351562f * SDR_TX_SCALEF, 0.0f));
            m_spectrumSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), true);
            m_sampleBuffer.clear();
            m_interpolatorDistanceRemain += m_interpolatorDistance;
        }
    }
}

void PacketModSource::modulateSample()
{
    Real audioMod;
    Real theta;
    Real f_delta;
    Real linearGain;
    Real linearRampGain;
    Real emphasis;

    if ((m_state == idle) || (m_state == wait))
    {
        audioMod = 0.0f;
        m_modSample.real(audioMod);
        m_modSample.imag(0);
        calculateLevel(audioMod);
        sampleToSpectrum(audioMod);
        if (m_state == wait)
        {
            m_waitCounter--;
            if (m_waitCounter == 0)
                initTX();
        }
    }
    else
    {
        // Bell 202 AFSK
        if (m_sampleIdx == 0)
        {
            if (bitsValid())
            {
                // NRZI encoding - encode 0 as change of freq, 1 no change
                if (getBit() == 0)
                    m_f = m_f == m_settings.m_markFrequency ? m_settings.m_spaceFrequency : m_settings.m_markFrequency;
            }
            // Should we start ramping down power?
            if ((m_bitCount < m_settings.m_rampDownBits) || ((m_bitCount == 0) && !m_settings.m_rampDownBits))
            {
                m_state = ramp_down;
                if (m_settings.m_rampDownBits > 0)
                    m_powRamp = -m_settings.m_rampRange/(m_settings.m_rampDownBits * (Real)m_samplesPerSymbol);
            }
        }
        m_sampleIdx++;
        if (m_sampleIdx > m_samplesPerSymbol)
            m_sampleIdx = 0;

        if (!m_settings.m_bbNoise)
            audioMod = sin(m_audioPhase);
        else
            audioMod = (Real)rand()/((Real)RAND_MAX)-0.5; // Noise to test filter frequency response
        if ((m_state == tx) || m_settings.m_modulateWhileRamping)
            m_audioPhase += (M_PI * 2.0f * m_f) / (m_channelSampleRate);
        if (m_audioPhase > M_PI)
            m_audioPhase -= (2.0f * M_PI);

        // Preemphasis filter
        if (m_settings.m_preEmphasis)
            audioMod = m_preemphasisFilter.filter(audioMod);

        if (m_audioFile.is_open())
            m_audioFile << audioMod << "\n";

        // Display baseband audio in spectrum analyser
        sampleToSpectrum(audioMod);

        // FM
        m_fmPhase += audioMod;
        if (m_fmPhase > M_PI)
            m_fmPhase -= (2.0f * M_PI);
        f_delta = m_settings.m_fmDeviation / m_channelSampleRate;
        theta = 2.0f * M_PI * f_delta * m_fmPhase;

        linearRampGain = powf(10.0f, m_pow/20.0f);
        linearGain = powf(10.0f,  m_settings.m_gain/20.0f);

        if (!m_settings.m_rfNoise)
        {
            m_modSample.real(linearGain * linearRampGain * cos(theta));
            m_modSample.imag(linearGain * linearRampGain * sin(theta));
        }
        else
        {
            // Noise to test filter frequency response
            m_modSample.real(linearGain * ((Real)rand()/((Real)RAND_MAX)-0.5f));
            m_modSample.imag(linearGain * ((Real)rand()/((Real)RAND_MAX)-0.5f));
        }

        // Apply low pass filter to limit RF BW
        m_modSample = m_lowpass.filter(m_modSample);

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
                    if ((m_packetRepeatCount == PacketModSettings::infinitePackets) || (m_packetRepeatCount > 0))
                    {
                        if (m_settings.m_repeatDelay > 0.0f)
                        {
                            // Wait before retransmitting
                            m_state = wait;
                            m_waitCounter = m_settings.m_repeatDelay * m_channelSampleRate;
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

        Real s = std::real(m_modSample);
        calculateLevel(s);
    }
}

void PacketModSource::calculateLevel(Real& sample)
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

void PacketModSource::applySettings(const PacketModSettings& settings, bool force)
{
    if ((settings.m_lpfTaps != m_settings.m_lpfTaps) || (settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        qDebug() << "PacketModSource::applySettings: Creating new lpf with taps " << settings.m_lpfTaps << " rfBW " << settings.m_rfBandwidth;
        m_lowpass.create(settings.m_lpfTaps, m_channelSampleRate, settings.m_rfBandwidth / 2.0);
    }
    if ((settings.m_preEmphasisTau != m_settings.m_preEmphasisTau) || (settings.m_preEmphasisHighFreq != m_settings.m_preEmphasisHighFreq) || force)
    {
        qDebug() << "PacketModSource::applySettings: Creating new preemphasis filter with tau " << settings.m_preEmphasisTau << " highFreq " << settings.m_preEmphasisHighFreq  << " sampleRate " << m_channelSampleRate;
        m_preemphasisFilter.configure(m_channelSampleRate, settings.m_preEmphasisTau, settings.m_preEmphasisHighFreq);
    }

    m_settings = settings;
}

void PacketModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "PacketModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset
            << " rfBandwidth: " << m_settings.m_rfBandwidth
            << " spectrumRate: " << m_settings.m_spectrumRate;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
     || (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_preemphasisFilter.configure(channelSampleRate, m_settings.m_preEmphasisTau);
        m_lowpass.create(m_settings.m_lpfTaps, channelSampleRate, m_settings.m_rfBandwidth / 2.0);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) channelSampleRate / (Real) m_settings.m_spectrumRate;
        m_interpolator.create(48, m_settings.m_spectrumRate, m_settings.m_spectrumRate / 2.2, 3.0);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

static uint8_t *ax25_address(uint8_t *p, QString address, uint8_t crrl)
{
    int len;
    int i;
    QByteArray b;
    int ssid;

    len = address.length();
    b = address.toUtf8();
    ssid = 0;
    for (i = 0; i < 6; i++)
    {
        if ((i < len) && (ssid == 0))
        {
            if (b[i] == '-')
            {
                if (len > i + 1)
                {
                    ssid = b[i+1] - '0';
                    if ((len > i + 2) && isdigit(b[i+2])) {
                        ssid = (ssid*10) + (b[i+1] - '0');
                    }
                    if (ssid >= 16)
                        qDebug() << "ax25_address: SSID greater than 15 not supported";
                }
                else
                    qDebug() << "ax25_address: SSID number missing";
                *p++ = ' ' << 1;
            }
            else
            {
                *p++ = b[i] << 1;
            }
        }
        else
        {
            *p++ = ' ' << 1;
        }
    }
    *p++ = crrl | (ssid << 1);

    return p;
}

bool PacketModSource::bitsValid()
{
    return m_bitCount > 0;
}

int PacketModSource::getBit()
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

void PacketModSource::addBit(int bit)
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

void PacketModSource::initTX()
{
    m_byteIdx = 0;
    m_bitIdx = 0;
    m_bitCount = m_bitCountTotal; // Reset to allow retransmission
    m_f = m_settings.m_spaceFrequency;
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

void PacketModSource::addTXPacket(QString callsign, QString to, QString via, QString data)
{
    uint8_t packet[AX25_MAX_BYTES];
    uint8_t *crc_start;
    uint8_t *p;
    crc16x25 crc;
    uint16_t crcValue;
    int len;
    int packet_length;

    // Create AX.25 packet
    p = packet;
    // Flag
    for (int i = 0; i < std::min(m_settings.m_ax25PreFlags, AX25_MAX_FLAGS); i++)
        *p++ = AX25_FLAG;
    crc_start = p;
    // Dest
    p = ax25_address(p, to, 0xe0);
    // From
    p = ax25_address(p, callsign, 0x60);
    // Via
    p = ax25_address(p, via, 0x61);
    // Control
    *p++ = m_settings.m_ax25Control;
    // PID
    *p++ = m_settings.m_ax25PID;
    // Data
    len = data.length();
    memcpy(p, data.toUtf8(), len);
    p += len;
    // CRC (do not include flags)
    crc.calculate(crc_start, p-crc_start);
    crcValue = crc.get();
    *p++ = crcValue & 0xff;
    *p++ = (crcValue >> 8);
    // Flag
    for (int i = 0; i < std::min(m_settings.m_ax25PostFlags, AX25_MAX_FLAGS); i++)
        *p++ = AX25_FLAG;

    packet_length = p-&packet[0];

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
            // Stuff 0 if last 5 bits are 1s
            if ((packet[i] != AX25_FLAG) && (m_last5Bits == 0x1f))
                addBit(0);
            addBit(tx_bit);
        }
    }
    m_samplesPerSymbol = m_channelSampleRate / m_settings.m_baud;
    m_packetRepeatCount = m_settings.m_repeatCount;
    initTX();
    // Only reset phases at start of new packet TX, not in initTX(), so that
    // there isn't a discontinuity in phase when repeatedly transmitting a
    // single tone
    m_sampleIdx = 0;
    m_audioPhase = 0.0f;
    m_fmPhase = 0.0f;

    if (m_settings.m_writeToFile)
        m_audioFile.open("packetmod.csv", std::ofstream::out);
}
