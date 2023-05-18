///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <errno.h>
#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>
#include <QDateTime>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"

#include "device/deviceapi.h"

#include "fileoutput.h"
#include "fileoutputworker.h"

MESSAGE_CLASS_DEFINITION(FileOutput::MsgConfigureFileOutput, Message)
MESSAGE_CLASS_DEFINITION(FileOutput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(FileOutput::MsgConfigureFileOutputName, Message)
MESSAGE_CLASS_DEFINITION(FileOutput::MsgConfigureFileOutputWork, Message)
MESSAGE_CLASS_DEFINITION(FileOutput::MsgConfigureFileOutputStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FileOutput::MsgReportFileOutputGeneration, Message)
MESSAGE_CLASS_DEFINITION(FileOutput::MsgReportFileOutputStreamTiming, Message)

FileOutput::FileOutput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_fileOutputWorker(nullptr),
	m_deviceDescription("FileOutput"),
	m_startingTimeStamp(0),
	m_masterTimer(deviceAPI->getMasterTimer())
{
    m_deviceAPI->setNbSinkStreams(1);
    m_networkManager = new QNetworkAccessManager();
}

FileOutput::~FileOutput()
{
	stop();
}

void FileOutput::destroy()
{
    delete this;
}

void FileOutput::openFileStream()
{
	if (m_ofstream.is_open()) {
		m_ofstream.close();
	}

	m_ofstream.open(m_settings.m_fileName.toStdString().c_str(), std::ios::binary);

    FileRecord::Header header;
	int actualSampleRate = m_settings.m_sampleRate * (1<<m_settings.m_log2Interp);
    header.sampleRate = actualSampleRate;
    header.centerFrequency = m_settings.m_centerFrequency;
    m_startingTimeStamp = QDateTime::currentMSecsSinceEpoch();
    header.startTimeStamp = (quint64)m_startingTimeStamp;
    header.sampleSize = SDR_RX_SAMP_SZ;

    FileRecord::writeHeader(m_ofstream, header);

	qDebug() << "FileOutput::openFileStream: " << m_settings.m_fileName.toStdString().c_str();
}

void FileOutput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool FileOutput::start()
{
	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "FileOutput::start";

	openFileStream();

	m_fileOutputWorker = new FileOutputWorker(&m_ofstream, &m_sampleSourceFifo);
    m_fileOutputWorker->moveToThread(&m_fileOutputWorkerThread);
	m_fileOutputWorker->setSamplerate(m_settings.m_sampleRate);
	m_fileOutputWorker->setLog2Interpolation(m_settings.m_log2Interp);
	m_fileOutputWorker->connectTimer(m_masterTimer);
	startWorker();

	mutexLocker.unlock();
	//applySettings(m_generalSettings, m_settings, true);
	qDebug("FileOutput::start: started");

    if (getMessageQueueToGUI())
    {
        MsgReportFileOutputGeneration *report = MsgReportFileOutputGeneration::create(true); // acquisition on
        getMessageQueueToGUI()->push(report);
    }

	return true;
}

void FileOutput::stop()
{
	qDebug() << "FileSourceInput::stop";
	QMutexLocker mutexLocker(&m_mutex);

	if (m_fileOutputWorker)
	{
		stopWorker();
		delete m_fileOutputWorker;
		m_fileOutputWorker = nullptr;
	}

    if (m_ofstream.is_open()) {
        m_ofstream.close();
    }

    if (getMessageQueueToGUI())
    {
        MsgReportFileOutputGeneration *report = MsgReportFileOutputGeneration::create(false); // acquisition off
        getMessageQueueToGUI()->push(report);
    }
}

void FileOutput::startWorker()
{
    m_fileOutputWorker->startWork();
    m_fileOutputWorkerThread.start();
}

void FileOutput::stopWorker()
{
    m_fileOutputWorker->stopWork();
    m_fileOutputWorkerThread.quit();
    m_fileOutputWorkerThread.wait();
}

QByteArray FileOutput::serialize() const
{
    return m_settings.serialize();
}

bool FileOutput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureFileOutput* message = MsgConfigureFileOutput::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureFileOutput* messageToGUI = MsgConfigureFileOutput::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& FileOutput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FileOutput::getSampleRate() const
{
	return m_settings.m_sampleRate;
}

quint64 FileOutput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void FileOutput::setCenterFrequency(qint64 centerFrequency)
{
    FileOutputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureFileOutput* message = MsgConfigureFileOutput::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureFileOutput* messageToGUI = MsgConfigureFileOutput::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

std::time_t FileOutput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool FileOutput::handleMessage(const Message& message)
{
	if (MsgConfigureFileOutputName::match(message))
	{
		MsgConfigureFileOutputName& conf = (MsgConfigureFileOutputName&) message;
		m_settings.m_fileName = conf.getFileName();
		openFileStream();
		return true;
	}
	else if (MsgStartStop::match(message))
	{
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "FileOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
	else if (MsgConfigureFileOutput::match(message))
    {
	    qDebug() << "FileOutput::handleMessage: MsgConfigureFileOutput";
	    MsgConfigureFileOutput& conf = (MsgConfigureFileOutput&) message;
        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
        return true;
    }
	else if (MsgConfigureFileOutputWork::match(message))
	{
		MsgConfigureFileOutputWork& conf = (MsgConfigureFileOutputWork&) message;
		bool working = conf.isWorking();

		if (m_fileOutputWorker != 0)
		{
			if (working) {
				startWorker();
			} else {
				stopWorker();
			}
		}

		return true;
	}
	else if (MsgConfigureFileOutputStreamTiming::match(message))
	{
        MsgReportFileOutputStreamTiming *report;

		if (m_fileOutputWorker != 0 && getMessageQueueToGUI())
		{
			report = MsgReportFileOutputStreamTiming::create(m_fileOutputWorker->getSamplesCount());
			getMessageQueueToGUI()->push(report);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void FileOutput::applySettings(const FileOutputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "FileOutput::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);
    QMutexLocker mutexLocker(&m_mutex);
    bool forwardChange = false;

    if (force || settingsKeys.contains("centerFrequency"))
    {
        forwardChange = true;
    }

    if (force || settingsKeys.contains("sampleRate"))
    {
        if (m_fileOutputWorker != 0) {
            m_fileOutputWorker->setSamplerate(settings.m_sampleRate);
        }

        forwardChange = true;
    }

    if (force || settingsKeys.contains("log2Interp"))
    {
        if (m_fileOutputWorker != 0) {
            m_fileOutputWorker->setLog2Interpolation(settings.m_log2Interp);
        }

        forwardChange = true;
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

    if (forwardChange)
    {
        qDebug("FileOutput::applySettings: forward: m_centerFrequency: %llu m_sampleRate: %llu m_log2Interp: %d",
                m_settings.m_centerFrequency,
                m_settings.m_sampleRate,
                m_settings.m_log2Interp);
        DSPSignalNotification *notif = new DSPSignalNotification(m_settings.m_sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }
}

int FileOutput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setFileOutputSettings(new SWGSDRangel::SWGFileOutputSettings());
    response.getFileOutputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int FileOutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    FileOutputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureFileOutput *msg = MsgConfigureFileOutput::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureFileOutput *msgToGUI = MsgConfigureFileOutput::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

int FileOutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int FileOutput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgStartStop *messagetoGui = MsgStartStop::create(run);
        m_guiMessageQueue->push(messagetoGui);
    }

    return 200;
}

void FileOutput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const FileOutputSettings& settings)
{
    response.getFileOutputSettings()->setFileName(new QString(settings.m_fileName));
    response.getFileOutputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getFileOutputSettings()->setSampleRate(settings.m_sampleRate);
    response.getFileOutputSettings()->setLog2Interp(settings.m_log2Interp);

    response.getFileOutputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getFileOutputSettings()->getReverseApiAddress()) {
        *response.getFileOutputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getFileOutputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getFileOutputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getFileOutputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void FileOutput::webapiUpdateDeviceSettings(
        FileOutputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("fileName")) {
        settings.m_fileName = *response.getFileOutputSettings()->getFileName();
    }
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getFileOutputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("sampleRate")) {
        settings.m_sampleRate = response.getFileOutputSettings()->getSampleRate();
    }
    if (deviceSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getFileOutputSettings()->getLog2Interp();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getFileOutputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getFileOutputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getFileOutputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getFileOutputSettings()->getReverseApiDeviceIndex();
    }
}

void FileOutput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const FileOutputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("FileOutput"));
    swgDeviceSettings->setFileOutputSettings(new SWGSDRangel::SWGFileOutputSettings());
    SWGSDRangel::SWGFileOutputSettings *swgFileOutputSettings = swgDeviceSettings->getFileOutputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgFileOutputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("sampleRate") || force) {
        swgFileOutputSettings->setSampleRate(settings.m_sampleRate);
    }
    if (deviceSettingsKeys.contains("log2Interp") || force) {
        swgFileOutputSettings->setLog2Interp(settings.m_log2Interp);
    }
    if (deviceSettingsKeys.contains("fileName") || force) {
        swgFileOutputSettings->setFileName(new QString(settings.m_fileName));
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

void FileOutput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("FileOutput"));

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

void FileOutput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "FileOutput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("FileOutput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
