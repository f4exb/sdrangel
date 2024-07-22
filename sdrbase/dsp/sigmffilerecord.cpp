///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021 Christoph Berg <myon@debian.org>                           //
// Copyright (C) 2021 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
    // sizeof(FixReal) is either 2 or 4 thus log2 sample size is either 4 (size 16) or 5 (size 32)
    m_log2RecordSampleSize = (sizeof(FixReal) / 2) + 3;
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
    // sizeof(FixReal) is either 2 or 4 thus log2 sample size is either 4 (size 16) or 5 (size 32)
    m_log2RecordSampleSize = (sizeof(FixReal) / 2) + 3;
}

SigMFFileRecord::~SigMFFileRecord()
{
    qDebug("SigMFFileRecord::~SigMFFileRecord");

    stopRecording();

    if (m_metaFile.isOpen()) {
        m_metaFile.close();
    }

    if (m_sampleFile.isOpen()) {
        m_sampleFile.close();
    }

    delete m_metaRecord;
}

void SigMFFileRecord::setFileName(const QString& fileName)
{
    if (!m_recordOn)
    {
        qDebug("SigMFFileRecord::setFileName: %s", qPrintable(fileName));

        if (m_metaFile.isOpen()) {
            m_metaFile.close();
        }

        if (m_sampleFile.isOpen()) {
            m_sampleFile.close();
        }

        m_fileName = fileName;
        QString metaFileName = m_fileName + ".sigmf-meta";
        m_metaFile.setFileName(metaFileName);
        QString sampleFileName = m_fileName + ".sigmf-data";
        m_sampleFile.setFileName(sampleFileName);

        if (QFile::exists(metaFileName) && QFile::exists(sampleFileName))
        {
            QFile metaStream(metaFileName);
            metaStream.open(QIODevice::ReadOnly);
            try
            {
                from_json(json::parse(metaStream.readAll().toStdString()), *m_metaRecord);
                metaStream.close();
                std::string sdrAngelVersion = m_metaRecord->global.access<sdrangel::GlobalT>().version;

                if (sdrAngelVersion.size() != 0)
                {
                    qDebug("SigMFFileRecord::setFileName: appending mode");
                    m_initialMsCount = 0;

                    for (auto capture : m_metaRecord->captures)
                    {
                        uint64_t length = capture.get<core::DescrT>().length;
                        int32_t sampleRate = capture.get<sdrangel::DescrT>().sample_rate;
                        m_initialMsCount += (length * 1000) / sampleRate;
                    }

                    m_sampleFile.setFileName(m_fileName + ".sigmf-data");
                    if (!m_sampleFile.open(QIODevice::WriteOnly | QIODevice::Append))
                    {
                        qWarning() << "SigMFFileRecord::setFileName: failed to open file: " << m_sampleFile.fileName();
                    }
                    m_initialBytesCount = (uint64_t) m_sampleFile.size();
                    m_sampleStart =  m_initialBytesCount / ((1<<m_log2RecordSampleSize)/4); // sizeof(Sample);

                    if (!m_metaFile.open(QIODevice::WriteOnly | QIODevice::Append))
                    {
                        qWarning() << "SigMFFileRecord::setFileName: failed to open file: " << m_metaFile.fileName();
                    }

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
        if (!m_sampleFile.open(QIODevice::WriteOnly))
        {
            qWarning() << "SigMFFileRecord::startRecording: failed to open file: " << m_sampleFile.fileName();
            success = false;
        }
        if (!m_metaFile.open(QIODevice::WriteOnly | QIODevice::Append))
        {
            qWarning() << "SigMFFileRecord::startRecording: failed to open file: " << m_metaFile.fileName();
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
        if (m_sampleFile.error())
        {
            qWarning() << "SigMFFileRecord::stopRecording: an error occurred while writing to " << m_sampleFile.fileName();
            return false;
        }
        if (m_metaFile.error())
        {
            qWarning() << "SigMFFileRecord::stopRecording: an error occurred while writing to " << m_metaFile.fileName();
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
    int size = 1<<m_log2RecordSampleSize;
    qDebug("SigMFFileRecord::makeHeader: %u %u", m_log2RecordSampleSize, (1<<m_log2RecordSampleSize));
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
        m_metaFile.seek(0);
        std::string jsonRecord = json(*m_metaRecord).dump(2);
        m_metaFile.write(jsonRecord.c_str(), jsonRecord.size());
        m_metaFile.flush();
        m_metaFile.resize(m_metaFile.pos());
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
        // m_sampleFile.write(reinterpret_cast<const char*>(&*(begin)), (end - begin)*sizeof(Sample));
        feedConv(begin, end);
        m_sampleCount += end - begin;
    }
}

void SigMFFileRecord::feedConv(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    uint32_t desiredIorQSampleSize = 1<<m_log2RecordSampleSize;

    if (2*desiredIorQSampleSize ==  8*sizeof(Sample)) // if size of sample matches desired sample size write directly
    {
        m_sampleFile.write(reinterpret_cast<const char*>(&*(begin)), (end - begin)*sizeof(Sample));
    }
    else
    {
        uint32_t nsamples = (end - begin);

        // Only the 24 LSBits of the 32 bits samples are significant
        if (desiredIorQSampleSize == 32) // can only be 16 bit samples => x8 (16 -> 24)
        {
            if (nsamples > m_samples32.size()) {
                m_samples32.resize(nsamples);
            }
            std::transform(
                begin,
                end,
                m_samples32.begin(),
                [](const Sample& s) -> Sample32 {
                    return Sample32{s.real()<<8, s.imag()<<8};
                }
            );
            m_sampleFile.write(reinterpret_cast<const char*>(&*(m_samples32.begin())), nsamples*sizeof(Sample32));
        }
        else if (desiredIorQSampleSize == 16) // can only be 32 bit samples size => /8 (24 -> 16)
        {
            if (nsamples > m_samples16.size()) {
                m_samples16.resize(nsamples);
            }
            std::transform(
                begin,
                end,
                m_samples16.begin(),
                [](const Sample& s) -> Sample16 {
                    return Sample16{(qint16)(s.real()>>8), (qint16)(s.imag()>>8)};
                }
            );
            m_sampleFile.write(reinterpret_cast<const char*>(&*(m_samples16.begin())), nsamples*sizeof(Sample16));
        }
        else // can only be 8 bit desired sample size
        {
            // divide by 8 for 16 -> 8 (sizeof(sample) == 4) or 16 for 24 -> 8 (sizeod(Sample) == 8)
            // thus division of a I or Q sample is done with >>(2*sizeof(sample)) operation
            if (nsamples > m_samples8.size()) {
                m_samples8.resize(nsamples);
            }
            std::transform(
                begin,
                end,
                m_samples8.begin(),
                [](const Sample& s) -> Sample8 {
                    return Sample8{(qint8)(s.real()>>(2*sizeof(Sample))), (qint8)(s.imag()>>(2*sizeof(Sample)))};
                }
            );
            m_sampleFile.write(reinterpret_cast<const char*>(&*(m_samples8.begin())), nsamples*sizeof(Sample8));
        }
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

