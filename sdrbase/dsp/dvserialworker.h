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

#ifndef SDRBASE_DSP_DVSERIALWORKER_H_
#define SDRBASE_DSP_DVSERIALWORKER_H_

#include <QObject>
#include <QDebug>

#include "dvcontroller.h"
#include "util/message.h"
#include "util/syncmessenger.h"
#include "util/messagequeue.h"

class AudioFifo;

class DVSerialWorker : public QObject {
    Q_OBJECT
public:
    class MsgTest : public Message
    {
        MESSAGE_CLASS_DECLARATION
    public:
        static MsgTest* create() { return new MsgTest(); }
    private:
        MsgTest() {}
    };

    DVSerialWorker();
    ~DVSerialWorker();

    bool open(const std::string& serialDevice);
    void close();
    void process();
    void stop();

    void postTest()
    {
        //emit inputMessageReady();
        m_inputMessageQueue.push(MsgTest::create());
    }

    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication

signals:
    void inputMessageReady();
    void finished();

private:
    class MsgMbeDecode : public Message
    {
        MESSAGE_CLASS_DECLARATION
    public:
        const unsigned char *getMbeFrame() const { return m_mbeFrame; }
        SerialDV::DVRate getMbeRate() const { return m_mbeRate; }
        int getVolumeIndex() const { return m_volumeIndex; }
        AudioFifo *getAudioFifo() { return m_audioFifo; }

        static MsgMbeDecode* create(const unsigned char *mbeFrame, int mbeRateIndex, int volumeIndex, AudioFifo *audioFifo)
        {
            return new MsgMbeDecode(mbeFrame, (SerialDV::DVRate) mbeRateIndex, volumeIndex, audioFifo);
        }

    private:
        unsigned char m_mbeFrame[SerialDV::MBE_FRAME_LENGTH_BYTES];
        SerialDV::DVRate m_mbeRate;
        int m_volumeIndex;
        AudioFifo *m_audioFifo;

        MsgMbeDecode(const unsigned char *mbeFrame,
                SerialDV::DVRate mbeRate,
                int volumeIndex,
                AudioFifo *audioFifo) :
            Message(),
            m_mbeRate(mbeRate),
            m_volumeIndex(volumeIndex),
            m_audioFifo(audioFifo)
        {
            memcpy((void *) m_mbeFrame, (const void *) mbeFrame, SerialDV::MBE_FRAME_LENGTH_BYTES);
        }
    };

    SerialDV::DVController m_dvController;
    bool m_running;
    int m_currentGainIn;
    int m_currentGainOut;
    short m_audioSamples[SerialDV::MBE_AUDIO_BLOCK_SIZE];

private slots:
    void handleTest();
};

#endif /* SDRBASE_DSP_DVSERIALWORKER_H_ */
