///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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
#include <chrono>
#include <thread>

#include "audio/audiofifo.h"
#include "ambeworker.h"

MESSAGE_CLASS_DEFINITION(AMBEWorker::MsgMbeDecode, Message)
MESSAGE_CLASS_DEFINITION(AMBEWorker::MsgTest, Message)

AMBEWorker::AMBEWorker() :
    m_running(false),
    m_currentGainIn(0),
    m_currentGainOut(0),
    m_upsamplerLastValue(0.0f),
    m_phase(0),
    m_upsampling(1),
    m_volume(1.0f),
    m_successCount(0),
    m_failureCount(0)
{
    m_audioBuffer.resize(48000);
    m_audioBufferFill = 0;
    m_audioFifo = 0;
    std::fill(m_dvAudioSamples, m_dvAudioSamples+SerialDV::MBE_AUDIO_BLOCK_SIZE, 0);
    setVolumeFactors();
}

AMBEWorker::~AMBEWorker()
{}

bool AMBEWorker::open(const std::string& deviceRef)
{
    return m_dvController.open(deviceRef);
}

void AMBEWorker::close()
{
    m_dvController.close();
}

void AMBEWorker::process()
{
    m_running  = true;
    qDebug("AMBEWorker::process: started");

    while (m_running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    qDebug("AMBEWorker::process: stopped");
    emit finished();
}

void AMBEWorker::stop()
{
    m_running = false;
}

void AMBEWorker::handleInputMessages()
{
    Message* message;
    m_audioBufferFill = 0;
    AudioFifo *audioFifo = 0;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (MsgMbeDecode::match(*message))
        {
            MsgMbeDecode *decodeMsg = (MsgMbeDecode *) message;
            int dBVolume = (decodeMsg->getVolumeIndex() - 30) / 4;
            float volume = pow(10.0, dBVolume / 10.0f);
            int upsampling = decodeMsg->getUpsampling();
            upsampling = upsampling > 6 ? 6 : upsampling < 1 ? 1 : upsampling;

            if ((volume != m_volume) || (upsampling != m_upsampling))
            {
                m_volume = volume;
                m_upsampling = upsampling;
                setVolumeFactors();
            }

            m_upsampleFilter.useHP(decodeMsg->getUseHP());

            if (m_dvController.decode(m_dvAudioSamples, decodeMsg->getMbeFrame(), decodeMsg->getMbeRate()))
            {
                if (upsampling > 1) {
                    upsample(upsampling, m_dvAudioSamples, SerialDV::MBE_AUDIO_BLOCK_SIZE, decodeMsg->getChannels());
                } else {
                    noUpsample(m_dvAudioSamples, SerialDV::MBE_AUDIO_BLOCK_SIZE, decodeMsg->getChannels());
                }

                audioFifo = decodeMsg->getAudioFifo();

                if (audioFifo && (m_audioBufferFill >= m_audioBuffer.size() - 960))
                {
                    uint res = audioFifo->write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);

                    if (res != m_audioBufferFill) {
                        qDebug("AMBEWorker::handleInputMessages: %u/%u audio samples written", res, m_audioBufferFill);
                    }

                    m_audioBufferFill = 0;
                }

                m_successCount++;
            }
            else
            {
                qDebug("AMBEWorker::handleInputMessages: MsgMbeDecode: decode failed");
                m_failureCount++;
            }
        }

        delete message;

        if (m_inputMessageQueue.size() > 100)
        {
            qDebug("AMBEWorker::handleInputMessages: MsgMbeDecode: too many messages in queue. Flushing...");
            m_inputMessageQueue.clear();
            break;
        }
    }

    if (audioFifo)
    {
        uint res = audioFifo->write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);

        if (res != m_audioBufferFill) {
            qDebug("AMBEWorker::handleInputMessages: %u/%u audio samples written", res, m_audioBufferFill);
        }

        m_audioBufferFill = 0;
    }

    m_timestamp = QDateTime::currentDateTime();
}

void AMBEWorker::pushMbeFrame(const unsigned char *mbeFrame,
        int mbeRateIndex,
        int mbeVolumeIndex,
        unsigned char channels,
        bool useHP,
        int upsampling,
        AudioFifo *audioFifo)
{
    m_audioFifo = audioFifo;
    m_inputMessageQueue.push(MsgMbeDecode::create(mbeFrame, mbeRateIndex, mbeVolumeIndex, channels, useHP, upsampling, audioFifo));
}

bool AMBEWorker::isAvailable()
{
	if (m_audioFifo == 0) {
		return true;
	}

	return m_timestamp.time().msecsTo(QDateTime::currentDateTime().time()) > 1000; // 1 second inactivity timeout
}

bool AMBEWorker::hasFifo(AudioFifo *audioFifo)
{
    return m_audioFifo == audioFifo;
}

void AMBEWorker::upsample(int upsampling, short *in, int nbSamplesIn, unsigned char channels)
{
    for (int i = 0; i < nbSamplesIn; i++)
    {
        //float cur = m_upsampleFilter.usesHP() ? m_upsampleFilter.runHP((float) m_compressor.compress(in[i])) : (float) m_compressor.compress(in[i]);
        float cur = m_upsampleFilter.usesHP() ? m_upsampleFilter.runHP((float) in[i]) : (float) in[i];
        float prev = m_upsamplerLastValue;
        qint16 upsample;

        for (int j = 1; j <= upsampling; j++)
        {
            upsample = (qint16) m_upsampleFilter.runLP(cur*m_upsamplingFactors[j] + prev*m_upsamplingFactors[upsampling-j]);
            m_audioBuffer[m_audioBufferFill].l = channels & 1 ? m_compressor.compress(upsample) : 0;
            m_audioBuffer[m_audioBufferFill].r = (channels>>1) & 1 ? m_compressor.compress(upsample) : 0;

            if (m_audioBufferFill < m_audioBuffer.size() - 1) {
                ++m_audioBufferFill;
            }
        }

        m_upsamplerLastValue = cur;
    }

    if (m_audioBufferFill >= m_audioBuffer.size() - 1) {
        qDebug("AMBEWorker::upsample(%d): audio buffer is full check its size", upsampling);
    }
}

void AMBEWorker::noUpsample(short *in, int nbSamplesIn, unsigned char channels)
{
    for (int i = 0; i < nbSamplesIn; i++)
    {
        float cur = m_upsampleFilter.usesHP() ? m_upsampleFilter.runHP((float) in[i]) : (float) in[i];
        m_audioBuffer[m_audioBufferFill].l = channels & 1 ? cur*m_upsamplingFactors[0] : 0;
        m_audioBuffer[m_audioBufferFill].r = (channels>>1) & 1 ? cur*m_upsamplingFactors[0] : 0;

        if (m_audioBufferFill < m_audioBuffer.size() - 1) {
            ++m_audioBufferFill;
        }
    }

    if (m_audioBufferFill >= m_audioBuffer.size() - 1) {
        qDebug("AMBEWorker::noUpsample: audio buffer is full check its size");
    }
}

void AMBEWorker::setVolumeFactors()
{
    m_upsamplingFactors[0] = m_volume;

    for (int i = 1; i <= m_upsampling; i++) {
        m_upsamplingFactors[i] = (i*m_volume) / (float) m_upsampling;
    }
}
