///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2016, 2019, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
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
#include "dsp/dspcommands.h"
#include "ambeworker.h"

MESSAGE_CLASS_DEFINITION(AMBEWorker::MsgMbeDecode, Message)
MESSAGE_CLASS_DEFINITION(AMBEWorker::MsgMbeEncode, Message)
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
    m_failureCount(0),
    m_encodeClient(nullptr),
    m_encodeReplyQueue(nullptr),
    m_encodeSuccessCount(0),
    m_encodeFailureCount(0)
{
    m_audioBuffer.resize(48000);
    m_audioBufferFill = 0;
    m_audioFifo = 0;
    std::fill(m_dvAudioSamples, m_dvAudioSamples + m_serialDVAudioBlockSize, 0);
    setVolumeFactors();
}

AMBEWorker::~AMBEWorker()
{}

bool AMBEWorker::open(const std::string& deviceRef)
{
    if (deviceRef.empty()) {
        return false;
    } else {
        return m_dvController.open(deviceRef);
    }
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

                if (audioFifo && (m_audioBufferFill >= m_audioBuffer.size() - 960)) {
                    writeAudio(audioFifo);
                }

                m_successCount++;
            }
            else
            {
                qDebug("AMBEWorker::handleInputMessages: MsgMbeDecode: decode failed");
                m_failureCount++;
            }
        }
        else if (MsgMbeEncode::match(*message))
        {
            MsgMbeEncode *encodeMsg = (MsgMbeEncode *) message;
            bool ok = m_dvController.encode(encodeMsg->getAudio(), m_encodedFrame, encodeMsg->getMbeRate(), encodeMsg->getGain());
            int nbBytes = SerialDV::DVController::getNbMbeBytes(encodeMsg->getMbeRate());

            if (ok) {
                m_encodeSuccessCount++;
            } else {
                m_encodeFailureCount++;
            }

            QMutexLocker locker(&m_encodeMutex);

            if ((m_encodeClient == encodeMsg->getClient()) && m_encodeReplyQueue)
            {
                m_encodeReplyQueue->push(new DSPReportMbeFrame(
                    m_encodedFrame,
                    nbBytes,
                    encodeMsg->getSequence(),
                    encodeMsg->getClient(),
                    ok
                ));
            }
        }
        else
        {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }

        delete message;

        if (m_inputMessageQueue.size() > 100)
        {
            qDebug("AMBEWorker::handleInputMessages: MsgMbeDecode: too many messages in queue. Flushing...");
            m_inputMessageQueue.clear();
            break;
        }
    }

    if (audioFifo) {
        writeAudio(audioFifo);
    }

    m_timestamp = QDateTime::currentDateTime();
}

void AMBEWorker::writeAudio(AudioFifo *audioFifo)
{
    QMutexLocker locker(&m_decodeMutex);

    // A FIFO that is no longer the current one may be gone: its client released it (releaseDecodeFifo)
    if (audioFifo == m_audioFifo)
    {
        uint res = audioFifo->write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);

        if (res != m_audioBufferFill) {
            qDebug("AMBEWorker::writeAudio: %u/%u audio samples written", res, m_audioBufferFill);
        }
    }

    m_audioBufferFill = 0;
}

void AMBEWorker::pushMbeFrame(const unsigned char *mbeFrame,
        int mbeRateIndex,
        int mbeVolumeIndex,
        unsigned char channels,
        bool useHP,
        int upsampling,
        AudioFifo *audioFifo)
{
    {
        QMutexLocker locker(&m_decodeMutex);
        m_audioFifo = audioFifo;
    }

    m_inputMessageQueue.push(MsgMbeDecode::create(mbeFrame, mbeRateIndex, mbeVolumeIndex, channels, useHP, upsampling, audioFifo));
}

void AMBEWorker::pushMbeAudio(const short *audio,
        int mbeRateIndex,
        int gain,
        unsigned int sequence,
        const void *client,
        MessageQueue *replyQueue)
{
    {
        QMutexLocker locker(&m_encodeMutex);
        m_encodeClient = client;
        m_encodeReplyQueue = replyQueue;
        m_encodeTimestamp = QDateTime::currentDateTime();
    }

    m_inputMessageQueue.push(MsgMbeEncode::create(audio, mbeRateIndex, gain, sequence, client));
}

bool AMBEWorker::hasEncodeClient(const void *client)
{
    QMutexLocker locker(&m_encodeMutex);
    return m_encodeClient == client;
}

bool AMBEWorker::isAvailableForEncode()
{
    QMutexLocker locker(&m_encodeMutex);

    if (m_encodeClient == nullptr) {
        return true;
    }

    return m_encodeTimestamp.msecsTo(QDateTime::currentDateTime()) > 1000; // 1 second inactivity timeout
}

void AMBEWorker::releaseEncodeClient(const void *client)
{
    QMutexLocker locker(&m_encodeMutex);

    if (m_encodeClient == client)
    {
        m_encodeClient = nullptr;
        m_encodeReplyQueue = nullptr;
    }
}

void AMBEWorker::releaseDecodeFifo(const void *fifo)
{
    QMutexLocker locker(&m_decodeMutex); // a write in progress finishes first, and later ones are skipped

    if (m_audioFifo == fifo) {
        m_audioFifo = nullptr;
    }
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
