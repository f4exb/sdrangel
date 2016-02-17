///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_SDRDAEMONINPUT_H
#define INCLUDE_SDRDAEMONINPUT_H

#include "dsp/samplesource.h"
#include <QString>
#include <QTimer>
#include <ctime>
#include <iostream>
#include <stdint.h>

class SDRdaemonUDPHandler;

class SDRdaemonInput : public SampleSource {
public:
	class MsgConfigureSDRdaemonUDPLink : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const QString& getAddress() const { return m_address; }
		quint16 getPort() const { return m_port; }

		static MsgConfigureSDRdaemonUDPLink* create(const QString& address, quint16 port)
		{
			return new MsgConfigureSDRdaemonUDPLink(address, port);
		}

	private:
		QString m_address;
		quint16 m_port;

		MsgConfigureSDRdaemonUDPLink(const QString& address, quint16 port) :
			Message(),
			m_address(address),
			m_port(port)
		{ }
	};

	class MsgConfigureSDRdaemonAutoCorr : public Message {
		MESSAGE_CLASS_DECLARATION
	public:
		bool getDCBlock() const { return m_dcBlock; }
		bool getIQImbalance() const { return m_iqCorrection; }

		static MsgConfigureSDRdaemonAutoCorr* create(bool dcBlock, bool iqImbalance)
		{
			return new MsgConfigureSDRdaemonAutoCorr(dcBlock, iqImbalance);
		}

	private:
		bool m_dcBlock;
		bool m_iqCorrection;

		MsgConfigureSDRdaemonAutoCorr(bool dcBlock, bool iqImbalance) :
			Message(),
			m_dcBlock(dcBlock),
			m_iqCorrection(iqImbalance)
		{ }
	};

	class MsgConfigureSDRdaemonWork : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isWorking() const { return m_working; }

		static MsgConfigureSDRdaemonWork* create(bool working)
		{
			return new MsgConfigureSDRdaemonWork(working);
		}

	private:
		bool m_working;

		MsgConfigureSDRdaemonWork(bool working) :
			Message(),
			m_working(working)
		{ }
	};

	class MsgConfigureSDRdaemonStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:

		static MsgConfigureSDRdaemonStreamTiming* create()
		{
			return new MsgConfigureSDRdaemonStreamTiming();
		}

	private:

		MsgConfigureSDRdaemonStreamTiming() :
			Message()
		{ }
	};

	class MsgReportSDRdaemonAcquisition : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool getAcquisition() const { return m_acquisition; }

		static MsgReportSDRdaemonAcquisition* create(bool acquisition)
		{
			return new MsgReportSDRdaemonAcquisition(acquisition);
		}

	protected:
		bool m_acquisition;

		MsgReportSDRdaemonAcquisition(bool acquisition) :
			Message(),
			m_acquisition(acquisition)
		{ }
	};

	class MsgReportSDRdaemonStreamData : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getSampleRate() const { return m_sampleRate; }
		quint64 getCenterFrequency() const { return m_centerFrequency; }
		uint32_t get_tv_sec() const { return m_tv_sec; }
		uint32_t get_tv_usec() const { return m_tv_usec; }

		static MsgReportSDRdaemonStreamData* create(int sampleRate, quint64 centerFrequency, uint32_t tv_sec, uint32_t tv_usec)
		{
			return new MsgReportSDRdaemonStreamData(sampleRate, centerFrequency, tv_sec, tv_usec);
		}

	protected:
		int m_sampleRate;
		quint64 m_centerFrequency;
		uint32_t m_tv_sec;
		uint32_t m_tv_usec;

		MsgReportSDRdaemonStreamData(int sampleRate, quint64 centerFrequency, uint32_t tv_sec, uint32_t tv_usec) :
			Message(),
			m_sampleRate(sampleRate),
			m_centerFrequency(centerFrequency),
			m_tv_sec(tv_sec),
			m_tv_usec(tv_usec)
		{ }
	};

	class MsgReportSDRdaemonStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		uint32_t get_tv_sec() const { return m_tv_sec; }
		uint32_t get_tv_usec() const { return m_tv_usec; }

		static MsgReportSDRdaemonStreamTiming* create(uint32_t tv_sec, uint32_t tv_usec)
		{
			return new MsgReportSDRdaemonStreamTiming(tv_sec, tv_usec);
		}

	protected:
		uint32_t m_tv_sec;
		uint32_t m_tv_usec;

		MsgReportSDRdaemonStreamTiming(uint32_t tv_sec, uint32_t tv_usec) :
			Message(),
			m_tv_sec(tv_sec),
			m_tv_usec(tv_usec)
		{ }
	};

	SDRdaemonInput(const QTimer& masterTimer);
	virtual ~SDRdaemonInput();

	virtual bool init(const Message& message);
	virtual bool start(int device);
	virtual void stop();

	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;
	std::time_t getStartingTimeStamp() const;

	virtual bool handleMessage(const Message& message);

private:
	QMutex m_mutex;
	QString m_address;
	quint16 m_port;
	SDRdaemonUDPHandler* m_SDRdaemonUDPHandler;
	QString m_deviceDescription;
	int m_sampleRate;
	quint64 m_centerFrequency;
	std::time_t m_startingTimeStamp;
	const QTimer& m_masterTimer;
};

#endif // INCLUDE_SDRDAEMONINPUT_H
