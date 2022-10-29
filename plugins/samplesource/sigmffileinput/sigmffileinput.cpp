///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <string>
#include <regex>
#include <errno.h>
#include <boost/lexical_cast.hpp>

#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>
#include <QTimeZone>

#include "libsigmf/sigmf_core_generated.h"
#include "libsigmf/sigmf_sdrangel_generated.h"
#include "libsigmf/sigmf.h"

#include "SWGDeviceSettings.h"
#include "SWGSigMFFileInputSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGDeviceActions.h"

#include "dsp/dspcommands.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"
#include "util/sha512.h"

#include "sigmffileinput.h"
#include "sigmffileinputworker.h"

MESSAGE_CLASS_DEFINITION(SigMFFileInput::MsgConfigureSigMFFileInput, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileInput::MsgConfigureTrackWork, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileInput::MsgConfigureTrackIndex, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileInput::MsgConfigureTrackSeek, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileInput::MsgConfigureFileSeek, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileInput::MsgConfigureFileWork, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileInput::MsgConfigureFileInputStreamTiming, Message)

MESSAGE_CLASS_DEFINITION(SigMFFileInput::MsgReportStartStop, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileInput::MsgReportMetaData, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileInput::MsgReportTrackChange, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileInput::MsgReportFileInputStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileInput::MsgReportCRC, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileInput::MsgReportTotalSamplesCheck, Message)

SigMFFileInput::SigMFFileInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
    m_trackMode(false),
    m_currentTrackIndex(0),
    m_recordOpen(false),
    m_crcAvailable(false),
    m_crcOK(false),
    m_recordLengthOK(false),
	m_fileInputWorker(nullptr),
	m_deviceDescription("SigMFFileInput"),
	m_sampleRate(48000),
	m_sampleBytes(1),
	m_centerFrequency(0),
	m_recordLength(0),
    m_startingTimeStamp(0)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_deviceAPI->setNbSourceStreams(1);
    qDebug("SigMFFileInput::SigMFFileInput: device source engine: %p", m_deviceAPI->getDeviceSourceEngine());
    qDebug("SigMFFileInput::SigMFFileInput: device source engine message queue: %p", m_deviceAPI->getDeviceEngineInputMessageQueue());
    qDebug("SigMFFileInput::SigMFFileInput: device source: %p", m_deviceAPI->getDeviceSourceEngine()->getSource());
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SigMFFileInput::networkManagerFinished
    );
    m_masterTimer.setTimerType(Qt::PreciseTimer);
    m_masterTimer.start(50);
}

SigMFFileInput::~SigMFFileInput()
{
    m_masterTimer.stop();
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SigMFFileInput::networkManagerFinished
    );
    delete m_networkManager;

	stop();
}

void SigMFFileInput::destroy()
{
    delete this;
}

bool SigMFFileInput::openFileStreams(const QString& fileName)
{
	if (m_metaStream.is_open()) {
		m_metaStream.close();
	}

	if (m_dataStream.is_open()) {
		m_dataStream.close();
	}

    QString metaFileName = fileName + ".sigmf-meta";
    QString dataFileName = fileName + ".sigmf-data";

#ifdef Q_OS_WIN
	m_metaStream.open(metaFileName.toStdWString().c_str());
#else
	m_metaStream.open(metaFileName.toStdString().c_str());
#endif

#ifdef Q_OS_WIN
	m_dataStream.open(dataFileName.toStdWString().c_str(), std::ios::binary | std::ios::ate);
#else
	m_dataStream.open(dataFileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
#endif

    if (!m_dataStream.is_open())
    {
        qCritical("SigMFFileInput::openFileStreams: error opening data file %s", qPrintable(dataFileName));
        return false;
    }

    uint64_t dataFileSize = m_dataStream.tellg();

    sigmf::SigMF<sigmf::Global<core::DescrT, sdrangel::DescrT>,
            sigmf::Capture<core::DescrT, sdrangel::DescrT>,
            sigmf::Annotation<core::DescrT> > metaRecord;

    std::ostringstream meta_buffer;
    meta_buffer << m_metaStream.rdbuf();
    from_json(json::parse(meta_buffer.str()), metaRecord);
    extractMeta(&metaRecord, dataFileSize);
    extractCaptures(&metaRecord);
    m_metaInfo.m_totalTimeMs = m_captures.back().m_cumulativeTime + ((m_captures.back().m_length * 1000)/m_captures.back().m_sampleRate);

    uint64_t centerFrequency = (m_captures.size() > 0) ? m_captures.at(0).m_centerFrequency : 0;
    DSPSignalNotification *notif = new DSPSignalNotification(m_metaInfo.m_coreSampleRate, centerFrequency);
    m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

    if (getMessageQueueToGUI())
    {
        MsgReportMetaData *report = MsgReportMetaData::create(m_metaInfo, m_captures);
        getMessageQueueToGUI()->push(report);
    }

    if (m_metaInfo.m_sha512.size() != 0)
    {
        qDebug("SigMFFileInput::openFileStreams: compute SHA512");
        m_crcAvailable = true;
        std::string sha512 = sw::sha512::file(dataFileName.toStdString());
        m_crcOK = m_metaInfo.m_sha512 == QString::fromStdString(sha512);

        if (m_crcOK) {
            qDebug("SigMFFileInput::openFileStreams: SHA512 OK: %s", sha512.c_str());
        } else {
            qCritical("SigMFFileInput::openFileStreams: bad SHA512: %s expected: %s", sha512.c_str(), qPrintable(m_metaInfo.m_sha512));
        }

        if (getMessageQueueToGUI())
        {
            MsgReportCRC *report = MsgReportCRC::create(m_crcOK);
            getMessageQueueToGUI()->push(report);
        }

        if (!m_crcOK) {
            return false;
        }
    }
    else
    {
        m_crcAvailable = false;
    }


    m_recordLengthOK = (m_metaInfo.m_totalSamples == m_captures.back().m_sampleStart + m_captures.back().m_length);

    if (m_recordLengthOK) {
        qDebug("SigMFFileInput::openFileStreams: total samples OK");
    } else {
        qCritical("SigMFFileInput::openFileStreams: invalid total samples: meta: %lu data: %lu",
            m_captures.back().m_sampleStart + m_captures.back().m_length, m_metaInfo.m_totalSamples);
    }

    if (getMessageQueueToGUI())
    {
        MsgReportTotalSamplesCheck *report = MsgReportTotalSamplesCheck::create(m_recordLengthOK);
        getMessageQueueToGUI()->push(report);
    }

    return true;
}

void SigMFFileInput::extractMeta(
    sigmf::SigMF<sigmf::Global<core::DescrT, sdrangel::DescrT>,
    sigmf::Capture<core::DescrT, sdrangel::DescrT>,
    sigmf::Annotation<core::DescrT> >* metaRecord,
    uint64_t dataFileSize
)
{
    // core
    m_metaInfo.m_dataTypeStr = QString::fromStdString(metaRecord->global.access<core::GlobalT>().datatype);
    analyzeDataType(m_metaInfo.m_dataTypeStr.toStdString(), m_metaInfo.m_dataType);
    m_sampleBytes = SigMFFileInputSettings::bitsToBytes(m_metaInfo.m_dataType.m_sampleBits);
    m_metaInfo.m_totalSamples = dataFileSize /
        (SigMFFileInputSettings::bitsToBytes(m_metaInfo.m_dataType.m_sampleBits)*(m_metaInfo.m_dataType.m_complex ? 2 : 1));
    m_metaInfo.m_coreSampleRate = metaRecord->global.access<core::GlobalT>().sample_rate;
    m_metaInfo.m_sigMFVersion = QString::fromStdString(metaRecord->global.access<core::GlobalT>().version);
    m_metaInfo.m_sha512 = QString::fromStdString(metaRecord->global.access<core::GlobalT>().sha512);
    m_metaInfo.m_offset = metaRecord->global.access<core::GlobalT>().offset;
    m_metaInfo.m_description = QString::fromStdString(metaRecord->global.access<core::GlobalT>().description);
    m_metaInfo.m_author = QString::fromStdString(metaRecord->global.access<core::GlobalT>().author);
    m_metaInfo.m_metaDOI = QString::fromStdString(metaRecord->global.access<core::GlobalT>().meta_doi);
    m_metaInfo.m_dataDOI = QString::fromStdString(metaRecord->global.access<core::GlobalT>().meta_doi);
    m_metaInfo.m_recorder = QString::fromStdString(metaRecord->global.access<core::GlobalT>().recorder);
    m_metaInfo.m_license = QString::fromStdString(metaRecord->global.access<core::GlobalT>().license);
    m_metaInfo.m_hw = QString::fromStdString(metaRecord->global.access<core::GlobalT>().hw);
    // sdrangel
    m_metaInfo.m_sdrAngelVersion = QString::fromStdString(metaRecord->global.access<sdrangel::GlobalT>().version);
    m_metaInfo.m_qtVersion = QString::fromStdString(metaRecord->global.access<sdrangel::GlobalT>().qt_version);
    m_metaInfo.m_rxBits = metaRecord->global.access<sdrangel::GlobalT>().rx_bits;
    m_metaInfo.m_arch = QString::fromStdString(metaRecord->global.access<sdrangel::GlobalT>().arch);
    m_metaInfo.m_os = QString::fromStdString(metaRecord->global.access<sdrangel::GlobalT>().os);
    // lists
    m_metaInfo.m_nbCaptures = metaRecord->captures.size();
    m_metaInfo.m_nbAnnotations = metaRecord->annotations.size();
    // correct sample bits if sdrangel
    if (m_metaInfo.m_sdrAngelVersion.size() > 0)
    {
        if (m_metaInfo.m_dataType.m_sampleBits == 32) {
            m_metaInfo.m_dataType.m_sampleBits = 24;
        }
    }
    // negative sample rate means inversion
    m_metaInfo.m_dataType.m_swapIQ = m_metaInfo.m_coreSampleRate < 0;
    if (m_metaInfo.m_coreSampleRate < 0) {
        m_metaInfo.m_coreSampleRate = -m_metaInfo.m_coreSampleRate;
    }
}

void SigMFFileInput::extractCaptures(
    sigmf::SigMF<sigmf::Global<core::DescrT, sdrangel::DescrT>,
    sigmf::Capture<core::DescrT, sdrangel::DescrT>,
    sigmf::Annotation<core::DescrT> >* metaRecord
)
{
    m_captures.clear();
    std::regex datetime_reg("(\\d{4})-(\\d\\d)-(\\d\\d)T(\\d\\d):(\\d\\d):(\\d\\d)(\\.\\d+)?(([+-]\\d\\d:\\d\\d)|Z)?");
    std::smatch datetime_match;

    sigmf::SigMFVector<sigmf::Capture<core::DescrT, sdrangel::DescrT>>::iterator it =
        metaRecord->captures.begin();
    uint64_t lastSampleStart = 0;
    unsigned int i = 0;
    uint64_t cumulativeTime = 0;

    for (; it != metaRecord->captures.end(); ++it, i++)
    {
        m_captures.push_back(SigMFFileCapture());
        m_captures.back().m_centerFrequency = it->get<core::DescrT>().frequency;
        m_captures.back().m_sampleStart = it->get<core::DescrT>().sample_start;
        m_captureStarts.push_back(m_captures.back().m_sampleStart);
        m_captures.back().m_cumulativeTime = cumulativeTime;
        int sdrangelSampleRate = it->get<sdrangel::DescrT>().sample_rate;
        double globalSampleRate = metaRecord->global.access<core::GlobalT>().sample_rate;

        if (sdrangelSampleRate == 0) {
            m_captures.back().m_sampleRate = globalSampleRate < 0 ? -globalSampleRate : globalSampleRate;
        } else {
            m_captures.back().m_sampleRate = sdrangelSampleRate;
        }

        uint64_t tsms = it->get<sdrangel::DescrT>().tsms;

        if (tsms)
        {
            m_captures.back().m_tsms = tsms;
        }
        else
        {
            std::regex_search(it->get<core::DescrT>().datetime, datetime_match, datetime_reg);
            QString dateTimeString;
            QDateTime dateTime;

            if (datetime_match.size() > 6) {
                dateTimeString = QString("%1-%2-%3T%4:%5:%6")
                    .arg(QString::fromStdString(datetime_match[1]))  // year
                    .arg(QString::fromStdString(datetime_match[2]))  // month
                    .arg(QString::fromStdString(datetime_match[3]))  // day
                    .arg(QString::fromStdString(datetime_match[4]))  // hour
                    .arg(QString::fromStdString(datetime_match[5]))  // minute
                    .arg(QString::fromStdString(datetime_match[6])); // second
                dateTime = QDateTime::fromString(dateTimeString, "yyyy-MM-ddThh:mm:ss");

                // skip timezone calculation - assume UTC
                if (dateTime.isValid()) {
                    dateTime.setTimeZone(QTimeZone::utc());
                } else {
                    dateTime = QDateTime::currentDateTimeUtc();
                }
            }
            else
            {
                dateTime = QDateTime::currentDateTimeUtc();
            }

            double seconds = dateTime.toSecsSinceEpoch();
            // the subsecond part can be milli (strict ISO-8601) or micro or nano (RFC-3339). This will take any width
            if (datetime_match.size() > 7)
            {
                try
                {
                    double fractionalSecs = boost::lexical_cast<double>(datetime_match[7]);
                    seconds += fractionalSecs;
                }
                catch (const boost::bad_lexical_cast &e)
                {
                    qDebug("SigMFFileInput::extractCaptures: invalid fractional seconds");
                }
            }

            m_captures.back().m_tsms = seconds * 1000.0;
        }

        m_captures.back().m_length = it->get<core::DescrT>().length;

        if ((i != 0) && (m_captures.at(i-1).m_length == 0))
        {
            m_captures[i-1].m_length = m_captures.at(i).m_sampleStart - lastSampleStart;
            lastSampleStart = m_captures.at(i).m_sampleStart;
        }

        cumulativeTime += (m_captures.back().m_length * 1000) / m_captures.back().m_sampleRate;
    }

    if (m_captures.back().m_length == 0) {
        m_captures.back().m_length = m_metaInfo.m_totalSamples - m_captures.back().m_sampleStart;
    }
}

void SigMFFileInput::analyzeDataType(const std::string& dataTypeString, SigMFFileDataType& dataType)
{
    std::regex dataType_reg("(\\w)(\\w)(\\d+)(_\\w\\w)?");
    std::smatch dataType_match;
    std::regex_search(dataTypeString, dataType_match, dataType_reg);

    if (dataType_match.size() > 3)
    {
        dataType.m_complex = (dataType_match[1] == "c");

        if (dataType_match[2] == "f")
        {
            dataType.m_floatingPoint = true;
            dataType.m_signed = true;
        }
        else if (dataType_match[2] == "i")
        {
            dataType.m_floatingPoint = false;
            dataType.m_signed = true;
        }
        else
        {
            dataType.m_floatingPoint = false;
            dataType.m_signed = false;
        }

        try
        {
            dataType.m_sampleBits = boost::lexical_cast<int>(dataType_match[3]);
        }
        catch(const boost::bad_lexical_cast &e)
        {
            qDebug("SigMFFileInput::analyzeDataType: invalid sample bits. Assume 32");
            dataType.m_sampleBits = 32;
        }
    }

    if (dataType_match.size() > 4) {
        dataType.m_bigEndian = (dataType_match[4] == "_be");
    }
}

uint64_t SigMFFileInput::getTrackSampleStart(unsigned int trackIndex)
{
    if (trackIndex < m_captureStarts.size()) {
        return m_captureStarts[trackIndex];
    } else {
        return m_metaInfo.m_totalSamples;
    }
}

int SigMFFileInput::getTrackIndex(uint64_t sampleIndex)
{
    auto it = std::upper_bound(m_captureStarts.begin(), m_captureStarts.end(), sampleIndex);
    return (it - m_captureStarts.begin()) - 1;
}

void SigMFFileInput::seekFileStream(uint64_t sampleIndex)
{
    QMutexLocker mutexLocker(&m_mutex);

	if (m_dataStream.is_open())
	{
        uint64_t seekPoint = sampleIndex*m_sampleBytes*2;
		m_dataStream.clear();
		m_dataStream.seekg(seekPoint, std::ios::beg);
    }
}

void SigMFFileInput::seekTrackMillis(int seekMillis)
{
    seekFileStream(m_captures[m_currentTrackIndex].m_sampleStart + ((m_captures[m_currentTrackIndex].m_length*seekMillis)/1000UL));
}

void SigMFFileInput::seekFileMillis(int seekMillis)
{
    seekFileStream((m_metaInfo.m_totalSamples*seekMillis)/1000UL);
}

void SigMFFileInput::init()
{
    DSPSignalNotification *notif = new DSPSignalNotification(m_sampleRate, m_centerFrequency);
    m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
}

bool SigMFFileInput::start()
{
    if (!m_dataStream.is_open())
    {
        qWarning("SigMFFileInput::start: file not open. not starting");
        return false;
    }

	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "SigMFFileInput::start";

	if (m_dataStream.tellg() != (std::streampos) 0) {
		m_dataStream.clear();
		m_dataStream.seekg(0, std::ios::beg);
	}

	if(!m_sampleFifo.setSize(m_settings.m_accelerationFactor * m_sampleRate * sizeof(Sample))) {
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	m_fileInputWorker = new SigMFFileInputWorker(&m_dataStream, &m_sampleFifo, m_masterTimer, &m_inputMessageQueue);
	startWorker();
    m_fileInputWorker->setMetaInformation(&m_metaInfo, &m_captures);
    m_fileInputWorker->setAccelerationFactor(m_settings.m_accelerationFactor);
    m_fileInputWorker->setTrackIndex(0);
    m_fileInputWorker->moveToThread(&m_fileInputWorkerThread);
	m_deviceDescription = "SigMFFileInput";

	mutexLocker.unlock();
	qDebug("SigMFFileInput::startInput: started");

	if (getMessageQueueToGUI()) {
        MsgReportStartStop *report = MsgReportStartStop::create(true);
        getMessageQueueToGUI()->push(report);
	}

	return true;
}

void SigMFFileInput::stop()
{
	qDebug() << "SigMFFileInput::stop";
	QMutexLocker mutexLocker(&m_mutex);

	if (m_fileInputWorker)
	{
		stopWorker();
		delete m_fileInputWorker;
		m_fileInputWorker = nullptr;
	}

	m_deviceDescription.clear();

	if (getMessageQueueToGUI()) {
        MsgReportStartStop *report = MsgReportStartStop::create(false);
        getMessageQueueToGUI()->push(report);
	}
}

void SigMFFileInput::startWorker()
{
    m_fileInputWorker->startWork();
    m_fileInputWorkerThread.start();
}

void SigMFFileInput::stopWorker()
{
    m_fileInputWorker->stopWork();
    m_fileInputWorkerThread.quit();
    m_fileInputWorkerThread.wait();
}

QByteArray SigMFFileInput::serialize() const
{
    return m_settings.serialize();
}

bool SigMFFileInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureSigMFFileInput* message = MsgConfigureSigMFFileInput::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (getMessageQueueToGUI())
    {
        MsgConfigureSigMFFileInput* messageToGUI = MsgConfigureSigMFFileInput::create(m_settings, QList<QString>(), true);
        getMessageQueueToGUI()->push(messageToGUI);
    }

    return success;
}

const QString& SigMFFileInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int SigMFFileInput::getSampleRate() const
{
	return m_sampleRate;
}

quint64 SigMFFileInput::getCenterFrequency() const
{
	return m_centerFrequency;
}

void SigMFFileInput::setCenterFrequency(qint64 centerFrequency)
{
    SigMFFileInputSettings settings = m_settings;
    m_centerFrequency = centerFrequency;

    MsgConfigureSigMFFileInput* message = MsgConfigureSigMFFileInput::create(m_settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (getMessageQueueToGUI())
    {
        MsgConfigureSigMFFileInput* messageToGUI = MsgConfigureSigMFFileInput::create(m_settings, QList<QString>{"centerFrequency"}, false);
        getMessageQueueToGUI()->push(messageToGUI);
    }
}

quint64 SigMFFileInput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool SigMFFileInput::handleMessage(const Message& message)
{
    if (MsgConfigureSigMFFileInput::match(message))
    {
        MsgConfigureSigMFFileInput& conf = (MsgConfigureSigMFFileInput&) message;
        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
        return true;
    }
    else if (MsgConfigureTrackIndex::match(message))
    {
        MsgConfigureTrackIndex& conf = (MsgConfigureTrackIndex&) message;
        m_currentTrackIndex = conf.getTrackIndex();
        qDebug("SigMFFileInput::handleMessage MsgConfigureTrackIndex: m_currentTrackIndex: %d", m_currentTrackIndex);
        seekTrackMillis(0);

		if (m_fileInputWorker)
		{
            bool working = m_fileInputWorker->isRunning();

            if (working) {
                stopWorker();
            }

            m_fileInputWorker->setTrackIndex(m_currentTrackIndex);
            m_fileInputWorker->setTotalSamples(
                m_trackMode ?
                    m_captures[m_currentTrackIndex].m_sampleStart + m_captures[m_currentTrackIndex].m_length :
                    m_metaInfo.m_totalSamples
            );

            if (working) {
        		startWorker();
            }
        }

        return true;
    }
	else if (MsgConfigureTrackWork::match(message))
	{
		MsgConfigureTrackWork& conf = (MsgConfigureTrackWork&) message;
		bool working = conf.isWorking();
        m_trackMode = true;

		if (m_fileInputWorker)
		{
			if (working)
            {
                m_fileInputWorker->setTotalSamples(
                    m_captures[m_currentTrackIndex].m_sampleStart + m_captures[m_currentTrackIndex].m_length);
				startWorker();
			}
            else
            {
				stopWorker();
			}
		}

		return true;
	}
    else if (MsgConfigureTrackSeek::match(message))
    {
        MsgConfigureTrackSeek& conf = (MsgConfigureTrackSeek&) message;
        int seekMillis = conf.getMillis();
        seekTrackMillis(seekMillis);

		if (m_fileInputWorker)
		{
            bool working = m_fileInputWorker->isRunning();

            if (working) {
                stopWorker();
            }

            m_fileInputWorker->setSamplesCount(
                m_captures[m_currentTrackIndex].m_sampleStart + ((m_captures[m_currentTrackIndex].m_length*seekMillis)/1000UL));

            if (working) {
        		startWorker();
            }
        }

        return true;
    }
    else if (MsgConfigureFileSeek::match(message))
    {
        MsgConfigureFileSeek& conf = (MsgConfigureFileSeek&) message;
        int seekMillis = conf.getMillis();
        seekFileStream(seekMillis);
        uint64_t sampleCount = (m_metaInfo.m_totalSamples*seekMillis)/1000UL;
        m_currentTrackIndex = getTrackIndex(sampleCount);

		if (m_fileInputWorker)
		{
            bool working = m_fileInputWorker->isRunning();

            if (working) {
                stopWorker();
            }

            m_fileInputWorker->setTrackIndex(m_currentTrackIndex);
            m_fileInputWorker->setSamplesCount(sampleCount);

            if (working) {
        		startWorker();
            }
        }

        return true;
    }
	else if (MsgConfigureFileWork::match(message))
	{
		MsgConfigureFileWork& conf = (MsgConfigureFileWork&) message;
		bool working = conf.isWorking();
        m_trackMode = false;

		if (m_fileInputWorker)
		{
			if (working)
            {
                m_fileInputWorker->setTotalSamples(m_metaInfo.m_totalSamples);
				startWorker();
			}
            else
            {
				stopWorker();
			}
		}

		return true;
	}
	else if (MsgConfigureFileInputStreamTiming::match(message))
	{
		if (m_fileInputWorker)
		{
			if (getMessageQueueToGUI())
			{
                quint64 totalSamplesCount = m_fileInputWorker->getSamplesCount();
                quint64 trackSamplesCount = totalSamplesCount - m_captures[m_currentTrackIndex].m_sampleStart;
        		MsgReportFileInputStreamTiming *report = MsgReportFileInputStreamTiming::create(
                    totalSamplesCount,
                    trackSamplesCount,
                    m_captures[m_currentTrackIndex].m_cumulativeTime,
                    m_currentTrackIndex
                );
                getMessageQueueToGUI()->push(report);
			}
		}

		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "FileInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine()) {
                m_deviceAPI->startDeviceEngine();
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine();
        }

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendStartStop(cmd.getStartStop());
        }

        return true;
    }
    else if (SigMFFileInputWorker::MsgReportEOF::match(message)) // End Of File or end of track
    {
        qDebug() << "FileInput::handleMessage: MsgReportEOF";
        bool working = m_fileInputWorker->isRunning();

        if (working) {
            stopWorker();
        }

        if (m_trackMode)
        {
            if (m_settings.m_trackLoop)
            {
                seekFileStream(m_captures[m_currentTrackIndex].m_sampleStart);
                m_fileInputWorker->setTrackIndex(m_currentTrackIndex);
            }
        }
        else
        {
            if (m_settings.m_fullLoop)
            {
                seekFileStream(0);
                m_fileInputWorker->setTrackIndex(0);
            }
        }

        if (working) {
            startWorker();
        }

        return true;
    }
    else if (SigMFFileInputWorker::MsgReportTrackChange::match(message))
    {
        SigMFFileInputWorker::MsgReportTrackChange& report = (SigMFFileInputWorker::MsgReportTrackChange&) message;
        m_currentTrackIndex = report.getTrackIndex();
        qDebug("SigMFFileInput::handleMessage MsgReportTrackChange: m_currentTrackIndex: %d", m_currentTrackIndex);
        int sampleRate = m_captures.at(m_currentTrackIndex).m_sampleRate;
        uint64_t centerFrequency = m_captures.at(m_currentTrackIndex).m_centerFrequency;

        if ((m_sampleRate != sampleRate) || (m_centerFrequency != centerFrequency))
        {
            DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, centerFrequency);
            m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

            m_sampleRate = sampleRate;
            m_centerFrequency = centerFrequency;
        }

        if (getMessageQueueToGUI())
        {
            MsgReportTrackChange *msgToGUI = MsgReportTrackChange::create(m_currentTrackIndex);
            getMessageQueueToGUI()->push(msgToGUI);
        }

        return true;
    }
	else
	{
		return false;
	}
}

bool SigMFFileInput::applySettings(const SigMFFileInputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "SigMFFileInput::applySettings: force: " << force << settings.getDebugString(settingsKeys, force);

    if (settingsKeys.contains("accelerationFactor") || force)
    {
        if (m_fileInputWorker)
        {
            QMutexLocker mutexLocker(&m_mutex);
            if (!m_sampleFifo.setSize(m_settings.m_accelerationFactor * m_sampleRate * sizeof(Sample))) {
                qCritical("SigMFFileInput::applySettings: could not reallocate sample FIFO size to %lu",
                        m_settings.m_accelerationFactor * m_sampleRate * sizeof(Sample));
            }

            m_fileInputWorker->setAccelerationFactor(settings.m_accelerationFactor); // Fast Forward: 1 corresponds to live. 1/2 is half speed, 2 is double speed
        }
    }

    if (settingsKeys.contains("fileName")) {
        openFileStreams(settings.m_fileName);
    }

    if (settingsKeys.contains("useReverseAPI"))
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
            settingsKeys.contains("reverseAPIAddress") ||
            settingsKeys.contains("reverseAPIPort") ||
            settingsKeys.contains("reverseAPIDeviceIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    return true;
}

int SigMFFileInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setSigMfFileInputSettings(new SWGSDRangel::SWGSigMFFileInputSettings());
    response.getSigMfFileInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int SigMFFileInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    SigMFFileInputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureSigMFFileInput *msg = MsgConfigureSigMFFileInput::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSigMFFileInput *msgToGUI = MsgConfigureSigMFFileInput::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void SigMFFileInput::webapiUpdateDeviceSettings(
        SigMFFileInputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("fileName")) {
        settings.m_fileName = *response.getSigMfFileInputSettings()->getFileName();
    }
    if (deviceSettingsKeys.contains("accelerationFactor")) {
        settings.m_accelerationFactor = response.getSigMfFileInputSettings()->getAccelerationFactor();
    }
    if (deviceSettingsKeys.contains("trackLoop")) {
        settings.m_trackLoop = response.getSigMfFileInputSettings()->getTrackLoop() != 0;
    }
    if (deviceSettingsKeys.contains("fullLoop")) {
        settings.m_trackLoop = response.getSigMfFileInputSettings()->getFullLoop() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getSigMfFileInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getSigMfFileInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getSigMfFileInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getSigMfFileInputSettings()->getReverseApiDeviceIndex();
    }
}

int SigMFFileInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int SigMFFileInput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (getMessageQueueToGUI()) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run);
        getMessageQueueToGUI()->push(msgToGUI);
    }

    return 200;
}

int SigMFFileInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setSigMfFileInputReport(new SWGSDRangel::SWGSigMFFileInputReport());
    response.getSigMfFileInputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

int SigMFFileInput::webapiActionsPost(
        const QStringList& deviceActionsKeys,
        SWGSDRangel::SWGDeviceActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGSigMFFileInputActions *swgSigMFFileInputActions = query.getSigMfFileInputActions();

    if (swgSigMFFileInputActions)
    {
        if (deviceActionsKeys.contains("playTrack"))
        {
            bool play = swgSigMFFileInputActions->getPlayTrack() != 0;
            MsgConfigureTrackWork * msg = MsgConfigureTrackWork::create(play);
            getInputMessageQueue()->push(msg);

            if (getMessageQueueToGUI())
            {
                MsgConfigureTrackWork *msgToGUI = MsgConfigureTrackWork::create(play);
                getMessageQueueToGUI()->push(msgToGUI);
            }
        }
        else if (deviceActionsKeys.contains("playRecord"))
        {
            bool play = swgSigMFFileInputActions->getPlayRecord() != 0;
            MsgConfigureFileWork * msg = MsgConfigureFileWork::create(play);
            getInputMessageQueue()->push(msg);

            if (getMessageQueueToGUI())
            {
                MsgConfigureFileWork *msgToGUI = MsgConfigureFileWork::create(play);
                getMessageQueueToGUI()->push(msgToGUI);
            }
        }
        else if (deviceActionsKeys.contains("seekTrack"))
        {
            int trackIndex = swgSigMFFileInputActions->getSeekTrack();
            MsgConfigureTrackIndex *msg = MsgConfigureTrackIndex::create(trackIndex);
            getInputMessageQueue()->push(msg);

            if (getMessageQueueToGUI())
            {
                MsgConfigureTrackIndex *msgToGUI = MsgConfigureTrackIndex::create(trackIndex);
                getMessageQueueToGUI()->push(msgToGUI);
            }
        }
        else if (deviceActionsKeys.contains("seekTrackMillis"))
        {
            int trackMillis = swgSigMFFileInputActions->getSeekTrackMillis();
            MsgConfigureTrackSeek *msg = MsgConfigureTrackSeek::create(trackMillis);
            getInputMessageQueue()->push(msg);

            if (getMessageQueueToGUI())
            {
                MsgConfigureTrackSeek *msgToGUI = MsgConfigureTrackSeek::create(trackMillis);
                getMessageQueueToGUI()->push(msgToGUI);
            }
        }
        else if (deviceActionsKeys.contains("seekRecordMillis"))
        {
            int recordMillis = swgSigMFFileInputActions->getSeekRecordMillis();
            MsgConfigureFileSeek *msg = MsgConfigureFileSeek::create(recordMillis);
            getInputMessageQueue()->push(msg);

            if (getMessageQueueToGUI())
            {
                MsgConfigureFileSeek *msgToGUI = MsgConfigureFileSeek::create(recordMillis);
                getMessageQueueToGUI()->push(msgToGUI);
            }
        }

        return 202;
    }
    else
    {
        errorMessage = "Missing AirspyActions in query";
        return 400;
    }
}

void SigMFFileInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const SigMFFileInputSettings& settings)
{
    response.getSigMfFileInputSettings()->setFileName(new QString(settings.m_fileName));
    response.getSigMfFileInputSettings()->setAccelerationFactor(settings.m_accelerationFactor);
    response.getSigMfFileInputSettings()->setTrackLoop(settings.m_trackLoop ? 1 : 0);
    response.getSigMfFileInputSettings()->setFullLoop(settings.m_fullLoop ? 1 : 0);

    response.getSigMfFileInputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getSigMfFileInputSettings()->getReverseApiAddress()) {
        *response.getSigMfFileInputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getSigMfFileInputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getSigMfFileInputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getSigMfFileInputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void SigMFFileInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    if (!m_metaStream.is_open()) {
        return;
    }

    response.getSigMfFileInputReport()->setSampleSize(m_metaInfo.m_dataType.m_sampleBits);
    response.getSigMfFileInputReport()->setSampleBytes(m_sampleBytes);
    response.getSigMfFileInputReport()->setSampleFormat(m_metaInfo.m_dataType.m_floatingPoint ? 1 : 0);
    response.getSigMfFileInputReport()->setSampleSigned(m_metaInfo.m_dataType.m_signed ? 1 : 0);
    response.getSigMfFileInputReport()->setSampleSwapIq(m_metaInfo.m_dataType.m_swapIQ ? 1 : 0);
    response.getSigMfFileInputReport()->setCrcStatus(!m_crcAvailable ? 0 : m_crcOK ? 1 : 2);
    response.getSigMfFileInputReport()->setTotalBytesStatus(m_recordLengthOK);
    response.getSigMfFileInputReport()->setTrackNumber(m_currentTrackIndex);
    QList<SigMFFileCapture>::const_iterator it = m_captures.begin();

    if (response.getSigMfFileInputReport()->getCaptures()) {
        response.getSigMfFileInputReport()->getCaptures()->clear();
    } else {
        response.getSigMfFileInputReport()->setCaptures(new QList<SWGSDRangel::SWGCapture*>);
    }

    for (; it != m_captures.end(); ++it)
    {
        response.getSigMfFileInputReport()->getCaptures()->append(new SWGSDRangel::SWGCapture);
        response.getSigMfFileInputReport()->getCaptures()->back()->setTsms(it->m_tsms);
        response.getSigMfFileInputReport()->getCaptures()->back()->setCenterFrequency(it->m_centerFrequency);
        response.getSigMfFileInputReport()->getCaptures()->back()->setSampleRate(it->m_sampleRate);
        response.getSigMfFileInputReport()->getCaptures()->back()->setSampleStart(it->m_sampleStart);
        response.getSigMfFileInputReport()->getCaptures()->back()->setLength(it->m_length);
        response.getSigMfFileInputReport()->getCaptures()->back()->setCumulativeTime(it->m_cumulativeTime);
    }

    uint64_t totalSamplesCount = 0;

    if (m_fileInputWorker) {
        totalSamplesCount = m_fileInputWorker->getSamplesCount();
    }

    unsigned int sampleRate =  m_captures[m_currentTrackIndex].m_sampleRate;
    uint64_t trackSamplesCount = totalSamplesCount - m_captures[m_currentTrackIndex].m_sampleStart;
    uint64_t startingTimeStampMs = m_captures[m_currentTrackIndex].m_tsms;

    uint64_t t = (trackSamplesCount*1000)/sampleRate;
    response.getSigMfFileInputReport()->setElapsedTrackimeMs(t);
    t += m_captures[m_currentTrackIndex].m_cumulativeTime;
    response.getSigMfFileInputReport()->setElapsedRecordTimeMs(t);
    response.getSigMfFileInputReport()->setAbsoluteTimeMs(startingTimeStampMs + ((trackSamplesCount*1000)/sampleRate));

	float posRatio = (float) trackSamplesCount / (float) m_captures[m_currentTrackIndex].m_length;
    response.getSigMfFileInputReport()->setTrackSamplesRatio(posRatio);

    posRatio = (float) totalSamplesCount / (float) m_metaInfo.m_totalSamples;
    response.getSigMfFileInputReport()->setRecordSamplesRatio(posRatio);

    if (m_captures.size() > 0 )
    {
        uint64_t totalTimeMs = m_captures.back().m_cumulativeTime + ((m_captures.back().m_length * 1000) / m_captures.back().m_sampleRate);
        response.getSigMfFileInputReport()->setRecordDurationMs(totalTimeMs);
    }
    else
    {
        response.getSigMfFileInputReport()->setRecordDurationMs(0);
    }
}

void SigMFFileInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const SigMFFileInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("SigMFFileInput"));
    swgDeviceSettings->setSigMfFileInputSettings(new SWGSDRangel::SWGSigMFFileInputSettings());
    SWGSDRangel::SWGSigMFFileInputSettings *swgSigMFFileInputSettings = swgDeviceSettings->getSigMfFileInputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("accelerationFactor") || force) {
        swgSigMFFileInputSettings->setAccelerationFactor(settings.m_accelerationFactor);
    }
    if (deviceSettingsKeys.contains("trackLoop") || force) {
        swgSigMFFileInputSettings->setTrackLoop(settings.m_trackLoop);
    }
    if (deviceSettingsKeys.contains("fullLoop") || force) {
        swgSigMFFileInputSettings->setFullLoop(settings.m_fullLoop);
    }
    if (deviceSettingsKeys.contains("fileName") || force) {
        swgSigMFFileInputSettings->setFileName(new QString(settings.m_fileName));
    }

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgDeviceSettings;
}

void SigMFFileInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("SigMFFileInput"));

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);
    QNetworkReply *reply;

    if (start) {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "POST", buffer);
    } else {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "DELETE", buffer);
    }

    buffer->setParent(reply);
    delete swgDeviceSettings;
}

void SigMFFileInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "SigMFFileInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("SigMFFileInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
