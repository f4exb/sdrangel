///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
#include <limits>

#include "selcaldemodsink.h"
#include "selcalmessages.h"

#include "dsp/fftfilt.h"
#include "dsp/spectrumvis.h"
#include "util/db.h"
#include "maincore.h"
#include "dsp/datafifo.h"

SELCALDemodSink::SELCALDemodSink(SELCALDemod *) :
    m_audioFifo(48000)
{
    m_demodBuffer.resize(4096);
    m_spectrumBuffer.reserve(256);
    m_decoder.setSampleHandler([this](float sample) {
        if (!m_spectrumSink) {
            return;
        }
        m_spectrumBuffer.emplace_back(static_cast<FixReal>(std::clamp(sample, -1.0f, 1.0f) * SDR_RX_SCALEF), 0);
        if (m_spectrumBuffer.size() >= 256)
        {
            // Real audio, displayed from 0 Hz to half the sample rate (as in FT8DemodSink)
            m_spectrumSink->feed(m_spectrumBuffer.begin(), m_spectrumBuffer.end(), true);
            m_spectrumBuffer.clear();
        }
    });
    m_decoder.setCallHandler([this](const SELCALDecoder::Call& result) {
        if (!m_messageQueueToChannel) {
            return;
        }
        SELCALMessages::Call call;
        call.m_code = QString::fromStdString(result.m_code);
        // Capture frequency at decode time, before a later retune can be applied.
        call.m_frequency = m_centerFrequency + m_settings.m_inputFrequencyOffset;
        call.m_dateTime = QDateTime::currentDateTimeUtc().addMSecs(
            -static_cast<qint64>((m_audioSampleCount - result.m_startSample) * 1000 / SELCALDecoder::m_sampleRate));
        if (trackingCarrier()) {
            call.m_dateTime = call.m_dateTime.addMSecs(-static_cast<qint64>(m_carrierDelay.size() * 1000 / SELCALDecoder::m_sampleRate));
        }
        // Measured over the bursts, excluding the gap.
        double first = burstPower(result.m_startSample, result.m_firstEndSample);
        double second = burstPower(result.m_secondStartSample, result.m_endSample);
        call.m_rssi = CalcDb::dbPower(first > 0 && second > 0 ? (first + second) / 2 : m_lastMagSq);
        call.m_snr = result.m_snr;
        call.m_toneFrequencyOffset = result.m_frequencyOffset;
        m_messageQueueToChannel->push(SELCALMessages::MsgCall::create(call));
    });
    applyChannelSettings(48000, 0, true);
    applyAudioSampleRate(48000);
}

SELCALDemodSink::~SELCALDemodSink() = default;

void SELCALDemodSink::configureFilter(bool recreate)
{
    // Same fftfilt sideband extraction used by SSBDemodSink. The audio band
    // includes the lowest legacy tone and the highest SELCAL-32 tone. Updating
    // an existing filter keeps its buffered audio.
    if (recreate || !m_filter) {
        m_filter = std::make_unique<fftfilt>(m_filterLength);
    }
    if (m_settings.m_modulation == SELCALDemodSettings::USB) {
        m_filter->create_filter(100.0f / 8000, m_settings.m_rfBandwidth / 8000);
    }
    else
    {
        m_filter->create_dsb_filter(m_settings.m_rfBandwidth / (2 * 8000));
    }
}

bool SELCALDemodSink::trackingCarrier() const
{
    // AM always has a carrier, used for coherent detection.
    return m_settings.m_modulation == SELCALDemodSettings::AM || m_settings.m_autoToneFrequencyOffset;
}

void SELCALDemodSink::updateDecoderOffset()
{
    double offset = m_settings.m_toneFrequencyOffset;
    double uncertainty = 2.0;
    if (m_settings.m_modulation == SELCALDemodSettings::USB)
    {
        // A measured carrier gives the tuning error precisely. Otherwise,
        // search for the tones around the manual offset, or around the carrier
        // offset for a while after a fade or the end of a transmission.
        if (m_settings.m_autoToneFrequencyOffset && m_carrierTracker.locked())
        {
            offset = m_carrierOffset;
        }
        else
        {
            if (m_settings.m_autoToneFrequencyOffset && m_carrierHoldover > 0) {
                offset = m_carrierOffset;
            }
            uncertainty = SELCALDecoder::m_searchRange;
        }
    }
    if (offset != m_decoderOffset || uncertainty != m_decoderUncertainty)
    {
        m_decoderOffset = offset;
        m_decoderUncertainty = uncertainty;
        m_decoder.setFrequencyOffset(offset, uncertainty);
    }
}

void SELCALDemodSink::resetTuning()
{
    // Retuning invalidates the measured carrier and any call in progress, but
    // monitoring audio continues uninterrupted.
    m_decoderOffset = m_settings.m_toneFrequencyOffset;
    m_decoderUncertainty = SELCALDecoder::m_searchRange;
    m_decoder.setFrequencyOffset(m_decoderOffset, m_decoderUncertainty);
    m_decoder.reset();
    m_carrierTracker.setRange(m_settings.m_modulation == SELCALDemodSettings::AM ? 1000.0 : SELCALCarrierTracker::m_defaultRange);
    m_carrierTracker.reset();
    m_carrierOffset = 0;
    m_carrierHoldover = 0;
    m_carrierTail = 0;
    m_carrierNco.setFreq(0, SELCALDecoder::m_sampleRate);
    m_carrierPhasor = {};
    m_amCarrier = {};
    restartSampleCount();
    if (m_messageQueueToChannel)
    {
        SELCALMessages::Status status;
        status.m_toneFrequencyOffset = m_decoder.frequencyOffset();
        m_messageQueueToChannel->push(SELCALMessages::MsgStatus::create(status));
    }
}

void SELCALDemodSink::restartSampleCount()
{
    // Follows a decoder reset, which restarts the decoder's sample count. Keep
    // the samples buffered in the receive filter between the counts, and
    // discard power history aligned with the old count.
    std::uint64_t buffered = m_filterInputCount - m_audioSampleCount;
    m_audioSampleCount = 0;
    m_filterInputCount = buffered;
    m_powerBlocks.fill(0);
    m_powerBlockSum = 0;
    m_powerBlockSamples = 0;
}

void SELCALDemodSink::reset()
{
    resetTuning();
    m_carrierDelay.fill({});
    m_carrierDelayPosition = 0;
    m_filterInputCount = 0;
    m_statusCounter = 0;
    m_dc = 0;
    m_audioPower = 1e-6;
    m_squelchPower = 1e-12;
    m_squelchCount = 0;
    m_squelchOpen = false;
    m_distanceRemain = 0;
    m_audioDistanceRemain = 0;
    m_demodBufferFill = 0;
    m_spectrumBuffer.clear();
    m_audioBuffer.clear();
    m_audioFifo.clear();
    m_magSqSum = m_magSqPeak = 0;
    m_magSqCount = 0;
    m_lastMagSq = 1e-12;
    configureFilter(true);
}

void SELCALDemodSink::applyChannelSettings(int sampleRate, int frequencyOffset, bool force)
{
    if (sampleRate <= 0) {
        return;
    }
    if (force || sampleRate != m_channelSampleRate)
    {
        m_channelSampleRate = sampleRate;
        m_channelFrequencyOffset = frequencyOffset;
        m_nco.setFreq(-frequencyOffset, sampleRate);
        m_interpolator.create(16, sampleRate, std::min(3600.0f, sampleRate * 0.45f));
        m_distance = static_cast<Real>(sampleRate) / SELCALDecoder::m_sampleRate;
        reset();
    }
    else if (frequencyOffset != m_channelFrequencyOffset)
    {
        // As in SSBDemodSink, retuning only moves the NCO.
        m_channelFrequencyOffset = frequencyOffset;
        m_nco.setFreq(-frequencyOffset, sampleRate);
        resetTuning();
        // Audio from before retuning is still in the carrier delay and receive
        // filter. Don't learn its steady signals as the new channel's background.
        m_decoder.holdLearning((trackingCarrier() ? m_carrierDelay.size() : 0) + 512);
    }
}

void SELCALDemodSink::applySettings(const QStringList& keys, const SELCALDemodSettings& settings, bool force)
{
    SELCALDemodSettings next = m_settings;

    if (force) {
        next = settings;
    }
    else
    {
        next.applySettings(keys, settings);
    }
    next.normalize();

    // Changing modulation or carrier tracking changes the signal path, so restarts it.
    // Retuning is applied by applyChannelSettings.
    bool changed = force || next.m_modulation != m_settings.m_modulation || next.m_autoToneFrequencyOffset != m_settings.m_autoToneFrequencyOffset ||
                   next.m_streamIndex != m_settings.m_streamIndex;
    bool decoderChanged = next.m_toneFrequencyOffset != m_settings.m_toneFrequencyOffset || next.m_threshold != m_settings.m_threshold;
    bool bandwidthChanged = next.m_rfBandwidth != m_settings.m_rfBandwidth;
    m_squelchLevel = CalcDb::powerFromdB(next.m_squelch);

    m_settings = next;
    if (changed)
    {
        m_decoder.configure(next.m_toneFrequencyOffset, next.m_threshold);
        reset();
    }
    else
    {
        if (decoderChanged)
        {
            // Restarts decoding, keeping the carrier measurement.
            m_decoder.configure(next.m_toneFrequencyOffset, next.m_threshold);
            restartSampleCount();
            m_decoderOffset = std::numeric_limits<double>::quiet_NaN();
            updateDecoderOffset();
        }
        if (bandwidthChanged) {
            configureFilter(false);
        }
    }
}

void SELCALDemodSink::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate <= 0) {
        return;
    }

    m_audioSampleRate = sampleRate;
    m_audioInterpolator.create(16, 8000, 3300);
    m_audioDistance = static_cast<Real>(8000) / sampleRate;
    m_audioDistanceRemain = 0;
    m_demodBufferFill = 0;
    m_audioBuffer.clear();
    m_audioFifo.clear();

    // The demod stream is at the audio rate, so tell the Demod Analyzer and
    // Denoiser features of a change, as AMDemodSink does.
    if (m_channel)
    {
        QList<ObjectPipe *> pipes;
        MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);
        for (const auto& pipe : pipes)
        {
            auto *messageQueue = qobject_cast<MessageQueue *>(pipe->m_element);
            if (messageQueue) {
                messageQueue->push(MainCore::MsgChannelDemodReport::create(m_channel, sampleRate));
            }
        }
    }
}

void SELCALDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    for (auto it = begin; it != end; ++it)
    {
        Complex sample(it->real() / static_cast<float>(SDR_RX_SCALEF), it->imag() / static_cast<float>(SDR_RX_SCALEF));
        sample *= m_nco.nextIQ();
        Complex output;
        if (m_distance < 1) {
            while (!m_interpolator.interpolate(&m_distanceRemain, sample, &output))
            {
                processOneSample(output);
                m_distanceRemain += m_distance;
            }
        }
        else if (m_interpolator.decimate(&m_distanceRemain, sample, &output))
        {
            processOneSample(output);
            m_distanceRemain += m_distance;
        }
    }
    if (!m_audioBuffer.empty())
    {
        m_audioFifo.write(reinterpret_cast<const quint8 *>(m_audioBuffer.data()), m_audioBuffer.size());
        m_audioBuffer.clear();
    }
}

void SELCALDemodSink::processOneSample(const Complex& sample)
{
    bool usb = m_settings.m_modulation == SELCALDemodSettings::USB;
    bool tracking = trackingCarrier();
    Complex delayed = sample;
    if (tracking)
    {
        m_carrierTracker.feed(sample);
        if (m_carrierTracker.locked() && std::abs(m_carrierTracker.offset() - m_carrierOffset) > 0.05)
        {
            m_carrierOffset = m_carrierTracker.offset();
            m_carrierNco.setFreq(-m_carrierOffset, SELCALDecoder::m_sampleRate);
        }
        if (m_carrierTracker.locked())
        {
            m_carrierHoldover = m_carrierHoldoverLength;
            m_carrierTail = m_carrierDelay.size();
        }
        else
        {
            m_carrierHoldover = std::max(m_carrierHoldover - 1, 0);
            m_carrierTail = std::max(m_carrierTail - 1, 0);
        }
        // Let carrier acquisition precede the corresponding audio. Otherwise
        // the first pair could be acquired without the carrier's correction.
        delayed = m_carrierDelay[m_carrierDelayPosition];
        m_carrierDelay[m_carrierDelayPosition] = sample;
        m_carrierDelayPosition = (m_carrierDelayPosition + 1) % m_carrierDelay.size();
    }
    updateDecoderOffset();

    double power = std::norm(sample);
    m_magSqSum += power;
    m_magSqPeak = std::max(m_magSqPeak, power);
    ++m_magSqCount;
    m_lastMagSq += (power - m_lastMagSq) * 0.001;
    // Squelch gates the monitoring audio, which is delayed with the carrier.
    m_squelchPower += (std::norm(delayed) - m_squelchPower) * 0.01;

    // Power aligned with the decoder's audio, which is delayed with the carrier.
    m_powerBlockSum += std::norm(delayed);
    ++m_powerBlockSamples;
    if (++m_filterInputCount % m_powerBlockSize == 0)
    {
        m_powerBlocks[(m_filterInputCount / m_powerBlockSize - 1) % m_powerBlocks.size()] = m_powerBlockSum / m_powerBlockSamples;
        m_powerBlockSum = 0;
        m_powerBlockSamples = 0;
    }

    // The carrier is measured ahead of the delayed audio, so the audio still
    // contains the carrier for the delay after it is lost.
    bool locked = tracking && (m_carrierTracker.locked() || m_carrierTail > 0);
    Complex filtered = delayed;
    if (locked)
    {
        Complex rotation = m_carrierNco.nextIQ();
        if (usb)
        {
            // Remove the measured carrier before sideband extraction. Near +100 Hz
            // a strong carrier can otherwise leak through the USB filter's lower
            // transition band and overwhelm the two audio tones.
            m_carrierPhasor += (filtered * rotation - m_carrierPhasor) * 0.01f;
            filtered -= m_carrierPhasor * std::conj(rotation);
        }
        else
        {
            // Move the AM carrier to 0 Hz for coherent detection.
            filtered *= rotation;
        }
    }
    else
    {
        m_carrierPhasor = {};
        m_amCarrier = {};
    }
    fftfilt::cmplx *output = nullptr;

    int count = usb ? m_filter->runSSB(filtered, &output, true, false) : m_filter->runDSB(filtered, &output, true);

    for (int i = 0; i < count; ++i)
    {
        float audio;
        if (usb)
        {
            // Recover the real audio from the analytic sideband.
            audio = output[i].real() + output[i].imag();
        }
        else if (locked)
        {
            // Coherent AM detection: the component in phase with the carrier.
            // Unlike envelope detection, it has no threshold at low SNR and
            // rejects the quadrature noise.
            m_amCarrier += (output[i] - m_amCarrier) * 0.002f;
            float magnitude = std::abs(m_amCarrier);
            audio = magnitude > 0 ? (output[i] * std::conj(m_amCarrier)).real() / magnitude : 0;
        }
        else
        {
            audio = std::abs(output[i]);
        }
        // Carrier/DC removal, as in AMDemodSink.
        m_dc += (audio - m_dc) * 0.001;
        processAudio(audio - m_dc);
    }
}

void SELCALDemodSink::processAudio(float sample)
{
    ++m_audioSampleCount;
    m_decoder.feed(sample);
    monitor(sample);

    if (++m_statusCounter >= 1600)
    {
        m_statusCounter = 0;
        if (m_messageQueueToChannel)
        {
            SELCALMessages::Status status;
            status.m_state = m_decoder.state();
            status.m_carrierLocked = trackingCarrier() && m_carrierTracker.locked();
            status.m_squelchOpen = m_squelchOpen;
            status.m_carrierFrequencyOffset = m_carrierTracker.offset();
            status.m_toneFrequencyOffset = m_decoder.frequencyOffset();
            status.m_pair = m_decoder.pair();
            status.m_levels = m_decoder.levels();
            m_messageQueueToChannel->push(SELCALMessages::MsgStatus::create(status));
        }
    }
}

void SELCALDemodSink::monitor(float sample)
{
    if (m_squelchPower >= m_squelchLevel) {
        m_squelchCount = SELCALDecoder::m_sampleRate / 10;
    }
    else if (m_squelchCount > 0) {
        --m_squelchCount;
    }
    m_squelchOpen = m_squelchCount > 0;
    m_audioPower += (sample * sample - m_audioPower) * 0.0005;
    float gain = std::min(1000.0, 0.15 / std::sqrt(std::max(m_audioPower, 1e-10)));
    Complex input(!m_squelchOpen ? 0 : sample * gain * m_settings.m_volume, 0), output;
    auto append = [this](const Complex& value) {
        qint16 audio = std::clamp(value.real() * 32768.0f, -32768.0f, 32767.0f);
        qint16 heard = m_settings.m_audioMute ? 0 : audio;
        m_audioBuffer.push_back({heard, heard});
        if (m_audioBuffer.size() >= 4096)
        {
            m_audioFifo.write(reinterpret_cast<const quint8 *>(m_audioBuffer.data()), m_audioBuffer.size());
            m_audioBuffer.clear();
        }
        // As in AMDemodSink, the demod stream for the Demod Analyzer and Denoiser
        // features carries the monitoring audio before muting.
        m_demodBuffer[m_demodBufferFill++] = audio;
        if (m_demodBufferFill == m_demodBuffer.size()) {
            writeDemod();
        }
    };
    if (m_audioDistance < 1) {
        while (!m_audioInterpolator.interpolate(&m_audioDistanceRemain, input, &output))
        {
            append(output);
            m_audioDistanceRemain += m_audioDistance;
        }
    }
    else if (m_audioInterpolator.decimate(&m_audioDistanceRemain, input, &output))
    {
        append(output);
        m_audioDistanceRemain += m_audioDistance;
    }
}

double SELCALDemodSink::burstPower(std::uint64_t start, std::uint64_t end) const
{
    // Decoder sample indices count the receive filter's output. Overlap-add
    // filtering outputs each block for the inputs just received, delayed by the
    // linear phase filter's group delay.
    constexpr std::uint64_t groupDelay = m_filterLength / 4;
    start = start > groupDelay ? start - groupDelay : 0;
    end = end > groupDelay ? end - groupDelay : 0;
    std::uint64_t blocks = m_filterInputCount / m_powerBlockSize;
    std::uint64_t first = start / m_powerBlockSize;
    std::uint64_t last = std::min(end / m_powerBlockSize, blocks);
    first = std::max(first, blocks > m_powerBlocks.size() ? blocks - m_powerBlocks.size() : 0);
    if (end <= start || first >= last) {
        return 0;
    }
    double sum = 0;
    for (std::uint64_t block = first; block < last; ++block) {
        sum += m_powerBlocks[block % m_powerBlocks.size()];
    }
    return sum / (last - first);
}

void SELCALDemodSink::writeDemod()
{
    if (m_channel)
    {
        QList<ObjectPipe *> pipes;
        MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", pipes);
        for (auto *pipe : pipes)
        {
            auto *fifo = qobject_cast<DataFifo *>(pipe->m_element);
            if (fifo) {
                fifo->write(reinterpret_cast<const quint8 *>(m_demodBuffer.constData()), m_demodBuffer.size() * sizeof(qint16),
                            DataFifo::DataTypeI16);
            }
        }
    }
    m_demodBufferFill = 0;
}

void SELCALDemodSink::getMagSqLevels(double& average, double& peak, int& count)
{
    count = m_magSqCount;
    average = count ? m_magSqSum / count : m_lastMagSq;
    peak = count ? m_magSqPeak : average;
    m_magSqSum = m_magSqPeak = 0;
    m_magSqCount = 0;
}
