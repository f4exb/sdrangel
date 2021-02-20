///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// File recorder in SigMF format single channel for SI plugins                   //
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

#include <QCoreApplication>
#include <QSysInfo>
#include <QFile>
#include <QDebug>

#include "libsigmf/sigmf_core_generated.h"
#include "libsigmf/sigmf_sdrangel_generated.h"
#include "libsigmf/sigmf.h"

#include "dsp/dspcommands.h"
//#include "util/sha512.h" - SHA512 is skipped because it takes too long

#include "sigmffilerecord.h"

SigMFFileRecord::SigMFFileRecord() :
	FileRecordInterface(),
    m_fileName("test"),
    m_sampleRate(0),
    m_centerFrequency(0),
    m_msShift(0),
	m_recordOn(false),
    m_recordStart(true),
    m_sampleStart(0),
    m_sampleCount(0),
    m_initialMsCount(0),
    m_initialBytesCount(0)
{
    qDebug("SigMFFileRecord::SigMFFileRecord: test");
	setObjectName("SigMFFileSink");
    m_metaRecord = new sigmf::SigMF<sigmf::Global<core::DescrT, sdrangel::DescrT>,
            sigmf::Capture<core::DescrT, sdrangel::DescrT>,
            sigmf::Annotation<core::DescrT> >();
}

SigMFFileRecord::SigMFFileRecord(const QString& fileName, const QString& hardwareId) :
    FileRecordInterface(),
    m_hardwareId(hardwareId),
    m_fileName(fileName),
    m_sampleRate(0),
    m_centerFrequency(0),
    m_recordOn(false),
    m_recordStart(true),
    m_sampleStart(0),
    m_sampleCount(0),
    m_initialMsCount(0),
    m_initialBytesCount(0)
{
    qDebug("SigMFFileRecord::SigMFFileRecord: %s", qPrintable(fileName));
    setObjectName("SigMFFileSink");
    m_metaRecord = new sigmf::SigMF<sigmf::Global<core::DescrT, sdrangel::DescrT>,
            sigmf::Capture<core::DescrT, sdrangel::DescrT>,
            sigmf::Annotation<core::DescrT> >();
}

SigMFFileRecord::~SigMFFileRecord()
{
    qDebug("SigMFFileRecord::~SigMFFileRecord");

    stopRecording();

    if (m_metaFile.is_open()) {
        m_metaFile.close();
    }

    if (m_sampleFile.is_open()) {
        m_sampleFile.close();
    }

    delete m_metaRecord;
}

void SigMFFileRecord::setFileName(const QString& fileName)
{
    if (!m_recordOn)
    {
        qDebug("SigMFFileRecord::setFileName: %s", qPrintable(fileName));

        if (m_metaFile.is_open()) {
            m_metaFile.close();
        }

        if (m_sampleFile.is_open()) {
            m_sampleFile.close();
        }

        m_fileName = fileName;

        if (QFile::exists(fileName + ".sigmf-data") && QFile::exists(fileName + ".sigmf-meta"))
        {
            m_metaFileName = m_fileName + ".sigmf-meta";
            std::ifstream metaStream;
#ifdef Q_OS_WIN
        	metaStream.open(m_metaFileName.toStdWString().c_str());
#else
        	metaStream.open(m_metaFileName.toStdString().c_str());
#endif
            std::ostringstream meta_buffer;
            meta_buffer << metaStream.rdbuf();
            try
            {
                from_json(json::parse(meta_buffer.str()), *m_metaRecord);
                metaStream.close();
                std::string sdrAngelVersion = m_metaRecord->global.access<sdrangel::GlobalT>().version;

                if (sdrAngelVersion.size() != 0)
                {
                    qDebug("SigMFFileRecord::setFileName: appending mode");
                    m_metaFile.open(m_metaFileName.toStdString().c_str(), std::ofstream::out);
                    m_initialMsCount = 0;

                    for (auto capture : m_metaRecord->captures)
                    {
                        uint64_t length = capture.get<core::DescrT>().length;
                        int32_t sampleRate = capture.get<sdrangel::DescrT>().sample_rate;
                        m_initialMsCount += (length * 1000) / sampleRate;
                    }

                    m_sampleFileName = m_fileName + ".sigmf-data";
                    m_sampleFile.open(m_sampleFileName.toStdString().c_str(), std::ios::binary & std::ios::app);
                    m_initialBytesCount = (uint64_t) m_sampleFile.tellp();
                    m_sampleStart =  m_initialBytesCount / sizeof(Sample);

                    m_recordStart = false;
                }
                else
                {
                    qDebug("SigMFFileRecord::setFileName: SigMF not recorded with SDRangel. Recreating files...");
                    m_initialBytesCount = 0;
                    m_initialMsCount = 0;
                    m_recordStart = true;
                }
            }
            catch(const std::exception& e)
            {
                qInfo("SigMFFileRecord::setFileName: exception: %s. Recreating files...", qPrintable(e.what()));
                m_initialBytesCount = 0;
                m_initialMsCount = 0;
                m_recordStart = true;
            }
        }
        else
        {
            m_initialBytesCount = 0;
            m_initialMsCount = 0;
            m_recordStart = true;
        }
    }
}

unsigned int SigMFFileRecord::getNbCaptures() const
{
    return m_metaRecord->captures.size();
}

bool SigMFFileRecord::startRecording()
{
    bool success = true;

    if (m_recordStart)
    {
      	qDebug("SigMFFileRecord::startRecording: new record %s", qPrintable(m_fileName));
        clearMeta();
        m_sampleStart = 0;
        m_sampleFileName = m_fileName + ".sigmf-data";
        m_metaFileName = m_fileName + ".sigmf-meta";
        m_sampleFile.open(m_sampleFileName.toStdString().c_str(), std::ios::binary);
        if (!m_sampleFile.is_open())
        {
            qWarning() << "SigMFFileRecord::startRecording: failed to open file: " << m_sampleFileName;
            success = false;
        }
        m_metaFile.open(m_metaFileName.toStdString().c_str(), std::ofstream::out);
        if (!m_metaFile.is_open())
        {
            qWarning() << "SigMFFileRecord::startRecording: failed to open file: " << m_metaFileName;
            success = false;
        }
        makeHeader();
        m_recordStart = false;
    }
    else
    {
        qDebug("SigMFFileRecord::startRecording: start new capture");
    }

    m_captureStartDT = QDateTime::currentDateTimeUtc().addMSecs(m_msShift);
    m_recordOn = true;
    m_sampleCount = 0;
    return success;
}

bool SigMFFileRecord::stopRecording()
{
    if (m_recordOn)
    {
      	qDebug("SigMFFileRecord::stopRecording: file previous capture");
        makeCapture();
        m_recordOn = false;
        if (m_sampleFile.bad())
        {
            qWarning() << "SigMFFileRecord::stopRecording: an error occurred while writing to " << m_sampleFileName;
            return false;
        }
        if (m_metaFile.bad())
        {
            qWarning() << "SigMFFileRecord::stopRecording: an error occurred while writing to " << m_metaFileName;
            return false;
        }
    }
    return true;
}

void SigMFFileRecord::makeHeader()
{
    m_metaRecord->global.access<core::GlobalT>().author = "SDRangel";
    m_metaRecord->global.access<core::GlobalT>().description = "SDRangel SigMF I/Q recording file";
    m_metaRecord->global.access<core::GlobalT>().sample_rate = m_sampleRate;
    m_metaRecord->global.access<core::GlobalT>().hw = m_hardwareId.toStdString();
    m_metaRecord->global.access<core::GlobalT>().recorder = QString(QCoreApplication::applicationName()).toStdString();
    m_metaRecord->global.access<core::GlobalT>().version = "0.0.2";
    m_metaRecord->global.access<sdrangel::GlobalT>().version = QString(QCoreApplication::applicationVersion()).toStdString();
    m_metaRecord->global.access<sdrangel::GlobalT>().qt_version = QT_VERSION_STR;
    m_metaRecord->global.access<sdrangel::GlobalT>().rx_bits = SDR_RX_SAMP_SZ;
    m_metaRecord->global.access<sdrangel::GlobalT>().arch = QString(QSysInfo::currentCpuArchitecture()).toStdString();
    m_metaRecord->global.access<sdrangel::GlobalT>().os = QString(QSysInfo::prettyProductName()).toStdString();
    QString endianSuffix = QSysInfo::ByteOrder == QSysInfo::LittleEndian ? "le" : "be";
    int size = 8*sizeof(FixReal);
    m_metaRecord->global.access<core::GlobalT>().datatype = QString("ci%1_%2").arg(size).arg(endianSuffix).toStdString();
}

void SigMFFileRecord::makeCapture()
{
    if (m_sampleCount)
    {
        qDebug("SigMFFileRecord::makeCapture: m_sampleStart: %llu m_sampleCount: %llu", m_sampleStart, m_sampleCount);
        // Flush samples to disk
        m_sampleFile.flush();
        // calculate SHA512 and write it to header
        // m_metaRecord->global.access<core::GlobalT>().sha512 = sw::sha512::file(m_sampleFileName.toStdString()); // skip takes too long
        // Add new capture
        auto recording_capture = sigmf::Capture<core::DescrT, sdrangel::DescrT>();
        recording_capture.get<core::DescrT>().frequency = m_centerFrequency;
        recording_capture.get<core::DescrT>().sample_start = m_sampleStart;
        recording_capture.get<core::DescrT>().length = m_sampleCount;
        recording_capture.get<core::DescrT>().datetime = m_captureStartDT.toString("yyyy-MM-ddTHH:mm:ss.zzzZ").toStdString();
        recording_capture.get<sdrangel::DescrT>().sample_rate = m_sampleRate;
        recording_capture.get<sdrangel::DescrT>().tsms = m_captureStartDT.toMSecsSinceEpoch();
        m_metaRecord->captures.emplace_back(recording_capture);
        m_sampleStart += m_sampleCount;
        // Flush meta to disk
        m_metaFile.seekp(0);
        std::string jsonRecord = json(*m_metaRecord).dump(2);
        m_metaFile << jsonRecord;
        m_metaFile.flush();
    }
    else
    {
        qDebug("SigMFFileRecord::makeCapture: skipped because of no samples");
    }
}

void SigMFFileRecord::clearMeta()
{
    m_metaRecord->captures.clear();
}

void SigMFFileRecord::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) positiveOnly;
    // if no recording is active, send the samples to /dev/null
    if(!m_recordOn)
        return;

    if (begin < end) // if there is something to put out
    {
        m_sampleFile.write(reinterpret_cast<const char*>(&*(begin)), (end - begin)*sizeof(Sample));
        m_sampleCount += end - begin;
    }
}

void SigMFFileRecord::start()
{
}

void SigMFFileRecord::stop()
{
    stopRecording();
}

bool SigMFFileRecord::handleMessage(const Message& message)
{
	if (DSPSignalNotification::match(message))
	{
        if (m_recordOn) {
            makeCapture();
            m_captureStartDT = QDateTime::currentDateTimeUtc();
            m_sampleCount = 0;
        }

		DSPSignalNotification& notif = (DSPSignalNotification&) message;
		m_sampleRate = notif.getSampleRate();
		m_centerFrequency = notif.getCenterFrequency();
		qDebug() << "SigMFFileRecord::handleMessage: DSPSignalNotification: "
            << " m_inputSampleRate: " << m_sampleRate
			<< " m_centerFrequency: " << m_centerFrequency;

		return true;
	}
    else
    {
        return false;
    }
}

