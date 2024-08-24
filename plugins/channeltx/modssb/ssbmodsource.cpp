///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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

#include "dsp/spectrumvis.h"
#include "dsp/misc.h"
#include "dsp/datafifo.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "ssbmodsource.h"

const int SSBModSource::m_ssbFftLen = 1024;
const int SSBModSource::m_levelNbSamples = 480; // every 10ms

SSBModSource::SSBModSource() :
    m_audioFifo(12000),
    m_feedbackAudioFifo(12000)
{
    m_audioFifo.setLabel("SSBModSource.m_audioFifo");
    m_feedbackAudioFifo.setLabel("SSBModSource.m_feedbackAudioFifo");
    m_SSBFilter = new fftfilt(m_settings.m_lowCutoff / (float) m_audioSampleRate, m_settings.m_bandwidth / (float) m_audioSampleRate, m_ssbFftLen);
    m_DSBFilter = new fftfilt((2.0f * m_settings.m_bandwidth) / (float) m_audioSampleRate, 2 * m_ssbFftLen);
    m_SSBFilterBuffer = new Complex[m_ssbFftLen>>1]; // filter returns data exactly half of its size
    m_DSBFilterBuffer = new Complex[m_ssbFftLen];
    std::fill(m_SSBFilterBuffer, m_SSBFilterBuffer+(m_ssbFftLen>>1), Complex{0,0});
    std::fill(m_DSBFilterBuffer, m_DSBFilterBuffer+m_ssbFftLen, Complex{0,0});

	m_audioBuffer.resize(24000);
	m_audioBufferFill = 0;
	m_audioReadBuffer.resize(24000);
	m_audioReadBufferFill = 0;

	m_feedbackAudioBuffer.resize(4800);
	m_feedbackAudioBufferFill = 0;

    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;

    m_sum.real(0.0f);
    m_sum.imag(0.0f);
    m_undersampleCount = 0;
    m_sumCount = 0;

	m_magsq = 0.0;
	m_toneNco.setFreq(1000.0, (float) m_audioSampleRate);

    m_audioCompressor.initSimple(
        m_audioSampleRate,
        (float) m_settings.m_cmpPreGainDB,   // pregain (dB)
        (float) m_settings.m_cmpThresholdDB, // threshold (dB)
        20,    // knee (dB)
        12,    // ratio (dB)
        0.003f,// attack (s)
        0.25   // release (s)
    );

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

SSBModSource::~SSBModSource()
{
    delete m_SSBFilter;
    delete m_DSBFilter;
    delete[] m_SSBFilterBuffer;
    delete[] m_DSBFilterBuffer;
}

void SSBModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void SSBModSource::pullOne(Sample& sample)
{
	Complex ci;

    if (m_interpolatorDistance > 1.0f) // decimate
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
    ci *= 0.891235351562f * SDR_TX_SCALEF; //scaling at -1 dB to account for possible filter overshoot

    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
	magsq /= (SDR_TX_SCALED*SDR_TX_SCALED);
	m_movingAverage(magsq);
	m_magsq = m_movingAverage.asDouble();

	sample.m_real = (FixReal) ci.real();
	sample.m_imag = (FixReal) ci.imag();
}

void SSBModSource::prefetch(unsigned int nbSamples)
{
    unsigned int nbSamplesAudio = (nbSamples * (unsigned int) ((Real) m_audioSampleRate / (Real) m_channelSampleRate));
    pullAudio(nbSamplesAudio);
}

void SSBModSource::pullAudio(unsigned int nbSamplesAudio)
{
    QMutexLocker mlock(&m_mutex);

    if (nbSamplesAudio > m_audioBuffer.size()) {
        m_audioBuffer.resize(nbSamplesAudio);
    }

    std::copy(&m_audioReadBuffer[0], &m_audioReadBuffer[nbSamplesAudio], &m_audioBuffer[0]);
    m_audioBufferFill = 0;

    if (m_audioReadBufferFill > nbSamplesAudio) // copy back remaining samples at the start of the read buffer
    {
        std::copy(&m_audioReadBuffer[nbSamplesAudio], &m_audioReadBuffer[m_audioReadBufferFill], &m_audioReadBuffer[0]);
        m_audioReadBufferFill = m_audioReadBufferFill - nbSamplesAudio; // adjust current read buffer fill pointer
    }
}

void SSBModSource::modulateSample()
{
    pullAF(m_modSample);

    if (m_settings.m_feedbackAudioEnable) {
        pushFeedback(m_modSample * m_settings.m_feedbackVolumeFactor * 16384.0f);
    }

    calculateLevel(m_modSample);

    if (m_settings.m_audioBinaural)
    {
        m_demodBuffer[m_demodBufferFill] = (qint16) (m_modSample.real() * std::numeric_limits<int16_t>::max());
        m_demodBufferFill++;
        m_demodBuffer[m_demodBufferFill] = (qint16) (m_modSample.imag() * std::numeric_limits<int16_t>::max());
        m_demodBufferFill++;
    }
    else
    {
        // take projection on real axis
        m_demodBuffer[m_demodBufferFill] = (qint16) (m_modSample.real() * std::numeric_limits<int16_t>::max());
        m_demodBufferFill++;
    }

    if (m_demodBufferFill >= m_demodBuffer.size())
    {
        QList<ObjectPipe*> dataPipes;
        MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", dataPipes);

        if (!dataPipes.empty())
        {
            for (auto& dataPipe : dataPipes)
            {
                DataFifo *fifo = qobject_cast<DataFifo*>(dataPipe->m_element);

                if (fifo)
                {
                    fifo->write(
                        (quint8*) &m_demodBuffer[0],
                        m_demodBuffer.size() * sizeof(qint16),
                        m_settings.m_audioBinaural ? DataFifo::DataTypeCI16 : DataFifo::DataTypeI16
                    );
                }
            }
        }

        m_demodBufferFill = 0;
    }
}

void SSBModSource::pullAF(Complex& sample)
{
	if (m_settings.m_audioMute)
	{
        sample.real(0.0f);
        sample.imag(0.0f);
        return;
	}

    Complex ci;
    fftfilt::cmplx *filtered;
    int n_out = 0;

    int decim = 1<<(m_settings.m_spanLog2 - 1);
    auto decim_mask = (unsigned char) ((decim - 1) & 0xFF); // counter LSB bit mask for decimation by 2^(m_scaleLog2 - 1)

    switch (m_settings.m_modAFInput)
    {
    case SSBModSettings::SSBModInputTone:
        if (m_settings.m_dsb)
        {
            Real t = m_toneNco.next()/1.25f;
            sample.real(t);
            sample.imag(t);
        }
        else
        {
            if (m_settings.m_usb) {
                sample = m_toneNco.nextIQ();
            } else {
                sample = m_toneNco.nextQI();
            }
        }
        break;
    case SSBModSettings::SSBModInputFile:
        if (m_ifstream && m_ifstream->is_open())
        {
            if (m_ifstream->eof() && m_settings.m_playLoop)
            {
                m_ifstream->clear();
                m_ifstream->seekg(0, std::ios::beg);
            }

            if (m_ifstream->eof())
            {
                ci.real(0.0f);
                ci.imag(0.0f);
            }
            else
            {
                if (m_settings.m_audioBinaural)
                {
                    Complex c;
                    m_ifstream->read(reinterpret_cast<char*>(&c), sizeof(Complex));

                    if (m_settings.m_audioFlipChannels)
                    {
                        ci.real(c.imag() * m_settings.m_volumeFactor);
                        ci.imag(c.real() * m_settings.m_volumeFactor);
                    }
                    else
                    {
                        ci = c * m_settings.m_volumeFactor;
                    }
                }
                else
                {
                    Real real;
                    m_ifstream->read(reinterpret_cast<char*>(&real), sizeof(Real));

                    if (m_settings.m_agc)
                    {
                        ci.real(clamp<float>(m_audioCompressor.compress(real), -1.0f, 1.0f));
                        ci.imag(0.0f);
                        ci *= m_settings.m_volumeFactor;
                    }
                    else
                    {
                        ci.real(real * m_settings.m_volumeFactor);
                        ci.imag(0.0f);
                    }
                }
            }
        }
        else
        {
            ci.real(0.0f);
            ci.imag(0.0f);
        }
        break;
    case SSBModSettings::SSBModInputAudio:
        if (m_settings.m_audioBinaural)
        {
            if (m_settings.m_audioFlipChannels)
            {
                ci.real((m_audioBuffer[m_audioBufferFill].r / SDR_TX_SCALEF) * m_settings.m_volumeFactor);
                ci.imag((m_audioBuffer[m_audioBufferFill].l / SDR_TX_SCALEF) * m_settings.m_volumeFactor);
            }
            else
            {
                ci.real((m_audioBuffer[m_audioBufferFill].l / SDR_TX_SCALEF) * m_settings.m_volumeFactor);
                ci.imag((m_audioBuffer[m_audioBufferFill].r / SDR_TX_SCALEF) * m_settings.m_volumeFactor);
            }
        }
        else
        {
            if (m_settings.m_agc)
            {
                float xsample = (m_audioBuffer[m_audioBufferFill].l + m_audioBuffer[m_audioBufferFill].r)  / 65536.0f;
                ci.real(clamp<float>(m_audioCompressor.compress(xsample), -1.0f, 1.0f));
                ci.imag(0.0f);
                ci *= m_settings.m_volumeFactor;
            }
            else
            {
                ci.real(((m_audioBuffer[m_audioBufferFill].l + m_audioBuffer[m_audioBufferFill].r)  / 65536.0f) * m_settings.m_volumeFactor);
                ci.imag(0.0f);
            }
        }

        if (m_audioBufferFill < m_audioBuffer.size() - 1)
        {
            m_audioBufferFill++;
        }
        else
        {
            qDebug("SSBModSource::pullAF: starve audio samples: size: %lu", m_audioBuffer.size());
            m_audioBufferFill = (unsigned int) (m_audioBuffer.size() - 1);
        }

        break;
    case SSBModSettings::SSBModInputCWTone:
        if (!m_cwKeyer) {
            break;
        }

        Real fadeFactor;

        if (m_cwKeyer->getSample())
        {
            m_cwKeyer->getCWSmoother().getFadeSample(true, fadeFactor);

            if (m_settings.m_dsb)
            {
                Real t = m_toneNco.next() * fadeFactor;
                sample.real(t);
                sample.imag(t);
            }
            else
            {
                if (m_settings.m_usb) {
                    sample = m_toneNco.nextIQ() * fadeFactor;
                } else {
                    sample = m_toneNco.nextQI() * fadeFactor;
                }
            }
        }
        else
        {
            if (m_cwKeyer->getCWSmoother().getFadeSample(false, fadeFactor))
            {
                if (m_settings.m_dsb)
                {
                    Real t = (m_toneNco.next() * fadeFactor)/1.25f;
                    sample.real(t);
                    sample.imag(t);
                }
                else
                {
                    if (m_settings.m_usb) {
                        sample = m_toneNco.nextIQ() * fadeFactor;
                    } else {
                        sample = m_toneNco.nextQI() * fadeFactor;
                    }
                }
            }
            else
            {
                sample.real(0.0f);
                sample.imag(0.0f);
                m_toneNco.setPhase(0);
            }
        }

        break;
    default:
        sample.real(0.0f);
        sample.imag(0.0f);
        break;
    }

    if ((m_settings.m_modAFInput == SSBModSettings::SSBModInputFile)
       || (m_settings.m_modAFInput == SSBModSettings::SSBModInputAudio)) // real audio
    {
        if (m_settings.m_dsb)
        {
            n_out = m_DSBFilter->runDSB(ci, &filtered);

            if (n_out > 0)
            {
                memcpy((void *) m_DSBFilterBuffer, (const void *) filtered, n_out*sizeof(Complex));
                m_DSBFilterBufferIndex = 0;
            }

            sample = m_DSBFilterBuffer[m_DSBFilterBufferIndex];
            m_DSBFilterBufferIndex++;
        }
        else
        {
            n_out = m_SSBFilter->runSSB(ci, &filtered, m_settings.m_usb);

            if (n_out > 0)
            {
                memcpy((void *) m_SSBFilterBuffer, (const void *) filtered, n_out*sizeof(Complex));
                m_SSBFilterBufferIndex = 0;
            }

            sample = m_SSBFilterBuffer[m_SSBFilterBufferIndex];
            m_SSBFilterBufferIndex++;
        }

        if (n_out > 0)
        {
            for (int i = 0; i < n_out; i++)
            {
                // Downsample by 2^(m_scaleLog2 - 1) for SSB band spectrum display
                // smart decimation with bit gain using float arithmetic (23 bits significand)

                m_sum += filtered[i];

                if (!(m_undersampleCount++ & decim_mask))
                {
                    Real avgr = (m_sum.real() / (float) decim) * 0.891235351562f * SDR_TX_SCALEF; //scaling at -1 dB to account for possible filter overshoot
                    Real avgi = (m_sum.imag() / (float) decim) * 0.891235351562f * SDR_TX_SCALEF;

                    if (!m_settings.m_dsb && !m_settings.m_usb)
                    { // invert spectrum for LSB
                        m_sampleBuffer.push_back(Sample((FixReal) avgi, (FixReal) avgr));
                    }
                    else
                    {
                        m_sampleBuffer.push_back(Sample((FixReal) avgr, (FixReal)avgi));
                    }

                    m_sum.real(0.0);
                    m_sum.imag(0.0);
                }
            }
        }
    } // Real audio
    else if ((m_settings.m_modAFInput == SSBModSettings::SSBModInputTone)
          || (m_settings.m_modAFInput == SSBModSettings::SSBModInputCWTone)) // tone
    {
        m_sum += sample;

        if (!(m_undersampleCount++ & decim_mask))
        {
            Real avgr = (m_sum.real() / (float) decim) * 0.891235351562f * SDR_TX_SCALEF; //scaling at -1 dB to account for possible filter overshoot
            Real avgi = (m_sum.imag() / (float) decim) * 0.891235351562f * SDR_TX_SCALEF;

            if (!m_settings.m_dsb && !m_settings.m_usb)
            { // invert spectrum for LSB
                m_sampleBuffer.push_back(Sample((FixReal) avgi, (FixReal) avgr));
            }
            else
            {
                m_sampleBuffer.push_back(Sample((FixReal) avgr, (FixReal) avgi));
            }

            m_sum.real(0.0);
            m_sum.imag(0.0);
        }

        if (m_sumCount < (m_settings.m_dsb ? m_ssbFftLen : m_ssbFftLen>>1))
        {
            n_out = 0;
            m_sumCount++;
        }
        else
        {
            n_out = m_sumCount;
            m_sumCount = 0;
        }
    }

    if (n_out > 0)
    {
        if (m_spectrumSink) {
            m_spectrumSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), !m_settings.m_dsb);
        }

        m_sampleBuffer.clear();
    }
}

void SSBModSource::pushFeedback(Complex c)
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

void SSBModSource::processOneSample(const Complex& ci)
{
    if (m_settings.m_modAFInput == SSBModSettings::SSBModInputCWTone) // minimize latency for CW
    {
        m_feedbackAudioBuffer[0].l = (qint16) ci.real();
        m_feedbackAudioBuffer[0].r = (qint16) ci.imag();
        m_feedbackAudioFifo.writeOne((const quint8*) &m_feedbackAudioBuffer[0]);
    }
    else
    {
        m_feedbackAudioBuffer[m_feedbackAudioBufferFill].l = (qint16) ci.real();
        m_feedbackAudioBuffer[m_feedbackAudioBufferFill].r = (qint16) ci.imag();
        ++m_feedbackAudioBufferFill;

        if (m_feedbackAudioBufferFill >= m_feedbackAudioBuffer.size())
        {
            uint res = m_feedbackAudioFifo.write((const quint8*)&m_feedbackAudioBuffer[0], m_feedbackAudioBufferFill);

            if (res != m_feedbackAudioBufferFill)
            {
                qDebug("SSBModSource::pushFeedback: %u/%u audio samples written m_feedbackInterpolatorDistance: %f",
                    res, m_feedbackAudioBufferFill, m_feedbackInterpolatorDistance);
                m_feedbackAudioFifo.clear();
            }

            m_feedbackAudioBufferFill = 0;
        }
    }
}

void SSBModSource::calculateLevel(const Complex& sample)
{
    Real t = sample.real();

    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), t);
        m_levelSum += t * t;
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

void SSBModSource::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("SSBModSource::applyAudioSampleRate: invalid sample rate %d", sampleRate);
        return;
    }

    qDebug("SSBModSource::applyAudioSampleRate: %d", sampleRate);

    m_interpolatorDistanceRemain = 0;
    m_interpolatorConsumed = false;
    m_interpolatorDistance = (Real) sampleRate / (Real) m_channelSampleRate;
    m_interpolator.create(48, sampleRate, m_settings.m_bandwidth, 3.0);

    float band = m_settings.m_bandwidth;
    float lowCutoff = m_settings.m_lowCutoff;
    bool usb = m_settings.m_usb;

    if (band < 100.0f) // at least 100 Hz
    {
        band = 100.0f;
        lowCutoff = 0;
    }

    if (band - lowCutoff < 100.0f) {
        lowCutoff = band - 100.0f;
    }

    m_SSBFilter->create_filter(lowCutoff / (float) sampleRate, band / (float) sampleRate);
    m_DSBFilter->create_dsb_filter((2.0f * band) / (float) sampleRate);

    m_settings.m_bandwidth = band;
    m_settings.m_lowCutoff = lowCutoff;
    m_settings.m_usb = usb;

    m_toneNco.setFreq(m_settings.m_toneFrequency, (float) sampleRate);

    if (m_cwKeyer)
    {
        m_cwKeyer->setSampleRate(sampleRate);
        m_cwKeyer->reset();
    }

    m_audioCompressor.m_rate = (float) sampleRate;
    m_audioCompressor.initState();
    m_audioSampleRate = sampleRate;

    applyFeedbackAudioSampleRate(m_feedbackAudioSampleRate);

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);

    if (!pipes.empty())
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue* messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(m_channel, sampleRate);
            messageQueue->push(msg);
        }
    }
}

void SSBModSource::applyFeedbackAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("SSBModSource::applyFeedbackAudioSampleRate: invalid sample rate %d", sampleRate);
        return;
    }

    qDebug("SSBModSource::applyFeedbackAudioSampleRate: %d", sampleRate);

    m_feedbackInterpolatorDistanceRemain = 0;
    m_feedbackInterpolatorConsumed = false;
    m_feedbackInterpolatorDistance = (Real) sampleRate / (Real) m_audioSampleRate;
    Real cutoff = (float) (std::min(sampleRate, m_audioSampleRate)) / 2.2f;
    m_feedbackInterpolator.create(48, sampleRate, cutoff, 3.0);
    m_feedbackAudioSampleRate = sampleRate;
}

void SSBModSource::applySettings(const SSBModSettings& settings, bool force)
{
    float band = settings.m_bandwidth;
    float lowCutoff = settings.m_lowCutoff;
    bool usb = settings.m_usb;

    if ((settings.m_bandwidth != m_settings.m_bandwidth) ||
        (settings.m_lowCutoff != m_settings.m_lowCutoff) || force)
    {
        if (band < 100.0f) // at least 100 Hz
        {
            band = 100.0f;
            lowCutoff = 0;
        }

        if (band - lowCutoff < 100.0f) {
            lowCutoff = band - 100.0f;
        }

        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_audioSampleRate / (Real) m_channelSampleRate;
        m_interpolator.create(48, m_audioSampleRate, band, 3.0);
        m_SSBFilter->create_filter(lowCutoff / (float) m_audioSampleRate, band / (float) m_audioSampleRate);
        m_DSBFilter->create_dsb_filter((2.0f * band) / (float) m_audioSampleRate);
    }

    if ((settings.m_toneFrequency != m_settings.m_toneFrequency) || force) {
        m_toneNco.setFreq(settings.m_toneFrequency, (float) m_audioSampleRate);
    }

    if ((settings.m_dsb != m_settings.m_dsb) || force)
    {
        if (settings.m_dsb)
        {
            std::fill(m_DSBFilterBuffer, m_DSBFilterBuffer+m_ssbFftLen, Complex{0,0});
            m_DSBFilterBufferIndex = 0;
        }
        else
        {
            std::fill(m_SSBFilterBuffer, m_SSBFilterBuffer+(m_ssbFftLen>>1), Complex{0,0});
            m_SSBFilterBufferIndex = 0;
        }
    }

    if ((settings.m_modAFInput != m_settings.m_modAFInput) || force)
    {
        if (settings.m_modAFInput == SSBModSettings::SSBModInputAudio) {
            connect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        } else {
            disconnect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        }
    }

    if ((settings.m_cmpThresholdDB != m_settings.m_cmpThresholdDB) ||
        (settings.m_cmpPreGainDB != m_settings.m_cmpPreGainDB) || force)
    {
        m_audioCompressor.initSimple(
            m_audioSampleRate,
            (float) settings.m_cmpPreGainDB,   // pregain (dB)
            (float) settings.m_cmpThresholdDB, // threshold (dB)
            20,    // knee (dB)
            12,    // ratio (dB)
            0.003f,// attack (s)
            0.25   // release (s)
        );
    }

    m_settings = settings;
    m_settings.m_bandwidth = band;
    m_settings.m_lowCutoff = lowCutoff;
    m_settings.m_usb = usb;
}

void SSBModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "SSBModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
    || (channelSampleRate != m_channelSampleRate) || force) {
        m_carrierNco.setFreq((float) channelFrequencyOffset, (float) channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_audioSampleRate / (Real) channelSampleRate;
        m_interpolator.create(48, m_audioSampleRate, m_settings.m_bandwidth, 3.0);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void SSBModSource::handleAudio()
{
    unsigned int nbRead;

    while ((nbRead = m_audioFifo.read(reinterpret_cast<quint8*>(&m_audioReadBuffer[m_audioReadBufferFill]), 4096)) != 0)
    {
        if (m_audioReadBufferFill + nbRead + 4096 < m_audioReadBuffer.size()) {
            m_audioReadBufferFill += nbRead;
        }
    }
}
