///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"
#include "SWGSimplePTTReport.h"
#include "SWGDeviceState.h"

#include "dsp/dspengine.h"

#include "gs232controllerworker.h"
#include "gs232controller.h"

MESSAGE_CLASS_DEFINITION(GS232Controller::MsgConfigureGS232Controller, Message)
MESSAGE_CLASS_DEFINITION(GS232Controller::MsgStartStop, Message)

const QString GS232Controller::m_featureIdURI = "sdrangel.feature.gs232controller";
const QString GS232Controller::m_featureId = "GS232Controller";

GS232Controller::GS232Controller(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_ptt(false)
{
    qDebug("GS232Controller::GS232Controller: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_worker = new GS232ControllerWorker(webAPIAdapterInterface);
    m_state = StIdle;
    m_errorMessage = "GS232Controller error";
}

GS232Controller::~GS232Controller()
{
    if (m_worker->isRunning()) {
        stop();
    }

    delete m_worker;
}

void GS232Controller::start()
{
    qDebug("GS232Controller::start");

    m_worker->reset();
    m_worker->setMessageQueueToFeature(getInputMessageQueue());
    m_worker->setMessageQueueToGUI(getMessageQueueToGUI());
    bool ok = m_worker->startWork();
    m_state = ok ? StRunning : StError;
    m_thread.start();

    GS232ControllerWorker::MsgConfigureGS232ControllerWorker *msg = GS232ControllerWorker::MsgConfigureGS232ControllerWorker::create(m_settings, true);
    m_worker->getInputMessageQueue()->push(msg);
}

void GS232Controller::stop()
{
    qDebug("GS232Controller::stop");
    m_worker->stopWork();
    m_state = StIdle;
    m_thread.quit();
    m_thread.wait();
}

bool GS232Controller::handleMessage(const Message& cmd)
{
    if (MsgConfigureGS232Controller::match(cmd))
    {
        MsgConfigureGS232Controller& cfg = (MsgConfigureGS232Controller&) cmd;
        qDebug() << "GS232Controller::handleMessage: MsgConfigureGS232Controller";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "GS232Controller::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
    else if (GS232ControllerSettings::MsgChannelIndexChange::match(cmd))
    {
        GS232ControllerSettings::MsgChannelIndexChange& cfg = (GS232ControllerSettings::MsgChannelIndexChange&) cmd;
        int newChannelIndex = cfg.getIndex();
        qDebug() << "GS232Controller::handleMessage: MsgChannelIndexChange: " << newChannelIndex;
        GS232ControllerSettings settings = m_settings;
        settings.m_channelIndex = newChannelIndex;
        applySettings(settings, false);

        if (getMessageQueueToGUI())
        {
            GS232ControllerSettings::MsgChannelIndexChange *msg = new GS232ControllerSettings::MsgChannelIndexChange(cfg);
            getMessageQueueToGUI()->push(msg);
        }

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray GS232Controller::serialize() const
{
    return m_settings.serialize();
}

bool GS232Controller::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureGS232Controller *msg = MsgConfigureGS232Controller::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureGS232Controller *msg = MsgConfigureGS232Controller::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void GS232Controller::applySettings(const GS232ControllerSettings& settings, bool force)
{
    qDebug() << "GS232Controller::applySettings:"
            << " m_azimuth: " << settings.m_azimuth
            << " m_elevation: " << settings.m_elevation
            << " m_serialPort: " << settings.m_serialPort
            << " m_baudRate: " << settings.m_baudRate
            << " m_track: " << settings.m_track
            << " m_deviceIndex: " << settings.m_deviceIndex
            << " m_channelIndex: " << settings.m_channelIndex
            << " m_title: " << settings.m_title
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIFeatureSetIndex: " << settings.m_reverseAPIFeatureSetIndex
            << " m_reverseAPIFeatureIndex: " << settings.m_reverseAPIFeatureIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_azimuth != settings.m_azimuth) || force) {
        reverseAPIKeys.append("azimuth");
    }
    if ((m_settings.m_elevation != settings.m_elevation) || force) {
        reverseAPIKeys.append("elevation");
    }
    if ((m_settings.m_serialPort != settings.m_serialPort) || force) {
        reverseAPIKeys.append("serialPort");
    }
    if ((m_settings.m_baudRate != settings.m_baudRate) || force) {
        reverseAPIKeys.append("baudRate");
    }
    if ((m_settings.m_track != settings.m_track) || force) {
        reverseAPIKeys.append("track");
    }
    if ((m_settings.m_deviceIndex != settings.m_deviceIndex) || force) {
        reverseAPIKeys.append("deviceIndex");
    }
    if ((m_settings.m_channelIndex != settings.m_channelIndex) || force) {
        reverseAPIKeys.append("channelIndex");
    }
    if ((m_settings.m_title != settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((m_settings.m_rgbColor != settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }

    GS232ControllerWorker::MsgConfigureGS232ControllerWorker *msg = GS232ControllerWorker::MsgConfigureGS232ControllerWorker::create(
        settings, force
    );
    m_worker->getInputMessageQueue()->push(msg);

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIFeatureSetIndex != settings.m_reverseAPIFeatureSetIndex) ||
                (m_settings.m_reverseAPIFeatureIndex != settings.m_reverseAPIFeatureIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

int GS232Controller::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int GS232Controller::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setGs232ControllerSettings(new SWGSDRangel::SWGGS232ControllerSettings());
    response.getGs232ControllerSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int GS232Controller::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    GS232ControllerSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureGS232Controller *msg = MsgConfigureGS232Controller::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("GS232Controller::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureGS232Controller *msgToGUI = MsgConfigureGS232Controller::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

void GS232Controller::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const GS232ControllerSettings& settings)
{
    response.getGs232ControllerSettings()->setAzimuth(settings.m_azimuth);
    response.getGs232ControllerSettings()->setElevation(settings.m_elevation);
    response.getGs232ControllerSettings()->setSerialPort(new QString(settings.m_serialPort));
    response.getGs232ControllerSettings()->setBaudRate(settings.m_baudRate);
    response.getGs232ControllerSettings()->setTrack(settings.m_track);
    response.getGs232ControllerSettings()->setDeviceIndex(settings.m_deviceIndex);
    response.getGs232ControllerSettings()->setChannelIndex(settings.m_channelIndex);

    if (response.getGs232ControllerSettings()->getTitle()) {
        *response.getGs232ControllerSettings()->getTitle() = settings.m_title;
    } else {
        response.getGs232ControllerSettings()->setTitle(new QString(settings.m_title));
    }

    response.getGs232ControllerSettings()->setRgbColor(settings.m_rgbColor);
    response.getGs232ControllerSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getGs232ControllerSettings()->getReverseApiAddress()) {
        *response.getGs232ControllerSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getGs232ControllerSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getGs232ControllerSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getGs232ControllerSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getGs232ControllerSettings()->setReverseApiChannelIndex(settings.m_reverseAPIFeatureIndex);
}

void GS232Controller::webapiUpdateFeatureSettings(
    GS232ControllerSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("azimuth")) {
        settings.m_azimuth = response.getGs232ControllerSettings()->getAzimuth();
    }
    if (featureSettingsKeys.contains("elevation")) {
        settings.m_elevation = response.getGs232ControllerSettings()->getElevation();
    }
    if (featureSettingsKeys.contains("serialPort")) {
        settings.m_serialPort = *response.getGs232ControllerSettings()->getSerialPort();
    }
    if (featureSettingsKeys.contains("baudRate")) {
        settings.m_serialPort = response.getGs232ControllerSettings()->getBaudRate();
    }
    if (featureSettingsKeys.contains("track")) {
        settings.m_track = response.getGs232ControllerSettings()->getTrack() != 0;
    }
    if (featureSettingsKeys.contains("deviceIndex")) {
        settings.m_deviceIndex = response.getGs232ControllerSettings()->getDeviceIndex();
    }
    if (featureSettingsKeys.contains("channelIndex")) {
        settings.m_channelIndex = response.getGs232ControllerSettings()->getChannelIndex();
    }
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getGs232ControllerSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getGs232ControllerSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getGs232ControllerSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getGs232ControllerSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getGs232ControllerSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getGs232ControllerSettings()->getReverseApiDeviceIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getGs232ControllerSettings()->getReverseApiChannelIndex();
    }
}

void GS232Controller::webapiReverseSendSettings(QList<QString>& featureSettingsKeys, const GS232ControllerSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("GS232Controller"));
    swgFeatureSettings->setGs232ControllerSettings(new SWGSDRangel::SWGGS232ControllerSettings());
    SWGSDRangel::SWGGS232ControllerSettings *swgGS232ControllerSettings = swgFeatureSettings->getGs232ControllerSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("azimuth") || force) {
        swgGS232ControllerSettings->setAzimuth(settings.m_azimuth);
    }
    if (featureSettingsKeys.contains("elevation") || force) {
        swgGS232ControllerSettings->setElevation(settings.m_elevation);
    }
    if (featureSettingsKeys.contains("serialPort") || force) {
        swgGS232ControllerSettings->setSerialPort(new QString(settings.m_serialPort));
    }
    if (featureSettingsKeys.contains("baudRate") || force) {
        swgGS232ControllerSettings->setBaudRate(settings.m_baudRate);
    }
    if (featureSettingsKeys.contains("track") || force) {
        swgGS232ControllerSettings->setTrack(settings.m_track);
    }
    if (featureSettingsKeys.contains("deviceIndex") || force) {
        swgGS232ControllerSettings->setDeviceIndex(settings.m_deviceIndex);
    }
    if (featureSettingsKeys.contains("channelIndex") || force) {
        swgGS232ControllerSettings->setChannelIndex(settings.m_channelIndex);
    }
    if (featureSettingsKeys.contains("title") || force) {
        swgGS232ControllerSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgGS232ControllerSettings->setRgbColor(settings.m_rgbColor);
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/featureset/%3/feature/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIFeatureSetIndex)
            .arg(settings.m_reverseAPIFeatureIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgFeatureSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgFeatureSettings;
}

void GS232Controller::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "GS232Controller::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("GS232Controller::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
