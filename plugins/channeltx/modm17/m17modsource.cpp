///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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
#include "dsp/basebandsamplesink.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "m17modprocessor.h"
#include "m17modsource.h"

const int M17ModSource::m_levelNbSamples = 480; // every 10ms
const float M17ModSource::m_preemphasis = 120.0e-6; // 120us

M17ModSource::M17ModSource() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_modPhasor(0.0f),
    m_audioSampleRate(48000),
    m_audioFifo(12000),
    m_feedbackAudioFifo(48000),
	m_levelCalcCount(0),
	m_peakLevel(0.0f),
	m_levelSum(0.0f),
    m_ifstream(nullptr),
    m_preemphasisFilter(m_preemphasis*48000),
    m_mutex(QMutex::Recursive)
{
    m_audioFifo.setLabel("M17ModSource.m_audioFifo");
    m_feedbackAudioFifo.setLabel("M17ModSource.m_feedbackAudioFifo");
	m_audioBuffer.resize(24000);
	m_audioBufferFill = 0;
	m_audioReadBuffer.resize(24000);
	m_audioReadBufferFill = 0;
    m_audioReadBufferIndex = 0;
    m_m17PullAudio = false;
    m_m17PullCount = 0;
    m_m17PullBERT = false;

	m_feedbackAudioBuffer.resize(1<<14);
	m_feedbackAudioBufferFill = 0;

    m_modBuffer.resize(1<<12);
    m_modBufferFill = 0;

	m_magsq = 0.0;

    m_processor = new M17ModProcessor();
    m_processor->moveToThread(&m_processorThread);
    m_processorThread.start();

    applySettings(m_settings, QList<QString>(), true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

M17ModSource::~M17ModSource()
{
    m_processorThread.exit();
    m_processorThread.wait();
    delete m_processor;
}

void M17ModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void M17ModSource::pullOne(Sample& sample)
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
}

void M17ModSource::prefetch(unsigned int nbSamples)
{
    if ((m_settings.m_m17Mode == M17ModSettings::M17ModeFMAudio))
    {
        unsigned int nbSamplesAudio = nbSamples * ((Real) m_audioSampleRate / (Real) m_channelSampleRate);
        pullAudio(nbSamplesAudio);
    }
}

void M17ModSource::pullAudio(unsigned int nbSamplesAudio)
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

void M17ModSource::modulateSample()
{
	Real t1, t;
    bool carrier;

    if ((m_settings.m_m17Mode == M17ModSettings::M17ModeFMTone) || (m_settings.m_m17Mode == M17ModSettings::M17ModeFMAudio))
    {
        pullAF(t, carrier);

        if (m_settings.m_feedbackAudioEnable) {
            pushFeedback(t * m_settings.m_feedbackVolumeFactor * 16384.0f);
        }
    }
    else
    {
        pullM17(t, carrier);
    }

    if (carrier)
    {
        calculateLevel(t);
        t1 = m_lowpass.filter(t) * 1.5f;

        m_modPhasor += (m_settings.m_fmDeviation / (float) m_audioSampleRate) * t1;

        // limit phasor range to ]-pi,pi]
        if (m_modPhasor > M_PI) {
            m_modPhasor -= (2.0f * M_PI);
        }

        m_modSample.real(cos(m_modPhasor) * 0.891235351562f * SDR_TX_SCALEF); // -1 dB
        m_modSample.imag(sin(m_modPhasor) * 0.891235351562f * SDR_TX_SCALEF);
    }
    else
    {
        m_modSample.real(0.0f);
        m_modSample.imag(0.0f);
    }

    m_modBuffer[m_modBufferFill] = t1 * std::numeric_limits<int16_t>::max();
    ++m_modBufferFill;

    if (m_modBufferFill >= m_modBuffer.size())
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
                    fifo->write((quint8*) &m_modBuffer[0], m_modBuffer.size() * sizeof(qint16), DataFifo::DataTypeI16);
                }
            }
        }

        m_modBufferFill = 0;
    }
}

void M17ModSource::pullAF(Real& sample, bool& carrier)
{
    carrier = true;

    if (m_settings.m_m17Mode == M17ModSettings::M17ModeFMTone)
    {
        sample = m_toneNco.next();
    }
    else if (m_settings.m_m17Mode == M17ModSettings::M17ModeFMAudio)
    {
        if (m_settings.m_audioType == M17ModSettings::AudioFile)
        {
            // sox f4exb_call.wav --encoding float --endian little --rate 48k f4exb_call.raw
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
                    m_ifstream->read(reinterpret_cast<char*>(&sample), sizeof(Real));
                    sample *= m_settings.m_volumeFactor;
                }
            }
            else
            {
                sample = 0.0f;
            }
        }
        else if (m_settings.m_audioType == M17ModSettings::AudioInput)
        {
            if (m_audioBufferFill < m_audioBuffer.size())
            {
                sample = ((m_audioBuffer[m_audioBufferFill].l + m_audioBuffer[m_audioBufferFill].r) / 65536.0f) * m_settings.m_volumeFactor;
                m_audioBufferFill++;
            }
            else
            {
                unsigned int size = m_audioBuffer.size();
                qDebug("NFMModSource::pullAF: starve audio samples: size: %u", size);
                sample = ((m_audioBuffer[size-1].l + m_audioBuffer[size-1].r) / 65536.0f) * m_settings.m_volumeFactor;
            }
        }
        else
        {
            sample = 0.0f;
        }
    }
}

void M17ModSource::pullM17(Real& sample, bool& carrier)
{
    // Prefetch audio data if buffer is low
    if (m_settings.m_m17Mode == M17ModSettings::M17ModeM17Audio)
    {
        if (!m_m17PullAudio)
        {
            M17ModProcessor::MsgStartAudio *msg = M17ModProcessor::MsgStartAudio::create(
                m_settings.m_sourceCall,
                m_settings.m_destCall,
                m_settings.m_can);
            m_processor->getInputMessageQueue()->push(msg);
            m_m17PullAudio = true;
        }

        if ((m_processor->getBasebandFifo()->getFill() < 1920) && (m_m17PullCount > 192))
        {
            M17ModProcessor::MsgSendAudioFrame *msg = M17ModProcessor::MsgSendAudioFrame::create(m_settings.m_sourceCall, m_settings.m_destCall);
            std::array<int16_t, 1920>& audioFrame = msg->getAudioFrame();

            if (m_settings.m_audioType == M17ModSettings::AudioFile)
            {
                if (m_ifstream && m_ifstream->is_open())
                {
                    std::vector<float> fileBuffer;
                    fileBuffer.resize(1920);
                    std::fill(fileBuffer.begin(), fileBuffer.end(), 0.0f);

                    if (m_ifstream->eof() && m_settings.m_playLoop)
                    {
                        m_ifstream->clear();
                        m_ifstream->seekg(0, std::ios::beg);
                    }

                    if (!m_ifstream->eof()) {
                        m_ifstream->read(reinterpret_cast<char*>(fileBuffer.data()), sizeof(float)*1920);
                    }

                    std::transform(
                        fileBuffer.begin(),
                        fileBuffer.end(),
                        audioFrame.begin(),
                        [this](const float& fs) -> int16_t {
                            return fs * 32768.0f * m_settings.m_volumeFactor;
                        }
                    );

                    if (m_settings.m_feedbackAudioEnable) {
                        pushFeedback(audioFrame);
                    }
                }
            }
            else if (m_settings.m_audioType == M17ModSettings::AudioInput)
            {
                std::transform(
                    m_audioReadBuffer.begin(),
                    m_audioReadBuffer.begin() + 1920,
                    audioFrame.begin(),
                    [this](const AudioSample& s) -> int16_t {
                        return (s.l + s.r) * m_settings.m_volumeFactor;
                    }
                );

                if (m_settings.m_feedbackAudioEnable) {
                    pushFeedback(audioFrame);
                }

                if (m_audioReadBufferFill > 1920) // copy back remaining samples at the start of the read buffer
                {
                    std::copy(&m_audioReadBuffer[1920], &m_audioReadBuffer[m_audioReadBufferFill], &m_audioReadBuffer[0]);
                    m_audioReadBufferFill = m_audioReadBufferFill - 1920; // adjust current read buffer fill pointer
                }
            }

            m_processor->getInputMessageQueue()->push(msg);
            m_m17PullCount = 0;
        }
    }
    else if (m_settings.m_m17Mode == M17ModSettings::M17ModeM17BERT)
    {
        if (!m_m17PullBERT)
        {
            M17ModProcessor::MsgStartBERT *msg = M17ModProcessor::MsgStartBERT::create();
            m_processor->getInputMessageQueue()->push(msg);
            m_m17PullBERT = true;
        }

        if ((m_processor->getBasebandFifo()->getFill() < 1920) && (m_m17PullCount > 192))
        {
            M17ModProcessor::MsgSendBERTFrame *msg = M17ModProcessor::MsgSendBERTFrame::create();
            m_processor->getInputMessageQueue()->push(msg);
            m_m17PullCount = 0;
        }
    }
    else
    {
        if (m_m17PullAudio)
        {
            M17ModProcessor::MsgStopAudio *msg = M17ModProcessor::MsgStopAudio::create();
            m_processor->getInputMessageQueue()->push(msg);
            m_m17PullAudio = false;
        }
        else if (m_m17PullBERT)
        {
            M17ModProcessor::MsgStopBERT *msg = M17ModProcessor::MsgStopBERT::create();
            m_processor->getInputMessageQueue()->push(msg);
            m_m17PullBERT = false;
        }
    }

    // get sample from processor FIFO
    int16_t basbandSample;
    carrier = m_processor->getBasebandFifo()->readOne(&basbandSample) != 0;

    if (carrier) {
        sample = basbandSample / 32768.0f;
    } else {
        sample = 0.0f;
    }

    m_m17PullCount++;
}

void M17ModSource::pushFeedback(std::array<int16_t, 1920>& audioFrame)
{
    for (const auto& sample : audioFrame) {
        pushFeedback(sample * m_settings.m_feedbackVolumeFactor);
    }
}

void M17ModSource::pushFeedback(Real sample)
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

void M17ModSource::processOneFeedbackSample(Complex& ci)
{
    m_feedbackAudioBuffer[m_feedbackAudioBufferFill].l = ci.real();
    m_feedbackAudioBuffer[m_feedbackAudioBufferFill].r = ci.imag();
    ++m_feedbackAudioBufferFill;

    if (m_feedbackAudioBufferFill >= m_feedbackAudioBuffer.size())
    {
        uint res = m_feedbackAudioFifo.write((const quint8*)&m_feedbackAudioBuffer[0], m_feedbackAudioBufferFill);

        if (res != m_feedbackAudioBufferFill)
        {
            qDebug("M17ModSource::processOneFeedbackSample: %u/%u audio samples written m_feedbackInterpolatorDistance: %f",
                res, m_feedbackAudioBufferFill, m_feedbackInterpolatorDistance);
            m_feedbackAudioFifo.clear();
        }

        m_feedbackAudioBufferFill = 0;
    }
}

void M17ModSource::calculateLevel(Real& sample)
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

void M17ModSource::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("M17ModSource::applyAudioSampleRate: invalid sample rate %d", sampleRate);
        return;
    }

    qDebug("M17ModSource::applyAudioSampleRate: %d", sampleRate);

    m_interpolatorDistanceRemain = 0;
    m_interpolatorConsumed = false;
    m_interpolatorDistance = (Real) sampleRate / (Real) m_channelSampleRate;
    m_interpolator.create(48, sampleRate, m_settings.m_rfBandwidth / 2.2, 3.0);
    m_lowpass.create(301, sampleRate, m_settings.m_rfBandwidth);
    m_toneNco.setFreq(m_settings.m_toneFrequency, sampleRate);
    m_preemphasisFilter.configure(m_preemphasis*sampleRate);
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

void M17ModSource::applyFeedbackAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("NFMModSource::applyFeedbackAudioSampleRate: invalid sample rate %d", sampleRate);
        return;
    }

    qDebug("NFMModSource::applyFeedbackAudioSampleRate: %d", sampleRate);

    m_feedbackInterpolatorDistanceRemain = 0;
    m_feedbackInterpolatorConsumed = false;
    m_feedbackInterpolatorDistance = (Real) sampleRate / (Real) m_audioSampleRate;
    Real cutoff = std::min(sampleRate, m_audioSampleRate) / 2.2f;
    m_feedbackInterpolator.create(48, sampleRate, cutoff, 3.0);
    m_feedbackAudioSampleRate = sampleRate;
}

void M17ModSource::applySettings(const M17ModSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    if (settingsKeys.contains("rfBandwidth") || force)
    {
        m_settings.m_rfBandwidth = settings.m_rfBandwidth;
        applyAudioSampleRate(m_audioSampleRate);
    }

    if (settingsKeys.contains("toneFrequency") || force) {
        m_toneNco.setFreq(settings.m_toneFrequency, m_audioSampleRate);
    }

    if (settingsKeys.contains("audioType") || force)
    {
        if (settings.m_audioType == M17ModSettings::AudioInput) {
            connect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        } else {
            disconnect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        }
    }

    if (settingsKeys.contains("insertPosition") || force)
    {
        if (settings.m_insertPosition != m_settings.m_insertPosition)
        {
            if (settings.m_insertPosition)
            {
                const MainSettings& mainSettings = MainCore::instance()->getSettings();
                M17ModProcessor::MsgSetGNSS *msg = M17ModProcessor::MsgSetGNSS::create(
                    mainSettings.getLatitude(), mainSettings.getLongitude(), mainSettings.getAltitude());
                m_processor->getInputMessageQueue()->push(msg);
            }
            else
            {
                M17ModProcessor::MsgStopGNSS *msg = M17ModProcessor::MsgStopGNSS::create();
                m_processor->getInputMessageQueue()->push(msg);
            }
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void M17ModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "M17ModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
     || (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
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

void M17ModSource::handleAudio()
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

void M17ModSource::sendPacket()
{
    qDebug("M17ModSource::sendPacket: %d", (int) m_settings.m_packetType);

    if (m_settings.m_packetType == M17ModSettings::PacketType::PacketSMS)
    {
        M17ModProcessor::MsgSendSMS *msg = M17ModProcessor::MsgSendSMS::create(
            m_settings.m_sourceCall,
            m_settings.m_destCall,
            m_settings.m_can,
            m_settings.m_smsText
        );
        m_processor->getInputMessageQueue()->push(msg);
    }
    else if (m_settings.m_packetType == M17ModSettings::PacketType::PacketAPRS)
    {
        M17ModProcessor::MsgSendAPRS *msg = M17ModProcessor::MsgSendAPRS::create(
            m_settings.m_sourceCall,
            m_settings.m_destCall,
            m_settings.m_can,
            m_settings.m_aprsCallsign,
            m_settings.m_aprsTo,
            m_settings.m_aprsVia,
            m_settings.m_aprsData,
            m_settings.m_aprsInsertPosition
        );
        m_processor->getInputMessageQueue()->push(msg);
    }
}
