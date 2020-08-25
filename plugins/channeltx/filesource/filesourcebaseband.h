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

#ifndef INCLUDE_FILESOURCEBASEBAND_H
#define INCLUDE_FILESOURCEBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "filesourcesource.h"

class UpChannelizer;

class FileSourceBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureFileSourceBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const FileSourceSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureFileSourceBaseband* create(const FileSourceSettings& settings, bool force)
        {
            return new MsgConfigureFileSourceBaseband(settings, force);
        }

    private:
        FileSourceSettings m_settings;
        bool m_force;

        MsgConfigureFileSourceBaseband(const FileSourceSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

	class MsgConfigureFileSourceName : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const QString& getFileName() const { return m_fileName; }

		static MsgConfigureFileSourceName* create(const QString& fileName)
		{
			return new MsgConfigureFileSourceName(fileName);
		}

	private:
		QString m_fileName;

		MsgConfigureFileSourceName(const QString& fileName) :
			Message(),
			m_fileName(fileName)
		{ }
	};

	class MsgConfigureFileSourceWork : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isWorking() const { return m_working; }

		static MsgConfigureFileSourceWork* create(bool working)
		{
			return new MsgConfigureFileSourceWork(working);
		}

	private:
		bool m_working;

		MsgConfigureFileSourceWork(bool working) :
			Message(),
			m_working(working)
		{ }
	};

	class MsgConfigureFileSourceSeek : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getMillis() const { return m_seekMillis; }

		static MsgConfigureFileSourceSeek* create(int seekMillis)
		{
			return new MsgConfigureFileSourceSeek(seekMillis);
		}

	protected:
		int m_seekMillis; //!< millis of seek position from the beginning 0..1000

		MsgConfigureFileSourceSeek(int seekMillis) :
			Message(),
			m_seekMillis(seekMillis)
		{ }
	};

    FileSourceBaseband();
    ~FileSourceBaseband();
    void reset();
	void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_source.setMessageQueueToGUI(messageQueue); }
    double getMagSq() const { return m_source.getMagSq(); }
    int getChannelSampleRate() const;
    quint64 getSamplesCount() const;

    uint32_t getFileSampleRate() const { return m_source.getFileSampleRate(); }
    quint64 getStartingTimeStamp() const { return m_source.getStartingTimeStamp(); }
    quint64 getRecordLengthMuSec() const { return m_source.getRecordLengthMuSec(); }
    quint32 getFileSampleSize() const { return m_source.getFileSampleSize(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) const
    {
        avg = m_avg;
        peak = m_peak;
        nbSamples = m_nbSamples;
    }

private:
    SampleSourceFifo m_sampleFifo;
    UpChannelizer *m_channelizer;
    FileSourceSource m_source;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    FileSourceSettings m_settings;
    double m_avg;
    double m_peak;
    int m_nbSamples;
    QMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const FileSourceSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};


#endif // INCLUDE_FILESOURCEBASEBAND_H
