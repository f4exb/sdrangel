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

#ifndef INCLUDE_LOCALSINKBASEBAND_H
#define INCLUDE_LOCALSINKBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "localsinksink.h"
#include "localsinksettings.h"

class DownChannelizer;

class LocalSinkBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureLocalSinkBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const LocalSinkSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureLocalSinkBaseband* create(const LocalSinkSettings& settings, bool force)
        {
            return new MsgConfigureLocalSinkBaseband(settings, force);
        }

    private:
        LocalSinkSettings m_settings;
        bool m_force;

        MsgConfigureLocalSinkBaseband(const LocalSinkSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgConfigureLocalDeviceSampleSource : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgConfigureLocalDeviceSampleSource* create(DeviceSampleSource *deviceSampleSource) {
            return new MsgConfigureLocalDeviceSampleSource(deviceSampleSource);
        }

        DeviceSampleSource *getDeviceSampleSource() const { return m_deviceSampleSource; }

    private:

        MsgConfigureLocalDeviceSampleSource(DeviceSampleSource *deviceSampleSource) :
            Message(),
            m_deviceSampleSource(deviceSampleSource)
        { }

        DeviceSampleSource *m_deviceSampleSource;
    };

    LocalSinkBaseband();
    ~LocalSinkBaseband();
    void reset();
	void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    void startSource() { m_sink.start(m_localSampleSource); }
    void stopSource() { m_sink.stop(); }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    LocalSinkSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    LocalSinkSettings m_settings;
    DeviceSampleSource *m_localSampleSource;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const LocalSinkSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};


#endif // INCLUDE_LOCALSINKBASEBAND_H
