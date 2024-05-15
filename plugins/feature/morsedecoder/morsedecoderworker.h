///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef INCLUDE_FEATURE_MORSEDECODERWORKER_H_
#define INCLUDE_FEATURE_MORSEDECODERWORKER_H_

#include <vector>

#include <QObject>
#include <QRecursiveMutex>
#include <QByteArray>

#include "dsp/dsptypes.h"
#include "dsp/datafifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "morsedecodersettings.h"


class MorseDecoderWorker : public QObject {
    Q_OBJECT
public:
    class MsgConfigureMorseDecoderWorker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const MorseDecoderSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureMorseDecoderWorker* create(const MorseDecoderSettings& settings, const QList<QString>& settingsKeys, bool force)
        {
            return new MsgConfigureMorseDecoderWorker(settings, settingsKeys, force);
        }

    private:
        MorseDecoderSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureMorseDecoderWorker(const MorseDecoderSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgConnectFifo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        DataFifo *getFifo() { return m_fifo; }
        bool getConnect() const { return m_connect; }

        static MsgConnectFifo* create(DataFifo *fifo, bool connect) {
            return new MsgConnectFifo(fifo, connect);
        }
    private:
        DataFifo *m_fifo;
        bool m_connect;
        MsgConnectFifo(DataFifo *fifo, bool connect) :
            Message(),
            m_fifo(fifo),
            m_connect(connect)
        { }
    };

    MorseDecoderWorker();
	~MorseDecoderWorker();
    void reset();
    void startWork();
    void stopWork();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToFeature(MessageQueue *messageQueue) { m_msgQueueToFeature = messageQueue; }

    void applySampleRate(int sampleRate);
	void applySettings(const MorseDecoderSettings& settings, const QList<QString>& settingsKeys, bool force = false);

private:
    DataFifo *m_dataFifo;
    int m_channelSampleRate;
    int m_sinkSampleRate;
	MessageQueue m_inputMessageQueue;  //!< Queue for asynchronous inbound communication
    MessageQueue *m_msgQueueToFeature; //!< Queue to report channel change to main feature object
    MorseDecoderSettings m_settings;
	double m_magsq;
	SampleVector m_sampleBuffer;
    int m_sampleBufferSize;
    int m_nbBytes;
    QRecursiveMutex m_mutex;

    void feedPart(
        const QByteArray::const_iterator& begin,
        const QByteArray::const_iterator& end,
        DataFifo::DataType dataType
    );

    bool handleMessage(const Message& cmd);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_FEATURE_MORSEDECODERWORKER_H_
