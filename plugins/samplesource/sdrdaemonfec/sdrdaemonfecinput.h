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

#ifndef INCLUDE_SDRDAEMONFECINPUT_H
#define INCLUDE_SDRDAEMONFECINPUT_H

#include "dsp/samplesource.h"
#include <QString>
#include <QTimer>
#include <ctime>
#include <iostream>
#include <stdint.h>

class DeviceAPI;
class SDRdaemonFECUDPHandler;

class SDRdaemonFECInput : public SampleSource {
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

    class MsgConfigureSDRdaemonAutoFollowPolicy : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool autoFollowRate() const { return m_autoFollowRate; }
        bool autoCorrBuffer() const { return m_autoCorrBuffer; }

        static MsgConfigureSDRdaemonAutoFollowPolicy* create(bool autoFollowRate, bool autoCorrBuffer)
		{
            return new MsgConfigureSDRdaemonAutoFollowPolicy(autoFollowRate, autoCorrBuffer);
		}

	private:
		bool m_autoFollowRate;
        bool m_autoCorrBuffer;

        MsgConfigureSDRdaemonAutoFollowPolicy(bool autoFollowRate, bool autoCorrBuffer) :
			Message(),
            m_autoFollowRate(autoFollowRate),
            m_autoCorrBuffer(autoCorrBuffer)
		{ }
	};

    class MsgConfigureSDRdaemonResetIndexes : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        static MsgConfigureSDRdaemonResetIndexes* create()
        {
            return new MsgConfigureSDRdaemonResetIndexes();
        }

    private:
        MsgConfigureSDRdaemonResetIndexes() :
            Message()
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
		int getSampleRateStream() const { return m_sampleRateStream; }
		int getSampleRate() const { return m_sampleRate; }
		quint64 getCenterFrequency() const { return m_centerFrequency; }
		uint32_t get_tv_sec() const { return m_tv_sec; }
		uint32_t get_tv_usec() const { return m_tv_usec; }

		static MsgReportSDRdaemonStreamData* create(int sampleRateStream, int sampleRate, quint64 centerFrequency, uint32_t tv_sec, uint32_t tv_usec)
		{
			return new MsgReportSDRdaemonStreamData(sampleRateStream, sampleRate, centerFrequency, tv_sec, tv_usec);
		}

	protected:
		int m_sampleRateStream;
		int m_sampleRate;
		quint64 m_centerFrequency;
		uint32_t m_tv_sec;
		uint32_t m_tv_usec;

		MsgReportSDRdaemonStreamData(int sampleRateStream, int sampleRate, quint64 centerFrequency, uint32_t tv_sec, uint32_t tv_usec) :
			Message(),
			m_sampleRateStream(sampleRateStream),
			m_sampleRate(sampleRate),
			m_centerFrequency(centerFrequency),
			m_tv_sec(tv_sec),
			m_tv_usec(tv_usec)
		{ }
	};

	class MsgReportSDRdaemonFECStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		uint32_t get_tv_sec() const { return m_tv_sec; }
		uint32_t get_tv_usec() const { return m_tv_usec; }
		float getBufferLengthInSecs() const { return m_bufferLenSec; }
        int32_t getBufferGauge() const { return m_bufferGauge; }
        int getCurNbBlocks() const { return m_curNbBlocks; }
        int getCurNbRecovery() const { return m_curNbRecovery; }
        float getAvgNbBlocks() const { return m_avgNbBlocks; }
        float getAvgNbRecovery() const { return m_avgNbRecovery; }

		static MsgReportSDRdaemonFECStreamTiming* create(uint32_t tv_sec,
				uint32_t tv_usec,
				float bufferLenSec,
                int32_t bufferGauge,
                int curNbBlocks,
                int curNbRecovery,
                float avgNbBlocks,
                float avgNbRecovery)
		{
			return new MsgReportSDRdaemonFECStreamTiming(tv_sec,
					tv_usec,
					bufferLenSec,
                    bufferGauge,
                    curNbBlocks,
                    curNbRecovery,
                    avgNbBlocks,
                    avgNbRecovery);
		}

	protected:
		uint32_t m_tv_sec;
		uint32_t m_tv_usec;
		float    m_bufferLenSec;
        int32_t  m_bufferGauge;
        int      m_curNbBlocks;
        int      m_curNbRecovery;
        float    m_avgNbBlocks;
        float    m_avgNbRecovery;

		MsgReportSDRdaemonFECStreamTiming(uint32_t tv_sec,
				uint32_t tv_usec,
				float bufferLenSec,
                int32_t bufferGauge,
                int curNbBlocks,
                int curNbRecovery,
                float avgNbBlocks,
                float avgNbRecovery) :
			Message(),
			m_tv_sec(tv_sec),
			m_tv_usec(tv_usec),
			m_bufferLenSec(bufferLenSec),
            m_bufferGauge(bufferGauge),
            m_curNbBlocks(curNbBlocks),
            m_curNbRecovery(curNbRecovery),
            m_avgNbBlocks(avgNbBlocks),
            m_avgNbRecovery(avgNbRecovery)
		{ }
	};

	SDRdaemonFECInput(const QTimer& masterTimer, DeviceAPI *deviceAPI);
	virtual ~SDRdaemonFECInput();

	virtual bool init(const Message& message);
	virtual bool start(int device);
	virtual void stop();

	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;
	std::time_t getStartingTimeStamp() const;
	void getRemoteAddress(QString &s);

	virtual bool handleMessage(const Message& message);

private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	QString m_address;
	quint16 m_port;
	SDRdaemonFECUDPHandler* m_SDRdaemonUDPHandler;
	QString m_deviceDescription;
	int m_sampleRate;
	quint64 m_centerFrequency;
	std::time_t m_startingTimeStamp;
	const QTimer& m_masterTimer;
    bool m_autoFollowRate;
    bool m_autoCorrBuffer;
};

#endif // INCLUDE_SDRDAEMONFECINPUT_H
