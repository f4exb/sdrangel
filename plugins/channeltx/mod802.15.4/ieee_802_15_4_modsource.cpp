///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2022 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2020-2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QVariant>

#include "dsp/basebandsamplesink.h"
#include "dsp/scopevis.h"
#include "ieee_802_15_4_modsource.h"
#include "util/crc.h"
#include "util/ieee_802_15_4.h"

MESSAGE_CLASS_DEFINITION(IEEE_802_15_4_ModSource::MsgCloseUDP, Message)
MESSAGE_CLASS_DEFINITION(IEEE_802_15_4_ModSource::MsgOpenUDP, Message)

IEEE_802_15_4_ModSource::IEEE_802_15_4_ModSource() :
    m_channelSampleRate(3000000),
    m_channelFrequencyOffset(0),
    m_spectrumRate(0),
    m_sinLUT(nullptr),
    m_scrambler(0x108, 0x1fe, 1),
    m_spectrumSink(nullptr),
    m_scopeSink(nullptr),
    m_specSampleBufferIndex(0),
    m_scopeSampleBufferIndex(0),
    m_magsq(0.0),
    m_levelCalcCount(0),
    m_peakLevel(0.0f),
    m_levelSum(0.0f),
    m_sampleIdx(0),
    m_samplesPerChip(10),
    m_modulationSampleRate(3000000),
    m_resample(false),
    m_resamplerDistance(1.0f),
    m_resamplerDistanceRemain(0.0f),
    m_chipsPerSymbol(15),
    m_bitsPerSymbol(1),
    m_chipRate(300000),
    m_state(idle),
    m_lpfFlushCount(0),
    m_byteIdx(0),
    m_bitIdx(0),
    m_bitCount(0),
    m_udpSocket(nullptr)
{
    m_specSampleBuffer.resize(m_specSampleBufferSize);
    m_scopeSampleBuffer.resize(m_scopeSampleBufferSize);
    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

IEEE_802_15_4_ModSource::~IEEE_802_15_4_ModSource()
{
    closeUDP();
    delete[] m_sinLUT;
}

void IEEE_802_15_4_ModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void IEEE_802_15_4_ModSource::pullOne(Sample& sample)
{
    if (m_settings.m_channelMute)
    {
        sample.m_real = 0.0f;
        sample.m_imag = 0.0f;
        return;
    }

    Complex ci;

    // The waveform is generated at m_modulationSampleRate, which is an integer multiple
    // of the chip rate, then resampled to the channel sample rate if that isn't
    if (!m_resample)
    {
        modulateSample();
        ci = m_modSample;
    }
    else if (m_resamplerDistance > 1.0f) // decimate
    {
        modulateSample();

        while (!m_resampler.decimate(&m_resamplerDistanceRemain, m_modSample, &ci)) {
            modulateSample();
        }

        m_resamplerDistanceRemain += m_resamplerDistance;
    }
    else // interpolate
    {
        if (m_resampler.interpolate(&m_resamplerDistanceRemain, m_modSample, &ci)) {
            modulateSample();
        }

        m_resamplerDistanceRemain += m_resamplerDistance;
    }

    // Display transmitted baseband signal in spectrum analyser and scope
    sampleToSpectrum(ci);
    sampleToScope(ci);

    // Shift to carrier frequency
    ci *= m_carrierNco.nextIQ();

    // Calculate power
    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();

    // Convert from float to fixed point
    sample.m_real = (FixReal) (ci.real() * SDR_TX_SCALEF);
    sample.m_imag = (FixReal) (ci.imag() * SDR_TX_SCALEF);
}

void IEEE_802_15_4_ModSource::sampleToSpectrum(Complex sample)
{
    if (m_spectrumSink && (m_settings.m_spectrumRate > 0))
    {
        Complex out;

        // Could use a simpler filter here, as currently m_spectrumRate is
        // always an integer multiple of m_channelSampleRate
        if (m_interpolator.decimate(&m_interpolatorDistanceRemain, sample, &out))
        {
            Real r = std::real(out) * SDR_TX_SCALEF;
            Real i = std::imag(out) * SDR_TX_SCALEF;
            m_specSampleBuffer[m_specSampleBufferIndex++] = Sample(r, i);

            if (m_specSampleBufferIndex == m_specSampleBufferSize)
            {
                m_spectrumSink->feed(m_specSampleBuffer.begin(), m_specSampleBuffer.end(), false);
                m_specSampleBufferIndex = 0;
            }

            m_interpolatorDistanceRemain += m_interpolatorDistance;
        }
    }
}

void IEEE_802_15_4_ModSource::sampleToScope(Complex sample)
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

void IEEE_802_15_4_ModSource::modulateSample()
{
    Real linearRampGain;
    Real i, q;

    if ((m_state == idle) || (m_state == wait))
    {
        Real audioMod = 0.0f;
        m_modSample.real(audioMod);
        m_modSample.imag(0);
        // Flush the RF bandwidth filter at the end of a frame, otherwise the last
        // lpfTaps/2 samples of the frame (which can be more than the padding) are lost
        if (m_lpfFlushCount > 0)
        {
            m_modSample = m_lowpass.filter(m_modSample);
            m_lpfFlushCount--;
        }
        calculateLevel(audioMod);
        if (m_state == wait)
        {
            m_waitCounter--;
            if (m_waitCounter == 0)
                initTX();
        }
    }
    else
    {

        if (m_sampleIdx == 0)
        {
            if (chipsValid())
                m_chips[m_chipOdd] = getChip();
            // Should we start ramping down power?
            if ((m_bitCount < m_settings.m_rampDownBits) || ((m_bitCount == 0) && !m_settings.m_rampDownBits))
            {
                m_state = ramp_down;
                if (m_settings.m_rampDownBits > 0)
                    m_powRamp = -m_settings.m_rampRange/(m_settings.m_rampDownBits * (Real)m_samplesPerChip);
            }
        }

        if (!m_settings.m_bbNoise)
        {
            if (m_settings.m_modulation == IEEE_802_15_4_ModSettings::BPSK)
            {
                // BPSK - Raised cosine pulse shaping
                if ((m_sampleIdx == 1) && (m_state != ramp_down))
                    i = m_pulseShapeI.filter(m_chips[0] ? 1.0f : -1.0f);
                else
                    i = m_pulseShapeI.filter(0.0f);
                q = 0.0f;
            }
            else
            {
                if (m_settings.m_pulseShaping == IEEE_802_15_4_ModSettings::SINE)
                {
                    // O-QPSK - Half-sine pulse shaping over 2 chips. Even chips on I, odd on Q. 1-chip out of phase.
                    i = (m_chips[0] ? 1.0f : -1.0f) * m_sinLUT[m_sampleIdx+(m_chipOdd ? m_samplesPerChip : 0)];
                    q = (m_chips[1] ? 1.0f : -1.0f) * m_sinLUT[m_sampleIdx+(m_chipOdd ? 0 : m_samplesPerChip)];
                }
                else
                {
                    // O-QPSK - Raised cosine pulse shaping. Even chips on I, odd on Q. 1-chip out of phase.
                    if ((m_sampleIdx == 1) && (m_state != ramp_down) && !m_chipOdd)
                        i = m_pulseShapeI.filter(m_chips[0] ? 1.0f : -1.0f);
                    else
                        i = m_pulseShapeI.filter(0.0f);
                    if ((m_sampleIdx == 1) && (m_state != ramp_down) && m_chipOdd)
                        q = m_pulseShapeQ.filter(m_chips[1] ? 1.0f : -1.0f);
                    else
                        q = m_pulseShapeQ.filter(0.0f);
                }
            }
        }
        else
        {
            i = (Real)rand()/((Real)RAND_MAX)-0.5; // Noise to test filter frequency response
            q = (Real)rand()/((Real)RAND_MAX)-0.5;
        }

        if (m_basebandFile.is_open())
            m_basebandFile << m_chips[0] << "," << m_chips[1] << "," << m_chipOdd << "," << i << "," << q << "," << (m_sampleIdx+(m_chipOdd ? m_samplesPerChip : 0)) << "," << (m_sampleIdx+(m_chipOdd ? 0 : m_samplesPerChip)) << "\n";

        m_sampleIdx++;
        if (m_sampleIdx >= m_samplesPerChip)
        {
            m_sampleIdx = 0;
            if (m_settings.m_modulation == IEEE_802_15_4_ModSettings::OQPSK)
                m_chipOdd = !m_chipOdd;
        }


        linearRampGain = powf(10.0f, m_pow/20.0f);

        m_modSample.real(m_linearGain * linearRampGain * i);
        m_modSample.imag(m_linearGain * linearRampGain * q);

        // Apply low pass filter to limit RF BW
        m_modSample = m_lowpass.filter(m_modSample);

        // Ramp up/down power at start/end of frame
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
                m_lpfFlushCount = m_settings.m_lpfTaps;
                // Do we need to retransmit the frame?
                if (m_settings.m_repeat)
                {
                    if (m_frameRepeatCount > 0)
                        m_frameRepeatCount--;
                    if ((m_frameRepeatCount == IEEE_802_15_4_ModSettings::infinitePackets) || (m_frameRepeatCount > 0))
                    {
                        if (m_settings.m_repeatDelay > 0.0f)
                        {
                            // Wait before retransmitting
                            m_state = wait;
                            m_waitCounter = m_settings.m_repeatDelay * m_modulationSampleRate;
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

void IEEE_802_15_4_ModSource::calculateLevel(Real& sample)
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

void IEEE_802_15_4_ModSource::applySettings(const QStringList& settingsKeys, const IEEE_802_15_4_ModSettings& settings, bool force)
{
    bool updateRates = (settingsKeys.contains("lpfTaps") && (settings.m_lpfTaps != m_settings.m_lpfTaps))
        || (settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth))
        || (settingsKeys.contains("pulseShaping") && (settings.m_pulseShaping != m_settings.m_pulseShaping))
        || (settingsKeys.contains("beta") && (settings.m_beta != m_settings.m_beta))
        || (settingsKeys.contains("symbolSpan") && (settings.m_symbolSpan != m_settings.m_symbolSpan))
        || (settingsKeys.contains("bitRate") && (settings.m_bitRate != m_settings.m_bitRate))
        || (settingsKeys.contains("modulation") && (settings.m_modulation != m_settings.m_modulation))
        || (settingsKeys.contains("subGHzBand") && (settings.m_subGHzBand != m_settings.m_subGHzBand))
        || force;
    bool updateSpectrum = (settingsKeys.contains("spectrumRate") && (settings.m_spectrumRate != m_settings.m_spectrumRate)) || force;

    if ((settingsKeys.contains("polynomial") && (settings.m_polynomial != m_settings.m_polynomial)) || force) {
        m_scrambler.setPolynomial(settings.m_polynomial);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (updateRates) {
        updateSampleRates();
    }

    if (updateSpectrum) {
        updateSpectrumInterpolator();
    }

    // Precalculate linear gain to save doing it in the loop
    m_linearGain = powf(10.0f,  m_settings.m_gain/20.0f);
}

void IEEE_802_15_4_ModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "IEEE_802_15_4_ModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset
            << " rfBandwidth: " << m_settings.m_rfBandwidth
            << " spectrumRate: " << m_settings.m_spectrumRate;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
     || (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    bool rateChanged = (m_channelSampleRate != channelSampleRate) || force;
    bool spectrumChanged = rateChanged || (m_spectrumRate != m_settings.m_spectrumRate);

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;

    if (rateChanged) {
        updateSampleRates();
    }

    if (spectrumChanged) {
        updateSpectrumInterpolator();
    }
}

// Choose the sample rate the waveform is generated at, and create the filters that depend on it.
// Pulse shaping requires an integer number of samples per chip. If the channel sample rate
// is a suitable integer multiple of the chip rate, the waveform is generated at that rate.
// Otherwise it is generated at m_resampleSamplesPerChip samples per chip, and resampled to
// the channel sample rate. This also limits CPU usage when the channel sample rate is much
// higher than the chip rate.
void IEEE_802_15_4_ModSource::updateSampleRates()
{
    if (m_settings.m_modulation == IEEE_802_15_4_ModSettings::BPSK)
    {
        m_chipsPerSymbol = 15;
        m_bitsPerSymbol = 1;
    }
    else
    {
        m_bitsPerSymbol = 4;
        m_chipsPerSymbol = m_settings.m_subGHzBand ? 16 : 32;
    }

    m_chipRate = m_settings.m_bitRate * m_chipsPerSymbol / m_bitsPerSymbol;

    if ((m_chipRate <= 0) || (m_channelSampleRate <= 0)) {
        return;
    }

    int ratio = m_channelSampleRate / m_chipRate;

    if (((m_channelSampleRate % m_chipRate) == 0) && (ratio >= m_minSamplesPerChip) && (ratio <= m_maxSamplesPerChip))
    {
        m_samplesPerChip = ratio;
        m_resample = false;
    }
    else
    {
        m_samplesPerChip = m_resampleSamplesPerChip;
        m_resample = true;
    }

    m_modulationSampleRate = m_samplesPerChip * m_chipRate;
    m_sampleIdx = 0;

    // RF bandwidth filter can't be wider than the sample rate
    float rfBandwidth = std::min(m_settings.m_rfBandwidth, 0.9f * m_modulationSampleRate);
    m_lowpass.create(m_settings.m_lpfTaps, m_modulationSampleRate, rfBandwidth / 2.0);

    if (m_settings.m_pulseShaping == IEEE_802_15_4_ModSettings::RC)
    {
        m_pulseShapeI.create(m_settings.m_beta, m_settings.m_symbolSpan, m_samplesPerChip, true);
        m_pulseShapeQ.create(m_settings.m_beta, m_settings.m_symbolSpan, m_samplesPerChip, true);
    }
    else
    {
        createHalfSine(m_samplesPerChip);
    }

    if (m_resample)
    {
        // Filter to the RF bandwidth, which also has to be within the Nyquist bandwidth of the
        // lower of the two sample rates, to avoid aliasing
        double cutoff = std::min((double) rfBandwidth / 2.0, 0.45 * std::min(m_modulationSampleRate, m_channelSampleRate));
        m_resamplerDistance = (Real) m_modulationSampleRate / (Real) m_channelSampleRate;
        m_resamplerDistanceRemain = 0.0f;
        m_resampler.create(48, m_modulationSampleRate, cutoff, m_resamplerTapsPerPhase);
    }

    qDebug() << "IEEE_802_15_4_ModSource::updateSampleRates:"
        << " channelSampleRate: " << m_channelSampleRate
        << " chipRate: " << m_chipRate
        << " samplesPerChip: " << m_samplesPerChip
        << " modulationSampleRate: " << m_modulationSampleRate
        << " resample: " << m_resample;
}

void IEEE_802_15_4_ModSource::updateSpectrumInterpolator()
{
    if (m_settings.m_spectrumRate > 0)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) m_settings.m_spectrumRate;
        m_interpolator.create(48, m_settings.m_spectrumRate, m_settings.m_spectrumRate / 2.2, 3.0);
    }

    m_spectrumRate = m_settings.m_spectrumRate;
}

// Half-sine pulse shaping for O-QPSK
void IEEE_802_15_4_ModSource::createHalfSine(int samplesPerChip)
{
    delete[] m_sinLUT;
    m_sinLUT = new double[2*samplesPerChip];
    for (int i = 0; i < 2*samplesPerChip; i++) {
        m_sinLUT[i] = sin(M_PI*i/(2.0*samplesPerChip));
    }
}

bool IEEE_802_15_4_ModSource::chipsValid()
{
    return (m_bitCount > 0) || (m_chipIdx < m_chipsPerSymbol);
}

// Symbol-to-chip mapping
int IEEE_802_15_4_ModSource::getChip()
{
    int chip = 0;

    if (m_chipIdx == 0)
        m_symbol = getSymbol();

    if (m_settings.m_bitRate <= 40000) {
        chip = IEEE_802_15_4_PHY::m_chipsBPSK[m_symbol][m_chipIdx];
    } else if (m_settings.m_subGHzBand) {
        chip = IEEE_802_15_4_PHY::m_chipsSubGHzOQPSK[m_symbol][m_chipIdx];
    } else {
        chip = IEEE_802_15_4_PHY::m_chips2450OQPSK[m_symbol][m_chipIdx];
    }

    m_chipIdx++;
    if (m_chipIdx >= m_chipsPerSymbol)
        m_chipIdx = 0;

    return chip;
}

int IEEE_802_15_4_ModSource::getSymbol()
{
    int symbol;

    if (m_bitCount > 0)
    {
        int mask = m_bitsPerSymbol == 1 ? 0x1 : 0xf;
        symbol = (m_bits[m_byteIdx] >> m_bitIdx) & mask;
        m_bitIdx += m_bitsPerSymbol;
        m_bitCount -= m_bitsPerSymbol;
        if (m_bitIdx == 8)
        {
            m_byteIdx++;
            m_bitIdx = 0;
        }
        if (m_settings.m_modulation == IEEE_802_15_4_ModSettings::BPSK)
        {
            // Differential encoding
            symbol = symbol ^ m_diffBit;
            m_diffBit = symbol;
        }
    }
    else
        symbol = 0;

    return symbol;
}

void IEEE_802_15_4_ModSource::initTX()
{
    m_sampleIdx = 0;
    m_chipOdd = false;
    m_chips[0] = 0;
    m_chips[1] = 0;
    m_chipIdx = 0;
    m_diffBit = 0;
    m_byteIdx = 0;
    m_bitIdx = 0;
    m_bitCount = m_bitCountTotal; // Reset to allow retransmission
    m_symbol = 0;
    if (m_settings.m_rampUpBits == 0)
    {
        m_state = tx;
        m_pow = 0.0f;
    }
    else
    {
        m_state = ramp_up;
        m_pow = -(Real)m_settings.m_rampRange;
        m_powRamp = m_settings.m_rampRange/(m_settings.m_rampUpBits * (Real)m_samplesPerChip);
    }
    m_scrambler.init();
}

void IEEE_802_15_4_ModSource::convert(const QString dataStr, QByteArray& data)
{
    // Convert string containing space separated list of hex values to binary
    QStringList list = dataStr.split(" ");

    for (int i = 0; i < list.size(); i++) {
        data.append(list[i].toInt(nullptr, 16));
    }
}

void IEEE_802_15_4_ModSource::addTxFrame(const QString& data)
{
    QByteArray ba;
    convert(data.trimmed(), ba);
    addTxFrame(ba);
}

void IEEE_802_15_4_ModSource::addTxFrame(const QByteArray& data)
{
    uint8_t *crcStart;
    uint8_t *p;
    uint8_t *pLength;
    crc16itut crc;
    uint16_t crcValue;

    // Create PHY frame
    p = m_bits;
    // Preamble
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    // SFD - start of frame delimiter
    *p++ = 0xa7;
    // PHR - length
    pLength = p;
    *p++ = 0;
    // PHY payload
    crcStart = p;
    // Data
    std::copy(data.data(), data.data() + data.length(), p);
    p += data.length();
    // MAC FCS
    crc.calculate(crcStart, p-crcStart);
    crcValue = crc.get();
    *p++ = crcValue & 0xff;
    *p++ = (crcValue >> 8);
    // Update length
    *pLength = p - pLength - 1;
    // Extra 0 to account for pulse shaping filter delay.
    // Should probably just be a few chips
    *p++ = 0x00;

    // Dump frame
    QByteArray qb((char *)m_bits, p-m_bits);

    // Save number of bits in frame
    m_bitCount = m_bitCountTotal = (p-&m_bits[0]) * 8;

    m_frameRepeatCount = m_settings.m_repeatCount;
    initTX();

    if (m_settings.m_writeToFile) {
        m_basebandFile.open("IEEE_802_15_4_Mod.csv", std::ofstream::out);
    } else if (m_basebandFile.is_open()) {
        m_basebandFile.close();
    }
}

void IEEE_802_15_4_ModSource::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()))
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }
        delete message;
    }
}

bool IEEE_802_15_4_ModSource::handleMessage(const Message& msg)
{
    if (MsgOpenUDP::match(msg))
    {
        qDebug("IEEE_802_15_4_ModSource::handleMessage: MsgOpenUDP");
        const MsgOpenUDP& cmd = (const MsgOpenUDP&) msg;
        openUDP(cmd.getUDPAddress(), cmd.getUDPPort());
        return true;
    }
    else if (MsgCloseUDP::match(msg))
    {
        qDebug("IEEE_802_15_4_ModSource::handleMessage: MsgCloseUDP");
        closeUDP();
        return true;
    }
    else
    {
        return false;
    }
}

void IEEE_802_15_4_ModSource::closeUDP()
{
    if (m_udpSocket != nullptr)
    {
        disconnect(m_udpSocket, &QUdpSocket::readyRead, this, &IEEE_802_15_4_ModSource::udpRx);
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

void IEEE_802_15_4_ModSource::openUDP(const QString& udpAddress, uint16_t udpPort)
{
    m_udpSocket = new QUdpSocket();

    if (m_udpSocket->bind(QHostAddress(udpAddress), udpPort))
    {
        connect(m_udpSocket, &QUdpSocket::readyRead, this, &IEEE_802_15_4_ModSource::udpRx);
        qDebug() << "IEEE_802_15_4_ModSource::openUDP: Listening for packets on "
            << udpAddress << ":"
            << udpPort;
            m_udpSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, IEEE_802_15_4_ModSettings::m_udpBufferSize);
    }
    else
    {
        qCritical() << "IEEE_802_15_4_Mod::openUDP: Failed to bind to port "
            << udpAddress << ":"
            << udpPort
            << ". Error: " << m_udpSocket->error();
    }
}

void IEEE_802_15_4_ModSource::udpRx()
{
    while (m_udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        QByteArray data = datagram.data();
        qDebug() << "IEEE_802_15_4_ModSource::udpRx: " << data.toHex();

        if (m_settings.m_udpBytesFormat)
        {
            addTxFrame(data);
        }
        else
        {
            QString string = data.toHex(' ');
            addTxFrame(string);
        }
    }
}
