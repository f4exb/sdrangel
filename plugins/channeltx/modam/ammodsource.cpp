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

#include "dsp/datafifo.h"
#include "dsp/cwkeyer.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "ammodsource.h"

const int AMModSource::m_levelNbSamples = 480; // every 10ms

AMModSource::AMModSource() :
    m_audioFifo(12000),
    m_feedbackAudioSampleRate(48000),
    m_feedbackAudioFifo(48000)
{
    m_audioFifo.setLabel("AMModSource.m_audioFifo");
    m_feedbackAudioFifo.setLabel("AMModSource.m_feedbackAudioFifo");
	m_audioBuffer.resize(24000);
	m_audioBufferFill = 0;
	m_audioReadBuffer.resize(24000);
	m_audioReadBufferFill = 0;
	m_feedbackAudioBuffer.resize(1<<14);
	m_feedbackAudioBufferFill = 0;
    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;
    m_demodBufferEnabled = false;

	m_magsq = 0.0;

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

AMModSource::~AMModSource() = default;

void AMModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void AMModSource::pullOne(Sample& sample)
{
	if (m_settings.m_channelMute)
	{
		sample.m_real = 0.0f;
		sample.m_imag = 0.0f;
		return;
	}

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
    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
	magsq /= (SDR_TX_SCALED*SDR_TX_SCALED);
	m_movingAverage(magsq);
	m_magsq = m_movingAverage.asDouble();

	sample.m_real = (FixReal) ci.real();
	sample.m_imag = (FixReal) ci.imag();

    m_demodBuffer[m_demodBufferFill] = (qint16) (ci.real() + ci.imag());
    ++m_demodBufferFill;

    if (m_demodBufferFill >= m_demodBuffer.size())
    {
        QList<ObjectPipe*> dataPipes;
        MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", dataPipes);

        if (!dataPipes.empty())
        {
            for (auto& dataPipe : dataPipes)
            {
                DataFifo *fifo = qobject_cast<DataFifo*>(dataPipe->m_element);

                if (fifo) {
                    fifo->write((quint8*) &m_demodBuffer[0], m_demodBuffer.size() * sizeof(qint16), DataFifo::DataTypeI16);
                }
            }
        }

        m_demodBufferFill = 0;
    }
}

void AMModSource::prefetch(unsigned int nbSamples)
{
    auto nbSamplesAudio = (nbSamples * (unsigned int) ((Real) m_audioSampleRate / (Real) m_channelSampleRate));
    pullAudio(nbSamplesAudio);
}

void AMModSource::pullAudio(unsigned int nbSamples)
{
    QMutexLocker mlock(&m_mutex);

    if (nbSamples > m_audioBuffer.size()) {
        m_audioBuffer.resize(nbSamples);
    }

    std::copy(&m_audioReadBuffer[0], &m_audioReadBuffer[nbSamples], &m_audioBuffer[0]);
    m_audioBufferFill = 0;

    if (m_audioReadBufferFill > nbSamples) // copy back remaining samples at the start of the read buffer
    {
        std::copy(&m_audioReadBuffer[nbSamples], &m_audioReadBuffer[m_audioReadBufferFill], &m_audioReadBuffer[0]);
        m_audioReadBufferFill = m_audioReadBufferFill - nbSamples; // adjust current read buffer fill pointer
    }
}

void AMModSource::modulateSample()
{
	Real t = 0.0f;

    pullAF(t);

    if (m_settings.m_feedbackAudioEnable) {
        pushFeedback(t * m_settings.m_feedbackVolumeFactor * 16384.0f);
    }

    calculateLevel(t);
    m_audioBufferFill++;

    m_modSample.real((t*m_settings.m_modFactor + 1.0f) * 16384.0f); // modulate and scale zero frequency carrier
    m_modSample.imag(0.0f);
}

void AMModSource::pullAF(Real& sample)
{
    switch (m_settings.m_modAFInput)
    {
    case AMModSettings::AMModInputTone:
        sample = m_toneNco.next();
        break;
    case AMModSettings::AMModInputFile:
        if (m_ifstream && m_ifstream->is_open())
        {
            if ((m_ifstream->eof()) && (m_settings.m_playLoop))
            {
                m_ifstream->clear();
                m_ifstream->seekg(0, std::ios::beg);
            }

            if (m_ifstream->eof())
            {
            	sample = 0.0f;
            }
            else
            {
            	m_ifstream->read(reinterpret_cast<char*>(&sample), sizeof(Real));
            	sample *= m_settings.m_volumeFactor;
            }
        }
        else
        {
            sample = 0.0f;
        }
        break;
    case AMModSettings::AMModInputAudio:
        sample = ((m_audioBuffer[m_audioBufferFill].l + m_audioBuffer[m_audioBufferFill].r) / 65536.0f) * m_settings.m_volumeFactor;
        break;
    case AMModSettings::AMModInputCWTone:
        Real fadeFactor;

        if (!m_cwKeyer) {
            break;
        }

        if (m_cwKeyer->getSample())
        {
            m_cwKeyer->getCWSmoother().getFadeSample(true, fadeFactor);
            sample = m_toneNco.next() * fadeFactor;
        }
        else
        {
            if (m_cwKeyer->getCWSmoother().getFadeSample(false, fadeFactor))
            {
                sample = m_toneNco.next() * fadeFactor;
            }
            else
            {
                sample = 0.0f;
                m_toneNco.setPhase(0);
            }
        }
        break;
    default:
        sample = 0.0f;
        break;
    }
}

void AMModSource::pushFeedback(Real sample)
{
    Complex c(sample, sample);
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

void AMModSource::processOneSample(const Complex& ci)
{
    m_feedbackAudioBuffer[m_feedbackAudioBufferFill].l = (qint16) ci.real();
    m_feedbackAudioBuffer[m_feedbackAudioBufferFill].r = (qint16) ci.imag();
    ++m_feedbackAudioBufferFill;

    if (m_feedbackAudioBufferFill >= m_feedbackAudioBuffer.size())
    {
        uint res = m_feedbackAudioFifo.write((const quint8*)&m_feedbackAudioBuffer[0], m_feedbackAudioBufferFill);

        if (res != m_feedbackAudioBufferFill)
        {
            qDebug("AMModChannelSource::pushFeedback: %u/%u audio samples written m_feedbackInterpolatorDistance: %f",
                res, m_feedbackAudioBufferFill, m_feedbackInterpolatorDistance);
            m_feedbackAudioFifo.clear();
        }

        m_feedbackAudioBufferFill = 0;
    }
}

void AMModSource::calculateLevel(const Real& sample)
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

void AMModSource::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("AMModSource::applyAudioSampleRate: invalid sample rate %d", sampleRate);
        return;
    }

    qDebug("AMModSource::applyAudioSampleRate: %d", sampleRate);

    m_interpolatorDistanceRemain = 0;
    m_interpolatorConsumed = false;
    m_interpolatorDistance = (Real) sampleRate / (Real) m_channelSampleRate;
    m_interpolator.create(48, sampleRate, m_settings.m_rfBandwidth / 2.2, 3.0);
    m_toneNco.setFreq(m_settings.m_toneFrequency, (float) sampleRate);

    if (m_cwKeyer)
    {
        m_cwKeyer->setSampleRate(sampleRate);
        m_cwKeyer->reset();
    }

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

    m_audioSampleRate = sampleRate;
    applyFeedbackAudioSampleRate(m_feedbackAudioSampleRate);
}

void AMModSource::applyFeedbackAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("AMModSource::applyFeedbackAudioSampleRate: invalid sample rate %d", sampleRate);
        return;
    }

    qDebug("AMModSource::applyFeedbackAudioSampleRate: %u", sampleRate);

    m_feedbackInterpolatorDistanceRemain = 0;
    m_feedbackInterpolatorDistance = (Real) sampleRate / (Real) m_audioSampleRate;
    Real cutoff = ((float) std::min(sampleRate, m_audioSampleRate)) / 2.2f;
    m_feedbackInterpolator.create(48, sampleRate, cutoff, 3.0);
    m_feedbackAudioSampleRate = sampleRate;
}

void AMModSource::applySettings(const AMModSettings& settings, bool force)
{
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_settings.m_rfBandwidth = settings.m_rfBandwidth;
        applyAudioSampleRate(m_audioSampleRate);
    }

    if ((settings.m_toneFrequency != m_settings.m_toneFrequency) || force) {
        m_toneNco.setFreq(settings.m_toneFrequency, (float) m_audioSampleRate);
    }

    if ((settings.m_modAFInput != m_settings.m_modAFInput) || force)
    {
        if (settings.m_modAFInput == AMModSettings::AMModInputAudio) {
            connect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        } else {
            disconnect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        }
    }

    m_settings = settings;
}

void AMModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "AMModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
     || (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq((float) channelFrequencyOffset, (float) channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_audioSampleRate / (Real) channelSampleRate;
        m_interpolator.create(48, m_audioSampleRate, m_settings.m_rfBandwidth / 2.2, 3.0);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void AMModSource::handleAudio()
{
    unsigned int nbRead;

    while ((nbRead = m_audioFifo.read(reinterpret_cast<quint8*>(&m_audioReadBuffer[m_audioReadBufferFill]), 4096)) != 0)
    {
        if (m_audioReadBufferFill + nbRead + 4096 < m_audioReadBuffer.size()) {
            m_audioReadBufferFill += nbRead;
        }
    }
}
