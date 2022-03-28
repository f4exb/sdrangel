///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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
#include "SWGDeviceState.h"

#include "dsp/dspengine.h"

#include "device/deviceset.h"
#include "channel/channelapi.h"
#include "settings/serializable.h"
#include "maincore.h"
#include "aprsworker.h"
#include "aprs.h"

MESSAGE_CLASS_DEFINITION(APRS::MsgConfigureAPRS, Message)
MESSAGE_CLASS_DEFINITION(APRS::MsgReportWorker, Message)
MESSAGE_CLASS_DEFINITION(APRS::MsgQueryAvailableChannels, Message)
MESSAGE_CLASS_DEFINITION(APRS::MsgReportAvailableChannels, Message)

const char* const APRS::m_featureIdURI = "sdrangel.feature.aprs";
const char* const APRS::m_featureId = "APRS";

APRS::APRS(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface)
{
    qDebug("APRS::APRS: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_worker = new APRSWorker(this, webAPIAdapterInterface);
    m_worker->moveToThread(&m_thread);
    m_state = StIdle;
    m_errorMessage = "APRS error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &APRS::networkManagerFinished
    );
    scanAvailableChannels();
    QObject::connect(
        MainCore::instance(),
        &MainCore::channelAdded,
        this,
        &APRS::handleChannelAdded
    );
}

APRS::~APRS()
{
    QObject::disconnect(
        MainCore::instance(),
        &MainCore::channelAdded,
        this,
        &APRS::handleChannelAdded
    );
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &APRS::networkManagerFinished
    );
    delete m_networkManager;
    if (m_worker->isRunning()) {
        stop();
    }

    delete m_worker;
}

void APRS::start()
{
    qDebug("APRS::start");
    m_worker->reset();
    m_worker->setMessageQueueToFeature(getInputMessageQueue());
    m_worker->setMessageQueueToGUI(getMessageQueueToGUI());
    bool ok = m_worker->startWork();
    m_state = ok ? StIdle : StError;
    m_thread.start();

    APRSWorker::MsgConfigureAPRSWorker *msg = APRSWorker::MsgConfigureAPRSWorker::create(m_settings, true);
    m_worker->getInputMessageQueue()->push(msg);
}

void APRS::stop()
{
    qDebug("APRS::stop");
    m_worker->stopWork();
    m_state = StIdle;
    m_thread.quit();
    m_thread.wait();
}

bool APRS::handleMessage(const Message& cmd)
{
    if (MsgConfigureAPRS::match(cmd))
    {
        MsgConfigureAPRS& cfg = (MsgConfigureAPRS&) cmd;
        qDebug() << "APRS::handleMessage: MsgConfigureAPRS";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgReportWorker::match(cmd))
    {
        MsgReportWorker& report = (MsgReportWorker&) cmd;
        if (report.getMessage() == "Connected")
            m_state = StRunning;
        else if (report.getMessage() == "Disconnected")
            m_state = StIdle;
        else
        {
            m_state = StError;
            m_errorMessage = report.getMessage();
        }
        return true;
    }
    else if (MsgQueryAvailableChannels::match(cmd))
    {
        notifyUpdateChannels();
        return true;
    }
    else if (MainCore::MsgPacket::match(cmd))
    {
        MainCore::MsgPacket& report = (MainCore::MsgPacket&) cmd;
        if (getMessageQueueToGUI())
        {
            MainCore::MsgPacket *copy = new MainCore::MsgPacket(report);
            getMessageQueueToGUI()->push(copy);
        }
        if (m_state == StRunning)
        {
            MainCore::MsgPacket *copy = new MainCore::MsgPacket(report);
            m_worker->getInputMessageQueue()->push(copy);
        }
        return true;
    }
    else
    {
        return false;
    }
}

QByteArray APRS::serialize() const
{
    return m_settings.serialize();
}

bool APRS::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureAPRS *msg = MsgConfigureAPRS::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureAPRS *msg = MsgConfigureAPRS::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void APRS::applySettings(const APRSSettings& settings, bool force)
{
    qDebug() << "APRS::applySettings:"
            << " m_igateEnabled: " << settings.m_igateEnabled
            << " m_title: " << settings.m_title
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIFeatureSetIndex: " << settings.m_reverseAPIFeatureSetIndex
            << " m_reverseAPIFeatureIndex: " << settings.m_reverseAPIFeatureIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_igateEnabled != settings.m_igateEnabled) || force)
    {
        if (settings.m_igateEnabled)
            start();
        else
            stop();
        reverseAPIKeys.append("igateEnabled");
    }

    if ((m_settings.m_title != settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((m_settings.m_rgbColor != settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }

    APRSWorker::MsgConfigureAPRSWorker *msg = APRSWorker::MsgConfigureAPRSWorker::create(
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

int APRS::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) run;
    (void) response;
    (void) errorMessage;
    //getFeatureStateStr(*response.getState());
    //MsgStartStopIGate *msg = MsgStartStopIGate::create(run);
    //getInputMessageQueue()->push(msg);
    return 202;
}

int APRS::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setAprsSettings(new SWGSDRangel::SWGAPRSSettings());
    response.getAprsSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int APRS::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    APRSSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureAPRS *msg = MsgConfigureAPRS::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("APRS::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAPRS *msgToGUI = MsgConfigureAPRS::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

void APRS::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const APRSSettings& settings)
{
    response.getAprsSettings()->setIgateServer(new QString(settings.m_igateServer));
    response.getAprsSettings()->setIgatePort(settings.m_igatePort);
    response.getAprsSettings()->setIgateCallsign(new QString(settings.m_igateCallsign));
    response.getAprsSettings()->setIgatePasscode(new QString(settings.m_igatePasscode));
    response.getAprsSettings()->setIgateFilter(new QString(settings.m_igateFilter));
    response.getAprsSettings()->setIgateEnabled(settings.m_igateEnabled ? 1 : 0);

    if (response.getAprsSettings()->getTitle()) {
        *response.getAprsSettings()->getTitle() = settings.m_title;
    } else {
        response.getAprsSettings()->setTitle(new QString(settings.m_title));
    }

    response.getAprsSettings()->setRgbColor(settings.m_rgbColor);
    response.getAprsSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAprsSettings()->getReverseApiAddress()) {
        *response.getAprsSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAprsSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAprsSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAprsSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getAprsSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getAprsSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getAprsSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getAprsSettings()->setRollupState(swgRollupState);
        }
    }

}

void APRS::webapiUpdateFeatureSettings(
    APRSSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("igateServer")) {
        settings.m_igateServer = *response.getAprsSettings()->getIgateServer();
    }
    if (featureSettingsKeys.contains("igatePort")) {
        settings.m_igatePort = response.getAprsSettings()->getIgatePort();
    }
    if (featureSettingsKeys.contains("igateCallsign")) {
        settings.m_igateCallsign = *response.getAprsSettings()->getIgateCallsign();
    }
    if (featureSettingsKeys.contains("igatePasscode")) {
        settings.m_igatePasscode = *response.getAprsSettings()->getIgatePasscode();
    }
    if (featureSettingsKeys.contains("igateFilter")) {
        settings.m_igateFilter = *response.getAprsSettings()->getIgateFilter();
    }
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getAprsSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAprsSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAprsSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAprsSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAprsSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getAprsSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getAprsSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getAprsSettings()->getRollupState());
    }
}

void APRS::webapiReverseSendSettings(QList<QString>& featureSettingsKeys, const APRSSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("APRS"));
    swgFeatureSettings->setAprsSettings(new SWGSDRangel::SWGAPRSSettings());
    SWGSDRangel::SWGAPRSSettings *swgAPRSSettings = swgFeatureSettings->getAprsSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("igateServer") || force) {
        swgAPRSSettings->setIgateServer(new QString(settings.m_igateServer));
    }
    if (featureSettingsKeys.contains("igatePort") || force) {
        swgAPRSSettings->setIgatePort(settings.m_igatePort);
    }
    if (featureSettingsKeys.contains("igateCallsign") || force) {
        swgAPRSSettings->setIgateCallsign(new QString(settings.m_igateCallsign));
    }
    if (featureSettingsKeys.contains("igatePasscode") || force) {
        swgAPRSSettings->setIgatePasscode(new QString(settings.m_igatePasscode));
    }
    if (featureSettingsKeys.contains("igateFilter") || force) {
        swgAPRSSettings->setIgateFilter(new QString(settings.m_igateFilter));
    }
    if (featureSettingsKeys.contains("title") || force) {
        swgAPRSSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgAPRSSettings->setRgbColor(settings.m_rgbColor);
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

void APRS::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "APRS::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("APRS::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void APRS::scanAvailableChannels()
{
    MainCore *mainCore = MainCore::instance();
    MessagePipes& messagePipes = mainCore->getMessagePipes();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
    m_availableChannels.clear();

    for (const auto& deviceSet : deviceSets)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  deviceSet->m_deviceSourceEngine;

        if (deviceSourceEngine)
        {
            for (int chi = 0; chi < deviceSet->getNumberOfChannels(); chi++)
            {
                ChannelAPI *channel = deviceSet->getChannelAt(chi);

                if (APRSSettings::m_pipeURIs.contains(channel->getURI()) && !m_availableChannels.contains(channel))
                {
                    qDebug("APRS::scanAvailableChannels: register %d:%d %s (%p)",
                        deviceSet->getIndex(), chi, qPrintable(channel->getURI()), channel);
                    ObjectPipe *pipe = messagePipes.registerProducerToConsumer(channel, this, "packets");
                    MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                    QObject::connect(
                        messageQueue,
                        &MessageQueue::messageEnqueued,
                        this,
                        [=](){ this->handleChannelMessageQueue(messageQueue); },
                        Qt::QueuedConnection
                    );
                    QObject::connect(
                        pipe,
                        &ObjectPipe::toBeDeleted,
                        this,
                        &APRS::handleMessagePipeToBeDeleted
                    );
                    APRSSettings::AvailableChannel availableChannel =
                        APRSSettings::AvailableChannel{deviceSet->getIndex(), chi, channel->getIdentifier()};
                    m_availableChannels[channel] = availableChannel;
                }
            }

            notifyUpdateChannels();
        }
    }
}

void APRS::notifyUpdateChannels()
{
    if (getMessageQueueToGUI())
    {
        MsgReportAvailableChannels *msg = MsgReportAvailableChannels::create();
        msg->getChannels() = m_availableChannels.values();
        getMessageQueueToGUI()->push(msg);
    }
}

void APRS::handleChannelAdded(int deviceSetIndex, ChannelAPI *channel)
{
    qDebug("APRS::handleChannelAdded: deviceSetIndex: %d:%d channel: %s (%p)",
        deviceSetIndex, channel->getIndexInDeviceSet(), qPrintable(channel->getURI()), channel);
    DeviceSet *deviceSet = MainCore::instance()->getDeviceSets()[deviceSetIndex];
    DSPDeviceSourceEngine *deviceSourceEngine =  deviceSet->m_deviceSourceEngine;

    if (deviceSourceEngine && APRSSettings::m_pipeURIs.contains(channel->getURI()))
    {
        int chi = channel->getIndexInDeviceSet();

        if (!m_availableChannels.contains(channel))
        {
            MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
            ObjectPipe *pipe = messagePipes.registerProducerToConsumer(channel, this, "packets");
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            QObject::connect(
                messageQueue,
                &MessageQueue::messageEnqueued,
                this,
                [=](){ this->handleChannelMessageQueue(messageQueue); },
                Qt::QueuedConnection
            );
            QObject::connect(
                pipe,
                &ObjectPipe::toBeDeleted,
                this,
                &APRS::handleMessagePipeToBeDeleted
            );
        }

        APRSSettings::AvailableChannel availableChannel =
            APRSSettings::AvailableChannel{deviceSet->getIndex(), chi, channel->getIdentifier()};
        m_availableChannels[channel] = availableChannel;

        notifyUpdateChannels();
    }
}

void APRS::handleMessagePipeToBeDeleted(int reason, QObject* object)
{
    if ((reason == 0) && m_availableChannels.contains((ChannelAPI*) object)) // producer (channel)
    {
        qDebug("APRS::handleMessagePipeToBeDeleted: removing channel at (%p)", object);
        m_availableChannels.remove((ChannelAPI*) object);
        notifyUpdateChannels();
    }
}

void APRS::handleChannelMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}
