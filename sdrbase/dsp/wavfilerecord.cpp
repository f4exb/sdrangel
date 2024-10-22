///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>               //
// Copyright (C) 2022-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2022 CRD716 <crd716@gmail.com>                                  //
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

#include <cstddef>
#include <cstring>

#include <QDebug>
#include <QDateTime>
#include <QRegularExpression>

#include "dsp/dspcommands.h"
#include "util/message.h"

#include "wavfilerecord.h"

WavFileRecord::WavFileRecord(quint32 sampleRate, quint64 centerFrequency) :
    FileRecordInterface(),
    m_fileBase("test"),
    m_fileBaseIsFileName(false),
    m_sampleRate(sampleRate),
    m_centerFrequency(centerFrequency),
    m_recordOn(false),
    m_recordStart(false),
    m_byteCount(0),
    m_msShift(0),
    m_nbChannels(2)
{
    setObjectName("WavFileRecord");
}

WavFileRecord::WavFileRecord(const QString& fileBase) :
    FileRecordInterface(),
    m_fileBase(fileBase),
    m_fileBaseIsFileName(false),
    m_sampleRate(0),
    m_centerFrequency(0),
    m_recordOn(false),
    m_recordStart(false),
    m_byteCount(0),
    m_nbChannels(2)
{
    setObjectName("WavFileRecord");
}

WavFileRecord::~WavFileRecord()
{
    stopRecording();
}

void WavFileRecord::setFileName(const QString& fileBase)
{
    if (!m_recordOn)
    {
        m_fileBase = fileBase;
    }
}

void WavFileRecord::genUniqueFileName(uint deviceUID, int istream)
{
    if (istream < 0) {
        setFileName(QString("rec%1_%2.wav").arg(deviceUID).arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH_mm_ss_zzz")));
    } else {
        setFileName(QString("rec%1_%2_%3.wav").arg(deviceUID).arg(istream).arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH_mm_ss_zzz")));
    }
}

void WavFileRecord::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) positiveOnly;

    if (!m_recordOn) {
        return;
    }

    if (begin < end) // if there is something to put out
    {
        if (m_recordStart)
        {
            writeHeader();
            m_recordStart = false;
        }

        if (SDR_RX_SAMP_SZ == 16)
        {
            m_sampleFile.write(reinterpret_cast<const char*>(&*(begin)), (end - begin)*sizeof(Sample));
            m_byteCount += end - begin;
        }
        else
        {
            for (SampleVector::const_iterator it = begin; it != end; ++it)
            {
                // Convert from 24-bit to 16-bit
                int16_t samples[2];
                samples[0] = std::min(32767, std::max(it->real() >> 8, -32768));
                samples[1] = std::min(32767, std::max(it->imag() >> 8, -32768));
                m_sampleFile.write(reinterpret_cast<const char*>(&samples), 4);
                m_byteCount += 4;
            }
        }
    }
}

void WavFileRecord::write(qint16 lSample, qint16 rSample)
{
    if (m_recordStart)
    {
        writeHeader();
        m_recordStart = false;
    }

    m_sampleFile.write(reinterpret_cast<const char*>(&lSample), 2);
    m_sampleFile.write(reinterpret_cast<const char*>(&rSample), 2);
    m_byteCount += 4;
}

void WavFileRecord::writeMono(qint16 sample)
{
    if (m_recordStart)
    {
        writeHeader();
        m_recordStart = false;
    }

    m_sampleFile.write(reinterpret_cast<const char*>(&sample), 2);
    m_byteCount += 2;
}

void WavFileRecord::writeMono(qint16 *samples, int nbSamples)
{
    if (m_recordStart)
    {
        writeHeader();
        m_recordStart = false;
    }

    m_sampleFile.write(reinterpret_cast<const char*>(samples), 2*nbSamples);
    m_byteCount += 2*nbSamples;
}

void WavFileRecord::start()
{
}

void WavFileRecord::stop()
{
    stopRecording();
}

bool WavFileRecord::startRecording()
{
    if (m_recordOn) {
        stopRecording();
    }

#ifdef ANDROID
    if (!m_sampleFile.isOpen())
#else
    if (!m_sampleFile.is_open())
#endif
    {
        qDebug() << "WavFileRecord::startRecording";
#ifdef ANDROID
        // FIXME: No idea how to write to a file where the filename doesn't come from the file picker
        m_currentFileName = m_fileBase + ".wav";
        m_sampleFile.setFileName(m_currentFileName);
        if (!m_sampleFile.open(QIODevice::ReadWrite))
        {
            qWarning() << "WavFileRecord::startRecording: failed to open file: " << m_currentFileName << " error " << m_sampleFile.error();
            return false;
        }
#else
        if (m_fileBaseIsFileName) {
            m_currentFileName = m_fileBase + ".wav";
        } else {
            m_currentFileName = m_fileBase + "_" + QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH_mm_ss_zzz") + ".wav"; // Don't use QString::arg on Android, as filename can contain %2
        }
        m_sampleFile.open(m_currentFileName.toStdString().c_str(), std::ios::binary);
        if (!m_sampleFile.is_open())
        {
            qWarning() << "WavFileRecord::startRecording: failed to open file: " << m_currentFileName;
            return false;
        }
#endif
        m_recordOn = true;
        m_recordStart = true;
        m_byteCount = 0;
    }
    return true;
}

bool WavFileRecord::stopRecording()
{
#ifdef ANDROID
    if (m_sampleFile.isOpen())
#else
    if (m_sampleFile.is_open())
#endif
    {
        qDebug() << "WavFileRecord::stopRecording";
        // Fix up chunk sizes
#ifdef ANDROID
        long fileSize = (long)m_sampleFile.size();
        m_sampleFile.seek(offsetof(Header, m_riffHeader.m_size));
#else
        long fileSize = m_sampleFile.tellp();
        m_sampleFile.seekp(offsetof(Header, m_riffHeader.m_size));
#endif
        qint32 size = (fileSize - 8);
        m_sampleFile.write((char *)&size, 4);
#ifdef ANDROID
        m_sampleFile.seek(offsetof(Header, m_dataHeader.m_size));
#else
        m_sampleFile.seekp(offsetof(Header, m_dataHeader.m_size));
#endif
        size = fileSize - sizeof(Header);
        m_sampleFile.write((char *)&size, 4);
        m_sampleFile.close();
        m_recordOn = false;
        m_recordStart = false;
#ifdef ANDROID
#else
        if (m_sampleFile.bad())
        {
            qWarning() << "WavFileRecord::stopRecording: an error occurred while writing to " << m_currentFileName;
            return false;
        }
#endif
    }
    return true;
}

bool WavFileRecord::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;

        int sampleRate = notif.getSampleRate();
        if ((sampleRate != (int)m_sampleRate) && m_recordOn) {
            qDebug() << "WavFileRecord::handleMessage: sample rate has changed. Creating a new .wav file";
            stopRecording();
            m_recordOn = true;
        }

        m_sampleRate = sampleRate;
        m_centerFrequency = notif.getCenterFrequency();
        qDebug() << "WavFileRecord::handleMessage: DSPSignalNotification: m_inputSampleRate: " << m_sampleRate
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

void WavFileRecord::writeHeader()
{
    Header header;
    header.m_riffHeader.m_id[0] = 'R';
    header.m_riffHeader.m_id[1] = 'I';
    header.m_riffHeader.m_id[2] = 'F';
    header.m_riffHeader.m_id[3] = 'F';
    header.m_riffHeader.m_size = 0; // Needs to be fixed on close
    header.m_type[0] = 'W';
    header.m_type[1] = 'A';
    header.m_type[2] = 'V';
    header.m_type[3] = 'E';
    header.m_fmtHeader.m_id[0] = 'f';
    header.m_fmtHeader.m_id[1] = 'm';
    header.m_fmtHeader.m_id[2] = 't';
    header.m_fmtHeader.m_id[3] = ' ';
    header.m_fmtHeader.m_size = 16;
    header.m_audioFormat = 1; // Linear PCM
    header.m_numChannels = m_nbChannels; // 2 for I/Q
    header.m_sampleRate = m_sampleRate;
    // We always use 16-bits regardless of SDR_RX_SAMP_SZ
    header.m_byteRate = m_sampleRate * m_nbChannels * 16 / 8;
    header.m_blockAlign = m_nbChannels * 16 / 8;
    header.m_bitsPerSample = 16;

    header.m_auxiHeader.m_id[0] = 'a';
    header.m_auxiHeader.m_id[1] = 'u';
    header.m_auxiHeader.m_id[2] = 'x';
    header.m_auxiHeader.m_id[3] = 'i';
    header.m_auxiHeader.m_size = sizeof(Auxi);
    QDateTime now = QDateTime::currentDateTime();
    header.m_auxi.m_startTime.m_year = now.date().year();
    header.m_auxi.m_startTime.m_month = now.date().month();
    header.m_auxi.m_startTime.m_dayOfWeek = now.date().dayOfWeek();
    header.m_auxi.m_startTime.m_day = now.date().day();
    header.m_auxi.m_startTime.m_hour = now.time().hour();
    header.m_auxi.m_startTime.m_minute = now.time().minute();
    header.m_auxi.m_startTime.m_second = now.time().second();
    header.m_auxi.m_startTime.m_milliseconds = now.time().msec();
    header.m_auxi.m_stopTime.m_year = 0; // Needs to be fixed on close
    header.m_auxi.m_stopTime.m_month = 0;
    header.m_auxi.m_stopTime.m_dayOfWeek = 0;
    header.m_auxi.m_stopTime.m_day = 0;
    header.m_auxi.m_stopTime.m_hour = 0;
    header.m_auxi.m_stopTime.m_minute = 0;
    header.m_auxi.m_stopTime.m_second = 0;
    header.m_auxi.m_stopTime.m_milliseconds = 0;
    header.m_auxi.m_centerFreq = m_centerFrequency;
    header.m_auxi.m_adFrequency = m_sampleRate;
    header.m_auxi.m_ifFrequency = 0;
    header.m_auxi.m_bandwidth = 0;
    header.m_auxi.m_iqOffset = 0;
    header.m_auxi.m_unused2 = 0;
    header.m_auxi.m_unused3 = 0;
    header.m_auxi.m_unused4 = 0;
    header.m_auxi.m_unused5 = 0;
    memset(&header.m_auxi.m_nextFilename[0], 0, 96);

    header.m_dataHeader.m_size = sizeof(Auxi);
    header.m_dataHeader.m_id[0] = 'd';
    header.m_dataHeader.m_id[1] = 'a';
    header.m_dataHeader.m_id[2] = 't';
    header.m_dataHeader.m_id[3] = 'a';
    header.m_dataHeader.m_size = 0; // Needs to be fixed on close

    writeHeader(m_sampleFile, header);
}

bool WavFileRecord::readHeader(std::ifstream& sampleFile, Header& header, bool check)
{
    memset(&header, 0, sizeof(Header));

    sampleFile.read((char *) &header, 8+4+8+16);
    if (!sampleFile)
    {
        qDebug() << "WavFileRecord::readHeader: End of file without reading header";
        return false;
    }

    if (check && !checkHeader(header)) {
        return false;
    }

    Chunk chunkHeader;
    bool gotData = false;
    while (!gotData)
    {
        sampleFile.read((char *) &chunkHeader, 8);
        if (!sampleFile)
        {
            qDebug() << "WavFileRecord::readHeader: End of file without reading data header";
            return false;
        }

        if (!strncmp(chunkHeader.m_id, "auxi", 4))
        {
            memcpy(&header.m_auxiHeader, &chunkHeader, sizeof(Chunk));
            sampleFile.read((char *) &header.m_auxi, sizeof(Auxi));
            if (!sampleFile)
                return false;
        }
        else if (!strncmp(chunkHeader.m_id, "data", 4))
        {
            memcpy(&header.m_dataHeader, &chunkHeader, sizeof(Chunk));
            gotData = true;
        }
    }

    return true;
}

bool WavFileRecord::readHeader(QFile& sampleFile, Header& header)
{
    memset(&header, 0, sizeof(Header));

    sampleFile.read((char *) &header, 8+4+8+16);

    if (!checkHeader(header)) {
        return false;
    }

    Chunk chunkHeader;
    bool gotData = false;
    while (!gotData)
    {
        if (sampleFile.read((char *) &chunkHeader, 8) != 8)
        {
            qDebug() << "WavFileRecord::readHeader: End of file without reading data header";
            return false;
        }

        if (!strncmp(chunkHeader.m_id, "auxi", 4))
        {
            memcpy(&header.m_auxiHeader, &chunkHeader, sizeof(Chunk));
            if (sampleFile.read((char *) &header.m_auxi, sizeof(Auxi)) != sizeof(Auxi)) {
                return false;
            }
        }
        else if (!strncmp(chunkHeader.m_id, "data", 4))
        {
            memcpy(&header.m_dataHeader, &chunkHeader, sizeof(Chunk));
            gotData = true;
        }
    }

    return true;
}

bool WavFileRecord::checkHeader(Header& header)
{
    if (strncmp(header.m_riffHeader.m_id, "RIFF", 4))
    {
        qDebug() << "WavFileRecord::readHeader: No RIFF header";
        return false;
    }
    if (strncmp(header.m_type, "WAVE", 4))
    {
        qDebug() << "WavFileRecord::readHeader: No WAVE header";
        return false;
    }
    if (strncmp(header.m_fmtHeader.m_id, "fmt ", 4))
    {
        qDebug() << "WavFileRecord::readHeader: No fmt header";
        return false;
    }
    if (header.m_audioFormat != 1)
    {
        qDebug() << "WavFileRecord::readHeader: Audio format is not PCM";
        return false;
    }
    if (header.m_numChannels != 2)
    {
        qDebug() << "WavFileRecord::readHeader: Number of channels is not 2";
        return false;
    }
    // FileInputWorker can't handle other bits sizes
    if (header.m_bitsPerSample != 16)
    {
        qDebug() << "WavFileRecord::readHeader: Number of bits per sample is not 16";
        return false;
    }
    return true;
}

void WavFileRecord::writeHeader(std::ofstream& sampleFile, Header& header)
{
    sampleFile.write((const char *) &header, sizeof(Header));
}

void WavFileRecord::writeHeader(QFile& sampleFile, Header& header)
{
    sampleFile.write((const char *) &header, sizeof(Header));
}

bool WavFileRecord::getCenterFrequency(QString fileName, quint64& centerFrequency)
{
    // Attempt to extract center frequency from filename
    QRegularExpression freqkRE("(([0-9]+)kHz)");
    QRegularExpression freqRE("(([0-9]+)Hz)");
    QRegularExpressionMatch freqkREMatch = freqkRE.match(fileName);
    QRegularExpressionMatch freqREMatch = freqRE.match(fileName);

    if (freqkREMatch.hasMatch())
    {
        centerFrequency = freqkREMatch.capturedTexts()[2].toLongLong() * 1000LL;
        return true;
    }
    else if (freqREMatch.hasMatch())
    {
        centerFrequency = freqREMatch.capturedTexts()[2].toLongLong();
        return true;
    }
    return false;
}

bool WavFileRecord::getStartTime(QString fileName, QDateTime& startTime)
{
    // Attempt to extract start time from filename
    QRegularExpression dateTimeRE("([12][0-9][0-9][0-9]).?([01][0-9]).?([0-3][0-9]).?([0-2][0-9]).?([0-5][0-9]).?([0-5][0-9])");
    QRegularExpressionMatch match = dateTimeRE.match(fileName);
    if (match.hasMatch())
    {
        startTime = QDateTime(QDate(
                                  match.capturedTexts()[1].toInt(),
                                  match.capturedTexts()[2].toInt(),
                                  match.capturedTexts()[3].toInt()),
                              QTime(
                                  match.capturedTexts()[4].toInt(),
                                  match.capturedTexts()[5].toInt(),
                                  match.capturedTexts()[6].toInt()));
        return true;
    }
    return false;
}

QDateTime WavFileRecord::Header::getStartTime() const
{
    return QDateTime(QDate(m_auxi.m_startTime.m_year,
                           m_auxi.m_startTime.m_month,
                           m_auxi.m_startTime.m_day),
                     QTime(m_auxi.m_startTime.m_hour,
                           m_auxi.m_startTime.m_minute,
                           m_auxi.m_startTime.m_second,
                           m_auxi.m_startTime.m_milliseconds));
}
