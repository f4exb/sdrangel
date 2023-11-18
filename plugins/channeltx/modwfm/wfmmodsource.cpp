// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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
// Copyright (C) 2019-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //

#include <QDebug>

#include "dsp/datafifo.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "wfmmodsource.h"

const int WFMModSource::m_rfFilterFFTLength = 1024;
const int WFMModSource::m_levelNbSamples = 480; // every 10ms

WFMModSource::WFMModSource() :
    m_channelSampleRate(384000),
    m_channelFrequencyOffset(0),
    m_modPhasor(0.0f),
    m_audioSampleRate(48000),
    m_audioFifo(12000),
    m_feedbackAudioSampleRate(48000),
    m_feedbackAudioFifo(48000),
	m_levelCalcCount(0),
	m_peakLevel(0.0f),
	m_levelSum(0.0f),
    m_ifstream(nullptr)
{
    m_audioFifo.setLabel("WFMModSource.m_audioFifo");
    m_feedbackAudioFifo.setLabel("WFMModSource.m_feedbackAudioFifo");
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

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

WFMModSource::~WFMModSource()
{
    delete m_rfFilter;
    delete[] m_rfFilterBuffer;
}

void WFMModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void WFMModSource::pullOne(Sample& sample)
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

	if ((m_settings.m_modAFInput == WFMModSettings::WFMModInputFile)
	 || (m_settings.m_modAFInput == WFMModSettings::WFMModInputAudio)
     || (m_settings.m_modAFInput == WFMModSettings::WFMModInputCWTone))
	{
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

        t = ri.real();
	    m_interpolatorDistanceRemain += m_interpolatorDistance;
	}
	else
	{
	    pullAF(t);
        calculateLevel(t);
	}

    m_modPhasor += (m_settings.m_fmDeviation / (float) m_channelSampleRate) * t * M_PI * 2.0f;

    // limit phasor range to ]-pi,pi]
    if (m_modPhasor > M_PI) {
        m_modPhasor -= (2.0f * M_PI);
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

    m_demodBuffer[m_demodBufferFill] = t * std::numeric_limits<int16_t>::max();
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

void WFMModSource::modulateAudio()
{
    Real t;
    pullAF(t);
    calculateLevel(t);
    m_modSample.real(t);
    m_modSample.imag(0.0f);

    if (m_settings.m_feedbackAudioEnable) {
        pushFeedback(t * m_settings.m_feedbackVolumeFactor * 16384.0f);
    }
}

void WFMModSource::prefetch(unsigned int nbSamples)
{
    unsigned int nbSamplesAudio = nbSamples * ((Real) m_audioSampleRate / (Real) m_channelSampleRate);
    pullAudio(nbSamplesAudio);
}

void WFMModSource::pullAudio(unsigned int nbSamplesAudio)
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

void WFMModSource::pullAF(Real& sample)
{
    switch (m_settings.m_modAFInput)
    {
    case WFMModSettings::WFMModInputTone:
        sample = m_toneNco.next() * m_settings.m_volumeFactor;
        break;
    case WFMModSettings::WFMModInputFile:
        // sox f4exb_call.wav --encoding float --endian little f4exb_call.raw
        // ffplay -f f32le -ar 48k -ac 1 f4exb_call.raw
        if (m_ifstream && m_ifstream->is_open())
        {
            if (m_ifstream->eof())
            {
            	if (m_settings.m_playLoop)
            	{
                    m_ifstream->clear();
                    m_ifstream->seekg(0, std::ios::beg);
            	}
            }

            if (m_ifstream->eof())
            {
            	sample = 0.0f;
            }
            else
            {
                Real s;
            	m_ifstream->read(reinterpret_cast<char*>(&s), sizeof(Real));
            	sample = s * m_settings.m_volumeFactor;
            }
        }
        else
        {
            sample = 0.0f;
        }
        break;
    case WFMModSettings::WFMModInputAudio:
        {
            if (m_audioBufferFill < m_audioBuffer.size())
            {
                sample = ((m_audioBuffer[m_audioBufferFill].l + m_audioBuffer[m_audioBufferFill].r) / 65536.0f) * m_settings.m_volumeFactor;
                m_audioBufferFill++;
            }
            else
            {
                unsigned int size = m_audioBuffer.size();
                qDebug("WFMModSource::pullAF: starve audio samples: size: %u", size);
                sample = ((m_audioBuffer[size-1].l + m_audioBuffer[size-1].r) / 65536.0f) * m_settings.m_volumeFactor;
            }
        }
        break;
    case WFMModSettings::WFMModInputCWTone:
        Real fadeFactor;

        if (m_cwKeyer.getSample())
        {
            m_cwKeyer.getCWSmoother().getFadeSample(true, fadeFactor);
            sample = m_cwToneNco.next() * m_settings.m_volumeFactor * fadeFactor * 0.99f;
        }
        else
        {
            if (m_cwKeyer.getCWSmoother().getFadeSample(false, fadeFactor))
            {
                sample = m_cwToneNco.next() * m_settings.m_volumeFactor * fadeFactor * 0.99f;
            }
            else
            {
                sample = 0.0f;
                m_cwToneNco.setPhase(0);
            }
        }
        break;
    case WFMModSettings::WFMModInputNone:
    default:
        sample = 0.0f;
        break;
    }
}

void WFMModSource::pushFeedback(Complex c)
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

void WFMModSource::processOneSample(Complex& ci)
{
    m_feedbackAudioBuffer[m_feedbackAudioBufferFill].l = ci.real();
    m_feedbackAudioBuffer[m_feedbackAudioBufferFill].r = ci.imag();
    ++m_feedbackAudioBufferFill;

    if (m_feedbackAudioBufferFill >= m_feedbackAudioBuffer.size())
    {
        unsigned int res = m_feedbackAudioFifo.write((const quint8*)&m_feedbackAudioBuffer[0], m_feedbackAudioBufferFill);

        if (res != m_feedbackAudioBufferFill)
        {
            qDebug("WFMModSource::processOneSample: %u/%u audio samples written m_feedbackInterpolatorDistance: %f",
                res, m_feedbackAudioBufferFill, m_feedbackInterpolatorDistance);
            m_feedbackAudioFifo.clear();
        }

        m_feedbackAudioBufferFill = 0;
    }
}

void WFMModSource::calculateLevel(const Real& sample)
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

void WFMModSource::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("WFMModSource::applyAudioSampleRate: %d", sampleRate);
        return;
    }

    qDebug("WFMModSource::applyAudioSampleRate: %d", sampleRate);

    m_interpolatorDistanceRemain = 0;
    m_interpolatorConsumed = false;
    m_interpolatorDistance = (Real) sampleRate / (Real) m_channelSampleRate;
    m_interpolator.create(48, sampleRate, m_settings.m_rfBandwidth / 2.2, 3.0);
    m_cwToneNco.setFreq(m_settings.m_toneFrequency, sampleRate);
    m_cwKeyer.setSampleRate(sampleRate);
    m_cwKeyer.reset();
    m_audioSampleRate = sampleRate;
    applyFeedbackAudioSampleRate(m_feedbackAudioSampleRate);

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue* messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(m_channel, sampleRate);
            messageQueue->push(msg);
        }
    }
}

void WFMModSource::applyFeedbackAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("WFMModSource::applyFeedbackAudioSampleRate: invalid sample rate %d", sampleRate);
        return;
    }

    qDebug("WFMModSource::applyFeedbackAudioSampleRate: %d", sampleRate);

    m_feedbackInterpolatorDistanceRemain = 0;
    m_feedbackInterpolatorConsumed = false;
    m_feedbackInterpolatorDistance = (Real) sampleRate / (Real) m_audioSampleRate;
    Real cutoff = std::min(sampleRate, m_audioSampleRate) / 2.2f;
    m_feedbackInterpolator.create(48, sampleRate, cutoff, 3.0);

    m_feedbackAudioSampleRate = sampleRate;
}

void WFMModSource::applySettings(const WFMModSettings& settings, bool force)
{
    if ((settings.m_afBandwidth != m_settings.m_afBandwidth) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_audioSampleRate / (Real) m_channelSampleRate;
        m_interpolator.create(48, m_audioSampleRate, settings.m_afBandwidth / 2.2, 3.0);
    }

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        Real lowCut = -(settings.m_rfBandwidth / 2.2) / m_channelSampleRate;
        Real hiCut  = (settings.m_rfBandwidth / 2.2) / m_channelSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
    }

    if ((settings.m_toneFrequency != m_settings.m_toneFrequency) || force)
    {
        m_toneNco.setFreq(settings.m_toneFrequency, m_channelSampleRate);
        m_cwToneNco.setFreq(settings.m_toneFrequency, m_audioSampleRate);
    }

    if ((settings.m_modAFInput != m_settings.m_modAFInput) || force)
    {
        if (settings.m_modAFInput == WFMModSettings::WFMModInputAudio) {
            connect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        } else {
            disconnect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        }
    }

    m_settings = settings;
}

void WFMModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "WFMModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
     || (channelSampleRate != m_channelSampleRate) || force) {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_audioSampleRate / (Real) channelSampleRate;
        m_interpolator.create(48, m_audioSampleRate, m_settings.m_afBandwidth / 2.2, 3.0);
        Real lowCut = -(m_settings.m_rfBandwidth / 2.0) / channelSampleRate;
        Real hiCut  = (m_settings.m_rfBandwidth / 2.0) / channelSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
        m_toneNco.setFreq(m_settings.m_toneFrequency, channelSampleRate);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void WFMModSource::handleAudio()
{
    QMutexLocker mlock(&m_mutex);
    unsigned int nbRead;

    while ((nbRead = m_audioFifo.read(reinterpret_cast<quint8*>(&m_audioReadBuffer[m_audioReadBufferFill]), 4096)) != 0)
    {
        if (m_audioReadBufferFill + nbRead + 4096 < m_audioReadBuffer.size()) {
            m_audioReadBufferFill += nbRead;
        }
    }
}
