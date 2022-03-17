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

#ifndef INCLUDE_REMOTESINKBASEBAND_H
#define INCLUDE_REMOTESINKBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "remotesinksink.h"
#include "remotesinksettings.h"

class DownChannelizer;

class RemoteSinkBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureRemoteSinkBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RemoteSinkSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureRemoteSinkBaseband* create(const RemoteSinkSettings& settings, bool force)
        {
            return new MsgConfigureRemoteSinkBaseband(settings, force);
        }

    private:
        RemoteSinkSettings m_settings;
        bool m_force;

        MsgConfigureRemoteSinkBaseband(const RemoteSinkSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    RemoteSinkBaseband();
    ~RemoteSinkBaseband();

    void reset();
	void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    void setNbTxBytes(uint32_t nbTxBytes) { m_sink.setNbTxBytes(nbTxBytes); }

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    void setBasebandSampleRate(int sampleRate);
    void setDeviceIndex(uint32_t deviceIndex) { m_sink.setDeviceIndex(deviceIndex); }
    void setChannelIndex(uint32_t channelIndex) { m_sink.setChannelIndex(channelIndex); }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    bool m_running;
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    int m_basebandSampleRate;
    RemoteSinkSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    RemoteSinkSettings m_settings;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const RemoteSinkSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};


#endif // INCLUDE_REMOTESINKBASEBAND_H
