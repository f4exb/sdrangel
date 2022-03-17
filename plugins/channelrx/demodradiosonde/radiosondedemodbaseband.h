///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_RADIOSONDEDEMODBASEBAND_H
#define INCLUDE_RADIOSONDEDEMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "dsp/scopevis.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "radiosondedemodsink.h"

class DownChannelizer;
class ChannelAPI;
class RadiosondeDemod;
class ScopeVis;

class RadiosondeDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureRadiosondeDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RadiosondeDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureRadiosondeDemodBaseband* create(const RadiosondeDemodSettings& settings, bool force)
        {
            return new MsgConfigureRadiosondeDemodBaseband(settings, force);
        }

    private:
        RadiosondeDemodSettings m_settings;
        bool m_force;

        MsgConfigureRadiosondeDemodBaseband(const RadiosondeDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    RadiosondeDemodBaseband(RadiosondeDemod *radiosondeDemod);
    ~RadiosondeDemodBaseband();
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
    void setChannel(ChannelAPI *channel);
    double getMagSq() const { return m_sink.getMagSq(); }
    bool isRunning() const { return m_running; }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    RadiosondeDemodSink m_sink;
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    RadiosondeDemodSettings m_settings;
    ScopeVis m_scopeSink;
    bool m_running;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void calculateOffset(RadiosondeDemodSink *sink);
    void applySettings(const RadiosondeDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_RADIOSONDEDEMODBASEBAND_H
