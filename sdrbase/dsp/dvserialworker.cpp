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
    m_currentGainOut(0),
    m_upsamplerLastValue(0)
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

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (MsgMbeDecode::match(*message))
        {
            MsgMbeDecode *decodeMsg = (MsgMbeDecode *) message;
            int dBVolume = (decodeMsg->getVolumeIndex() - 50) / 5;

            if (m_dvController.decode(m_dvAudioSamples, decodeMsg->getMbeFrame(), decodeMsg->getMbeRate(), dBVolume))
            {
                upsample6(m_dvAudioSamples, m_audioSamples, SerialDV::MBE_AUDIO_BLOCK_SIZE);
                decodeMsg->getAudioFifo()->write((const quint8 *) m_audioSamples, SerialDV::MBE_AUDIO_BLOCK_SIZE * 6, 10);
            }
        }

        delete message;
    }
}

void DVSerialWorker::upsample6(short *in, short *out, int nbSamplesIn)
{
    for (int i = 0; i < nbSamplesIn; i++)
    {
        int cur = (int) in[i];
        int prev = (int) m_upsamplerLastValue;
        short up;

        up = (cur*1 + prev*5) / 6;
        *out = up;
        out++;
        *out = up;
        out++;

        up = (cur*2 + prev*4) / 6;
        *out = up;
        out++;
        *out = up;
        out++;

        up = (cur*3 + prev*3) / 6;
        *out = up;
        out++;
        *out = up;
        out++;

        up = (cur*4 + prev*2) / 6;
        *out = up;
        out++;
        *out = up;
        out++;

        up = (cur*5 + prev*1) / 6;
        *out = up;
        out++;
        *out = up;
        out++;

        up = in[i];
        *out = up;
        out++;
        *out = up;
        out++;

        m_upsamplerLastValue = in[i];
    }
}
