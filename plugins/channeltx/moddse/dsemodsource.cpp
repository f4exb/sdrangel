///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include <cmath>

#include <QDebug>
#include <QtEndian>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"

#include "dsemodprocessor.h"
#include "dsemodprotocol.h"
#include "dsemodsource.h"

MESSAGE_CLASS_DEFINITION(DSEModSource::MsgSendDTMF, Message)
MESSAGE_CLASS_DEFINITION(DSEModSource::MsgReportDTMFDone, Message)
MESSAGE_CLASS_DEFINITION(DSEModSource::MsgSendMessage, Message)
MESSAGE_CLASS_DEFINITION(DSEModSource::MsgLinkState, Message)
MESSAGE_CLASS_DEFINITION(DSEModSource::MsgSendAPRS, Message)
MESSAGE_CLASS_DEFINITION(DSEModSource::MsgSendFile, Message)
MESSAGE_CLASS_DEFINITION(DSEModSource::MsgSendDatagram, Message)

// A sample to int16, clamped: the volume goes to 3, and a tone at 16384 or DTMF at 2 x 8192 times
// that wrapped round (undefined, and harsh distortion where it wraps)
static inline int16_t toInt16(float v)
{
    return (int16_t) std::max(-32768.0f, std::min(32767.0f, v));
}
MESSAGE_CLASS_DEFINITION(DSEModSource::MsgTunnelState, Message)
MESSAGE_CLASS_DEFINITION(DSEModSource::MsgTunnelControl, Message)
MESSAGE_CLASS_DEFINITION(DSEModSource::MsgFileControl, Message)
MESSAGE_CLASS_DEFINITION(DSEModSource::MsgFileNoAnswer, Message)
MESSAGE_CLASS_DEFINITION(DSEModSource::MsgReportFileTransfer, Message)
MESSAGE_CLASS_DEFINITION(DSEModSource::MsgReportTransmissionEnded, Message)

const int DSEModSource::m_levelNbSamples = 800; // 100 ms at 8 kHz

DSEModSource::DSEModSource() :
    m_channelSampleRate(m_basebandSampleRate),
    m_pcmSampleRate(8000),
    m_pcmFrameSize(DSEModVocoder::m_pcmFrameSize),
    m_channelFrequencyOffset(0),
    m_channel(nullptr),
    m_spectrumSink(nullptr),
    m_modPhasor(0.0f),
    m_envelope(0.0f),
    m_modSample(0.0f, 0.0f),
    m_peakDeviation(1200),
    m_interpolatorDistance(1.0f),
    m_interpolatorDistanceRemain(0.0f),
    m_magsq(0.0),
    m_audioSampleRate(48000),
    m_audioReadBufferFill(0),
    m_audioFifo(12000),
    m_feedbackAudioSampleRate(48000),
    m_feedbackInputSampleRate(8000),
    m_feedbackAudioBufferFill(0),
    m_feedbackAudioFifo(48000),
    m_feedbackInterpolatorDistance(1.0f),
    m_feedbackInterpolatorDistanceRemain(0.0f),
    m_audioUnderruns(0),
    m_audioOverruns(0),
    m_audioFile(nullptr),
    m_fileMutex(nullptr),
    m_pcmSampleCount(0),
    m_pcmSequence(0),
    m_dtmfDigitFrame(0),
    m_dtmfOnly(false),
    m_generatePullCount(0),
    m_spectrumAudioActive(false),
    m_levelCalcCount(0),
    m_rmsLevel(0.0),
    m_peakLevelOut(0.0),
    m_peakLevel(0.0f),
    m_levelSum(0.0f)
{
    m_audioFifo.setLabel("DSEModSource.m_audioFifo");
    m_feedbackAudioFifo.setLabel("DSEModSource.m_feedbackAudioFifo");
    m_feedbackAudioBuffer.resize(1 << 14);
    m_audioReadBuffer.resize(48000);
    m_spectrumBuffer.reserve(4800);
    m_cwKeyer.setSampleRate(m_pcmSampleRate);
    m_toneNco.setFreq(1000.0, m_pcmSampleRate);

    m_processor = new DSEModProcessor();
    m_processor->setFeedbackSource(this);
    m_processor->moveToThread(&m_processorThread);
    connect(&m_processorThread, &QThread::finished, m_processor, &QObject::deleteLater);
    m_processorThread.start();

    applyAudioSampleRate(m_audioSampleRate);
    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

DSEModSource::~DSEModSource()
{
    m_processorThread.exit();
    m_processorThread.wait();
}

void DSEModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void DSEModSource::pullOne(Sample& sample)
{
    Complex ci;

    if (m_interpolatorDistance > 1.0f) // decimate
    {
        modulateSample();

        while (!m_interpolator.decimate(&m_interpolatorDistanceRemain, m_modSample, &ci)) {
            modulateSample();
        }
    }
    else
    {
        if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, m_modSample, &ci)) {
            modulateSample();
        }
    }

    m_interpolatorDistanceRemain += m_interpolatorDistance;

    ci *= m_carrierNco.nextIQ(); // shift to carrier frequency

    if (m_settings.m_channelMute) { // the modulator keeps running (FIFO, frame timing), only the output is blanked
        ci = Complex(0.0f, 0.0f);
    }

    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    magsq /= (SDR_TX_SCALED*SDR_TX_SCALED);
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();

    sample.m_real = (FixReal) ci.real();
    sample.m_imag = (FixReal) ci.imag();
}

void DSEModSource::modulateSample()
{
    // The FIFO's format is the processor's protocol's (a linear protocol writes I,Q pairs, an FM
    // one the frequency): read as it says now, a pair in one go, so that a change of protocol
    // while transmitting (the processor empties the FIFO with the change) cannot split a pair
    int channels = m_processor->getFifoChannels();
    int16_t basebandSamples[2];
    bool carrier = m_processor->getBasebandFifo().read(basebandSamples, (uint32_t) channels) != 0;

    if (carrier && (channels == 2))
    {
        // Linear protocols: the FIFO holds the shaped IQ itself, I then Q
        m_modPhasor = 0.0f;
        m_modSample.real(basebandSamples[0] / 32768.0f * SDR_TX_SCALEF);
        m_modSample.imag(basebandSamples[1] / 32768.0f * SDR_TX_SCALEF);
    }
    else if (carrier || (m_envelope > 0.0f))
    {
        // FM path. A carrier off sample (burst protocols: DMR MS between bursts) or an empty FIFO
        // (end of transmission) ramps the amplitude down over m_envelopeSamples and the next
        // carrier sample ramps it up again, so bursts have no hard edges
        bool on = carrier && (basebandSamples[0] != DSEModProcessor::m_fifoOff);
        float x = on ? DSEModShaper::fifoToFm(basebandSamples[0]) : 0.0f;
        m_modPhasor += ((2.0f * M_PI * m_peakDeviation) / (float) m_basebandSampleRate) * x;

        // limit phasor range to ]-pi,pi]
        if (m_modPhasor > M_PI) {
            m_modPhasor -= (2.0f * M_PI);
        } else if (m_modPhasor <= -M_PI) {
            m_modPhasor += (2.0f * M_PI);
        }

        float step = 1.0f / m_envelopeSamples;
        m_envelope = on ? std::min(1.0f, m_envelope + step) : std::max(0.0f, m_envelope - step);
        float amplitude = m_envelope * 0.891235351562f * SDR_TX_SCALEF; // -1 dB
        m_modSample.real(cos(m_modPhasor) * amplitude);
        m_modSample.imag(sin(m_modPhasor) * amplitude);

        if (m_envelope == 0.0f) {
            m_modPhasor = 0.0f;
        }
    }
    else
    {
        m_modPhasor = 0.0f;
        m_modSample.real(0.0f);
        m_modSample.imag(0.0f);
    }

    if (m_spectrumSink && !m_settings.m_spectrumAudio)
    {
        if (m_spectrumAudioActive)
        {
            m_spectrumBuffer.clear(); // switched from audio: drop the audio samples
            m_spectrumAudioActive = false;
        }

        m_spectrumBuffer.push_back(Sample(m_modSample.real(), m_modSample.imag()));

        if (m_spectrumBuffer.size() >= 4800)
        {
            m_spectrumSink->feed(m_spectrumBuffer.begin(), m_spectrumBuffer.end(), false);
            m_spectrumBuffer.clear();
        }
    }

    // Ask the processor to top up the baseband FIFO when it runs low (at most every 10 ms)
    if ((m_processor->getBasebandFifo().getFill() < m_processor->getFifoLowWater()) && (++m_generatePullCount >= 480))
    {
        m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgGenerate::create());
        m_generatePullCount = 0;
    }

    // Feed one 20 ms audio frame per 20 ms of transmitted baseband
    if (m_settings.m_transmit && !m_processor->isAPIActive())
    {
        if (++m_pcmSampleCount >= DSEModProcessor::m_samplesPerAudioFrame)
        {
            m_pcmSampleCount = 0;

            // The audio at the rate of the encoder this transmission opened: Angel61 WB changes
            // codec with its tier (V1 at 24 kHz, Opus at 48), and a V1 tier fed 48 kHz for Opus's
            // sake would have its audio resampled twice. The frames already queued at the old rate
            // are converted by the processor (each says its rate)
            int encoderRate = m_processor->getEncoderSampleRate();

            if ((encoderRate > 0) && (encoderRate != m_pcmSampleRate)) {
                applyPcmSampleRate(encoderRate);
            }

            pushAudioFrame();
        }
    }
}

void DSEModSource::applyFeedbackAudioSampleRate(int sampleRate)
{
    if (sampleRate <= 0)
    {
        qWarning("DSEModSource::applyFeedbackAudioSampleRate: %d", sampleRate);
        return;
    }

    qDebug("DSEModSource::applyFeedbackAudioSampleRate: %d", sampleRate);
    QMutexLocker mlock(&m_mutex);
    m_feedbackAudioSampleRate = sampleRate;
    configureFeedbackInputRate(m_feedbackInputSampleRate);
}

void DSEModSource::configureFeedbackInputRate(int sampleRate)
{
    m_feedbackInterpolator.create(48, m_feedbackAudioSampleRate, m_feedbackAudioSampleRate / 2.2, 3.0);
    m_feedbackInterpolatorDistanceRemain = 0;
    m_feedbackInterpolatorDistance = (Real) sampleRate / (Real) m_feedbackAudioSampleRate;
    m_feedbackInputSampleRate = sampleRate;
}

void DSEModSource::playGatewayFeedback(const QByteArray& pcm, int sampleRate)
{
    if (sampleRate <= 0 || (pcm.size() & 1)) return;
    QMutexLocker mlock(&m_mutex);
    if (!m_settings.m_feedbackAudioEnable) return;
    if (m_feedbackInputSampleRate != sampleRate) {
        flushFeedback();
        configureFeedbackInputRate(sampleRate);
    }
    for (int i = 0; i < pcm.size(); i += 2) {
        const qint16 sample = qFromLittleEndian<qint16>(reinterpret_cast<const uchar*>(pcm.constData() + i));
        pushFeedback(sample * m_settings.m_feedbackVolumeFactor);
    }
    flushFeedback();
}

void DSEModSource::flushFeedback()
{
    if (!m_feedbackAudioBufferFill) return;
    m_feedbackAudioFifo.write(reinterpret_cast<const quint8*>(m_feedbackAudioBuffer.data()), m_feedbackAudioBufferFill);
    m_feedbackAudioBufferFill = 0;
}

void DSEModSource::pushFeedback(Real sample)
{
    Complex c(sample, sample);
    Complex ci;

    if (m_feedbackInterpolatorDistance < 1.0f) // interpolate
    {
        while (!m_feedbackInterpolator.interpolate(&m_feedbackInterpolatorDistanceRemain, c, &ci))
        {
            processOneFeedbackSample(ci);
            m_feedbackInterpolatorDistanceRemain += m_feedbackInterpolatorDistance;
        }
    }
    else // decimate
    {
        if (m_feedbackInterpolator.decimate(&m_feedbackInterpolatorDistanceRemain, c, &ci))
        {
            processOneFeedbackSample(ci);
            m_feedbackInterpolatorDistanceRemain += m_feedbackInterpolatorDistance;
        }
    }
}

void DSEModSource::processOneFeedbackSample(Complex& ci)
{
    m_feedbackAudioBuffer[m_feedbackAudioBufferFill].l = toInt16(ci.real());
    m_feedbackAudioBuffer[m_feedbackAudioBufferFill].r = toInt16(ci.imag());
    ++m_feedbackAudioBufferFill;

    if (m_feedbackAudioBufferFill >= m_feedbackAudioBuffer.size())
    {
        uint res = m_feedbackAudioFifo.write((const quint8*) &m_feedbackAudioBuffer[0], m_feedbackAudioBufferFill);

        if (res != m_feedbackAudioBufferFill) {
            qDebug("DSEModSource::processOneFeedbackSample: %u/%u audio samples written",
                res, m_feedbackAudioBufferFill);
        }

        m_feedbackAudioBufferFill = 0;
    }
}

// The audio spectrum: the frame as the encoder gets it, real samples (the display shows the positive half)
void DSEModSource::feedSpectrumAudio(const int16_t *pcm, int n)
{
    if (!m_spectrumSink) {
        return;
    }

    if (!m_spectrumAudioActive)
    {
        m_spectrumBuffer.clear(); // switched from IQ
        m_spectrumAudioActive = true;
    }

    for (int i = 0; i < n; i++) {
        m_spectrumBuffer.push_back(Sample((FixReal) ((pcm[i] / 32768.0f) * SDR_TX_SCALEF), 0));
    }

    // About 20 ms per feed whatever the encoder rate
    if (m_spectrumBuffer.size() >= (size_t) (m_pcmSampleRate / 50))
    {
        m_spectrumSink->feed(m_spectrumBuffer.begin(), m_spectrumBuffer.end(), false);
        m_spectrumBuffer.clear();
    }
}

void DSEModSource::pushAudioFrame()
{
    const int n = m_pcmFrameSize;
    DSEModProcessor::MsgPushAudioFrame *msg = DSEModProcessor::MsgPushAudioFrame::create(m_pcmSequence++);
    msg->setSampleRate(m_pcmSampleRate);
    int16_t *pcm = msg->getPCM().data();
    QMutexLocker mlock(&m_mutex);

    if (m_feedbackInputSampleRate != m_pcmSampleRate) {
        flushFeedback();
        configureFeedbackInputRate(m_pcmSampleRate);
    }

    if (!m_dtmfDigits.empty())
    {
        // A DTMF sequence replaces the input: tone frames for the digits, silence in the gaps
        float low, high;

        if (fillDTMF(pcm, low, high)) {
            msg->setTone(low, high, toneAmplitude(8192.0f * m_settings.m_volumeFactor));
        }

        for (int i = 0; i < n; i++) {
            calculateLevel(pcm[i]);
        }

        if (m_dtmfDigits.empty() && m_channel) {
            m_channel->getInputMessageQueue()->push(MsgReportDTMFDone::create());
        }
    }
    else if (m_dtmfOnly)
    {
        std::fill(pcm, pcm + n, 0); // waiting for the automatic release after the digits
    }
    else
    {
        int toneSamples = fillPCM(pcm);

        if (liveDTMF())
        {
            processLiveDTMF(pcm, n);    // Angel61: keys as data (3.8)
        }
        else if (m_settings.m_toneDetect && (toneSamples >= (3 * n) / 4))
        {
            // A frame filled with the tone goes out as a tone frame when the encoder detects tones anyway
            msg->setTone(m_settings.m_toneFrequency, 0.0f, toneAmplitude(16384.0f * m_settings.m_volumeFactor));
        }
    }

    if (m_settings.m_spectrumAudio) {
        feedSpectrumAudio(pcm, n);
    }

    // Whatever filled the frame - the microphone, a file, a tone, the keyer - is what the operator
    // hears, the speech rather than the codec's idea of it. Before the push: the processor's thread
    // deletes the message once it has it (read after, it was freed memory)
    if (m_settings.m_feedbackAudioEnable)
    {
        for (int i = 0; i < n; i++) {
            pushFeedback(pcm[i] * m_settings.m_feedbackVolumeFactor);
        }
    }

    m_processor->getInputMessageQueue()->push(msg);
}

bool DSEModSource::liveDTMF() const
{
    return m_settings.m_toneDetect && DSEModSettings::isAngel61(m_settings.m_mode);
}

void DSEModSource::processLiveDTMF(int16_t *pcm, int n)
{
    if (m_liveDTMF.getSampleRate() != m_pcmSampleRate) {
        m_liveDTMF.setSampleRate(m_pcmSampleRate);
    }

    std::vector<DSEModLiveDTMF::Key> keys;
    m_liveDTMF.process(pcm, n, keys);

    // Each key once its tone has ended (3.8): its measured length, the silence before it as its gap
    for (const DSEModLiveDTMF::Key& key : keys)
    {
        m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgSendDTMF::create(std::string(1, key.m_key),
            key.m_lengthMs, key.m_gapMs, key.m_newSequence, false, false));
    }
}

void DSEModSource::sendDTMF(const QString& digits, bool autoTransmit)
{
    QMutexLocker mlock(&m_mutex);
    float low, high;

    // Angel61: the keys go as data in the DTMF slot (3.8), 100 ms each with 100 ms gaps, the voice
    // idle while they play; a transmission started for them carries them alone
    if (DSEModSettings::isAngel61(m_settings.m_mode))
    {
        const std::string keys = DVDTMF::validKeys(digits.toStdString());

        if (!keys.empty())
        {
            m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgSendDTMF::create(keys,
                m_dtmfToneFrames * 20, m_dtmfGapFrames * 20, true, true, autoTransmit));
        }

        return;
    }

    if (m_dtmfDigits.empty()) {
        m_dtmfDigitFrame = 0;
    }

    m_dtmfOnly = m_dtmfOnly || autoTransmit;

    for (QChar qc : digits.toUpper())
    {
        char c = qc.toLatin1();

        if (DSEModVocoder::dtmfFrequencies(c, low, high)) {
            m_dtmfDigits.push_back(c);
        }
    }

    qDebug("DSEModSource::sendDTMF: %s queued: %s", qPrintable(digits), m_dtmfDigits.c_str());
}

bool DSEModSource::fillDTMF(int16_t *pcm, float& low, float& high)
{
    const int n = m_pcmFrameSize;
    bool tone = false;

    if (m_dtmfDigitFrame < m_dtmfToneFrames)
    {
        DSEModVocoder::dtmfFrequencies(m_dtmfDigits[0], low, high);
        tone = true;
        // The PCM carries the same dual tone for vocoders without tone frames
        float phaseInc1 = 2.0f * (float) M_PI * low / (float) m_pcmSampleRate;
        float phaseInc2 = 2.0f * (float) M_PI * high / (float) m_pcmSampleRate;
        float phase1 = phaseInc1 * (float) (m_dtmfDigitFrame * n);
        float phase2 = phaseInc2 * (float) (m_dtmfDigitFrame * n);

        for (int i = 0; i < n; i++)
        {
            pcm[i] = toInt16((std::sin(phase1) + std::sin(phase2)) * 8192.0f * m_settings.m_volumeFactor);
            phase1 += phaseInc1;
            phase2 += phaseInc2;
        }
    }
    else
    {
        std::fill(pcm, pcm + n, 0);
    }

    if (++m_dtmfDigitFrame >= m_dtmfToneFrames + m_dtmfGapFrames)
    {
        m_dtmfDigits.erase(0, 1);
        m_dtmfDigitFrame = 0;
    }

    return tone;
}

int DSEModSource::toneAmplitude(float peak)
{
    // The AMBE-3000R codes a component at L dBFS with amplitude 127 - (0.2 - L) / 0.714 (0.71 dB per step, 127 = full scale)
    float levelDb = 20.0f * std::log10(std::max(1.0f, peak) / 32768.0f);
    int amplitude = 127 - (int) std::lround((0.2f - levelDb) / 0.714f);
    return std::max(0, std::min(127, amplitude));
}

int DSEModSource::fillPCM(int16_t *pcm)
{
    const int n = m_pcmFrameSize;
    int toneSamples = 0;

    switch (m_settings.m_modAFInput)
    {
    case DSEModSettings::DSEModInputTone:
        for (int i = 0; i < n; i++) {
            pcm[i] = toInt16(m_toneNco.next() * 16384.0f * m_settings.m_volumeFactor);
        }
        toneSamples = n;
        break;
    case DSEModSettings::DSEModInputFile:
    {
        std::fill(pcm, pcm + n, 0);
        QMutexLocker fileLock(m_fileMutex);

        if (m_audioFile && m_audioFile->isOpen())
        {
            if (m_audioFile->getOutputSampleRate() != m_pcmSampleRate) {
                m_audioFile->setOutputSampleRate(m_pcmSampleRate);
            }

            m_audioFile->read(pcm, n, m_settings.m_playLoop);

            if (m_settings.m_volumeFactor != 1.0f)
            {
                for (int i = 0; i < n; i++) {
                    pcm[i] = (int16_t) std::max(-32768.0f, std::min(32767.0f, pcm[i] * m_settings.m_volumeFactor));
                }
            }
        }
        break;
    }
    case DSEModSettings::DSEModInputAudio:
    {
        QMutexLocker mlock(&m_mutex);
        unsigned int index = 0;
        bool short_ = false;
        int produced = 0;

        // The device's samples through the resampler until the frame is full (a fractional ratio:
        // the whole quotient took 44.1 kHz for 40 or 24 kHz and played it 10% sharp or worse)
        while (produced < n)
        {
            if (index < m_audioReadBufferFill)
            {
                const AudioSample& s = m_audioReadBuffer[index++];
                resampleMic((qint16) std::max(-32768.0f, std::min(32767.0f, (s.l + s.r) * (m_settings.m_volumeFactor / 2.0f))), pcm, produced, n);
            }
            else
            {
                short_ = true; // the microphone is behind: zero fill
                std::fill(pcm + produced, pcm + n, 0);
                produced = n;
            }
        }

        if (index < m_audioReadBufferFill)
        {
            std::copy(&m_audioReadBuffer[index], &m_audioReadBuffer[m_audioReadBufferFill], &m_audioReadBuffer[0]);
            m_audioReadBufferFill -= index;
        }
        else
        {
            m_audioReadBufferFill = 0;
        }

        if (short_) {
            m_audioUnderruns++;
        }

        break;
    }
    case DSEModSettings::DSEModInputCWTone:
        for (int i = 0; i < n; i++)
        {
            Real fadeFactor;

            if (m_cwKeyer.getSample())
            {
                m_cwKeyer.getCWSmoother().getFadeSample(true, fadeFactor);
                pcm[i] = toInt16(m_toneNco.next() * 16384.0f * fadeFactor * m_settings.m_volumeFactor);
                toneSamples++;
            }
            else
            {
                if (m_cwKeyer.getCWSmoother().getFadeSample(false, fadeFactor))
                {
                    pcm[i] = toInt16(m_toneNco.next() * 16384.0f * fadeFactor * m_settings.m_volumeFactor);
                }
                else
                {
                    pcm[i] = 0;
                    m_toneNco.setPhase(0);
                }
            }
        }
        break;
    case DSEModSettings::DSEModInputSilence:
    case DSEModSettings::DSEModInputNone:
    default:
        std::fill(pcm, pcm + n, 0);
        break;
    }

    for (int i = 0; i < n; i++) {
        calculateLevel(pcm[i]);
    }

    return toneSamples;
}

// The channel filter is never narrower than the mode's signal, whatever RF bandwidth is set
float DSEModSource::channelFilterCutoff(const DSEModSettings& settings) const
{
    float cutoff = std::max(settings.m_rfBandwidth, (float) DSEModProtocol::occupiedBandwidth(settings.m_mode)) / 2.0f;

    // A linear signal is shaped already: the interpolator's low pass must not touch its edge
    // (2.88 kHz in NB, 5.76 kHz in WB)
    if (settings.m_mode == DSEModSettings::DSEModModeAngel61) {
        cutoff = std::max(cutoff, 4000.0f);
    } else if (settings.m_mode == DSEModSettings::DSEModModeAngel61WB) {
        cutoff = std::max(cutoff, 7000.0f);
    } else if (settings.m_mode == DSEModSettings::DSEModModeAngel61HFNB) {
        cutoff = std::max(cutoff, 2000.0f);     // the edge carriers at +-1275 Hz and the taper's skirt
    } else if (settings.m_mode == DSEModSettings::DSEModModeAngel61HFWB) {
        cutoff = std::max(cutoff, 3500.0f);     // +-2775 Hz
    }

    return cutoff;
}

void DSEModSource::calculateLevel(int16_t sample)
{
    Real s = std::fabs(sample / 32768.0f);

    if (m_levelCalcCount < (quint32) m_levelNbSamples)
    {
        m_peakLevel = std::max(m_peakLevel, s);
        m_levelSum += s * s;
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

void DSEModSource::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate <= 0)
    {
        qWarning("DSEModSource::applyAudioSampleRate: invalid sample rate %d", sampleRate);
        return;
    }

    qDebug("DSEModSource::applyAudioSampleRate: %d", sampleRate);
    QMutexLocker mlock(&m_mutex);
    m_audioSampleRate = sampleRate;
    configureMicResampler();
    m_audioReadBufferFill = 0;
}

void DSEModSource::configureMicResampler()
{
    // A 48 phase polyphase low-pass at 0.45 of the lower rate, as the file input's: the audio
    // resampler before it filtered only the samples it kept, with a filter designed for the device's
    // rate, so the 8 kHz modes heard the microphone through a 550 Hz low-pass with everything above
    // 4 kHz aliased into it
    const int rate = std::max(1, m_audioSampleRate);
    m_micDistance = (Real) rate / (Real) m_pcmSampleRate;
    m_micDistanceRemain = 0.0f;
    m_micInterpolator.create(48, rate, 0.45 * std::min(rate, m_pcmSampleRate), 3.0);
}

void DSEModSource::resampleMic(qint16 in, int16_t *pcm, int& produced, int n)
{
    const Complex c((Real) in, 0.0f);
    Complex ci;
    auto put = [&](const Complex& v) {
        if (produced < n) {
            pcm[produced++] = (int16_t) std::max(-32768.0f, std::min(32767.0f, v.real()));
        }
    };

    if (m_micDistance < 1.0f) // interpolate
    {
        while (!m_micInterpolator.interpolate(&m_micDistanceRemain, c, &ci))
        {
            put(ci);
            m_micDistanceRemain += m_micDistance;
        }
    }
    else if (m_micInterpolator.decimate(&m_micDistanceRemain, c, &ci))
    {
        put(ci);
        m_micDistanceRemain += m_micDistance;
    }
}

void DSEModSource::applyPcmSampleRate(int sampleRate)
{
    if (sampleRate == m_pcmSampleRate) {
        return;
    }

    qDebug("DSEModSource::applyPcmSampleRate: %d", sampleRate);
    QMutexLocker mlock(&m_mutex);
    m_pcmSampleRate = sampleRate;
    m_pcmFrameSize = sampleRate / 50; // 20 ms frames
    m_pcmSampleCount = 0;
    m_cwKeyer.setSampleRate(m_pcmSampleRate);
    m_toneNco.setFreq(m_settings.m_toneFrequency, m_pcmSampleRate);

    configureMicResampler();
    m_audioReadBufferFill = 0;
}

void DSEModSource::setChannel(ChannelAPI *channel)
{
    m_channel = channel;
    m_processor->setReportQueue(channel ? channel->getInputMessageQueue() : nullptr);
}

void DSEModSource::sendMessage(const QString& text, bool autoTransmit)
{
    m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgSendMessage::create(text, autoTransmit));
}

void DSEModSource::setLinkState(int requestedTier, int sendTier, qint64 timestampMs)
{
    m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgLinkState::create(requestedTier, sendTier, timestampMs));
}

void DSEModSource::sendAPRS(const QString& info)
{
    m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgSendAPRS::create(info));
}

void DSEModSource::sendFile(const QString& name, const QByteArray& data)
{
    m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgSendFile::create(name, data));
}

void DSEModSource::sendDatagram(const QByteArray& datagram, bool autoTransmit)
{
    m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgSendDatagram::create(datagram, autoTransmit));
}

void DSEModSource::tunnelState(bool enabled, const QByteArray& advertise)
{
    m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgTunnelState::create(enabled, advertise));
}

void DSEModSource::tunnelControl(const QByteArray& control, bool send)
{
    m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgTunnelControl::create(control, send));
}

void DSEModSource::fileNoAnswer()
{
    m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgFileNoAnswer::create());
}

void DSEModSource::fileControl(const QByteArray& control, bool send, const QString& destination, bool autoTransmit)
{
    m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgFileControl::create(control, send, destination, autoTransmit));
}

void DSEModSource::setAMBEFeature(Feature *feature, std::shared_ptr<MbeEncoderPort> port)
{
    m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgSetAMBEFeature::create(feature, port));
}

void DSEModSource::applySettings(const QStringList& settingsKeys, const DSEModSettings& settings, bool force)
{
    // (set once the settings are: a mode change's applyPcmSampleRate sets it from them, and the
    // old tone would have stayed with a tone and mode changed together)
    const bool toneChanged = (settingsKeys.contains("toneFrequency") && (settings.m_toneFrequency != m_settings.m_toneFrequency)) || force;

    if ((settingsKeys.contains("modAFInput") && (settings.m_modAFInput != m_settings.m_modAFInput)) || force)
    {
        if (settings.m_modAFInput == DSEModSettings::DSEModInputAudio) {
            connect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        } else {
            disconnect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        }
    }

    bool modeChanged = (settingsKeys.contains("mode") && (settings.m_mode != m_settings.m_mode)) || force;

    if (modeChanged)
    {
        m_peakDeviation = DSEModProtocol::peakDeviation(settings.m_mode);
        applyPcmSampleRate(DSEModProtocol::inputSampleRate(settings.m_mode));
    }

    if ((settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth)) || modeChanged)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) m_basebandSampleRate / (Real) m_channelSampleRate;
        m_interpolator.create(48, m_basebandSampleRate, channelFilterCutoff(settings) , 3.0);
    }

    bool transmitChanged = (settingsKeys.contains("transmit") && (settings.m_transmit != m_settings.m_transmit)) || force;

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (toneChanged || modeChanged) {
        m_toneNco.setFreq(m_settings.m_toneFrequency, m_pcmSampleRate);
    }

    // The processor gets all settings
    m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgConfigure::create(settingsKeys, settings, force));

    if (transmitChanged)
    {
        m_pcmSampleCount = 0;

        if (m_settings.m_transmit)
        {
            m_audioUnderruns = 0;
            m_audioOverruns = 0;
            m_liveDTMF.reset();     // the last transmission's delayed speech and a key it cut off
        }
        m_processor->getInputMessageQueue()->push(DSEModProcessor::MsgTransmit::create(m_settings.m_transmit));

        if (!m_settings.m_transmit)
        {
            QMutexLocker mlock(&m_mutex);
            m_dtmfDigits.clear();
            m_dtmfOnly = false;
        }
    }
}

void DSEModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "DSEModSource::applyChannelSettings:"
        << " channelSampleRate: " << channelSampleRate
        << " channelFrequencyOffset: " << channelFrequencyOffset;

    if (channelSampleRate <= 0) { // channelizer not configured yet
        return;
    }

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
     || (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) m_basebandSampleRate / (Real) channelSampleRate;
        m_interpolator.create(48, m_basebandSampleRate, channelFilterCutoff(m_settings), 3.0);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void DSEModSource::handleAudio()
{
    QMutexLocker mlock(&m_mutex);
    unsigned int nbRead;

    while ((nbRead = m_audioFifo.read(reinterpret_cast<quint8*>(&m_audioReadBuffer[m_audioReadBufferFill]), 4096)) != 0)
    {
        if (!m_settings.m_transmit) { // idle: discard, so that the transmission starts on fresh audio, not a full buffer
            m_audioReadBufferFill = 0;
        } else if (m_audioReadBufferFill + nbRead + 4096 < m_audioReadBuffer.size()) {
            m_audioReadBufferFill += nbRead;
        } else {
            m_audioOverruns++; // the microphone is ahead: block discarded
        }
    }
}
