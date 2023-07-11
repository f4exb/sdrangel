///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_AUDIO_AUDIOINPUTDEVICE_H_
#define SDRBASE_AUDIO_AUDIOINPUTDEVICE_H_

#include <QRecursiveMutex>
#include <QIODevice>
#include <QAudioFormat>
#include <list>
#include <vector>
#include "export.h"
#include "util/message.h"
#include "util/messagequeue.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QAudioSource;
#else
class QAudioInput;
#endif
class AudioFifo;
class AudioOutputPipe;


class SDRBASE_API AudioInputDevice : public QIODevice {
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

	AudioInputDevice();
	virtual ~AudioInputDevice();

	bool startDirect(int deviceIndex, int sampleRate) {
        return start(deviceIndex, sampleRate);
    }
	void stopDirect() {
        stop();
    }

    void addFifo(AudioFifo* audioFifo);
	void removeFifo(AudioFifo* audioFifo);
    int getNbFifos() const { return m_audioFifos.size(); }

	uint getRate() const { return m_audioFormat.sampleRate(); }
	void setOnExit(bool onExit) { m_onExit = onExit; }
	void setVolume(float volume);
    void setDeviceName(const QString& deviceName) { m_deviceName = deviceName;}

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setManagerMessageQueue(MessageQueue *messageQueue) { m_managerMessageQueue = messageQueue; }

private:
	QRecursiveMutex m_mutex;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QAudioSource* m_audioInput;
#else
	QAudioInput* m_audioInput;
#endif
	uint m_audioUsageCount;
	bool m_onExit;
	float m_volume;

	std::list<AudioFifo*> m_audioFifos;
	std::vector<qint32> m_mixBuffer;

	QAudioFormat m_audioFormat;
    QString m_deviceName;

    MessageQueue m_inputMessageQueue;
    MessageQueue *m_managerMessageQueue;

	//virtual bool open(OpenMode mode);
	virtual qint64 readData(char* data, qint64 maxLen);
	virtual qint64 writeData(const char* data, qint64 len);
    bool handleMessage(const Message& cmd);
	bool start(int deviceIndex, int sampleRate);
	void stop();

	friend class AudioOutputPipe;

private slots:
    void handleInputMessages();
};



#endif /* SDRBASE_AUDIO_AUDIOINPUTDEVICE_H_ */
