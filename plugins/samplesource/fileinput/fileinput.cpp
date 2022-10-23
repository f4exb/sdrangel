///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB                              //
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

#include <string.h>
#include <errno.h>

#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGFileInputSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"
#include "dsp/wavfilerecord.h"
#include "device/deviceapi.h"

#include "fileinput.h"
#include "fileinputworker.h"

MESSAGE_CLASS_DEFINITION(FileInput::MsgConfigureFileInput, Message)
MESSAGE_CLASS_DEFINITION(FileInput::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(FileInput::MsgConfigureFileInputWork, Message)
MESSAGE_CLASS_DEFINITION(FileInput::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(FileInput::MsgConfigureFileInputStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FileInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(FileInput::MsgPlayPause, Message)
MESSAGE_CLASS_DEFINITION(FileInput::MsgReportFileSourceAcquisition, Message)
MESSAGE_CLASS_DEFINITION(FileInput::MsgReportFileInputStreamData, Message)
MESSAGE_CLASS_DEFINITION(FileInput::MsgReportFileInputStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FileInput::MsgReportHeaderCRC, Message)

FileInput::FileInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_fileInputWorker(nullptr),
	m_deviceDescription("FileInput"),
	m_sampleRate(48000),
	m_sampleSize(0),
	m_centerFrequency(435000000),
	m_recordLengthMuSec(0),
    m_startingTimeStamp(0)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_deviceAPI->setNbSourceStreams(1);
    qDebug("FileInput::FileInput: device source engine: %p", m_deviceAPI->getDeviceSourceEngine());
    qDebug("FileInput::FileInput: device source engine message queue: %p", m_deviceAPI->getDeviceEngineInputMessageQueue());
    qDebug("FileInput::FileInput: device source: %p", m_deviceAPI->getDeviceSourceEngine()->getSource());
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &FileInput::networkManagerFinished
    );
    m_masterTimer.setTimerType(Qt::PreciseTimer);
    m_masterTimer.start(50);
}

FileInput::~FileInput()
{
    m_masterTimer.stop();
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &FileInput::networkManagerFinished
    );
    delete m_networkManager;

	stop();
}

void FileInput::destroy()
{
    delete this;
}

void FileInput::openFileStream()
{
	//stopInput();

	if (m_ifstream.is_open()) {
		m_ifstream.close();
	}

#ifdef Q_OS_WIN
	m_ifstream.open(m_settings.m_fileName.toStdWString().c_str(), std::ios::binary | std::ios::ate);
#else
	m_ifstream.open(m_settings.m_fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
#endif
	quint64 fileSize = m_ifstream.tellg();

	if (m_settings.m_fileName.endsWith(".wav"))
    {
        WavFileRecord::Header header;
        m_ifstream.seekg(0, std::ios_base::beg);
        bool headerOK = WavFileRecord::readHeader(m_ifstream, header);
        m_sampleRate = header.m_sampleRate;
        if (header.m_auxiHeader.m_size > 0)
        {
            // Some WAV files written by SDR tools have auxi header
            m_centerFrequency = header.m_auxi.m_centerFreq;
            m_startingTimeStamp = header.getStartTime().toMSecsSinceEpoch();
        }
        else
        {
            // Attempt to extract start time and frequency from filename
            QDateTime startTime;
            if (WavFileRecord::getStartTime(m_settings.m_fileName, startTime)) {
                m_startingTimeStamp = startTime.toMSecsSinceEpoch();
            }
            WavFileRecord::getCenterFrequency(m_settings.m_fileName, m_centerFrequency);
        }
        m_sampleSize = header.m_bitsPerSample;

        if (headerOK && (m_sampleRate > 0) && (m_sampleSize > 0))
        {
            m_recordLengthMuSec = ((fileSize - m_ifstream.tellg()) * 1000000UL) / ((m_sampleSize == 24 ? 8 : 4) * m_sampleRate);
        }
        else
        {
            qCritical("FileInput::openFileStream: broken or unsupported format of .wav file");
            m_recordLengthMuSec = 0;
        }

        if (getMessageQueueToGUI())
        {
            MsgReportHeaderCRC *report = MsgReportHeaderCRC::create(headerOK);
            getMessageQueueToGUI()->push(report);
        }
    }
    else if (fileSize > sizeof(FileRecord::Header))
	{
	    FileRecord::Header header;
	    m_ifstream.seekg(0,std::ios_base::beg);
		bool crcOK = FileRecord::readHeader(m_ifstream, header);
		m_sampleRate = header.sampleRate;
		m_centerFrequency = header.centerFrequency;
		m_startingTimeStamp = header.startTimeStamp;
		m_sampleSize = header.sampleSize;
		QString crcHex = QString("%1").arg(header.crc32 , 0, 16);

	    if (crcOK && (m_sampleRate > 0) && (m_sampleSize > 0))
	    {
	        qDebug("FileInput::openFileStream: CRC32 OK for header: %s", qPrintable(crcHex));
	        m_recordLengthMuSec = ((fileSize - sizeof(FileRecord::Header)) * 1000000UL) / ((m_sampleSize == 24 ? 8 : 4) * m_sampleRate);
	    }
	    else if (!crcOK)
	    {
	        qCritical("FileInput::openFileStream: bad CRC32 for header: %s", qPrintable(crcHex));
	        m_recordLengthMuSec = 0;
	    }
            else
	    {
	        qCritical("FileInput::openFileStream: invalid header");
	        m_recordLengthMuSec = 0;
	    }

		if (getMessageQueueToGUI())
        {
			MsgReportHeaderCRC *report = MsgReportHeaderCRC::create(crcOK);
			getMessageQueueToGUI()->push(report);
		}
	}
	else
	{
		m_recordLengthMuSec = 0;
	}

	qDebug() << "FileInput::openFileStream: " << m_settings.m_fileName.toStdString().c_str()
			<< " fileSize: " << fileSize << " bytes"
			<< " length: " << m_recordLengthMuSec << " microseconds"
			<< " sample rate: " << m_sampleRate << " S/s"
			<< " center frequency: " << m_centerFrequency << " Hz"
			<< " sample size: " << m_sampleSize << " bits";

	if (getMessageQueueToGUI())
    {
        DSPSignalNotification *notif = new DSPSignalNotification(m_sampleRate, m_centerFrequency);
        getMessageQueueToGUI()->push(notif);
	    MsgReportFileInputStreamData *report = MsgReportFileInputStreamData::create(m_sampleRate,
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

void FileInput::seekFileStream(int seekMillis)
{
	QMutexLocker mutexLocker(&m_mutex);

	if ((m_ifstream.is_open()) && m_fileInputWorker && !m_fileInputWorker->isRunning())
	{
        quint64 seekPoint = ((m_recordLengthMuSec * seekMillis) / 1000) * m_sampleRate;
        seekPoint /= 1000000UL;
		m_fileInputWorker->setSamplesCount(seekPoint);
        seekPoint *= (m_sampleSize == 24 ? 8 : 4); // + sizeof(FileRecord::Header)
		m_ifstream.clear();
		m_ifstream.seekg(seekPoint + sizeof(FileRecord::Header), std::ios::beg);
	}
}

void FileInput::init()
{
    DSPSignalNotification *notif = new DSPSignalNotification(m_sampleRate, m_centerFrequency);
    m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
}

bool FileInput::start()
{
    if (!m_ifstream.is_open())
    {
        qWarning("FileInput::start: file not open. not starting");
        return false;
    }

	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "FileInput::start";

	if (m_ifstream.tellg() != (std::streampos)0)
    {
		m_ifstream.clear();
		m_ifstream.seekg(sizeof(FileRecord::Header), std::ios::beg);
	}

	if (!m_sampleFifo.setSize(m_settings.m_accelerationFactor * m_sampleRate * sizeof(Sample)))
    {
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	m_fileInputWorker = new FileInputWorker(&m_ifstream, &m_sampleFifo, m_masterTimer, &m_inputMessageQueue);
	m_fileInputWorker->moveToThread(&m_fileInputWorkerThread);
	m_fileInputWorker->setSampleRateAndSize(m_settings.m_accelerationFactor * m_sampleRate, m_sampleSize); // Fast Forward: 1 corresponds to live. 1/2 is half speed, 2 is double speed
	startWorker();

	m_deviceDescription = "FileInput";

	mutexLocker.unlock();
	qDebug("FileInput::startInput: started");

	if (getMessageQueueToGUI())
    {
        MsgReportFileSourceAcquisition *report = MsgReportFileSourceAcquisition::create(true); // acquisition on
        getMessageQueueToGUI()->push(report);
	}

	return true;
}

void FileInput::stop()
{
	qDebug() << "FileInput::stop";
	QMutexLocker mutexLocker(&m_mutex);

	if (m_fileInputWorker)
	{
		stopWorker();
		delete m_fileInputWorker;
		m_fileInputWorker = nullptr;
	}

	m_deviceDescription.clear();

	if (getMessageQueueToGUI())
    {
        MsgReportFileSourceAcquisition *report = MsgReportFileSourceAcquisition::create(false); // acquisition off
        getMessageQueueToGUI()->push(report);
	}
}

void FileInput::startWorker()
{
	m_fileInputWorker->startWork();
	m_fileInputWorkerThread.start();
}

void FileInput::stopWorker()
{
	m_fileInputWorker->stopWork();
	m_fileInputWorkerThread.quit();
	m_fileInputWorkerThread.wait();
}


QByteArray FileInput::serialize() const
{
    return m_settings.serialize();
}

bool FileInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureFileInput* message = MsgConfigureFileInput::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (getMessageQueueToGUI())
    {
        MsgConfigureFileInput* messageToGUI = MsgConfigureFileInput::create(m_settings, QList<QString>(), true);
        getMessageQueueToGUI()->push(messageToGUI);
    }

    return success;
}

const QString& FileInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FileInput::getSampleRate() const
{
	return m_sampleRate;
}

quint64 FileInput::getCenterFrequency() const
{
	return m_centerFrequency;
}

void FileInput::setCenterFrequency(qint64 centerFrequency)
{
    FileInputSettings settings = m_settings;
    m_centerFrequency = centerFrequency;

    MsgConfigureFileInput* message = MsgConfigureFileInput::create(m_settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (getMessageQueueToGUI())
    {
        MsgConfigureFileInput* messageToGUI = MsgConfigureFileInput::create(m_settings, QList<QString>{"centerFrequency"}, false);
        getMessageQueueToGUI()->push(messageToGUI);
    }
}

quint64 FileInput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool FileInput::handleMessage(const Message& message)
{
    if (MsgConfigureFileInput::match(message))
    {
        MsgConfigureFileInput& conf = (MsgConfigureFileInput&) message;
        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
        return true;
    }
    else if (MsgConfigureFileSourceName::match(message))
	{
		MsgConfigureFileSourceName& conf = (MsgConfigureFileSourceName&) message;
		m_settings.m_fileName = conf.getFileName();
		openFileStream();
		return true;
	}
	else if (MsgConfigureFileInputWork::match(message))
	{
		MsgConfigureFileInputWork& conf = (MsgConfigureFileInputWork&) message;
		bool working = conf.isWorking();

		if (m_fileInputWorker)
		{
			if (working) {
				startWorker();
			} else {
				stopWorker();
			}
		}

		return true;
	}
	else if (MsgConfigureFileSourceSeek::match(message))
	{
		MsgConfigureFileSourceSeek& conf = (MsgConfigureFileSourceSeek&) message;
		int seekMillis = conf.getMillis();
		seekFileStream(seekMillis);

		return true;
	}
	else if (MsgConfigureFileInputStreamTiming::match(message))
	{
		MsgReportFileInputStreamTiming *report;

		if (m_fileInputWorker)
		{
			if (getMessageQueueToGUI())
			{
                report = MsgReportFileInputStreamTiming::create(m_fileInputWorker->getSamplesCount());
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
    else if (FileInputWorker::MsgReportEOF::match(message))
    {
        qDebug() << "FileInput::handleMessage: MsgReportEOF";
		stopWorker();

        if (getMessageQueueToGUI())
        {
            MsgReportFileInputStreamTiming *report = MsgReportFileInputStreamTiming::create(m_fileInputWorker->getSamplesCount());
            getMessageQueueToGUI()->push(report);
        }

        if (m_settings.m_loop)
        {
            seekFileStream(0);
			startWorker();
        }
        else
        {
            if (getMessageQueueToGUI())
            {
                MsgPlayPause *report = MsgPlayPause::create(false);
                getMessageQueueToGUI()->push(report);
            }
        }

        return true;
    }
	else
	{
		return false;
	}
}

bool FileInput::applySettings(const FileInputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "FileInput::applySettings: force: " << force << settings.getDebugString(settingsKeys, force);

    if (settingsKeys.contains("accelerationFactor") || force)
    {
        if (m_fileInputWorker)
        {
            QMutexLocker mutexLocker(&m_mutex);
            if (!m_sampleFifo.setSize(m_settings.m_accelerationFactor * m_sampleRate * sizeof(Sample))) {
                qCritical("FileInput::applySettings: could not reallocate sample FIFO size to %lu",
                        m_settings.m_accelerationFactor * m_sampleRate * sizeof(Sample));
            }
			m_fileInputWorker->setSampleRateAndSize(settings.m_accelerationFactor * m_sampleRate, m_sampleSize); // Fast Forward: 1 corresponds to live. 1/2 is half speed, 2 is double speed
        }
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

    // Open the file if there isn't a GUI which will open it
    if ((m_guiMessageQueue == nullptr) && settingsKeys.contains("fileName") && !m_settings.m_fileName.isEmpty()) {
        openFileStream();
    }

    return true;
}

int FileInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setFileInputSettings(new SWGSDRangel::SWGFileInputSettings());
    response.getFileInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int FileInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    FileInputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureFileInput *msg = MsgConfigureFileInput::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureFileInput *msgToGUI = MsgConfigureFileInput::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void FileInput::webapiUpdateDeviceSettings(
        FileInputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("fileName")) {
        settings.m_fileName = *response.getFileInputSettings()->getFileName();
    }
    if (deviceSettingsKeys.contains("accelerationFactor")) {
        settings.m_accelerationFactor = response.getFileInputSettings()->getAccelerationFactor();
    }
    if (deviceSettingsKeys.contains("loop")) {
        settings.m_loop = response.getFileInputSettings()->getLoop() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getFileInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getFileInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getFileInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getFileInputSettings()->getReverseApiDeviceIndex();
    }
}

int FileInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int FileInput::webapiRun(
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

int FileInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFileInputReport(new SWGSDRangel::SWGFileInputReport());
    response.getFileInputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void FileInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const FileInputSettings& settings)
{
    response.getFileInputSettings()->setFileName(new QString(settings.m_fileName));
    response.getFileInputSettings()->setAccelerationFactor(settings.m_accelerationFactor);
    response.getFileInputSettings()->setLoop(settings.m_loop ? 1 : 0);

    response.getFileInputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getFileInputSettings()->getReverseApiAddress()) {
        *response.getFileInputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getFileInputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getFileInputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getFileInputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void FileInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    qint64 t_sec = 0;
    qint64 t_msec = 0;
    quint64 samplesCount = 0;

    if (m_fileInputWorker) {
        samplesCount = m_fileInputWorker->getSamplesCount();
    }

    if (m_sampleRate > 0)
    {
        t_sec = samplesCount / m_sampleRate;
        t_msec = (samplesCount - (t_sec * m_sampleRate)) * 1000 / m_sampleRate;
    }

    QTime t(0, 0, 0, 0);
    t = t.addSecs(t_sec);
    t = t.addMSecs(t_msec);
    response.getFileInputReport()->setElapsedTime(new QString(t.toString("HH:mm:ss.zzz")));

    qint64 startingTimeStampMsec = m_startingTimeStamp;
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(startingTimeStampMsec);
    dt = dt.addSecs(t_sec);
    dt = dt.addMSecs(t_msec);
    response.getFileInputReport()->setAbsoluteTime(new QString(dt.toString("yyyy-MM-dd HH:mm:ss.zzz")));

    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addMSecs(m_recordLengthMuSec / 1000UL);
    response.getFileInputReport()->setDurationTime(new QString(recordLength.toString("HH:mm:ss.zzz")));

    response.getFileInputReport()->setFileName(new QString(m_settings.m_fileName));
    response.getFileInputReport()->setSampleRate(m_sampleRate);
    response.getFileInputReport()->setSampleSize(m_sampleSize);
}

void FileInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const FileInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("FileInput"));
    swgDeviceSettings->setFileInputSettings(new SWGSDRangel::SWGFileInputSettings());
    SWGSDRangel::SWGFileInputSettings *swgFileInputSettings = swgDeviceSettings->getFileInputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("accelerationFactor") || force) {
        swgFileInputSettings->setAccelerationFactor(settings.m_accelerationFactor);
    }
    if (deviceSettingsKeys.contains("loop") || force) {
        swgFileInputSettings->setLoop(settings.m_loop);
    }
    if (deviceSettingsKeys.contains("fileName") || force) {
        swgFileInputSettings->setFileName(new QString(settings.m_fileName));
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

void FileInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("FileInput"));

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

void FileInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "FileInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("FileInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
