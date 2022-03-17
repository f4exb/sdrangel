///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_ATVDEMODBASEBAND_H
#define INCLUDE_ATVDEMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "dsp/scopevis.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "atvdemodsink.h"

class DownChannelizer;

class ATVDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureATVDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ATVDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureATVDemodBaseband* create(const ATVDemodSettings& settings, bool force)
        {
            return new MsgConfigureATVDemodBaseband(settings, force);
        }

    private:
        ATVDemodSettings m_settings;
        bool m_force;

        MsgConfigureATVDemodBaseband(const ATVDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    ATVDemodBaseband();
    ~ATVDemodBaseband();
    void reset();
    void startWork();
    void stopWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    double getMagSq() const { return m_sink.getMagSq(); }
    ScopeVis *getScopeSink() { return &m_scopeSink; }
    void setTVScreen(TVScreenAnalog *tvScreen) { m_sink.setTVScreen(tvScreen); }
    bool getBFOLocked() { return m_sink.getBFOLocked(); }
    void setVideoTabIndex(int videoTabIndex) { m_sink.setVideoTabIndex(videoTabIndex); }
    void setBasebandSampleRate(int sampleRate); //!< To be used when supporting thread is stopped
    bool isRunning() const { return m_running; }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    ATVDemodSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    ATVDemodSettings m_settings;
    ScopeVis m_scopeSink;
    bool m_running;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const ATVDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_CHANNELANALYZERBASEBAND_H
