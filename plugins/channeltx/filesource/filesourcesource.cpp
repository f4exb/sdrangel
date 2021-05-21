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

#include "filesourcesource.h"
#include "filesourcereport.h"

#if (defined _WIN32_) || (defined _MSC_VER)
#include "windows_time.h"
#include <stdint.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#include <QDebug>

#include "dsp/dspcommands.h"
#include "dsp/devicesamplesink.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/filerecord.h"
#include "dsp/wavfilerecord.h"
#include "util/db.h"

FileSourceSource::FileSourceSource() :
	m_fileName("..."),
	m_sampleSize(0),
	m_centerFrequency(0),
    m_frequencyOffset(0),
    m_fileSampleRate(0),
    m_samplesCount(0),
	m_sampleRate(0),
    m_deviceSampleRate(0),
	m_recordLengthMuSec(0),
    m_startingTimeStamp(0),
    m_running(false),
    m_guiMessageQueue(nullptr)
{
    m_linearGain = 1.0f;
	m_magsq = 0.0f;
	m_magsqSum = 0.0f;
	m_magsqPeak = 0.0f;
	m_magsqCount = 0;
}

FileSourceSource::~FileSourceSource()
{
}

void FileSourceSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void FileSourceSource::pullOne(Sample& sample)
{
    Real re;
    Real im;

    struct Sample16
    {
        int16_t real;
        int16_t imag;
    };

    struct Sample24
    {
        int32_t real;
        int32_t imag;
    };

    if (!m_running)
    {
        re = 0;
        im = 0;
    }
	else if (m_sampleSize == 16)
	{
        Sample16 sample16;
        m_ifstream.read(reinterpret_cast<char*>(&sample16), sizeof(Sample16));

        if (m_ifstream.eof()) {
            handleEOF();
        } else {
            m_samplesCount++;
        }

        // scale to +/-1.0
        re = (sample16.real * m_linearGain) / 32760.0f;
        im = (sample16.imag * m_linearGain) / 32760.0f;
    }
    else if (m_sampleSize == 24)
    {
        Sample24 sample24;
        m_ifstream.read(reinterpret_cast<char*>(&sample24), sizeof(Sample24));

        if (m_ifstream.eof()) {
            handleEOF();
        } else {
            m_samplesCount++;
        }

        // scale to +/-1.0
        re = (sample24.real * m_linearGain) / 8388608.0f;
        im = (sample24.imag * m_linearGain) / 8388608.0f;
    }
    else
    {
        re = 0;
        im = 0;
    }


    if (SDR_TX_SAMP_SZ == 16)
    {
        sample.setReal(re * 32768.0f);
        sample.setImag(im * 32768.0f);
    }
    else if (SDR_TX_SAMP_SZ == 24)
    {
        sample.setReal(re * 8388608.0f);
        sample.setImag(im * 8388608.0f);
    }
    else
    {
        sample.setReal(0);
        sample.setImag(0);
    }

    Real magsq = re*re + im*im;
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;

    if (magsq > m_magsqPeak) {
        m_magsqPeak = magsq;
    }

    m_magsqCount++;
}

void FileSourceSource::openFileStream(const QString& fileName)
{
	m_fileName = fileName;

	if (m_ifstream.is_open()) {
		m_ifstream.close();
	}

#ifdef Q_OS_WIN
	m_ifstream.open(m_fileName.toStdWString().c_str(), std::ios::binary | std::ios::ate);
#else
	m_ifstream.open(m_fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
#endif
	quint64 fileSize = m_ifstream.tellg();
    m_samplesCount = 0;

    if (m_fileName.endsWith(".wav"))
    {
        WavFileRecord::Header header;
        m_ifstream.seekg(0, std::ios_base::beg);
        bool headerOK = WavFileRecord::readHeader(m_ifstream, header);
        m_fileSampleRate = header.m_sampleRate;
        if (header.m_auxiHeader.m_size > 0)
        {
            // Some WAV files written by SDR tools have auxi header
            m_centerFrequency = header.m_auxi.m_centerFreq;
            m_startingTimeStamp = header.getStartTime().toMSecsSinceEpoch() / 1000;
        }
        else
        {
            // Attempt to extract start time and frequency from filename
            QDateTime startTime;
            if (WavFileRecord::getStartTime(m_fileName, startTime)) {
                m_startingTimeStamp = startTime.toMSecsSinceEpoch() / 1000;
            }
            WavFileRecord::getCenterFrequency(m_fileName, m_centerFrequency);
        }
        m_sampleSize = header.m_bitsPerSample;

        if (headerOK && (m_fileSampleRate > 0) && (m_sampleSize > 0))
        {
            m_recordLengthMuSec = ((fileSize - m_ifstream.tellg()) * 1000000UL) / ((m_sampleSize == 24 ? 8 : 4) * m_fileSampleRate);
        }
        else
        {
            qCritical("FileSourceSource::openFileStream: invalid .wav file");
            m_recordLengthMuSec = 0;
        }

        if (getMessageQueueToGUI())
        {
            FileSourceReport::MsgReportHeaderCRC *report = FileSourceReport::MsgReportHeaderCRC::create(headerOK);
            getMessageQueueToGUI()->push(report);
        }
    }
    else if (fileSize > sizeof(FileRecord::Header))
	{
	    FileRecord::Header header;
	    m_ifstream.seekg(0,std::ios_base::beg);
		bool crcOK = FileRecord::readHeader(m_ifstream, header);
		m_fileSampleRate = header.sampleRate;
		m_centerFrequency = header.centerFrequency;
		m_startingTimeStamp = header.startTimeStamp;
		m_sampleSize = header.sampleSize;
		QString crcHex = QString("%1").arg(header.crc32 , 0, 16);

	    if (crcOK)
	    {
	        qDebug("FileSourceSource::openFileStream: CRC32 OK for header: %s", qPrintable(crcHex));
	        m_recordLengthMuSec = ((fileSize - sizeof(FileRecord::Header)) * 1000000UL) / ((m_sampleSize == 24 ? 8 : 4) * m_fileSampleRate);
	    }
	    else
	    {
	        qCritical("FileSourceSource::openFileStream: bad CRC32 for header: %s", qPrintable(crcHex));
	        m_recordLengthMuSec = 0;
	    }

		if (getMessageQueueToGUI())
        {
			FileSourceReport::MsgReportHeaderCRC *report = FileSourceReport::MsgReportHeaderCRC::create(crcOK);
			getMessageQueueToGUI()->push(report);
		}
	}
	else
	{
		m_recordLengthMuSec = 0;
	}

	qDebug() << "FileSourceSource::openFileStream: " << m_fileName.toStdString().c_str()
			<< " fileSize: " << fileSize << " bytes"
			<< " length: " << m_recordLengthMuSec << " microseconds"
			<< " sample rate: " << m_fileSampleRate << " S/s"
			<< " center frequency: " << m_centerFrequency << " Hz"
			<< " sample size: " << m_sampleSize << " bits"
            << " starting TS: " << m_startingTimeStamp << "s";

	if (getMessageQueueToGUI())
    {
	    FileSourceReport::MsgReportFileSourceStreamData *report = FileSourceReport::MsgReportFileSourceStreamData::create(m_fileSampleRate,
	            m_sampleSize,
	            m_centerFrequency,
	            m_startingTimeStamp,
	            m_recordLengthMuSec); // file stream data
	    getMessageQueueToGUI()->push(report);
	}

	if (m_recordLengthMuSec == 0) {
	    m_ifstream.close();
	}
}

void FileSourceSource::seekFileStream(int seekMillis)
{
	if ((m_ifstream.is_open()) && !m_running)
	{
        quint64 seekPoint = ((m_recordLengthMuSec * seekMillis) / 1000) * m_fileSampleRate;
        seekPoint /= 1000000UL;
        m_samplesCount = seekPoint;
        seekPoint *= (m_sampleSize == 24 ? 8 : 4); // + sizeof(FileRecord::Header)
		m_ifstream.clear();
		m_ifstream.seekg(seekPoint + sizeof(FileRecord::Header), std::ios::beg);
	}
}

void FileSourceSource::handleEOF()
{
    if (!m_ifstream.is_open()) {
        return;
    }

    if (getMessageQueueToGUI())
    {
        FileSourceReport::MsgReportFileSourceStreamTiming *report = FileSourceReport::MsgReportFileSourceStreamTiming::create(getSamplesCount());
        getMessageQueueToGUI()->push(report);
    }

    if (m_settings.m_loop)
    {
        m_ifstream.clear();
        m_ifstream.seekg(0, std::ios::beg);
        m_samplesCount = 0;
    }
    else
    {
        if (getMessageQueueToGUI())
        {
            FileSourceReport::MsgPlayPause *report = FileSourceReport::MsgPlayPause::create(false);
            getMessageQueueToGUI()->push(report);
        }
    }
}

void FileSourceSource::applySettings(const FileSourceSettings& settings, bool force)
{
    qDebug() << "FileSourceSource::applySettings:"
        << "m_fileName:" << settings.m_fileName
        << "m_loop:" << settings.m_loop
        << "m_gainDB:" << settings.m_gainDB
        << "m_log2Interp:" << settings.m_log2Interp
        << "m_filterChainHash:" << settings.m_filterChainHash
        << " force: " << force;


    if ((m_settings.m_gainDB != settings.m_gainDB) || force) {
        m_linearGain = CalcDb::powerFromdB(settings.m_gainDB/2.0); // Divide by two for power gain to voltage gain conversion
    }

    m_settings = settings;
}
