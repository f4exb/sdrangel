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

#ifndef PLUGINS_CHANNELTX_FILESOURCE_FILESOURCEREPORT_H_
#define PLUGINS_CHANNELTX_FILESOURCE_FILESOURCEREPORT_H_

#include <QObject>
#include "util/message.h"

class FileSourceReport : public QObject
{
    Q_OBJECT
public:
	class MsgReportHeaderCRC : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isOK() const { return m_ok; }

		static MsgReportHeaderCRC* create(bool ok) {
			return new MsgReportHeaderCRC(ok);
		}

	protected:
		bool m_ok;

		MsgReportHeaderCRC(bool ok) :
			Message(),
			m_ok(ok)
		{ }
	};

	class MsgReportFileSourceStreamData : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getSampleRate() const { return m_sampleRate; }
		quint32 getSampleSize() const { return m_sampleSize; }
		quint64 getCenterFrequency() const { return m_centerFrequency; }
        quint64 getStartingTimeStamp() const { return m_startingTimeStamp; }
        quint64 getRecordLengthMuSec() const { return m_recordLengthMuSec; }

		static MsgReportFileSourceStreamData* create(int sampleRate,
		        quint32 sampleSize,
				quint64 centerFrequency,
                quint64 startingTimeStamp,
                quint64 recordLengthMuSec)
		{
			return new MsgReportFileSourceStreamData(sampleRate, sampleSize, centerFrequency, startingTimeStamp, recordLengthMuSec);
		}

	protected:
		int m_sampleRate;
		quint32 m_sampleSize;
		quint64 m_centerFrequency;
        quint64 m_startingTimeStamp;
        quint64 m_recordLengthMuSec;

		MsgReportFileSourceStreamData(int sampleRate,
		        quint32 sampleSize,
				quint64 centerFrequency,
                quint64 startingTimeStamp,
                quint64 recordLengthMuSec) :
			Message(),
			m_sampleRate(sampleRate),
			m_sampleSize(sampleSize),
			m_centerFrequency(centerFrequency),
			m_startingTimeStamp(startingTimeStamp),
			m_recordLengthMuSec(recordLengthMuSec)
		{ }
	};

	class MsgReportFileSourceStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
        quint64 getSamplesCount() const { return m_samplesCount; }

        static MsgReportFileSourceStreamTiming* create(quint64 samplesCount)
		{
			return new MsgReportFileSourceStreamTiming(samplesCount);
		}

	protected:
        quint64 m_samplesCount;

        MsgReportFileSourceStreamTiming(quint64 samplesCount) :
			Message(),
			m_samplesCount(samplesCount)
		{ }
	};

    class MsgPlayPause : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getPlayPause() const { return m_playPause; }

        static MsgPlayPause* create(bool playPause) {
            return new MsgPlayPause(playPause);
        }

    protected:
        bool m_playPause;

        MsgPlayPause(bool playPause) :
            Message(),
            m_playPause(playPause)
        { }
    };

    FileSourceReport();
    ~FileSourceReport();
};

#endif // PLUGINS_CHANNELTX_FILESOURCE_FILESOURCEREPORT_H_