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

#ifndef SDRBASE_AMBE_AMBEWORKER_H_
#define SDRBASE_AMBE_AMBEWORKER_H_

#include <QObject>
#include <QDebug>
#include <QDateTime>

#include "export.h"
#include "dvcontroller.h"

#include "util/messagequeue.h"
#include "util/message.h"
#include "dsp/filtermbe.h"
#include "dsp/dsptypes.h"
#include "audio/audiocompressor.h"

class AudioFifo;

class AMBEWorker : public QObject {
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
        bool getUseHP() const { return m_useHP; }
        int getUpsampling() const { return m_upsampling; }
        AudioFifo *getAudioFifo() { return m_audioFifo; }

        static MsgMbeDecode* create(
                const unsigned char *mbeFrame,
                int mbeRateIndex,
                int volumeIndex,
                unsigned char channels,
                bool useHP,
                int upsampling,
                AudioFifo *audioFifo)
        {
            return new MsgMbeDecode(mbeFrame, (SerialDV::DVRate) mbeRateIndex, volumeIndex, channels, useHP, upsampling, audioFifo);
        }

    private:
        unsigned char m_mbeFrame[SerialDV::MBE_FRAME_MAX_LENGTH_BYTES];
        SerialDV::DVRate m_mbeRate;
        int m_volumeIndex;
        unsigned char m_channels;
        bool m_useHP;
        int m_upsampling;
        AudioFifo *m_audioFifo;

        MsgMbeDecode(const unsigned char *mbeFrame,
                SerialDV::DVRate mbeRate,
                int volumeIndex,
                unsigned char channels,
                bool useHP,
                int upsampling,
                AudioFifo *audioFifo) :
            Message(),
            m_mbeRate(mbeRate),
            m_volumeIndex(volumeIndex),
            m_channels(channels),
            m_useHP(useHP),
            m_upsampling(upsampling),
            m_audioFifo(audioFifo)
        {
            memcpy((void *) m_mbeFrame, (const void *) mbeFrame, SerialDV::DVController::getNbMbeBytes(m_mbeRate));
        }
    };

    AMBEWorker();
    ~AMBEWorker();

    void pushMbeFrame(const unsigned char *mbeFrame,
            int mbeRateIndex,
            int mbeVolumeIndex,
            unsigned char channels,
            bool useHP,
            int upsampling,
            AudioFifo *audioFifo);

    bool open(const std::string& deviceRef); //!< Either serial device or ip:port
    void close();
    void process();
    void stop();
    bool isAvailable();
    bool hasFifo(AudioFifo *audioFifo);
    uint32_t getSuccessCount() const { return m_successCount; }
    uint32_t getFailureCount() const { return m_failureCount; }

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
    void upsample(int upsampling, short *in, int nbSamplesIn, unsigned char channels);
    void noUpsample(short *in, int nbSamplesIn, unsigned char channels);
    void setVolumeFactors();

    SerialDV::DVController m_dvController;
    AudioFifo *m_audioFifo;
    QDateTime m_timestamp;
    volatile bool m_running;
    int m_currentGainIn;
    int m_currentGainOut;
    short m_dvAudioSamples[SerialDV::MBE_AUDIO_BLOCK_SIZE];
    AudioVector m_audioBuffer;
    uint m_audioBufferFill;
    float m_upsamplerLastValue;
    float m_phase;
    MBEAudioInterpolatorFilter m_upsampleFilter;
    int m_upsampling;
    float m_volume;
    float m_upsamplingFactors[7];
    AudioCompressor m_compressor;
    uint32_t m_successCount;
    uint32_t m_failureCount;
};

#endif // SDRBASE_AMBE_AMBEWORKER_H_
