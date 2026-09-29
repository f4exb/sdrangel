///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                    //
// Some code by AI                                                               //
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

#include <algorithm>
#include <cmath>

#include "dsp/scopevis.h"
#ifndef PSK31_DEMOD_SINK_BENCH
#include "psk31demod.h"
#endif
#include "psk31demodsink.h"

PSK31DemodSink::PSK31DemodSink() :
    m_scopeSink(nullptr),
    m_messageQueueToChannel(nullptr),
    m_channelSampleRate(m_internalSampleRate),
    m_channelFrequencyOffset(0),
    m_interpolatorDistance(1.0f),
    m_interpolatorDistanceRemain(1.0f),
    m_costasLoop(2.0f * M_PI / 300.0f, 2),
    m_magsq(0.0),
    m_magsqSum(0.0),
    m_magsqPeak(0.0),
    m_magsqCount(0),
    m_snrInPhaseSum(0.0),
    m_snrQuadratureSum(0.0),
    m_snrCount(0),
    m_snr(0.0f),
    m_clockCount(m_samplesPerSymbol / 2),
    m_previousPolarity(false),
    m_lockCount(0),
    m_unlockCount(0),
    m_locked(false),
    m_reportSampleCount(0),
    m_scopeBufferIndex(0)
{
    m_scopeBuffer.resize(m_scopeBufferSize);
    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

void PSK31DemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    Complex output;

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex sample(it->real(), it->imag());
        sample *= m_nco.nextIQ();

        if (m_interpolatorDistance < 1.0f)
        {
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, sample, &output))
            {
                processOneSample(output);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else if (m_interpolator.decimate(&m_interpolatorDistanceRemain, sample, &output))
        {
            processOneSample(output);
            m_interpolatorDistanceRemain += m_interpolatorDistance;
        }
    }
}

void PSK31DemodSink::processOneSample(Complex sample)
{
    const double magsqRaw = sample.real() * sample.real() + sample.imag() * sample.imag();
    const Real magsq = magsqRaw / (SDR_RX_SCALED * SDR_RX_SCALED);
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    m_magsqPeak = std::max(m_magsqPeak, static_cast<double>(magsq));
    ++m_magsqCount;

    if (m_magsqCount >= m_internalSampleRate / 10)
    {
        m_magSqLevelStore.m_magsq = m_magsqSum / m_magsqCount;
        m_magSqLevelStore.m_magsqPeak = m_magsqPeak;
        m_magSqLevelStore.m_nbSamples = m_magsqCount;
        m_magsqSum = 0.0;
        m_magsqPeak = 0.0;
        m_magsqCount = 0;
    }

    sample /= SDR_RX_SCALEF;
    sample = m_lowpass.filter(sample);

    // The Costas detector gain is proportional to input power. Normalize from
    // the short-term envelope so acquisition bandwidth does not collapse for
    // signals below full scale, while preserving the shaped zero crossings.
    // The recovered carrier is applied to the original sample below.
    const Real carrierScale = 1.0f / std::max(static_cast<Real>(std::sqrt(m_magsq)), 1.0e-4f);
    m_costasLoop.feed(sample.real() * carrierScale, sample.imag() * carrierScale);
    const Complex carrier = -std::conj(m_costasLoop.getComplex());
    const Complex corrected = sample * carrier;
    const Real correctedMagnitude = std::abs(corrected);

    if (correctedMagnitude > 0.0f) {
        m_lockAverage((std::abs(corrected.real()) - std::abs(corrected.imag())) / correctedMagnitude);
    } else {
        m_lockAverage(0.0f);
    }

    const bool lockDetected = m_lockAverage.instantAverage() > 0.35f;

    // Use a Schmitt decision for transition timing. A pulse-shaped BPSK phase
    // reversal crosses zero, so testing the instantaneous magnitude at the
    // sign change suppresses the very transition needed to recenter the clock.
    bool polarity = m_previousPolarity;
    const Real polarityThreshold = 0.2f * std::sqrt(m_magsq);
    if (corrected.real() > polarityThreshold) {
        polarity = true;
    } else if (corrected.real() < -polarityThreshold) {
        polarity = false;
    }

    if (polarity != m_previousPolarity)
    {
        // A phase reversal should occur half a symbol before the next sample.
        // Nudge the free-running clock toward that point rather than resetting
        // it, so an isolated noise crossing cannot insert or delete a symbol.
        const Real timingError = m_samplesPerSymbol / 2.0f - m_clockCount;
        const Real correction = std::clamp(
            m_timingGain * timingError,
            -m_maxTimingCorrection,
            m_maxTimingCorrection);
        m_clockCount += correction;
    }
    m_previousPolarity = polarity;

    bool symbolSample = false;
    m_clockCount -= 1.0f;

    if (m_clockCount <= 0.0f)
    {
        const bool wasLocked = m_locked;

        // Qualify lock at symbol rate. Noise can briefly cross the instantaneous
        // lock threshold, so require a sustained acquisition before accepting
        // bits and use a short dropout hysteresis once a signal is acquired.
        if (lockDetected)
        {
            m_unlockCount = 0;
            m_lockCount = std::min(m_lockCount + 1, m_lockConfirmSymbols);
            m_locked = m_locked || (m_lockCount >= m_lockConfirmSymbols);
        }
        else
        {
            m_lockCount = 0;
            m_unlockCount = std::min(m_unlockCount + 1, m_unlockConfirmSymbols);
            m_locked = m_locked && (m_unlockCount < m_unlockConfirmSymbols);
        }

        if (m_locked) {
            receiveSymbol(polarity);
        } else {
            resetDecoder();
        }

        if (m_locked && !wasLocked)
        {
            // Start a fresh one-second measurement at acquisition. Otherwise
            // the first locked report includes pre-lock samples and reads low.
            resetSNR();
            m_reportSampleCount = 0;
        }

        m_clockCount += m_samplesPerSymbol;
        symbolSample = true;
    }

    if (m_locked)
    {
        m_snrInPhaseSum += corrected.real() * corrected.real();
        m_snrQuadratureSum += corrected.imag() * corrected.imag();
        ++m_snrCount;
    }

    if (++m_reportSampleCount >= m_internalSampleRate)
    {
        if (m_locked && (m_snrCount > 0))
        {
            const double inPhasePower = m_snrInPhaseSum / m_snrCount;
            const double quadraturePower = m_snrQuadratureSum / m_snrCount;
            const double signalPower = std::max(inPhasePower - quadraturePower, 1.0e-20);
            const double measuredNoisePower = std::max(2.0 * quadraturePower, 1.0e-20);
            const double referenceNoisePower = measuredNoisePower
                * m_snrReferenceBandwidth / m_settings.m_rfBandwidth;
            m_snr = std::clamp(
                static_cast<float>(10.0 * std::log10(signalPower / referenceNoisePower)),
                -99.9f,
                99.9f);
            resetSNR();
        }

#ifndef PSK31_DEMOD_SINK_BENCH
        if (m_messageQueueToChannel) {
            m_messageQueueToChannel->push(
                PSK31Demod::MsgDemodReport::create(getFrequencyOffset(), m_snr, m_locked));
        }
#endif
        m_reportSampleCount = 0;
    }

    sampleToScope(Complex(corrected.real(), symbolSample ? 1.0f : 0.0f));
}

void PSK31DemodSink::receiveSymbol(bool symbol)
{
    QChar character;
    if (m_decoder.decodeSymbol(symbol, character))
    {
#ifndef PSK31_DEMOD_SINK_BENCH
        if (m_messageQueueToChannel)
        {
        PSK31Demod::MsgCharacter *message = PSK31Demod::MsgCharacter::create(QString(character));
        m_messageQueueToChannel->push(message);
        }
#endif
        if (m_characterSink) {
            m_characterSink(character);
        }
    }
}

void PSK31DemodSink::resetDecoder()
{
    m_decoder.reset();
}

void PSK31DemodSink::resetSNR()
{
    m_snrInPhaseSum = 0.0;
    m_snrQuadratureSum = 0.0;
    m_snrCount = 0;
}

void PSK31DemodSink::sampleToScope(const Complex& sample)
{
    if (!m_scopeSink) {
        return;
    }

    m_scopeBuffer[m_scopeBufferIndex++] = Sample(
        sample.real() * SDR_RX_SCALEF,
        sample.imag() * SDR_RX_SCALEF);

    if (m_scopeBufferIndex == m_scopeBufferSize)
    {
        std::vector<SampleVector::const_iterator> begin;
        begin.push_back(m_scopeBuffer.begin());
        m_scopeSink->feed(begin, m_scopeBufferSize);
        m_scopeBufferIndex = 0;
    }
}

void PSK31DemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    if ((channelSampleRate != m_channelSampleRate) ||
        (channelFrequencyOffset != m_channelFrequencyOffset) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
        m_interpolator.create(32, channelSampleRate, m_settings.m_rfBandwidth / 2.0f);
        m_interpolatorDistance = static_cast<Real>(channelSampleRate) / m_internalSampleRate;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
        m_channelSampleRate = channelSampleRate;
        m_channelFrequencyOffset = channelFrequencyOffset;
        m_costasLoop.reset();
        m_lockAverage.reset();
        m_lockCount = 0;
        m_unlockCount = 0;
        m_locked = false;
        resetSNR();
        m_snr = 0.0f;
        m_clockCount = m_samplesPerSymbol / 2;
        m_previousPolarity = false;
        resetDecoder();
    }
}

void PSK31DemodSink::applySettings(const QStringList& settingsKeys, const PSK31DemodSettings& settings, bool force)
{
    PSK31DemodSettings validatedSettings = settings;
    validatedSettings.m_rfBandwidth = PSK31DemodSettings::validateRFBandwidth(settings.m_rfBandwidth);

    if (settingsKeys.contains("rfBandwidth") || force)
    {
        m_interpolator.create(32, m_channelSampleRate, validatedSettings.m_rfBandwidth / 2.0f);
        m_lowpass.create(129, m_internalSampleRate, validatedSettings.m_rfBandwidth / 2.0f);
        const Real maxFrequency = std::max(5.0f, validatedSettings.m_rfBandwidth / 2.0f - 20.0f);
        m_costasLoop.setMaxFreq(maxFrequency * 2.0f * M_PI / m_internalSampleRate);
        m_costasLoop.setMinFreq(-maxFrequency * 2.0f * M_PI / m_internalSampleRate);
        resetSNR();
        m_snr = 0.0f;
        m_reportSampleCount = 0;
    }

    if (force) {
        m_settings = validatedSettings;
    } else {
        m_settings.applySettings(settingsKeys, validatedSettings);
    }
}

float PSK31DemodSink::getFrequencyOffset() const
{
    return m_costasLoop.getFreq() * m_internalSampleRate / (2.0f * M_PI);
}
