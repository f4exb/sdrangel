///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGSDRdaemonSourceReport.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/devicesourceapi.h"
#include "dsp/filerecord.h"

#include "sdrdaemonsourceinput.h"
#include "sdrdaemonsourceudphandler.h"

MESSAGE_CLASS_DEFINITION(SDRdaemonSourceInput::MsgConfigureSDRdaemonSource, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSourceInput::MsgConfigureSDRdaemonStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSourceInput::MsgReportSDRdaemonAcquisition, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSourceInput::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSourceInput::MsgStartStop, Message)

SDRdaemonSourceInput::SDRdaemonSourceInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
	m_SDRdaemonUDPHandler(0),
	m_deviceDescription(),
	m_startingTimeStamp(0)
{
	m_sampleFifo.setSize(96000 * 4);
	m_SDRdaemonUDPHandler = new SDRdaemonSourceUDPHandler(&m_sampleFifo, m_deviceAPI);

    m_fileSink = new FileRecord(QString("test_%1.sdriq").arg(m_deviceAPI->getDeviceUID()));
    m_deviceAPI->addSink(m_fileSink);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

SDRdaemonSourceInput::~SDRdaemonSourceInput()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
	stop();
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
	delete m_SDRdaemonUDPHandler;
}

void SDRdaemonSourceInput::destroy()
{
    delete this;
}

void SDRdaemonSourceInput::init()
{
    applySettings(m_settings, true);
}

bool SDRdaemonSourceInput::start()
{
	qDebug() << "SDRdaemonSourceInput::start";
    m_SDRdaemonUDPHandler->start();
	return true;
}

void SDRdaemonSourceInput::stop()
{
	qDebug() << "SDRdaemonSourceInput::stop";
    m_SDRdaemonUDPHandler->stop();
}

QByteArray SDRdaemonSourceInput::serialize() const
{
    return m_settings.serialize();
}

bool SDRdaemonSourceInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureSDRdaemonSource* message = MsgConfigureSDRdaemonSource::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureSDRdaemonSource* messageToGUI = MsgConfigureSDRdaemonSource::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

void SDRdaemonSourceInput::setMessageQueueToGUI(MessageQueue *queue)
{
    m_guiMessageQueue = queue;
    m_SDRdaemonUDPHandler->setMessageQueueToGUI(queue);
}

const QString& SDRdaemonSourceInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int SDRdaemonSourceInput::getSampleRate() const
{
    return m_SDRdaemonUDPHandler->getSampleRate();
}

quint64 SDRdaemonSourceInput::getCenterFrequency() const
{
    return m_SDRdaemonUDPHandler->getCenterFrequency();
}

void SDRdaemonSourceInput::setCenterFrequency(qint64 centerFrequency)
{
    (void) centerFrequency;
}

std::time_t SDRdaemonSourceInput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool SDRdaemonSourceInput::isStreaming() const
{
    return m_SDRdaemonUDPHandler->isStreaming();
}

bool SDRdaemonSourceInput::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        return m_fileSink->handleMessage(notif); // forward to file sink
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "SDRdaemonSourceInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop())
        {
            if (m_settings.m_fileRecordName.size() != 0) {
                m_fileSink->setFileName(m_settings.m_fileRecordName);
            } else {
                m_fileSink->genUniqueFileName(m_deviceAPI->getDeviceUID());
            }

            m_fileSink->startRecording();
        }
        else
        {
            m_fileSink->stopRecording();
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "SDRdaemonSourceInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else if (MsgConfigureSDRdaemonSource::match(message))
    {
        qDebug() << "SDRdaemonSourceInput::handleMessage:" << message.getIdentifier();
        MsgConfigureSDRdaemonSource& conf = (MsgConfigureSDRdaemonSource&) message;
        applySettings(conf.getSettings(), conf.getForce());
        return true;
    }
	else
	{
		return false;
	}
}

void SDRdaemonSourceInput::applySettings(const SDRdaemonSourceSettings& settings, bool force)
{
    QMutexLocker mutexLocker(&m_mutex);
    std::ostringstream os;
    QString remoteAddress;
    m_SDRdaemonUDPHandler->getRemoteAddress(remoteAddress);
    QList<QString> reverseAPIKeys;

    if ((m_settings.m_dcBlock != settings.m_dcBlock) || force) {
        reverseAPIKeys.append("dcBlock");
    }
    if ((m_settings.m_iqCorrection != settings.m_iqCorrection) || force) {
        reverseAPIKeys.append("iqCorrection");
    }
    if ((m_settings.m_dataAddress != settings.m_dataAddress) || force) {
        reverseAPIKeys.append("dataAddress");
    }
    if ((m_settings.m_dataPort != settings.m_dataPort) || force) {
        reverseAPIKeys.append("dataPort");
    }
    if ((m_settings.m_apiAddress != settings.m_apiAddress) || force) {
        reverseAPIKeys.append("apiAddress");
    }
    if ((m_settings.m_apiPort != settings.m_apiPort) || force) {
        reverseAPIKeys.append("apiPort");
    }
    if ((m_settings.m_fileRecordName != settings.m_fileRecordName) || force) {
        reverseAPIKeys.append("fileRecordName");
    }

    if ((m_settings.m_dcBlock != settings.m_dcBlock) || (m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
        qDebug("SDRdaemonSourceInput::applySettings: corrections: DC block: %s IQ imbalance: %s",
                settings.m_dcBlock ? "true" : "false",
                settings.m_iqCorrection ? "true" : "false");
    }

    m_SDRdaemonUDPHandler->configureUDPLink(settings.m_dataAddress, settings.m_dataPort);
    m_SDRdaemonUDPHandler->getRemoteAddress(remoteAddress);

    mutexLocker.unlock();

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
    m_remoteAddress = remoteAddress;

    qDebug() << "SDRdaemonSourceInput::applySettings: "
            << " m_dataAddress: " << m_settings.m_dataAddress
            << " m_dataPort: " << m_settings.m_dataPort
            << " m_apiAddress: " << m_settings.m_apiAddress
            << " m_apiPort: " << m_settings.m_apiPort
            << " m_remoteAddress: " << m_remoteAddress;
}

int SDRdaemonSourceInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int SDRdaemonSourceInput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run);
        m_guiMessageQueue->push(msgToGUI);
    }

    return 200;
}

int SDRdaemonSourceInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setSdrDaemonSourceSettings(new SWGSDRangel::SWGSDRdaemonSourceSettings());
    response.getSdrDaemonSourceSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int SDRdaemonSourceInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    SDRdaemonSourceSettings settings = m_settings;

    if (deviceSettingsKeys.contains("apiAddress")) {
        settings.m_apiAddress = *response.getSdrDaemonSourceSettings()->getApiAddress();
    }
    if (deviceSettingsKeys.contains("apiPort")) {
        settings.m_apiPort = response.getSdrDaemonSourceSettings()->getApiPort();
    }
    if (deviceSettingsKeys.contains("dataAddress")) {
        settings.m_dataAddress = *response.getSdrDaemonSourceSettings()->getDataAddress();
    }
    if (deviceSettingsKeys.contains("dataPort")) {
        settings.m_dataPort = response.getSdrDaemonSourceSettings()->getDataPort();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getSdrDaemonSourceSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getSdrDaemonSourceSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("fileRecordName")) {
        settings.m_fileRecordName = *response.getSdrDaemonSourceSettings()->getFileRecordName();
    }

    MsgConfigureSDRdaemonSource *msg = MsgConfigureSDRdaemonSource::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSDRdaemonSource *msgToGUI = MsgConfigureSDRdaemonSource::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void SDRdaemonSourceInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const SDRdaemonSourceSettings& settings)
{
    response.getSdrDaemonSourceSettings()->setApiAddress(new QString(settings.m_apiAddress));
    response.getSdrDaemonSourceSettings()->setApiPort(settings.m_apiPort);
    response.getSdrDaemonSourceSettings()->setDataAddress(new QString(settings.m_dataAddress));
    response.getSdrDaemonSourceSettings()->setDataPort(settings.m_dataPort);
    response.getSdrDaemonSourceSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getSdrDaemonSourceSettings()->setIqCorrection(settings.m_iqCorrection);

    if (response.getSdrDaemonSourceSettings()->getFileRecordName()) {
        *response.getSdrDaemonSourceSettings()->getFileRecordName() = settings.m_fileRecordName;
    } else {
        response.getSdrDaemonSourceSettings()->setFileRecordName(new QString(settings.m_fileRecordName));
    }
}

int SDRdaemonSourceInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setSdrDaemonSourceReport(new SWGSDRangel::SWGSDRdaemonSourceReport());
    response.getSdrDaemonSourceReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void SDRdaemonSourceInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getSdrDaemonSourceReport()->setCenterFrequency(m_SDRdaemonUDPHandler->getCenterFrequency());
    response.getSdrDaemonSourceReport()->setSampleRate(m_SDRdaemonUDPHandler->getSampleRate());
    response.getSdrDaemonSourceReport()->setBufferRwBalance(m_SDRdaemonUDPHandler->getBufferGauge());

    QDateTime dt = QDateTime::fromMSecsSinceEpoch(m_SDRdaemonUDPHandler->getTVmSec());
    response.getSdrDaemonSourceReport()->setDaemonTimestamp(new QString(dt.toString("yyyy-MM-dd  HH:mm:ss.zzz")));

    response.getSdrDaemonSourceReport()->setMinNbBlocks(m_SDRdaemonUDPHandler->getMinNbBlocks());
    response.getSdrDaemonSourceReport()->setMaxNbRecovery(m_SDRdaemonUDPHandler->getMaxNbRecovery());
}

void SDRdaemonSourceInput::webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const SDRdaemonSourceSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setTx(0);
    swgDeviceSettings->setDeviceHwType(new QString("SDRdaemonSource"));
    swgDeviceSettings->setSdrDaemonSourceSettings(new SWGSDRangel::SWGSDRdaemonSourceSettings());
    SWGSDRangel::SWGSDRdaemonSourceSettings *swgSDRDaemonSourceSettings = swgDeviceSettings->getSdrDaemonSourceSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("apiAddress") || force) {
        swgSDRDaemonSourceSettings->setApiAddress(new QString(settings.m_apiAddress));
    }
    if (deviceSettingsKeys.contains("apiPort") || force) {
        swgSDRDaemonSourceSettings->setApiPort(settings.m_apiPort);
    }
    if (deviceSettingsKeys.contains("dataAddress") || force) {
        swgSDRDaemonSourceSettings->setDataAddress(new QString(settings.m_dataAddress));
    }
    if (deviceSettingsKeys.contains("dataPort") || force) {
        swgSDRDaemonSourceSettings->setDataPort(settings.m_dataPort);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgSDRDaemonSourceSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgSDRDaemonSourceSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("fileRecordName") || force) {
        swgSDRDaemonSourceSettings->setFileRecordName(new QString(settings.m_fileRecordName));
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

void SDRdaemonSourceInput::webapiReverseSendStartStop(bool start)
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

void SDRdaemonSourceInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "SDRdaemonSourceInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("SDRdaemonSourceInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
