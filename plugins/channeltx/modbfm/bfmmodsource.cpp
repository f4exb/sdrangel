///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include <QDebug>

#include "dsp/datafifo.h"
#include "audio/audiofilereader.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "bfmmodsource.h"

const int BFMModSource::m_rfFilterFFTLength = 1024;
const int BFMModSource::m_levelNbSamples = 480; // every 10ms

BFMModSource::BFMModSource() :
    m_channelSampleRate(384000),
    m_channelFrequencyOffset(0),
    m_channel(nullptr),
    m_modPhasor(0.0f),
    m_mpxPhase(0.0),
    m_preemphasisLeft(48000),
    m_preemphasisRight(48000),
    m_audioSampleRate(48000),
    m_audioFifo(12000),
    m_feedbackAudioSampleRate(48000),
    m_feedbackAudioFifo(48000),
	m_levelCalcCount(0),
	m_rmsLevel(0.0),
	m_peakLevelOut(0.0),
	m_peakLevel(0.0f),
	m_levelSum(0.0f),
    m_audioFileReader(nullptr),
    m_limiterGain(1.0f),
    m_limiterRelease(0.0f)
{
    m_audioFifo.setLabel("BFMModSource.m_audioFifo");
    m_feedbackAudioFifo.setLabel("BFMModSource.m_feedbackAudioFifo");
    m_rfFilter = new fftfilt(-62500.0 / 384000.0, 62500.0 / 384000.0, m_rfFilterFFTLength);
    m_rfFilterBuffer = new Complex[m_rfFilterFFTLength];
    std::fill(m_rfFilterBuffer, m_rfFilterBuffer+m_rfFilterFFTLength, Complex{0,0});
    m_rfFilterBufferIndex = 0;
	m_audioBuffer.resize(24000);
	m_audioBufferFill = 0;
	m_audioReadBuffer.resize(24000);
	m_audioReadBufferFill = 0;
	m_magsq = 0.0;
	m_feedbackAudioBuffer.resize(1<<14);
	m_feedbackAudioBufferFill = 0;
    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;

    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
    applyAudioSampleRate(m_audioSampleRate);
}

BFMModSource::~BFMModSource()
{
    delete m_rfFilter;
    delete[] m_rfFilterBuffer;
}

void BFMModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void BFMModSource::pullOne(Sample& sample)
{
	if (m_settings.m_channelMute)
	{
		sample.m_real = 0.0f;
		sample.m_imag = 0.0f;
		return;
	}

	Complex ci, ri;
    fftfilt::cmplx *rf;
    int rf_out;
	Real t;

	if (m_interpolatorDistance > 1.0f) // decimate
	{
        modulateAudio();

        while (!m_interpolator.decimate(&m_interpolatorDistanceRemain, m_modSample, &ri)) {
            modulateAudio();
        }
    }
    else // interpolate
    {
        if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, m_modSample, &ri)) {
            modulateAudio();
        }
    }

    // The audio was limited before interpolation, but the interpolation filter
    // can overshoot. The program part of the multiplex is a weighted average of
    // left and right, so limiting both to 1 keeps the multiplex within full scale.
    const Complex limited = m_lookaheadLimiter.process(ri);
    const Real left = limited.real();
    const Real right = limited.imag();
    const Real mono = 0.5f * (left + right);
    const bool pilotActive = m_settings.m_audioStereo || m_settings.m_rdsActive;
    const Real pilotLevel = pilotActive ? m_settings.m_pilotLevel : 0.0f;
    const Real rdsLevel = m_settings.m_rdsActive ? m_settings.m_rdsLevel : 0.0f;
    const Real programLevel = std::max(0.0f, 1.0f - pilotLevel - rdsLevel);
    t = programLevel * mono;

    if (pilotActive)
    {
        // The 38 and 57 kHz subcarriers are derived from the pilot's sin/cos
        // with trigonometric identities, so only one sin/cos pair is needed.
        const Real pilotSin = std::sin(m_mpxPhase);
        const Real pilotCos = std::cos(m_mpxPhase);

        if (m_settings.m_audioStereo)
        {
            const Real difference = 0.5f * (left - right);
            // sin(2x): the 38 kHz subcarrier crosses zero positively at every pilot zero.
            t += programLevel * difference * 2.0f * pilotSin * pilotCos;
        }

        t += pilotLevel * pilotSin;

        if (m_settings.m_rdsActive)
        {
            // cos(3x): the 57 kHz subcarrier is in quadrature with the third pilot harmonic.
            const Real cos3 = pilotCos * (4.0f * pilotCos * pilotCos - 3.0f);
            t += rdsLevel * m_rdsEncoder.sample() * cos3;
        }
    }

    m_mpxPhase += (2.0 * M_PI * 19000.0) / m_channelSampleRate;
    if (m_mpxPhase >= 2.0 * M_PI) {
        m_mpxPhase -= 2.0 * M_PI;
    }
    m_interpolatorDistanceRemain += m_interpolatorDistance;

    m_modPhasor += (m_settings.m_fmDeviation / (float) m_channelSampleRate) * t * M_PI * 2.0f;

    // limit phasor range to ]-pi,pi]
    if (m_modPhasor > M_PI) {
        m_modPhasor -= (2.0f * M_PI);
    } else if (m_modPhasor <= -M_PI) {
        m_modPhasor += (2.0f * M_PI);
    }

    ci.real(cos(m_modPhasor) * 0.891235351562f * SDR_TX_SCALEF); // -1 dB
    ci.imag(sin(m_modPhasor) * 0.891235351562f * SDR_TX_SCALEF);

    // RF filtering
    rf_out = m_rfFilter->runFilt(ci, &rf);

    if (rf_out > 0)
    {
        memcpy((void *) m_rfFilterBuffer, (const void *) rf, rf_out*sizeof(Complex));
        m_rfFilterBufferIndex = 0;

    }

    ci = m_rfFilterBuffer[m_rfFilterBufferIndex] * m_carrierNco.nextIQ(); // shift to carrier frequency
    m_rfFilterBufferIndex++;

    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
	magsq /= (SDR_TX_SCALED*SDR_TX_SCALED);
	m_movingAverage(magsq);
	m_magsq = m_movingAverage.asDouble();

	sample.m_real = (FixReal) ci.real();
	sample.m_imag = (FixReal) ci.imag();

    m_demodBuffer[m_demodBufferFill] = std::clamp(t, -1.0f, 1.0f) * std::numeric_limits<int16_t>::max();
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

void BFMModSource::modulateAudio()
{
	Real left;
    Real right;
	pullAF(left, right);

    if (m_settings.m_preEmphasis != 0)
    {
        left = m_preemphasisLeft.filter(left);
        right = m_preemphasisRight.filter(right);
    }

    applyLimiter(left, right);

	calculateLevel(0.5f * (left + right));
	m_modSample.real(left);
	m_modSample.imag(right);

    if (m_settings.m_feedbackAudioEnable) {
        pushFeedback(Complex(left, right) * m_settings.m_feedbackVolumeFactor * 16384.0f);
    }
}

void BFMModSource::prefetch(unsigned int nbSamples)
{
    if (m_settings.m_modAFInput != BFMModSettings::BFMModInputAudio) {
        return;
    }

    // The interpolator consumes a fractional number of audio samples per block, so
    // request one more than needed. pullAudio keeps samples that are not consumed.
    unsigned int nbSamplesAudio = (unsigned int) std::ceil(nbSamples * ((double) m_audioSampleRate / m_channelSampleRate)) + 1;
    pullAudio(nbSamplesAudio);
}

void BFMModSource::pullAudio(unsigned int nbSamplesAudio)
{
    QMutexLocker mlock(&m_mutex);

    // Keep samples prefetched for the previous block that it did not consume,
    // then top up to the number requested with as much audio as is available.
    // If not enough is available, pullAF outputs silence after the last sample.
    const unsigned int unconsumed = m_audioBufferFill < m_audioBuffer.size() ? m_audioBuffer.size() - m_audioBufferFill : 0;
    std::copy(m_audioBuffer.begin() + m_audioBufferFill, m_audioBuffer.end(), m_audioBuffer.begin());
    const unsigned int wanted = nbSamplesAudio > unconsumed ? nbSamplesAudio - unconsumed : 0;

    // Audio queued beyond this block is latency. It builds up while the transmitter
    // is stopped, or slowly when the audio device's clock is faster than the SDR's.
    // When it exceeds 200 ms, skip the oldest audio, keeping 50 ms to absorb jitter.
    const unsigned int backlog = m_audioReadBufferFill > wanted ? m_audioReadBufferFill - wanted : 0;
    if (backlog > (unsigned int) m_audioSampleRate / 5) {
        discardAudio(backlog - m_audioSampleRate / 20);
    }

    const unsigned int available = std::min(wanted, m_audioReadBufferFill);
    m_audioBuffer.resize(unconsumed + available);
    std::copy_n(m_audioReadBuffer.begin(), available, m_audioBuffer.begin() + unconsumed);
    m_audioBufferFill = 0;

    discardAudio(available);
}

void BFMModSource::discardAudio(unsigned int nbSamples)
{
    // Remove the oldest samples from the audio read buffer
    nbSamples = std::min(nbSamples, m_audioReadBufferFill);
    std::copy(m_audioReadBuffer.begin() + nbSamples,
        m_audioReadBuffer.begin() + m_audioReadBufferFill, m_audioReadBuffer.begin());
    m_audioReadBufferFill -= nbSamples;
}

void BFMModSource::pullAF(Real& left, Real& right)
{
    switch (m_settings.m_modAFInput)
    {
    case BFMModSettings::BFMModInputTone:
        left = m_toneNco.next() * m_settings.m_volumeFactor;
        right = left;
        break;
    case BFMModSettings::BFMModInputFile:
        if (m_audioFileReader)
        {
            if (!m_audioFileReader->readFrame(left, right))
            {
                if (!m_settings.m_playLoop ||
                    !m_audioFileReader->rewind() ||
                    !m_audioFileReader->readFrame(left, right))
                {
                    left = 0.0f;
                    right = 0.0f;
                }
            }

            left *= m_settings.m_volumeFactor;
            right *= m_settings.m_volumeFactor;
        }
        else
        {
            left = 0.0f;
            right = 0.0f;
        }
        break;
    case BFMModSettings::BFMModInputAudio:
        {
            if (m_audioBufferFill < m_audioBuffer.size())
            {
                left = (m_audioBuffer[m_audioBufferFill].l / 32768.0f) * m_settings.m_volumeFactor;
                right = (m_audioBuffer[m_audioBufferFill].r / 32768.0f) * m_settings.m_volumeFactor;
                m_audioBufferFill++;
            }
            else
            {
                left = 0.0f;
                right = 0.0f;
            }
        }
        break;
    case BFMModSettings::BFMModInputCWTone:
        Real fadeFactor;

        if (m_cwKeyer.getSample())
        {
            m_cwKeyer.getCWSmoother().getFadeSample(true, fadeFactor);
            left = m_cwToneNco.next() * m_settings.m_volumeFactor * fadeFactor * 0.99f;
        }
        else
        {
            if (m_cwKeyer.getCWSmoother().getFadeSample(false, fadeFactor))
            {
                left = m_cwToneNco.next() * m_settings.m_volumeFactor * fadeFactor * 0.99f;
            }
            else
            {
                left = 0.0f;
                m_cwToneNco.setPhase(0);
            }
        }
        right = left;
        break;
    case BFMModSettings::BFMModInputNone:
    default:
        left = 0.0f;
        right = 0.0f;
        break;
    }
}

void BFMModSource::pushFeedback(Complex c)
{
    Complex ci;

    if (m_feedbackInterpolatorDistance < 1.0f) // interpolate
    {
        while (!m_feedbackInterpolator.interpolate(&m_feedbackInterpolatorDistanceRemain, c, &ci))
        {
            processOneSample(ci);
            m_feedbackInterpolatorDistanceRemain += m_feedbackInterpolatorDistance;
        }
    }
    else // decimate
    {
        if (m_feedbackInterpolator.decimate(&m_feedbackInterpolatorDistanceRemain, c, &ci))
        {
            processOneSample(ci);
            m_feedbackInterpolatorDistanceRemain += m_feedbackInterpolatorDistance;
        }
    }
}

void BFMModSource::processOneSample(Complex& ci)
{
    m_feedbackAudioBuffer[m_feedbackAudioBufferFill].l = ci.real();
    m_feedbackAudioBuffer[m_feedbackAudioBufferFill].r = ci.imag();
    ++m_feedbackAudioBufferFill;

    if (m_feedbackAudioBufferFill >= m_feedbackAudioBuffer.size())
    {
        unsigned int res = m_feedbackAudioFifo.write((const quint8*)&m_feedbackAudioBuffer[0], m_feedbackAudioBufferFill);

        if (res != m_feedbackAudioBufferFill)
        {
            qDebug("BFMModSource::processOneSample: %u/%u audio samples written m_feedbackInterpolatorDistance: %f",
                res, m_feedbackAudioBufferFill, m_feedbackInterpolatorDistance);
            m_feedbackAudioFifo.clear();
        }

        m_feedbackAudioBufferFill = 0;
    }
}

void BFMModSource::applyLimiter(Real& left, Real& right)
{
    // Stereo-linked peak limiter with instant attack and slow release. Pre-emphasis
    // can boost high frequencies well above full scale, and reducing the gain
    // briefly is far less audible than hard clipping.
    const Real peak = std::max(std::fabs(left), std::fabs(right));
    m_limiterGain += (1.0f - m_limiterGain) * m_limiterRelease;

    if (peak * m_limiterGain > 1.0f) {
        m_limiterGain = 1.0f / peak;
    }

    left *= m_limiterGain;
    right *= m_limiterGain;
}

void BFMModSource::calculateLevel(const Real& sample)
{
    m_peakLevel = std::max(m_peakLevel, std::fabs(sample));
    m_levelSum += sample * sample;
    m_levelCalcCount++;

    if (m_levelCalcCount >= (quint32) m_levelNbSamples)
    {
        m_rmsLevel = sqrt(m_levelSum / m_levelNbSamples);
        m_peakLevelOut = m_peakLevel;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

Real BFMModSource::audioCutoff(Real afBandwidth, int audioSampleRate)
{
    // Audio must stay clear of the 19 kHz pilot and, once modulated onto the
    // 38 kHz subcarrier, of the RDS band starting at 54.6 kHz.
    return std::min<Real>({afBandwidth, BFMModSettings::m_maxAFBandwidth, audioSampleRate * 0.45f});
}

void BFMModSource::createAudioInterpolator(Real afBandwidth)
{
    m_interpolatorDistanceRemain = 0;
    m_interpolatorDistance = (Real) m_audioSampleRate / (Real) m_channelSampleRate;
    m_interpolator.create(48, m_audioSampleRate, audioCutoff(afBandwidth, m_audioSampleRate), 3.0);
}

void BFMModSource::createRFFilter(Real rfBandwidth, int channelSampleRate)
{
    const Real cut = std::min<Real>(0.5f, (rfBandwidth / 2.0f) / channelSampleRate);
    m_rfFilter->create_filter(-cut, cut);
}

void BFMModSource::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("BFMModSource::applyAudioSampleRate: %d", sampleRate);
        return;
    }

    qDebug("BFMModSource::applyAudioSampleRate: %d", sampleRate);

    m_audioSampleRate = sampleRate;

    {
        // Hold at least 0.5 s, comfortably more than the latency allowed by pullAudio
        QMutexLocker mlock(&m_mutex);
        m_audioReadBuffer.resize(std::max(24000, sampleRate / 2));
        m_audioReadBufferFill = std::min<unsigned int>(m_audioReadBufferFill, m_audioReadBuffer.size());
    }

    createAudioInterpolator(m_settings.m_afBandwidth);
    m_toneNco.setFreq(m_settings.m_toneFrequency, sampleRate);
    m_cwToneNco.setFreq(m_settings.m_toneFrequency, sampleRate);
    m_cwKeyer.setSampleRate(sampleRate);
    m_cwKeyer.reset();
    const Real tau = m_settings.m_preEmphasis == 2 ? FMPREEMPHASIS_TAU_US : FMPREEMPHASIS_TAU_EU;
    m_preemphasisLeft.configure(sampleRate, tau, audioCutoff(m_settings.m_afBandwidth, sampleRate));
    m_preemphasisRight.configure(sampleRate, tau, audioCutoff(m_settings.m_afBandwidth, sampleRate));
    m_limiterRelease = 1.0f - std::exp(-1.0f / (0.2f * sampleRate)); // 200 ms release
    applyFeedbackAudioSampleRate(m_feedbackAudioSampleRate);
}

void BFMModSource::applyFeedbackAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("BFMModSource::applyFeedbackAudioSampleRate: invalid sample rate %d", sampleRate);
        return;
    }

    qDebug("BFMModSource::applyFeedbackAudioSampleRate: %d", sampleRate);

    // Resample from the audio input rate to the feedback device rate. The distance is
    // input samples per output sample and the filter is designed at the input rate.
    m_feedbackInterpolatorDistanceRemain = 0;
    m_feedbackInterpolatorDistance = (Real) m_audioSampleRate / (Real) sampleRate;
    Real cutoff = std::min(sampleRate, m_audioSampleRate) / 2.2f;
    m_feedbackInterpolator.create(48, m_audioSampleRate, cutoff, 3.0);

    m_feedbackAudioSampleRate = sampleRate;
}

void BFMModSource::applySettings(const QStringList& settingsKeys, const BFMModSettings& settings, bool force)
{
    if ((settingsKeys.contains("preEmphasis") || settingsKeys.contains("afBandwidth")) || force)
    {
        const Real tau = settings.m_preEmphasis == 2 ? FMPREEMPHASIS_TAU_US : FMPREEMPHASIS_TAU_EU;
        const Real highFreq = audioCutoff(settings.m_afBandwidth, m_audioSampleRate);
        m_preemphasisLeft.configure(m_audioSampleRate, tau, highFreq);
        m_preemphasisRight.configure(m_audioSampleRate, tau, highFreq);
    }

    if (settingsKeys.contains("rdsPI") || settingsKeys.contains("rdsPTY")
     || settingsKeys.contains("rdsPS") || settingsKeys.contains("rdsRadioText") || force)
    {
        m_rdsEncoder.setData(settings.m_rdsPI, settings.m_rdsPTY, settings.m_rdsPS, settings.m_rdsRadioText);
    }

    if (settingsKeys.contains("audioStereo") || force) {
        m_rdsEncoder.setStereo(settings.m_audioStereo);
    }

    if ((settingsKeys.contains("afBandwidth") && (settings.m_afBandwidth != m_settings.m_afBandwidth)) || force) {
        createAudioInterpolator(settings.m_afBandwidth);
    }

    if ((settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth)) || force) {
        createRFFilter(settings.m_rfBandwidth, m_channelSampleRate);
    }

    if ((settingsKeys.contains("toneFrequency") && (settings.m_toneFrequency != m_settings.m_toneFrequency)) || force)
    {
        m_toneNco.setFreq(settings.m_toneFrequency, m_audioSampleRate);
        m_cwToneNco.setFreq(settings.m_toneFrequency, m_audioSampleRate);
    }

    if ((settingsKeys.contains("modAFInput") && (settings.m_modAFInput != m_settings.m_modAFInput))
     || (settingsKeys.contains("audioDeviceName") && settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force)
    {
        QMutexLocker mlock(&m_mutex);
        m_audioFifo.clear();
        m_audioReadBufferFill = 0;
        m_audioBuffer.clear();
        m_audioBufferFill = 0;
        if (settings.m_modAFInput == BFMModSettings::BFMModInputAudio) {
            connect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()), Qt::UniqueConnection);
        } else {
            disconnect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void BFMModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "BFMModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
     || (channelSampleRate != m_channelSampleRate) || force) {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    const bool sampleRateChanged = (channelSampleRate != m_channelSampleRate) || force;
    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;

    if (sampleRateChanged)
    {
        createAudioInterpolator(m_settings.m_afBandwidth);
        createRFFilter(m_settings.m_rfBandwidth, channelSampleRate);
        m_toneNco.setFreq(m_settings.m_toneFrequency, m_audioSampleRate);
        // 1 ms look-ahead keeps gain changes, and so distortion, below about 1 kHz
        m_lookaheadLimiter.configure(std::max(1, channelSampleRate / 1000), channelSampleRate / 5);
        m_mpxPhase = 0.0;
        m_rdsEncoder.setSampleRate(channelSampleRate);

        m_demodBufferFill = 0;
        QList<ObjectPipe*> pipes;
        MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);
        for (const auto& pipe : pipes)
        {
            MessageQueue* messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            messageQueue->push(MainCore::MsgChannelDemodReport::create(m_channel, m_channelSampleRate));
        }
    }
}

void BFMModSource::handleAudio()
{
    QMutexLocker mlock(&m_mutex);
    while (!m_audioFifo.isEmpty())
    {
        // When the buffer is full, such as while the transmitter is stopped,
        // discard the oldest audio, so that the newest is sent when it starts
        const unsigned int chunk = std::min<unsigned int>(m_audioFifo.fill(), 4096U);
        if (m_audioReadBufferFill + chunk > m_audioReadBuffer.size()) {
            discardAudio(m_audioReadBufferFill + chunk - m_audioReadBuffer.size());
        }
        const unsigned int nbRead = m_audioFifo.read(
            reinterpret_cast<quint8*>(m_audioReadBuffer.data() + m_audioReadBufferFill),
            chunk);
        if (nbRead == 0) {
            break;
        }
        m_audioReadBufferFill += nbRead;
    }
}
