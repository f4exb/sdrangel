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

#ifndef INCLUDE_REMOTESOURCEBASEBAND_H
#define INCLUDE_REMOTESOURCEBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "remotesourcesource.h"
#include "remotesourcesettings.h"

class UpChannelizer;

class RemoteSourceBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureRemoteSourceBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RemoteSourceSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureRemoteSourceBaseband* create(const RemoteSourceSettings& settings, bool force)
        {
            return new MsgConfigureRemoteSourceBaseband(settings, force);
        }

    private:
        RemoteSourceSettings m_settings;
        bool m_force;

        MsgConfigureRemoteSourceBaseband(const RemoteSourceSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

	class MsgConfigureRemoteSourceWork : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isWorking() const { return m_working; }

		static MsgConfigureRemoteSourceWork* create(bool working)
		{
			return new MsgConfigureRemoteSourceWork(working);
		}

	private:
		bool m_working;

		MsgConfigureRemoteSourceWork(bool working) :
			Message(),
			m_working(working)
		{ }
	};

    RemoteSourceBaseband();
    ~RemoteSourceBaseband();
    void reset();
	void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    RemoteDataReadQueue& getDataQueue() { return m_source.getDataQueue(); }
    uint32_t getNbCorrectableErrors() const { return m_source.getNbCorrectableErrors(); }
    uint32_t getNbUncorrectableErrors() const { return m_source.getNbUncorrectableErrors(); }
    const RemoteMetaDataFEC& getRemoteMetaDataFEC() const { return m_source.getRemoteMetaDataFEC(); }

private:
    SampleSourceFifo m_sampleFifo;
    UpChannelizer *m_channelizer;
    RemoteSourceSource m_source;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    RemoteSourceSettings m_settings;
    QMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const RemoteSourceSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
    void newRemoteSampleRate(unsigned int sampleRate);
};


#endif // INCLUDE_REMOTESOURCEBASEBAND_H
