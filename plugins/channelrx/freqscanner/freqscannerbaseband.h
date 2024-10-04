///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>          //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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

#ifndef INCLUDE_FREQSCANNERBASEBAND_H
#define INCLUDE_FREQSCANNERBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "freqscannersink.h"

class DownChannelizer;
class ChannelAPI;
class FreqScanner;

class FreqScannerBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureFreqScannerBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const FreqScannerSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureFreqScannerBaseband* create(const FreqScannerSettings& settings, const QStringList& settingsKeys, bool force)
        {
            return new MsgConfigureFreqScannerBaseband(settings, settingsKeys, force);
        }

    private:
        FreqScannerSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureFreqScannerBaseband(const FreqScannerSettings& settings, const QStringList& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    FreqScannerBaseband(FreqScanner *freqScanner);
    ~FreqScannerBaseband();
    void reset();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_sink.setMessageQueueToChannel(messageQueue); }
    void setMessageQueueToGUI(MessageQueue* messageQueue) { m_messageQueueToGUI = messageQueue; }
    void setBasebandSampleRate(int sampleRate);
    int getChannelSampleRate() const;
    void setChannel(ChannelAPI *channel);
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    FreqScanner *m_freqScanner;
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    int m_channelSampleRate;
    int m_scannerSampleRate;
    FreqScannerSink m_sink;
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    MessageQueue *m_messageQueueToGUI;
    FreqScannerSettings m_settings;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const FreqScannerSettings& settings, const QStringList& settingsKeys, bool force = false);
    void calcScannerSampleRate(int basebandSampleRate, float rfBandwidth, int inputFrequencyOffset);
    MessageQueue* getMessageQueueToGUI() { return m_messageQueueToGUI; }

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_FREQSCANNERBASEBAND_H
