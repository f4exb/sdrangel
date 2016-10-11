///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <unistd.h>

#include "dsp/dvserialworker.h"
#include "audio/audiofifo.h"

MESSAGE_CLASS_DEFINITION(DVSerialWorker::MsgMbeDecode, Message)
MESSAGE_CLASS_DEFINITION(DVSerialWorker::MsgTest, Message)

DVSerialWorker::DVSerialWorker() :
    m_running(false),
    m_currentGainIn(0),
    m_currentGainOut(0)
{
}

DVSerialWorker::~DVSerialWorker()
{
}

bool DVSerialWorker::open(const std::string& serialDevice)
{
    return m_dvController.open(serialDevice);
}

void DVSerialWorker::close()
{
    m_dvController.close();
}

void DVSerialWorker::process()
{
    m_running  = true;
    qDebug("DVSerialWorker::process: started");

    while (m_running)
    {
        sleep(1);
    }

    qDebug("DVSerialWorker::process: stopped");
    emit finished();
}

void DVSerialWorker::stop()
{
    m_running = false;
}

void DVSerialWorker::handleInputMessages()
{
    Message* message;
    AudioFifo *audioFifo[m_nbFifoSlots];

    for (int i = 0; i < m_nbFifoSlots; i++)
    {
        audioFifo[i] = 0;
        m_fifoSlots[i].m_audioBufferFill = 0;
    }

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (MsgMbeDecode::match(*message))
        {
            MsgMbeDecode *decodeMsg = (MsgMbeDecode *) message;
            int dBVolume = (decodeMsg->getVolumeIndex() - 30) / 2;
            unsigned int fifoSlot = decodeMsg->getFifoSlot();

            if (m_dvController.decode(m_fifoSlots[fifoSlot].m_dvAudioSamples, decodeMsg->getMbeFrame(), decodeMsg->getMbeRate(), dBVolume))
            {
                upsample6(m_fifoSlots[fifoSlot].m_dvAudioSamples,
                        SerialDV::MBE_AUDIO_BLOCK_SIZE,
                        decodeMsg->getChannels(),
                        fifoSlot);
                audioFifo[fifoSlot] = decodeMsg->getAudioFifo();
            }
            else
            {
                qDebug("DVSerialWorker::handleInputMessages: MsgMbeDecode: decode failed");
            }
        }

        delete message;
    }

    for (int i = 0; i < m_nbFifoSlots; i++)
    {
        if (audioFifo[i])
        {
            uint res = audioFifo[i]->write((const quint8*)&m_fifoSlots[i].m_audioBuffer[0], m_fifoSlots[i].m_audioBufferFill, 10);

            if (res != m_fifoSlots[i].m_audioBufferFill)
            {
                qDebug("DVSerialWorker::handleInputMessages: %u/%u audio samples written", res, m_fifoSlots[i].m_audioBufferFill);
            }

            m_fifoSlots[i].m_timestamp = QDateTime::currentDateTime();
        }
    }
}

void DVSerialWorker::pushMbeFrame(const unsigned char *mbeFrame,
        int mbeRateIndex,
        int mbeVolumeIndex,
        unsigned char channels, AudioFifo *audioFifo,
        unsigned int fifoSlot)
{
    m_fifoSlots[0].m_audioFifo = audioFifo;
    m_inputMessageQueue.push(MsgMbeDecode::create(mbeFrame, mbeRateIndex, mbeVolumeIndex, channels, audioFifo, fifoSlot));
}

bool DVSerialWorker::isAvailable(unsigned int& fifoSlot)
{
    for (fifoSlot = 0; fifoSlot < m_nbFifoSlots; fifoSlot++)
    {
        if (m_fifoSlots[fifoSlot].m_audioFifo == 0)
        {
            return true;
        }
        else if (m_fifoSlots[fifoSlot].m_timestamp.time().msecsTo(QDateTime::currentDateTime().time()) > 1000)
        {
            return true;
        }
    }

	return false;
}

bool DVSerialWorker::hasFifo(AudioFifo *audioFifo, unsigned int& fifoSlot)
{
    for (fifoSlot = 0; fifoSlot < m_nbFifoSlots; fifoSlot++)
    {
        if (m_fifoSlots[fifoSlot].m_audioFifo == audioFifo)
        {
            return true;
        }
    }

    return false;
}

void DVSerialWorker::upsample6(short *in, int nbSamplesIn, unsigned char channels, unsigned int fifoSlot)
{
    for (int i = 0; i < nbSamplesIn; i++)
    {
        int cur = (int) in[i];
        int prev = (int) m_fifoSlots[fifoSlot].m_upsamplerLastValue;
        qint16 upsample;

        for (int j = 1; j < 7; j++)
        {
            upsample = m_fifoSlots[fifoSlot].m_upsampleFilter.run((qint16) ((cur*j + prev*(6-j)) / 6));
            m_fifoSlots[fifoSlot].m_audioBuffer[m_fifoSlots[fifoSlot].m_audioBufferFill].l = channels & 1 ? upsample : 0;
            m_fifoSlots[fifoSlot].m_audioBuffer[m_fifoSlots[fifoSlot].m_audioBufferFill].r = (channels>>1) & 1 ? upsample : 0;

            if (m_fifoSlots[fifoSlot].m_audioBufferFill < m_fifoSlots[fifoSlot].m_audioBuffer.size() - 1)
            {
                ++m_fifoSlots[fifoSlot].m_audioBufferFill;
            }
            else
            {
                qDebug("DVSerialWorker::upsample6: audio buffer is full check its size");
            }
        }

        m_fifoSlots[fifoSlot].m_upsamplerLastValue = in[i];
    }
}
