///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_ENDOFTRAINDEMODBASEBAND_H
#define INCLUDE_ENDOFTRAINDEMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesinkfifo.h"
#include "dsp/scopevis.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "endoftraindemodsink.h"

class DownChannelizer;
class ChannelAPI;
class EndOfTrainDemod;
class ScopeVis;

class EndOfTrainDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureEndOfTrainDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const EndOfTrainDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureEndOfTrainDemodBaseband* create(const EndOfTrainDemodSettings& settings, const QStringList& settingsKeys, bool force)
        {
            return new MsgConfigureEndOfTrainDemodBaseband(settings, settingsKeys, force);
        }

    private:
        EndOfTrainDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureEndOfTrainDemodBaseband(const EndOfTrainDemodSettings& settings, const QStringList& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    EndOfTrainDemodBaseband(EndOfTrainDemod *endoftrainDemod);
    ~EndOfTrainDemodBaseband();
    void reset();
    void startWork();
    void stopWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_sink.getMagSqLevels(avg, peak, nbSamples);
    }
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_sink.setMessageQueueToChannel(messageQueue); }
    void setBasebandSampleRate(int sampleRate);
    ScopeVis *getScopeSink() { return &m_scopeSink; }
    int getChannelSampleRate() const;
    void setChannel(ChannelAPI *channel);
    double getMagSq() const { return m_sink.getMagSq(); }
    bool isRunning() const { return m_running; }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    EndOfTrainDemodSink m_sink;
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    EndOfTrainDemodSettings m_settings;
    ScopeVis m_scopeSink;
    bool m_running;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const EndOfTrainDemodSettings& settings, const QStringList& settingsKeys, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_ENDOFTRAINDEMODBASEBAND_H
