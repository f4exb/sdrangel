///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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

#include <algorithm>

#include <QObject>
#include <QDebug>
#include <QDateTime>
#include <QMutex>

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

    // SerialDV 1.1.5 exposes 160 samples per vocoder frame, but its transport
    // implementation reads/writes the AMBE-3000 packet maximum of 192 samples.
    // Keep the public 160-sample frame size for processing and pad the backing
    // storage so that the library cannot overrun the adjacent object members.
    static constexpr unsigned int m_serialDVAudioBlockSize = 192U;
    static_assert(m_serialDVAudioBlockSize >= SerialDV::MBE_AUDIO_BLOCK_SIZE,
        "SerialDV transport buffer must hold a public audio block");

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

    class MsgMbeEncode : public Message
    {
        MESSAGE_CLASS_DECLARATION
    public:
        const short *getAudio() const { return m_audio; }
        SerialDV::DVRate getMbeRate() const { return m_mbeRate; }
        int getGain() const { return m_gain; }
        unsigned int getSequence() const { return m_sequence; }
        const void *getClient() const { return m_client; }

        static MsgMbeEncode* create(
                const short *audio,
                int mbeRateIndex,
                int gain,
                unsigned int sequence,
                const void *client)
        {
            return new MsgMbeEncode(audio, (SerialDV::DVRate) mbeRateIndex, gain, sequence, client);
        }

    private:
        short m_audio[m_serialDVAudioBlockSize];
        SerialDV::DVRate m_mbeRate;
        int m_gain;
        unsigned int m_sequence;
        const void *m_client;

        MsgMbeEncode(const short *audio,
                SerialDV::DVRate mbeRate,
                int gain,
                unsigned int sequence,
                const void *client) :
            Message(),
            m_mbeRate(mbeRate),
            m_gain(gain),
            m_sequence(sequence),
            m_client(client)
        {
            std::fill(m_audio, m_audio + m_serialDVAudioBlockSize, 0);
            memcpy((void *) m_audio, (const void *) audio, SerialDV::MBE_AUDIO_BLOCK_SIZE * sizeof(short));
        }
    };

    AMBEWorker();
    ~AMBEWorker();

    //! Push audio to encode. The encoded frame is returned on the reply queue as a DSPReportMbeFrame
    void pushMbeAudio(const short *audio,
            int mbeRateIndex,
            int gain,
            unsigned int sequence,
            const void *client,
            MessageQueue *replyQueue);
    bool hasEncodeClient(const void *client);
    bool isAvailableForEncode();
    bool hasDecodeFifo() const { return m_audioFifo != nullptr; }
    void releaseEncodeClient(const void *client);
    void releaseDecodeFifo(const void *fifo); //!< The FIFO is not written after this returns, frames still queued for it are dropped
    uint32_t getEncodeSuccessCount() const { return m_encodeSuccessCount; }
    uint32_t getEncodeFailureCount() const { return m_encodeFailureCount; }

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
    void writeAudio(AudioFifo *audioFifo); //!< The buffer to the FIFO unless it was released, then empties it

    SerialDV::DVController m_dvController;
    AudioFifo *m_audioFifo;
    QDateTime m_timestamp;
    volatile bool m_running;
    int m_currentGainIn;
    int m_currentGainOut;
    short m_dvAudioSamples[m_serialDVAudioBlockSize];
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

    QMutex m_encodeMutex;
    QMutex m_decodeMutex; //!< Guards m_audioFifo against its release while the worker writes to it
    const void *m_encodeClient;
    MessageQueue *m_encodeReplyQueue;
    QDateTime m_encodeTimestamp;
    unsigned char m_encodedFrame[SerialDV::MBE_FRAME_MAX_LENGTH_BYTES];
    uint32_t m_encodeSuccessCount;
    uint32_t m_encodeFailureCount;
};

#endif // SDRBASE_AMBE_AMBEWORKER_H_
