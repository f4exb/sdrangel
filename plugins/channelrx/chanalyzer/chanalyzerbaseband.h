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

#ifndef INCLUDE_CHANNELANALYZERBASEBAND_H
#define INCLUDE_CHANNELANALYZERBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "chanalyzersink.h"

class DownChannelizer;

class ChannelAnalyzerBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureChannelAnalyzerBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ChannelAnalyzerSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureChannelAnalyzerBaseband* create(const ChannelAnalyzerSettings& settings, bool force)
        {
            return new MsgConfigureChannelAnalyzerBaseband(settings, force);
        }

    private:
        ChannelAnalyzerSettings m_settings;
        bool m_force;

        MsgConfigureChannelAnalyzerBaseband(const ChannelAnalyzerSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    ChannelAnalyzerBaseband();
    ~ChannelAnalyzerBaseband();
    void reset();
    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    double getMagSq() { return m_sink.getMagSq(); }
    double getMagSqAvg() const { return (double) m_sink.getMagSqAvg(); }
    void setScopeVis(ScopeVis *scopeVis) { m_sink.setScopeVis(scopeVis); }
    bool isPllLocked() const { return m_sink.isPllLocked(); }
    Real getPllFrequency() const { return m_sink.getPllFrequency(); }
    Real getPllDeltaPhase() const { return m_sink.getPllDeltaPhase(); }
    Real getPllPhase() const { return m_sink.getPllPhase(); }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    ChannelAnalyzerSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    ChannelAnalyzerSettings m_settings;
    bool m_running;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const ChannelAnalyzerSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_CHANNELANALYZERBASEBAND_H
