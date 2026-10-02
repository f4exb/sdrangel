///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "util/db.h"
#include "util/popcount.h"
#include "dsp/scopevis.h"
#include "device/deviceapi.h"
#include "channel/channelwebapiutils.h"

#include "dscdemod.h"
#include "dscdemodsink.h"

DSCDemodSink::DSCDemodSink(DSCDemod *dscDemod) :
        m_scopeSink(nullptr),
        m_dscDemod(dscDemod),
        m_channel(nullptr),
        m_channelSampleRate(DSCDemodSettings::DSCDEMOD_MFHF_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_samplesPerBit(DSCDemodSettings::DSCDEMOD_MFHF_CHANNEL_SAMPLE_RATE / DSCDemodSettings::DSCDEMOD_MFHF_BAUD_RATE),
        m_exp(nullptr),
        m_vhfToneIndex(0),
        m_vhfToneCount(0),
        m_vhfDC(0.0f),
        m_data(false),
        m_dataPrev(false),
        m_dscDecoder{},
        m_sampleBufferSize(DSCDemodSettings::DSCDEMOD_MFHF_CHANNEL_SAMPLE_RATE / 20),
        m_sampleBufferIndex(0)
{
    m_magsq = 0.0;

    resizeScopeBuffer();

    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
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

    if (m_settings.m_mode == DSCDemodSettings::ModeVHF) {
        processVHFSample(ci);
    } else {
        processMFHFSample(ci);
    }
}

void DSCDemodSink::processMFHFSample(const Complex& ci)
{

    // Correlate with expected frequencies
    Complex exp = m_exp[m_expIdx];
    m_expIdx = (m_expIdx + 1) % m_expLength;
    Complex corr1 = ci * exp;
    Complex corr2 = ci * std::conj(exp);

    // Integrate each tone over one bit, which is the matched filter for a bit.
    // This rejects more noise than a low pass filter wide enough for the data.
    m_mfhfCorr1[m_mfhfCorrIdx] = corr1;
    m_mfhfCorr2[m_mfhfCorrIdx] = corr2;
    m_mfhfCorrIdx = (m_mfhfCorrIdx + 1) % m_mfhfSamplesPerBit;
    Complex sum1, sum2;
    for (int i = 0; i < m_mfhfSamplesPerBit; i++)
    {
        sum1 += m_mfhfCorr1[i];
        sum2 += m_mfhfCorr2[i];
    }
    Real abs1Filt = std::abs(sum1) / m_mfhfSamplesPerBit;
    Real abs2Filt = std::abs(sum2) / m_mfhfSamplesPerBit;

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

    processData(biasedData > 0, abs1Filt, abs2Filt, unbiasedData, biasedData, ci);
}

void DSCDemodSink::processVHFSample(const Complex& ci)
{
    // The 800 Hz tone spacing is narrow enough that discriminator linearity
    // matters; use the atan2 implementation rather than its wideband approximation.
    Real fmDemod = m_phaseDiscri.phaseDiscriminator(ci);

    // Remove receiver tuning error from the audio before tone correlation.
    m_vhfDC += 0.001f * (fmDemod - m_vhfDC);
    Real audio = fmDemod - m_vhfDC;

    m_vhfToneBuffer[m_vhfToneIndex] = audio;
    if (m_vhfToneCount < m_samplesPerBit) {
        m_vhfToneCount++;
    }

    Real low = 0.0f;
    Real high = 0.0f;

    if (m_vhfToneCount == m_samplesPerBit)
    {
        Complex corrLow(0.0f, 0.0f);
        Complex corrHigh(0.0f, 0.0f);

        for (int i = 0; i < m_samplesPerBit; i++)
        {
            int j = m_vhfToneIndex - i;
            if (j < 0) {
                j += m_samplesPerBit;
            }
            corrLow += m_vhfToneExpLow[i] * m_vhfToneBuffer[j];
            corrHigh += m_vhfToneExpHigh[i] * m_vhfToneBuffer[j];
        }

        // VHF DSC applies 6 dB/octave pre-emphasis, so normalize each
        // correlation by its tone frequency before comparing them.
        low = std::abs(corrLow) / DSCDemodSettings::DSCDEMOD_VHF_LOW_TONE;
        high = std::abs(corrHigh) / DSCDemodSettings::DSCDEMOD_VHF_HIGH_TONE;
    }

    m_vhfToneIndex = (m_vhfToneIndex + 1) % m_samplesPerBit;
    Real data = low - high;
    processData(data > 0, low, high, data, data, Complex(audio, 0.0f));
}

void DSCDemodSink::processData(
    bool data,
    Real level1,
    Real level2,
    Real unbiasedData,
    Real biasedData,
    const Complex& scopeSample)
{
    // Save current data for edge detection.
    m_dataPrev = m_data;
    m_data = data;

    // Calculate timing error (we expect clockCount to be 0 when data changes), and add a proportion of it.
    // Use both transitions and a high gain to acquire the clock from the dot pattern, which is
    // only 20 bits on VHF. Keep tracking at a lower gain during the message, which is long
    // enough for transmitter and receiver clock errors to shift the bit timing, but where
    // noise on individual transitions should not.
    if (m_data != m_dataPrev) {
        m_clockCount -= m_clockCount * (m_gotSOP ? 0.05 : 0.2);
    }

    m_clockCount += 1.0;
    if (m_clockCount >= m_samplesPerBit/2.0-1.0)
    {
        // Sample in middle of symbol
        receiveBit(biasedData);
        m_clock = 1;
        // Wrap clock counter
        m_clockCount -= m_samplesPerBit;
    }
    else
    {
        m_clock = 0;
    }

    sampleToScope(scopeSample, level1, level2, unbiasedData, biasedData);
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

void DSCDemodSink::receiveBit(Real soft)
{
    m_bit = soft > 0.0f;

    // Store in shift reg
    m_bits = (m_bits << 1) | m_bit;
    if (m_gotSOP && (m_bitCount < 10)) {
        m_softBits[m_bitCount] = soft;
    }
    m_bitCount++;

    // A phasing sequence while decoding a message means the message was not a real call
    // (phasing symbols cannot occur in a message), so restart on the new call. Also
    // restart if acquisition allowed bit errors, as it may have been misaligned.
    if (m_gotSOP && (!m_dscDecoder.isPhasing() || (m_phasingErrors > 0)))
    {
        unsigned int pat = m_bits & 0x3fffffff;
        for (int i = 0; i < m_phasingPatterns.size(); i++)
        {
            if (pat == m_phasingPatterns[i].m_pattern)
            {
                m_dscDecoder.init(m_phasingPatterns[i].m_offset);
                m_phasingErrors = 0;
                m_bitCount = 0;
                m_rssiMagSqSum = 0.0;
                m_rssiMagSqCount = 0;
                return;
            }
        }
    }

    if (!m_gotSOP)
    {
        // Dot pattern - 200 1/0s or 20 1/0s
        // Phasing pattern - 6 DX=125 RX=111 110 109 108 107 106 105 104
        // Phasing is considered to be achieved when two DXs and one RX, or two RXs and one DX, or three RXs in the appropriate DX or RX positions, respectively, are successfully received.
        if (m_bitCount == 10*3)
        {
            m_bitCount--;

            // Allow a few bit errors, choosing the closest pattern. A false match only
            // produces an invalid message, whereas a missed match loses a call.
            unsigned int pat = m_bits & 0x3fffffff;
            int bestIdx = -1;
            int bestErrors = m_maxPhasingErrors + 1;
            for (int i = 0; i < m_phasingPatterns.size(); i++)
            {
                int errors = popcount(pat ^ m_phasingPatterns[i].m_pattern);
                if (errors < bestErrors)
                {
                    bestErrors = errors;
                    bestIdx = i;
                }
            }
            if (bestIdx >= 0)
            {
                m_dscDecoder.init(m_phasingPatterns[bestIdx].m_offset);
                m_phasingErrors = bestErrors;
                m_gotSOP = true;
                m_bitCount = 0;
                m_rssiMagSqSum = 0.0;
                m_rssiMagSqCount = 0;
            }
        }
    }
    else
    {
        if (m_bitCount == 10)
        {
            if (m_dscDecoder.decodeSoftBits(m_softBits))
            {
                QDateTime dateTime = QDateTime::currentDateTime();

                if (m_settings.m_useFileTime)
                {
                    QString hardwareId = m_dscDemod->getDeviceAPI()->getHardwareId();

                    if ((hardwareId == "FileInput") || (hardwareId == "SigMFFileInput"))
                    {
                        QString dateTimeStr;
                        int deviceIdx = m_dscDemod->getDeviceSetIndex();

                        if (ChannelWebAPIUtils::getDeviceReportValue(deviceIdx, "absoluteTime", dateTimeStr)) {
                            dateTime = QDateTime::fromString(dateTimeStr, Qt::ISODateWithMs);
                        }
                    }
                }

                QByteArray bytes = m_dscDecoder.getMessage();
                DSCMessage message(bytes, dateTime);
                //qDebug() << "RX Bytes: " << bytes.toHex();
                //qDebug() << "DSC Message: " << message.toString();

                float rssi = m_rssiMagSqCount > 0
                    ? CalcDb::dbPower(m_rssiMagSqSum / m_rssiMagSqCount) : -200.0f;

                if (getMessageQueueToChannel())
                {
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) m_settings.getChannelSampleRate();
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void DSCDemodSink::init()
{
    m_expIdx = 0;
    m_mfhfCorrIdx = 0;
    m_phasingErrors = 0;
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
    m_phaseDiscri.reset();
    m_vhfToneIndex = 0;
    m_vhfToneCount = 0;
    m_vhfDC = 0.0f;
    std::fill(m_vhfToneBuffer.begin(), m_vhfToneBuffer.end(), 0.0f);
}

void DSCDemodSink::applySettings(const QStringList& settingsKeys, const DSCDemodSettings& settings, bool force)
{
    qDebug() << "DSCDemodSink::applySettings:" << settings.getDebugString(settingsKeys, force);

    bool modeChanged = settingsKeys.contains("mode") && (settings.m_mode != m_settings.m_mode);
    bool bandwidthChanged = settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth);

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (bandwidthChanged || modeChanged || force)
    {
        m_interpolator.create(16, m_channelSampleRate, m_settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) m_settings.getChannelSampleRate();
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    if (modeChanged || force)
    {
        configureDemod();
    }
}

void DSCDemodSink::configureDemod()
{
    m_samplesPerBit = m_settings.getChannelSampleRate() / m_settings.getBaudRate();

    delete[] m_exp;
    m_exp = new Complex[m_expLength];
    Real phase = 0.0f;
    for (int i = 0; i < m_expLength; i++)
    {
        m_exp[i] = Complex(cos(phase), sin(phase));
        phase += 2.0f * (Real) M_PI * (DSCDemodSettings::DSCDEMOD_MFHF_FREQUENCY_SHIFT / 2.0f)
            / DSCDemodSettings::DSCDEMOD_MFHF_CHANNEL_SAMPLE_RATE;
    }

    m_movMax1.setSize(m_samplesPerBit * 8);
    m_movMax2.setSize(m_samplesPerBit * 8);

    m_vhfToneBuffer.assign(m_samplesPerBit, 0.0f);
    m_vhfToneExpLow.resize(m_samplesPerBit);
    m_vhfToneExpHigh.resize(m_samplesPerBit);
    for (int i = 0; i < m_samplesPerBit; i++)
    {
        Real lowPhase = 2.0f * (Real) M_PI * DSCDemodSettings::DSCDEMOD_VHF_LOW_TONE * i
            / m_settings.getChannelSampleRate();
        Real highPhase = 2.0f * (Real) M_PI * DSCDemodSettings::DSCDEMOD_VHF_HIGH_TONE * i
            / m_settings.getChannelSampleRate();
        m_vhfToneExpLow[i] = Complex(cos(lowPhase), sin(lowPhase));
        m_vhfToneExpHigh[i] = Complex(cos(highPhase), sin(highPhase));
    }

    resizeScopeBuffer();
    init();
}

void DSCDemodSink::resizeScopeBuffer()
{
    m_sampleBufferSize = std::max(1, m_settings.getChannelSampleRate() / 20);
    for (int i = 0; i < DSCDemodSettings::m_scopeStreams; i++) {
        m_sampleBuffer[i].resize(m_sampleBufferSize);
    }
    m_sampleBufferIndex = 0;
}
