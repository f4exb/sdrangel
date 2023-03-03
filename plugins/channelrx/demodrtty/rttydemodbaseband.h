///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_RTTYDEMODBASEBAND_H
#define INCLUDE_RTTYDEMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesinkfifo.h"
#include "dsp/scopevis.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "rttydemodsink.h"

class DownChannelizer;
class ChannelAPI;
class RttyDemod;
class ScopeVis;

class RttyDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureRttyDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RttyDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureRttyDemodBaseband* create(const RttyDemodSettings& settings, bool force)
        {
            return new MsgConfigureRttyDemodBaseband(settings, force);
        }

    private:
        RttyDemodSettings m_settings;
        bool m_force;

        MsgConfigureRttyDemodBaseband(const RttyDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    RttyDemodBaseband(RttyDemod *packetDemod);
    ~RttyDemodBaseband();
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
    int getChannelSampleRate() const;
    ScopeVis *getScopeSink() { return &m_scopeSink; }
    void setChannel(ChannelAPI *channel);
    double getMagSq() const { return m_sink.getMagSq(); }
    bool isRunning() const { return m_running; }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    RttyDemodSink m_sink;
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    RttyDemodSettings m_settings;
    ScopeVis m_scopeSink;
    bool m_running;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void calculateOffset(RttyDemodSink *sink);
    void applySettings(const RttyDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_RTTYDEMODBASEBAND_H

