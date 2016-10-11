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
#include <QTimer>
#include <QDateTime>

#include <vector>

#include "dvcontroller.h"
#include "util/message.h"
#include "util/syncmessenger.h"
#include "util/messagequeue.h"
#include "dsp/filtermbe.h"

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

    class MsgMbeDecode : public Message
    {
        MESSAGE_CLASS_DECLARATION
    public:
        const unsigned char *getMbeFrame() const { return m_mbeFrame; }
        SerialDV::DVRate getMbeRate() const { return m_mbeRate; }
        int getVolumeIndex() const { return m_volumeIndex; }
        unsigned char getChannels() const { return m_channels % 4; }
        AudioFifo *getAudioFifo() { return m_audioFifo; }
        unsigned int getFifoSlot() const { return m_fifoSlot; }

        static MsgMbeDecode* create(
                const unsigned char *mbeFrame,
                int mbeRateIndex,
                int volumeIndex,
                unsigned char channels,
                AudioFifo *audioFifo,
                unsigned int fifoSlot)
        {
            return new MsgMbeDecode(mbeFrame, (SerialDV::DVRate) mbeRateIndex, volumeIndex, channels, audioFifo, fifoSlot);
        }

    private:
        unsigned char m_mbeFrame[SerialDV::MBE_FRAME_MAX_LENGTH_BYTES];
        SerialDV::DVRate m_mbeRate;
        int m_volumeIndex;
        unsigned char m_channels;
        AudioFifo *m_audioFifo;
        unsigned int m_fifoSlot;

        MsgMbeDecode(const unsigned char *mbeFrame,
                SerialDV::DVRate mbeRate,
                int volumeIndex,
                unsigned char channels,
                AudioFifo *audioFifo,
                unsigned int fifoSlot) :
            Message(),
            m_mbeRate(mbeRate),
            m_volumeIndex(volumeIndex),
            m_channels(channels),
            m_audioFifo(audioFifo),
            m_fifoSlot(fifoSlot)
        {
            memcpy((void *) m_mbeFrame, (const void *) mbeFrame, SerialDV::DVController::getNbMbeBytes(m_mbeRate));
        }
    };

    DVSerialWorker();
    ~DVSerialWorker();

    void pushMbeFrame(const unsigned char *mbeFrame,
            int mbeRateIndex,
            int mbeVolumeIndex,
            unsigned char channels,
            AudioFifo *audioFifo,
            unsigned int fifoSlot);

    bool open(const std::string& serialDevice);
    void close();
    void process();
    void stop();
    bool isAvailable(unsigned int& fifoSlot);
    bool hasFifo(AudioFifo *audioFifo, unsigned int& fifoSlot);

    void postTest()
    {
        //emit inputMessageReady();
        m_inputMessageQueue.push(MsgTest::create());
    }

    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication

signals:
    void finished();

public slots:
    void handleInputMessages();

private:
    struct AudioSample {
        qint16 l;
        qint16 r;
    };

    typedef std::vector<AudioSample> AudioVector;

    struct FifoSlot
    {
        FifoSlot() :
            m_audioFifo(0),
            m_timestamp(QDate(2000, 1, 1)),
            m_audioBufferFill(0),
            m_upsamplerLastValue(0)
        {
            m_audioBuffer.resize(48000);
        }

        AudioFifo *m_audioFifo;
        QDateTime m_timestamp;
        AudioVector m_audioBuffer;
        uint m_audioBufferFill;
        short m_upsamplerLastValue;
        MBEAudioInterpolatorFilter m_upsampleFilter;
        short m_dvAudioSamples[SerialDV::MBE_AUDIO_BLOCK_SIZE];
    };

    void upsample6(short *in, int nbSamplesIn, unsigned char channels, unsigned int fifoSlot);

    SerialDV::DVController m_dvController;
    bool m_running;
    int m_currentGainIn;
    int m_currentGainOut;
    static const unsigned int m_nbFifoSlots = 1;
    FifoSlot m_fifoSlots[m_nbFifoSlots];
};

#endif /* SDRBASE_DSP_DVSERIALWORKER_H_ */
