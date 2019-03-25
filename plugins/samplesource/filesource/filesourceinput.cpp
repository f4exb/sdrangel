///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include <string.h>
#include <errno.h>

#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGFileSourceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGFileSourceSettings.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"
#include "device/devicesourceapi.h"

#include "filesourceinput.h"
#include "filesourcethread.h"

MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSource, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSourceWork, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgPlayPause, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgReportFileSourceAcquisition, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgReportFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgReportHeaderCRC, Message)

FileSourceInput::FileSourceInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_fileSourceThread(NULL),
	m_deviceDescription(),
	m_fileName("..."),
	m_sampleRate(0),
	m_sampleSize(0),
	m_centerFrequency(0),
	m_recordLength(0),
    m_startingTimeStamp(0),
    m_masterTimer(deviceAPI->getMasterTimer())
{
    qDebug("FileSourceInput::FileSourceInput: device source engine: %p", m_deviceAPI->getDeviceSourceEngine());
    qDebug("FileSourceInput::FileSourceInput: device source engine message queue: %p", m_deviceAPI->getDeviceEngineInputMessageQueue());
    qDebug("FileSourceInput::FileSourceInput: device source: %p", m_deviceAPI->getDeviceSourceEngine()->getSource());
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

FileSourceInput::~FileSourceInput()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;

	stop();
}

void FileSourceInput::destroy()
{
    delete this;
}

void FileSourceInput::openFileStream()
{
	//stopInput();

	if (m_ifstream.is_open()) {
		m_ifstream.close();
	}

	m_ifstream.open(m_fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
	quint64 fileSize = m_ifstream.tellg();

	if (fileSize > sizeof(FileRecord::Header))
	{
	    FileRecord::Header header;
	    m_ifstream.seekg(0,std::ios_base::beg);
		bool crcOK = FileRecord::readHeader(m_ifstream, header);
		m_sampleRate = header.sampleRate;
		m_centerFrequency = header.centerFrequency;
		m_startingTimeStamp = header.startTimeStamp;
		m_sampleSize = header.sampleSize;
		QString crcHex = QString("%1").arg(header.crc32 , 0, 16);

	    if (crcOK)
	    {
	        qDebug("FileSourceInput::openFileStream: CRC32 OK for header: %s", qPrintable(crcHex));
	        m_recordLength = (fileSize - sizeof(FileRecord::Header)) / ((m_sampleSize == 24 ? 8 : 4) * m_sampleRate);
	    }
	    else
	    {
	        qCritical("FileSourceInput::openFileStream: bad CRC32 for header: %s", qPrintable(crcHex));
	        m_recordLength = 0;
	    }

		if (getMessageQueueToGUI()) {
			MsgReportHeaderCRC *report = MsgReportHeaderCRC::create(crcOK);
			getMessageQueueToGUI()->push(report);
		}
	}
	else
	{
		m_recordLength = 0;
	}

	qDebug() << "FileSourceInput::openFileStream: " << m_fileName.toStdString().c_str()
			<< " fileSize: " << fileSize << " bytes"
			<< " length: " << m_recordLength << " seconds"
			<< " sample rate: " << m_sampleRate << " S/s"
			<< " center frequency: " << m_centerFrequency << " Hz"
			<< " sample size: " << m_sampleSize << " bits";

	if (getMessageQueueToGUI()) {
	    MsgReportFileSourceStreamData *report = MsgReportFileSourceStreamData::create(m_sampleRate,
	            m_sampleSize,
	            m_centerFrequency,
	            m_startingTimeStamp,
	            m_recordLength); // file stream data
	    getMessageQueueToGUI()->push(report);
	}

	if (m_recordLength == 0) {
	    m_ifstream.close();
	}
}

void FileSourceInput::seekFileStream(int seekMillis)
{
	QMutexLocker mutexLocker(&m_mutex);

	if ((m_ifstream.is_open()) && m_fileSourceThread && !m_fileSourceThread->isRunning())
	{
        quint64 seekPoint = ((m_recordLength * seekMillis) / 1000) * m_sampleRate;
		m_fileSourceThread->setSamplesCount(seekPoint);
        seekPoint *= (m_sampleSize == 24 ? 8 : 4); // + sizeof(FileSink::Header)
		m_ifstream.clear();
		m_ifstream.seekg(seekPoint + sizeof(FileRecord::Header), std::ios::beg);
	}
}

void FileSourceInput::init()
{
    DSPSignalNotification *notif = new DSPSignalNotification(m_settings.m_sampleRate, m_settings.m_centerFrequency);
    m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
}

bool FileSourceInput::start()
{
    if (!m_ifstream.is_open())
    {
        qWarning("FileSourceInput::start: file not open. not starting");
        return false;
    }

	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "FileSourceInput::start";

	if (m_ifstream.tellg() != 0) {
		m_ifstream.clear();
		m_ifstream.seekg(sizeof(FileRecord::Header), std::ios::beg);
	}

	if(!m_sampleFifo.setSize(m_settings.m_accelerationFactor * m_sampleRate * sizeof(Sample))) {
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	m_fileSourceThread = new FileSourceThread(&m_ifstream, &m_sampleFifo, m_masterTimer, &m_inputMessageQueue);
	m_fileSourceThread->setSampleRateAndSize(m_settings.m_accelerationFactor * m_sampleRate, m_sampleSize); // Fast Forward: 1 corresponds to live. 1/2 is half speed, 2 is double speed
	m_fileSourceThread->startWork();
	m_deviceDescription = "FileSource";

	mutexLocker.unlock();
	qDebug("FileSourceInput::startInput: started");

	if (getMessageQueueToGUI()) {
        MsgReportFileSourceAcquisition *report = MsgReportFileSourceAcquisition::create(true); // acquisition on
        getMessageQueueToGUI()->push(report);
	}

	return true;
}

void FileSourceInput::stop()
{
	qDebug() << "FileSourceInput::stop";
	QMutexLocker mutexLocker(&m_mutex);

	if(m_fileSourceThread != 0)
	{
		m_fileSourceThread->stopWork();
		delete m_fileSourceThread;
		m_fileSourceThread = 0;
	}

	m_deviceDescription.clear();

	if (getMessageQueueToGUI()) {
        MsgReportFileSourceAcquisition *report = MsgReportFileSourceAcquisition::create(false); // acquisition off
        getMessageQueueToGUI()->push(report);
	}
}

QByteArray FileSourceInput::serialize() const
{
    return m_settings.serialize();
}

bool FileSourceInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureFileSource* message = MsgConfigureFileSource::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (getMessageQueueToGUI())
    {
        MsgConfigureFileSource* messageToGUI = MsgConfigureFileSource::create(m_settings, true);
        getMessageQueueToGUI()->push(messageToGUI);
    }

    return success;
}

const QString& FileSourceInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FileSourceInput::getSampleRate() const
{
	return m_sampleRate;
}

quint64 FileSourceInput::getCenterFrequency() const
{
	return m_centerFrequency;
}

void FileSourceInput::setCenterFrequency(qint64 centerFrequency)
{
    FileSourceSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureFileSource* message = MsgConfigureFileSource::create(m_settings, false);
    m_inputMessageQueue.push(message);

    if (getMessageQueueToGUI())
    {
        MsgConfigureFileSource* messageToGUI = MsgConfigureFileSource::create(m_settings, false);
        getMessageQueueToGUI()->push(messageToGUI);
    }
}

quint64 FileSourceInput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool FileSourceInput::handleMessage(const Message& message)
{
    if (MsgConfigureFileSource::match(message))
    {
        MsgConfigureFileSource& conf = (MsgConfigureFileSource&) message;
        FileSourceSettings settings = conf.getSettings();
        applySettings(settings);
        return true;
    }
    else if (MsgConfigureFileSourceName::match(message))
	{
		MsgConfigureFileSourceName& conf = (MsgConfigureFileSourceName&) message;
		m_fileName = conf.getFileName();
		openFileStream();
		return true;
	}
	else if (MsgConfigureFileSourceWork::match(message))
	{
		MsgConfigureFileSourceWork& conf = (MsgConfigureFileSourceWork&) message;
		bool working = conf.isWorking();

		if (m_fileSourceThread != 0)
		{
			if (working) {
				m_fileSourceThread->startWork();
			} else {
				m_fileSourceThread->stopWork();
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
	else if (MsgConfigureFileSourceStreamTiming::match(message))
	{
		MsgReportFileSourceStreamTiming *report;

		if (m_fileSourceThread != 0)
		{
			if (getMessageQueueToGUI())
			{
                report = MsgReportFileSourceStreamTiming::create(m_fileSourceThread->getSamplesCount());
                getMessageQueueToGUI()->push(report);
			}
		}

		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "FileSourceInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initAcquisition())
            {
                m_deviceAPI->startAcquisition();
            }
        }
        else
        {
            m_deviceAPI->stopAcquisition();
        }

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendStartStop(cmd.getStartStop());
        }

        return true;
    }
    else if (FileSourceThread::MsgReportEOF::match(message))
    {
        qDebug() << "FileSourceInput::handleMessage: MsgReportEOF";
        m_fileSourceThread->stopWork();

        if (getMessageQueueToGUI())
        {
            MsgReportFileSourceStreamTiming *report = MsgReportFileSourceStreamTiming::create(m_fileSourceThread->getSamplesCount());
            getMessageQueueToGUI()->push(report);
        }

        if (m_settings.m_loop)
        {
            seekFileStream(0);
            m_fileSourceThread->startWork();
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

bool FileSourceInput::applySettings(const FileSourceSettings& settings, bool force)
{
    QList<QString> reverseAPIKeys;

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency) || force) {
        m_centerFrequency = settings.m_centerFrequency;
    }

    if ((m_settings.m_accelerationFactor != settings.m_accelerationFactor) || force)
    {
        reverseAPIKeys.append("accelerationFactor");

        if (m_fileSourceThread)
        {
            QMutexLocker mutexLocker(&m_mutex);
            if (!m_sampleFifo.setSize(m_settings.m_accelerationFactor * m_sampleRate * sizeof(Sample))) {
                qCritical("FileSourceInput::applySettings: could not reallocate sample FIFO size to %lu",
                        m_settings.m_accelerationFactor * m_sampleRate * sizeof(Sample));
            }
            m_fileSourceThread->setSampleRateAndSize(settings.m_accelerationFactor * m_sampleRate, m_sampleSize); // Fast Forward: 1 corresponds to live. 1/2 is half speed, 2 is double speed
        }
    }

    if ((m_settings.m_loop != settings.m_loop)) {
        reverseAPIKeys.append("loop");
    }
    if ((m_settings.m_fileName != settings.m_fileName)) {
        reverseAPIKeys.append("fileName");
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
    return true;
}

int FileSourceInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setFileSourceSettings(new SWGSDRangel::SWGFileSourceSettings());
    response.getFileSourceSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int FileSourceInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    FileSourceSettings settings = m_settings;

    if (deviceSettingsKeys.contains("fileName")) {
        settings.m_fileName = *response.getFileSourceSettings()->getFileName();
    }
    if (deviceSettingsKeys.contains("accelerationFactor")) {
        settings.m_accelerationFactor = response.getFileSourceSettings()->getAccelerationFactor();
    }
    if (deviceSettingsKeys.contains("loop")) {
        settings.m_loop = response.getFileSourceSettings()->getLoop() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getFileSourceSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getFileSourceSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getFileSourceSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getFileSourceSettings()->getReverseApiDeviceIndex();
    }

    MsgConfigureFileSource *msg = MsgConfigureFileSource::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureFileSource *msgToGUI = MsgConfigureFileSource::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

int FileSourceInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int FileSourceInput::webapiRun(
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

int FileSourceInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFileSourceReport(new SWGSDRangel::SWGFileSourceReport());
    response.getFileSourceReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void FileSourceInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const FileSourceSettings& settings)
{
    response.getFileSourceSettings()->setFileName(new QString(settings.m_fileName));
    response.getFileSourceSettings()->setAccelerationFactor(settings.m_accelerationFactor);
    response.getFileSourceSettings()->setLoop(settings.m_loop ? 1 : 0);

    response.getFileSourceSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getFileSourceSettings()->getReverseApiAddress()) {
        *response.getFileSourceSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getFileSourceSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getFileSourceSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getFileSourceSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void FileSourceInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    qint64 t_sec = 0;
    qint64 t_msec = 0;
    quint64 samplesCount = 0;

    if (m_fileSourceThread) {
        samplesCount = m_fileSourceThread->getSamplesCount();
    }

    if (m_sampleRate > 0)
    {
        t_sec = samplesCount / m_sampleRate;
        t_msec = (samplesCount - (t_sec * m_sampleRate)) * 1000 / m_sampleRate;
    }

    QTime t(0, 0, 0, 0);
    t = t.addSecs(t_sec);
    t = t.addMSecs(t_msec);
    response.getFileSourceReport()->setElapsedTime(new QString(t.toString("HH:mm:ss.zzz")));

    qint64 startingTimeStampMsec = m_startingTimeStamp * 1000LL;
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(startingTimeStampMsec);
    dt = dt.addSecs(t_sec);
    dt = dt.addMSecs(t_msec);
    response.getFileSourceReport()->setAbsoluteTime(new QString(dt.toString("yyyy-MM-dd HH:mm:ss.zzz")));

    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_recordLength);
    response.getFileSourceReport()->setDurationTime(new QString(recordLength.toString("HH:mm:ss")));

    response.getFileSourceReport()->setFileName(new QString(m_fileName));
    response.getFileSourceReport()->setSampleRate(m_sampleRate);
    response.getFileSourceReport()->setSampleSize(m_sampleSize);
}

void FileSourceInput::webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const FileSourceSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setTx(0);
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("FileSource"));
    swgDeviceSettings->setFileSourceSettings(new SWGSDRangel::SWGFileSourceSettings());
    SWGSDRangel::SWGFileSourceSettings *swgFileSourceSettings = swgDeviceSettings->getFileSourceSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("accelerationFactor") || force) {
        swgFileSourceSettings->setAccelerationFactor(settings.m_accelerationFactor);
    }
    if (deviceSettingsKeys.contains("loop") || force) {
        swgFileSourceSettings->setLoop(settings.m_loop);
    }
    if (deviceSettingsKeys.contains("fileName") || force) {
        swgFileSourceSettings->setFileName(new QString(settings.m_fileName));
    }

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer=new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);

    delete swgDeviceSettings;
}

void FileSourceInput::webapiReverseSendStartStop(bool start)
{
    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));

    if (start) {
        m_networkManager->sendCustomRequest(m_networkRequest, "POST");
    } else {
        m_networkManager->sendCustomRequest(m_networkRequest, "DELETE");
    }
}

void FileSourceInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "FileSourceInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("FileSourceInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
