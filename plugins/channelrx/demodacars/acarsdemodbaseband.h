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

#ifndef INCLUDE_ACARSDEMODBASEBAND_H
#define INCLUDE_ACARSDEMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "dsp/scopevis.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "acarsdemodsink.h"

class DownChannelizer;
class ChannelAPI;
class AcarsDemod;

class AcarsDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureAcarsDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AcarsDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureAcarsDemodBaseband* create(const AcarsDemodSettings& settings, bool force)
        {
            return new MsgConfigureAcarsDemodBaseband(settings, force);
        }

    private:
        AcarsDemodSettings m_settings;
        bool m_force;

        MsgConfigureAcarsDemodBaseband(const AcarsDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    AcarsDemodBaseband(AcarsDemod *packetDemod);
    ~AcarsDemodBaseband();
    void reset();
    void startWork();
    void stopWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void getAeroQuality(double& evm, double& suRate) const {
        m_sink.getAeroQuality(evm, suRate);
    }

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
    AcarsDemodSink m_sink;
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    AcarsDemodSettings m_settings;
    ScopeVis m_scopeSink;
    bool m_running;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void calculateOffset(AcarsDemodSink *sink);
    void applySettings(const AcarsDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_ACARSDEMODBASEBAND_H
