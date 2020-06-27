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

#ifndef INCLUDE_LOCALSOURCEBASEBAND_H
#define INCLUDE_LOCALSOURCEBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "localsourcesource.h"
#include "localsourcesettings.h"

class UpChannelizer;

class LocalSourceBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureLocalSourceBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const LocalSourceSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureLocalSourceBaseband* create(const LocalSourceSettings& settings, bool force)
        {
            return new MsgConfigureLocalSourceBaseband(settings, force);
        }

    private:
        LocalSourceSettings m_settings;
        bool m_force;

        MsgConfigureLocalSourceBaseband(const LocalSourceSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

	class MsgConfigureLocalSourceWork : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isWorking() const { return m_working; }

		static MsgConfigureLocalSourceWork* create(bool working)
		{
			return new MsgConfigureLocalSourceWork(working);
		}

	private:
		bool m_working;

		MsgConfigureLocalSourceWork(bool working) :
			Message(),
			m_working(working)
		{ }
	};

    class MsgConfigureLocalDeviceSampleSink : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgConfigureLocalDeviceSampleSink* create(DeviceSampleSink *deviceSampleSink) {
            return new MsgConfigureLocalDeviceSampleSink(deviceSampleSink);
        }

        DeviceSampleSink *getDeviceSampleSink() const { return m_deviceSampleSink; }

    private:

        MsgConfigureLocalDeviceSampleSink(DeviceSampleSink *deviceSampleSink) :
            Message(),
            m_deviceSampleSink(deviceSampleSink)
        { }

        DeviceSampleSink *m_deviceSampleSink;
    };

    LocalSourceBaseband();
    ~LocalSourceBaseband();
    void reset();
	void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    void startSource() { m_source.start(m_localSampleSink); }
    void stopSource() { m_source.stop(); }

private:
    SampleSourceFifo m_sampleFifo;
    UpChannelizer *m_channelizer;
    LocalSourceSource m_source;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    LocalSourceSettings m_settings;
    DeviceSampleSink *m_localSampleSink;
    QMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const LocalSourceSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};


#endif // INCLUDE_LOCALSOURCEBASEBAND_H
