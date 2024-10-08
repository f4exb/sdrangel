///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Copyright (C) 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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
#include <QJsonParseError>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGRemoteTCPInputReport.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"

#include "remotetcpinput.h"
#include "remotetcpinputtcphandler.h"

MESSAGE_CLASS_DEFINITION(RemoteTCPInput::MsgConfigureRemoteTCPInput, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPInput::MsgReportTCPBuffer, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPInput::MsgSaveReplay, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPInput::MsgSendMessage, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPInput::MsgReportPosition, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPInput::MsgReportDirection, Message)

RemoteTCPInput::RemoteTCPInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_remoteInputTCPPHandler(nullptr),
    m_deviceDescription("RemoteTCPInput"),
    m_running(false),
    m_latitude(std::numeric_limits<float>::quiet_NaN()),
    m_longitude(std::numeric_limits<float>::quiet_NaN()),
    m_altitude(std::numeric_limits<float>::quiet_NaN()),
    m_isotropic(false),
    m_azimuth(std::numeric_limits<float>::quiet_NaN()),
    m_elevation(std::numeric_limits<float>::quiet_NaN())
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_sampleFifo.setSize(48000 * 8);
    m_remoteInputTCPPHandler = new RemoteTCPInputTCPHandler(&m_sampleFifo, m_deviceAPI, &m_replayBuffer);
    m_remoteInputTCPPHandler->moveToThread(&m_thread);
    m_remoteInputTCPPHandler->setMessageQueueToInput(&m_inputMessageQueue);

    m_deviceAPI->setNbSourceStreams(1);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RemoteTCPInput::networkManagerFinished
    );
}

RemoteTCPInput::~RemoteTCPInput()
{
    qDebug() << "RemoteTCPInput::~RemoteTCPInput";
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RemoteTCPInput::networkManagerFinished
    );
    delete m_networkManager;
    stop();
    m_remoteInputTCPPHandler->deleteLater();
}

void RemoteTCPInput::destroy()
{
    delete this;
}

void RemoteTCPInput::init()
{
    qDebug() << "RemoteTCPInput::init";
    applySettings(m_settings, QList<QString>(), true);
}

bool RemoteTCPInput::start()
{
    qDebug() << "RemoteTCPInput::start";
    if (m_running) {
        qDebug() << "RemoteTCPInput::stop - Already running";
        return true;
    }
    m_remoteInputTCPPHandler->reset();
    m_remoteInputTCPPHandler->start();
    m_remoteInputTCPPHandler->getInputMessageQueue()->push(RemoteTCPInputTCPHandler::MsgConfigureTcpHandler::create(m_settings, QList<QString>(), true));
    m_thread.start();
    m_running = true;
    return true;
}

void RemoteTCPInput::stop()
{
    qDebug() << "RemoteTCPInput::stop";
    if (!m_running) {
        // For wasm, important not to call m_remoteInputTCPPHandler->stop() twice
        // as mutex can deadlock when this object is being deleted
        return;
    }
    m_remoteInputTCPPHandler->stop();
    m_thread.quit();
#ifndef __EMSCRIPTEN__
    m_thread.wait();
#endif
    m_running = false;
}

QByteArray RemoteTCPInput::serialize() const
{
    return m_settings.serialize();
}

bool RemoteTCPInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }
    MsgConfigureRemoteTCPInput* message = MsgConfigureRemoteTCPInput::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureRemoteTCPInput* messageToGUI = MsgConfigureRemoteTCPInput::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

void RemoteTCPInput::setMessageQueueToGUI(MessageQueue *queue)
{
    m_guiMessageQueue = queue;
    m_remoteInputTCPPHandler->setMessageQueueToGUI(queue);
}

const QString& RemoteTCPInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int RemoteTCPInput::getSampleRate() const
{
    return m_settings.m_channelSampleRate;
}

quint64 RemoteTCPInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency + m_settings.m_inputFrequencyOffset;
}

void RemoteTCPInput::setCenterFrequency(qint64 centerFrequency)
{
    RemoteTCPInputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureRemoteTCPInput* message = MsgConfigureRemoteTCPInput::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureRemoteTCPInput* messageToGUI = MsgConfigureRemoteTCPInput::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool RemoteTCPInput::handleMessage(const Message& message)
{
    if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "RemoteTCPInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else if (MsgConfigureRemoteTCPInput::match(message))
    {
        qDebug() << "RemoteTCPInput::handleMessage:" << message.getIdentifier();
        const MsgConfigureRemoteTCPInput& conf = (const MsgConfigureRemoteTCPInput&) message;
        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
        return true;
    }
    else if (RemoteTCPInputTCPHandler::MsgReportConnection::match(message))
    {
        qDebug() << "RemoteTCPInput::handleMessage:" << message.getIdentifier();
        const RemoteTCPInputTCPHandler::MsgReportConnection& report = (const RemoteTCPInputTCPHandler::MsgReportConnection&) message;
        if (report.getConnected())
        {
            qDebug() << "Disconnected - stopping DSP";
            m_deviceAPI->stopDeviceEngine();
        }
        return true;
    }
    else if (MsgSaveReplay::match(message))
    {
        const MsgSaveReplay& cmd = (const MsgSaveReplay&) message;
        m_replayBuffer.save(cmd.getFilename(), m_settings.m_devSampleRate, getCenterFrequency());
        return true;
    }
    else if (MsgSendMessage::match(message))
    {
        const MsgSendMessage& msg = (const MsgSendMessage&) message;
        m_remoteInputTCPPHandler->getInputMessageQueue()->push(MsgSendMessage::create(msg.getCallsign(), msg.getText(), msg.getBroadcast()));
        return true;
    }
    else if (MsgReportPosition::match(message))
    {
        const MsgReportPosition& report = (const MsgReportPosition&) message;

        m_latitude = report.getLatitude();
        m_longitude = report.getLongitude();
        m_altitude = report.getAltitude();

        emit positionChanged(m_latitude, m_longitude, m_altitude);

        return true;
    }
    else if (MsgReportDirection::match(message))
    {
        const MsgReportDirection& report = (const MsgReportDirection&) message;

        m_isotropic = report.getIsotropic();
        m_azimuth = report.getAzimuth();
        m_elevation = report.getElevation();

        emit directionChanged(m_isotropic, m_azimuth, m_elevation);

        return true;
    }
    else
    {
        return false;
    }
}

void RemoteTCPInput::applySettings(const RemoteTCPInputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "RemoteTCPInput::applySettings: force: " << force << settings.getDebugString(settingsKeys, force);
    QMutexLocker mutexLocker(&m_mutex);
    std::ostringstream os;
    bool forwardChange = false;

    // Should this only be applied if not applied on remote?
    if (settingsKeys.contains("dcBlock") || settingsKeys.contains("iqCorrection") || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
        qDebug("RemoteTCPInput::applySettings: corrections: DC block: %s IQ imbalance: %s",
                settings.m_dcBlock ? "true" : "false",
                settings.m_iqCorrection ? "true" : "false");
    }

    if (settingsKeys.contains("centerFrequency") || force) {
        forwardChange = true;
    }
    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        forwardChange = true;
    }
    if (settingsKeys.contains("channelSampleRate") || force) {
        forwardChange = true;
    }

    if ((settingsKeys.contains("channelSampleRate") || force)
        && (settings.m_devSampleRate != m_settings.m_devSampleRate))
    {
        m_replayBuffer.clear();
    }

    mutexLocker.unlock();

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
            settingsKeys.contains("reverseAPIAddress") ||
            settingsKeys.contains("reverseAPIPort") ||
            settingsKeys.contains("reverseAPIDeviceIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (forwardChange && (settings.m_channelSampleRate != 0))
    {
        DSPSignalNotification *notif = new DSPSignalNotification(settings.m_channelSampleRate, settings.m_centerFrequency + settings.m_inputFrequencyOffset);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (settingsKeys.contains("replayLength") || settingsKeys.contains("devSampleRate") || force) {
        m_replayBuffer.setSize(m_settings.m_replayLength, m_settings.m_devSampleRate);
    }

    if (settingsKeys.contains("replayOffset") || settingsKeys.contains("devSampleRate")  || force) {
        m_replayBuffer.setReadOffset(((unsigned)(m_settings.m_replayOffset * m_settings.m_devSampleRate)) * 2);
    }

    if (settingsKeys.contains("replayLoop") || force) {
        m_replayBuffer.setLoop(m_settings.m_replayLoop);
    }

    m_remoteInputTCPPHandler->getInputMessageQueue()->push(RemoteTCPInputTCPHandler::MsgConfigureTcpHandler::create(m_settings, settingsKeys, force));
}

int RemoteTCPInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int RemoteTCPInput::webapiRun(
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

int RemoteTCPInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setRemoteTcpInputSettings(new SWGSDRangel::SWGRemoteTCPInputSettings());
    response.getRemoteTcpInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int RemoteTCPInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    RemoteTCPInputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureRemoteTCPInput *msg = MsgConfigureRemoteTCPInput::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRemoteTCPInput *msgToGUI = MsgConfigureRemoteTCPInput::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void RemoteTCPInput::webapiUpdateDeviceSettings(
    RemoteTCPInputSettings& settings,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getRemoteTcpInputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("loPpmCorrection")) {
        settings.m_loPpmCorrection = response.getRemoteTcpInputSettings()->getLoPpmCorrection();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getRemoteTcpInputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getRemoteTcpInputSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("biasTee")) {
        settings.m_biasTee = response.getRemoteTcpInputSettings()->getBiasTee() != 0;
    }
    if (deviceSettingsKeys.contains("directSampling")) {
        settings.m_directSampling = response.getRemoteTcpInputSettings()->getDirectSampling() != 0;
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getRemoteTcpInputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getRemoteTcpInputSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("agc")) {
        settings.m_agc = response.getRemoteTcpInputSettings()->getAgc() != 0;
    }
    if (deviceSettingsKeys.contains("rfBW")) {
        settings.m_rfBW = response.getRemoteTcpInputSettings()->getRfBw();
    }
    if (deviceSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getRemoteTcpInputSettings()->getInputFrequencyOffset();
    }
    if (deviceSettingsKeys.contains("channelGain")) {
        settings.m_channelGain = response.getRemoteTcpInputSettings()->getChannelGain();
    }
    if (deviceSettingsKeys.contains("channelSampleRate")) {
        settings.m_channelSampleRate = response.getRemoteTcpInputSettings()->getChannelSampleRate();
    }
    if (deviceSettingsKeys.contains("channelDecimation")) {
        settings.m_channelDecimation = response.getRemoteTcpInputSettings()->getChannelDecimation();
    }
    if (deviceSettingsKeys.contains("sampleBits")) {
        settings.m_sampleBits = response.getRemoteTcpInputSettings()->getSampleBits();
    }
    if (deviceSettingsKeys.contains("dataAddress")) {
        settings.m_dataAddress = *response.getRemoteTcpInputSettings()->getDataAddress();
    }
    if (deviceSettingsKeys.contains("dataPort")) {
        settings.m_dataPort = response.getRemoteTcpInputSettings()->getDataPort();
    }
    if (deviceSettingsKeys.contains("overrideRemoteSettings")) {
        settings.m_overrideRemoteSettings = response.getRemoteTcpInputSettings()->getOverrideRemoteSettings() != 0;
    }
    if (deviceSettingsKeys.contains("preFill")) {
        settings.m_preFill = response.getRemoteTcpInputSettings()->getPreFill() != 0;
    }
    if (deviceSettingsKeys.contains("protocol")) {
        settings.m_protocol = *response.getRemoteTcpInputSettings()->getProtocol();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRemoteTcpInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRemoteTcpInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRemoteTcpInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getRemoteTcpInputSettings()->getReverseApiDeviceIndex();
    }
}

void RemoteTCPInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const RemoteTCPInputSettings& settings)
{
    response.getRemoteTcpInputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getRemoteTcpInputSettings()->setLoPpmCorrection(settings.m_loPpmCorrection);
    response.getRemoteTcpInputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getRemoteTcpInputSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getRemoteTcpInputSettings()->setBiasTee(settings.m_biasTee ? 1 : 0);
    response.getRemoteTcpInputSettings()->setDirectSampling(settings.m_directSampling ? 1 : 0);
    response.getRemoteTcpInputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getRemoteTcpInputSettings()->setLog2Decim(settings.m_log2Decim);
    response.getRemoteTcpInputSettings()->setGain(settings.m_gain[0]);
    response.getRemoteTcpInputSettings()->setAgc(settings.m_agc ? 1 : 0);
    response.getRemoteTcpInputSettings()->setRfBw(settings.m_rfBW);
    response.getRemoteTcpInputSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getRemoteTcpInputSettings()->setChannelGain(settings.m_channelGain);
    response.getRemoteTcpInputSettings()->setChannelSampleRate(settings.m_channelSampleRate);
    response.getRemoteTcpInputSettings()->setChannelDecimation(settings.m_channelDecimation);
    response.getRemoteTcpInputSettings()->setSampleBits(settings.m_sampleBits);
    response.getRemoteTcpInputSettings()->setDataAddress(new QString(settings.m_dataAddress));
    response.getRemoteTcpInputSettings()->setDataPort(settings.m_dataPort);
    response.getRemoteTcpInputSettings()->setOverrideRemoteSettings(settings.m_overrideRemoteSettings ? 1 : 0);
    response.getRemoteTcpInputSettings()->setPreFill(settings.m_preFill ? 1 : 0);
    response.getRemoteTcpInputSettings()->setProtocol(new QString(settings.m_protocol));

    response.getRemoteTcpInputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRemoteTcpInputSettings()->getReverseApiAddress()) {
        *response.getRemoteTcpInputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRemoteTcpInputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRemoteTcpInputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRemoteTcpInputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int RemoteTCPInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRemoteTcpInputReport(new SWGSDRangel::SWGRemoteTCPInputReport());
    response.getRemoteTcpInputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void RemoteTCPInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getRemoteTcpInputReport()->setSampleRate(m_settings.m_channelSampleRate);
    response.getRemoteTcpInputReport()->setLatitude(m_latitude);
    response.getRemoteTcpInputReport()->setLongitude(m_longitude);
    response.getRemoteTcpInputReport()->setAltitude(m_altitude);
}

void RemoteTCPInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const RemoteTCPInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("RemoteTCPInput"));
    swgDeviceSettings->setRemoteTcpInputSettings(new SWGSDRangel::SWGRemoteTCPInputSettings());
    SWGSDRangel::SWGRemoteTCPInputSettings *swgRemoteTCPInputSettings = swgDeviceSettings->getRemoteTcpInputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgRemoteTCPInputSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgRemoteTCPInputSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("biasTee") || force) {
        swgRemoteTCPInputSettings->setBiasTee(settings.m_biasTee ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("dataAddress") || force) {
        swgRemoteTCPInputSettings->setDataAddress(new QString(settings.m_dataAddress));
    }
    if (deviceSettingsKeys.contains("dataPort") || force) {
        swgRemoteTCPInputSettings->setDataPort(settings.m_dataPort);
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

void RemoteTCPInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("RemoteTCPInput"));

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

void RemoteTCPInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "RemoteTCPInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("RemoteTCPInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
