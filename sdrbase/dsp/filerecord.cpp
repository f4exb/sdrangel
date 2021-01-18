///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB                              //
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

#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include <QDebug>
#include <QDateTime>

#include "dsp/dspcommands.h"
#include "util/simpleserializer.h"
#include "util/message.h"

#include "filerecord.h"

FileRecord::FileRecord() :
	FileRecordInterface(),
    m_fileBase("test"),
    m_sampleRate(0),
    m_centerFrequency(0),
	m_recordOn(false),
    m_recordStart(false),
    m_byteCount(0),
    m_msShift(0)
{
	setObjectName("FileRecord");
}

FileRecord::FileRecord(const QString& fileBase) :
    FileRecordInterface(),
    m_fileBase(fileBase),
    m_sampleRate(0),
    m_centerFrequency(0),
    m_recordOn(false),
    m_recordStart(false),
    m_byteCount(0)
{
    setObjectName("FileRecord");
}

FileRecord::~FileRecord()
{
    stopRecording();
}

void FileRecord::setFileName(const QString& fileBase)
{
    if (!m_recordOn)
    {
        m_fileBase = fileBase;
    }
}

void FileRecord::genUniqueFileName(uint deviceUID, int istream)
{
    if (istream < 0) {
        setFileName(QString("rec%1_%2.sdriq").arg(deviceUID).arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH_mm_ss_zzz")));
    } else {
        setFileName(QString("rec%1_%2_%3.sdriq").arg(deviceUID).arg(istream).arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH_mm_ss_zzz")));
    }
}

void FileRecord::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) positiveOnly;
    // if no recording is active, send the samples to /dev/null
    if(!m_recordOn)
        return;

    if (begin < end) // if there is something to put out
    {
        if (m_recordStart)
        {
            writeHeader();
            m_recordStart = false;
        }

        m_sampleFile.write(reinterpret_cast<const char*>(&*(begin)), (end - begin)*sizeof(Sample));
        m_byteCount += end - begin;
    }
}

void FileRecord::start()
{
}

void FileRecord::stop()
{
    stopRecording();
}

bool FileRecord::startRecording()
{
    if (m_recordOn) {
        stopRecording();
    }

    if (!m_sampleFile.is_open())
    {
    	qDebug() << "FileRecord::startRecording";
        m_curentFileName = QString("%1.%2.sdriq").arg(m_fileBase).arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH_mm_ss_zzz"));
        m_sampleFile.open(m_curentFileName.toStdString().c_str(), std::ios::binary);
        if (!m_sampleFile.is_open())
        {
            qWarning() << "FileRecord::startRecording: failed to open file: " << m_curentFileName;
            return false;
        }
        m_recordOn = true;
        m_recordStart = true;
        m_byteCount = 0;
    }
    return true;
}

bool FileRecord::stopRecording()
{
    if (m_sampleFile.is_open())
    {
    	qDebug() << "FileRecord::stopRecording";
        m_sampleFile.close();
        m_recordOn = false;
        m_recordStart = false;
        if (m_sampleFile.bad())
        {
            qWarning() << "FileRecord::stopRecording: an error occured while writing to " << m_curentFileName;
            return false;
        }
    }
    return true;
}

bool FileRecord::handleMessage(const Message& message)
{
	if (DSPSignalNotification::match(message))
	{
		DSPSignalNotification& notif = (DSPSignalNotification&) message;
		m_sampleRate = notif.getSampleRate();
		m_centerFrequency = notif.getCenterFrequency();
		qDebug() << "FileRecord::handleMessage: DSPSignalNotification: m_inputSampleRate: " << m_sampleRate
				<< " m_centerFrequency: " << m_centerFrequency;

        if (m_recordOn) {
            startRecording();
        }

        return true;
	}
    else
    {
        return false;
    }
}

void FileRecord::writeHeader()
{
    Header header;
    header.sampleRate = m_sampleRate;
    header.centerFrequency = m_centerFrequency;
    std::time_t ts = time(0);
    header.startTimeStamp = ts + (m_msShift / 1000);
    header.sampleSize = SDR_RX_SAMP_SZ;
    header.filler = 0;

    writeHeader(m_sampleFile, header);
}

bool FileRecord::readHeader(std::ifstream& sampleFile, Header& header)
{
    sampleFile.read((char *) &header, sizeof(Header));
    boost::crc_32_type crc32;
    crc32.process_bytes(&header, 28);
    return header.crc32 == crc32.checksum();
}

void FileRecord::writeHeader(std::ofstream& sampleFile, Header& header)
{
    boost::crc_32_type crc32;
    crc32.process_bytes(&header, 28);
    header.crc32 = crc32.checksum();
    sampleFile.write((const char *) &header, sizeof(Header));
}
