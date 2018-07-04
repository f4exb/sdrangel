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
    m_upsamplerLastValue(0.0f),
    m_phase(0),
    m_upsampling(1),
    m_volume(1.0f)
{
    m_audioBuffer.resize(48000);
    m_audioBufferFill = 0;
    m_audioFifo = 0;
    memset(m_dvAudioSamples, 0, SerialDV::MBE_AUDIO_BLOCK_SIZE*sizeof(short));
    setVolumeFactors();
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
            }
            else
            {
                qDebug("DVSerialWorker::handleInputMessages: MsgMbeDecode: decode failed");
            }
        }

        delete message;
    }

    if (audioFifo)
    {
        uint res = audioFifo->write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 10);

        if (res != m_audioBufferFill)
        {
            qDebug("DVSerialWorker::handleInputMessages: %u/%u audio samples written", res, m_audioBufferFill);
        }
    }

    m_timestamp = QDateTime::currentDateTime();
}

void DVSerialWorker::pushMbeFrame(const unsigned char *mbeFrame,
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

bool DVSerialWorker::isAvailable()
{
	if (m_audioFifo == 0) {
		return true;
	}

	return m_timestamp.time().msecsTo(QDateTime::currentDateTime().time()) > 1000; // 1 second inactivity timeout
}

bool DVSerialWorker::hasFifo(AudioFifo *audioFifo)
{
    return m_audioFifo == audioFifo;
}

void DVSerialWorker::upsample(int upsampling, short *in, int nbSamplesIn, unsigned char channels)
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

            if (m_audioBufferFill < m_audioBuffer.size() - 1)
            {
                ++m_audioBufferFill;
            }
            else
            {
                qDebug("DVSerialWorker::upsample6: audio buffer is full check its size");
            }
        }

        m_upsamplerLastValue = cur;
    }
}

void DVSerialWorker::noUpsample(short *in, int nbSamplesIn, unsigned char channels)
{
    for (int i = 0; i < nbSamplesIn; i++)
    {
        float cur = m_upsampleFilter.usesHP() ? m_upsampleFilter.runHP((float) in[i]) : (float) in[i];
        m_audioBuffer[m_audioBufferFill].l = channels & 1 ? cur*m_upsamplingFactors[0] : 0;
        m_audioBuffer[m_audioBufferFill].r = (channels>>1) & 1 ? cur*m_upsamplingFactors[0] : 0;

        if (m_audioBufferFill < m_audioBuffer.size() - 1)
        {
            ++m_audioBufferFill;
        }
        else
        {
            qDebug("DVSerialWorker::noUpsample: audio buffer is full check its size");
        }
    }
}

void DVSerialWorker::setVolumeFactors()
{
    m_upsamplingFactors[0] = m_volume;

    for (int i = 1; i <= m_upsampling; i++) {
        m_upsamplingFactors[i] = (i*m_volume) / (float) m_upsampling;
    }
}

//void DVSerialWorker::upsample6(short *in, short *out, int nbSamplesIn)
//{
//    for (int i = 0; i < nbSamplesIn; i++)
//    {
//        int cur = (int) in[i];
//        int prev = (int) m_upsamplerLastValue;
//        short up;
//
//// DEBUG:
////        for (int j = 0; j < 6; j++)
////        {
////            up = 32768.0f * cos(m_phase);
////            *out = up;
////            out ++;
////            *out = up;
////            out ++;
////            m_phase += M_PI / 6.0;
////        }
////
////        if ((i % 2) == 1)
////        {
////            m_phase = 0.0f;
////        }
//
//        up = m_upsampleFilter.run((cur*1 + prev*5) / 6);
//        *out = up;
//        out++;
//        *out = up;
//        out++;
//
//        up = m_upsampleFilter.run((cur*2 + prev*4) / 6);
//        *out = up;
//        out++;
//        *out = up;
//        out++;
//
//        up = m_upsampleFilter.run((cur*3 + prev*3) / 6);
//        *out = up;
//        out++;
//        *out = up;
//        out++;
//
//        up = m_upsampleFilter.run((cur*4 + prev*2) / 6);
//        *out = up;
//        out++;
//        *out = up;
//        out++;
//
//        up = m_upsampleFilter.run((cur*5 + prev*1) / 6);
//        *out = up;
//        out++;
//        *out = up;
//        out++;
//
//        up = m_upsampleFilter.run(in[i]);
//        *out = up;
//        out++;
//        *out = up;
//        out++;
//
//        m_upsamplerLastValue = in[i];
//    }
//}
