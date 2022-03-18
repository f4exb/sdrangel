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

#ifndef INCLUDE_AMDEMODBASEBAND_H
#define INCLUDE_AMDEMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "amdemodsink.h"

class DownChannelizer;
class ChannelAPI;

class AMDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureAMDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AMDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureAMDemodBaseband* create(const AMDemodSettings& settings, bool force)
        {
            return new MsgConfigureAMDemodBaseband(settings, force);
        }

    private:
        AMDemodSettings m_settings;
        bool m_force;

        MsgConfigureAMDemodBaseband(const AMDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    AMDemodBaseband();
    ~AMDemodBaseband();
    void reset();
    void startWork();
    void stopWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_sink.getMagSqLevels(avg, peak, nbSamples); }
    bool getSquelchOpen() const { return m_sink.getSquelchOpen(); }
    int getAudioSampleRate() const { return m_sink.getAudioSampleRate(); }
    void setBasebandSampleRate(int sampleRate);
    double getMagSq() const { return m_sink.getMagSq(); }
    bool getPllLocked() const { return m_sink.getPllLocked(); }
    Real getPllFrequency() const { return m_sink.getPllFrequency(); }
    void setChannel(ChannelAPI *channel);
    bool isRunning() const { return m_running; }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }
    void setAudioFifoLabel(const QString& label) { m_sink.setAudioFifoLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    int m_channelSampleRate;
    AMDemodSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    AMDemodSettings m_settings;
    bool m_running;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const AMDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_AMDEMODBASEBAND_H
