///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef INCLUDE_AUDIOOUTPUTDEVICE_H
#define INCLUDE_AUDIOOUTPUTDEVICE_H

#include <QRecursiveMutex>
#include <QIODevice>
#include <QAudioFormat>
#include <list>
#include <vector>
#include <stdint.h>
#include "util/message.h"
#include "util/messagequeue.h"
#include "export.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QAudioSink;
#else
class QAudioOutput;
#endif
class AudioFifo;
class AudioOutputPipe;
class AudioNetSink;
class WavFileRecord;

class SDRBASE_API AudioOutputDevice : public QIODevice {
    Q_OBJECT
public:
    class MsgStart : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        int getDeviceIndex() const { return m_deviceIndex; }
        int getSampleRate() const { return m_sampleRate; }

        static MsgStart* create(int deviceIndex, int sampleRate) {
            return new MsgStart(deviceIndex, sampleRate);
        }

    private:
        int m_deviceIndex;
        int m_sampleRate;

        MsgStart(int deviceIndex, int sampleRate) :
            Message(),
            m_deviceIndex(deviceIndex),
            m_sampleRate(sampleRate)
        { }
    };

    class MsgStop : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        static MsgStop* create() {
            return new MsgStop();
        }

    private:

        MsgStop() :
            Message()
        { }
    };

    class MsgReportSampleRate : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        int getDeviceIndex() const { return m_deviceIndex; }
        const QString& getDeviceName() const { return m_deviceName; }
        int getSampleRate() const { return m_sampleRate; }

        static MsgReportSampleRate* create(int deviceIndex, const QString& deviceName, int sampleRate) {
            return new MsgReportSampleRate(deviceIndex, deviceName, sampleRate);
        }

    private:
        int m_deviceIndex;
        QString m_deviceName;
        int m_sampleRate;

        MsgReportSampleRate(int deviceIndex, const QString& deviceName, int sampleRate) :
            Message(),
            m_deviceIndex(deviceIndex),
            m_deviceName(deviceName),
            m_sampleRate(sampleRate)
        { }
    };

    enum UDPChannelMode
    {
        UDPChannelLeft,
        UDPChannelRight,
        UDPChannelMixed,
        UDPChannelStereo
    };

    enum UDPChannelCodec
    {
        UDPCodecL16,   //!< Linear 16 bit (no codec)
		UDPCodecL8,    //!< Linear 8 bit
        UDPCodecALaw,  //!< PCM A-law 8 bit
        UDPCodecULaw,  //!< PCM Mu-law 8 bit
        UDPCodecG722,  //!< G722 compression
		UDPCodecOpus   //!< Opus compression
    };

	AudioOutputDevice();
	virtual ~AudioOutputDevice();

	void addFifo(AudioFifo* audioFifo);
	void removeFifo(AudioFifo* audioFifo);
	int getNbFifos() const { return m_audioFifos.size(); }

	unsigned int getRate() const { return m_audioFormat.sampleRate(); }
	void setOnExit(bool onExit) { m_onExit = onExit; }

	void setUdpDestination(const QString& address, uint16_t port);
	void setUdpCopyToUDP(bool copyToUDP);
	void setUdpUseRTP(bool useRTP);
	void setUdpChannelMode(UDPChannelMode udpChannelMode);
	void setUdpChannelFormat(UDPChannelCodec udpChannelCodec, bool stereo, int sampleRate);
	void setUdpDecimation(uint32_t decimation);
	void setVolume(float volume);
    void setFileRecordName(const QString& fileRecordName);
    void setRecordToFile(bool recordToFile);
    void setRecordSilenceTime(int recordSilenceTime);
    void setDeviceName(const QString& deviceName) { m_deviceName = deviceName;}

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setManagerMessageQueue(MessageQueue *messageQueue) { m_managerMessageQueue = messageQueue; }

private:
	QRecursiveMutex m_mutex;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QAudioSink* m_audioOutput;
#else
	QAudioOutput* m_audioOutput;
#endif
	AudioNetSink* m_audioNetSink;
    WavFileRecord* m_wavFileRecord;
	bool m_copyAudioToUdp;
	UDPChannelMode m_udpChannelMode;
	UDPChannelCodec m_udpChannelCodec;
	uint m_audioUsageCount;
	bool m_onExit;
	float m_volume;
    QString m_fileRecordName;
    bool m_recordToFile;
    int m_recordSilenceTime;
    int m_recordSilenceNbSamples;
    int m_recordSilenceCount;

	std::list<AudioFifo*> m_audioFifos;
	std::vector<qint32> m_mixBuffer;

	QAudioFormat m_audioFormat;
    QString m_deviceName;

    MessageQueue m_inputMessageQueue;
    MessageQueue *m_managerMessageQueue;

	//virtual bool open(OpenMode mode);
	virtual qint64 readData(char* data, qint64 maxLen);
	virtual qint64 writeData(const char* data, qint64 len);
    virtual qint64 bytesAvailable() const override;
    void writeSampleToFile(qint16 lSample, qint16 rSample);
    bool handleMessage(const Message& cmd);

	bool start(int deviceIndex, int sampleRate);
	void stop();

	friend class AudioOutputPipe;

private slots:
    void handleInputMessages();
};

#endif // INCLUDE_AUDIOOUTPUTDEVICE_H
