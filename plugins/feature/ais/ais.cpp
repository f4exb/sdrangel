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
#include "SWGDeviceState.h"

#include "dsp/dspengine.h"
#include "device/deviceset.h"
#include "channel/channelapi.h"
#include "feature/featureset.h"
#include "settings/serializable.h"
#include "maincore.h"

#include "ais.h"

MESSAGE_CLASS_DEFINITION(AIS::MsgConfigureAIS, Message)

const char* const AIS::m_featureIdURI = "sdrangel.feature.ais";
const char* const AIS::m_featureId = "AIS";

AIS::AIS(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface)
{
    qDebug("AIS::AIS: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "AIS error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AIS::networkManagerFinished
    );
    scanAvailableChannels();
    QObject::connect(
        MainCore::instance(),
        &MainCore::channelAdded,
        this,
        &AIS::handleChannelAdded
    );
}

AIS::~AIS()
{
    QObject::disconnect(
        MainCore::instance(),
        &MainCore::channelAdded,
        this,
        &AIS::handleChannelAdded
    );
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AIS::networkManagerFinished
    );
    delete m_networkManager;
}

void AIS::start()
{
    qDebug("AIS::start");
    m_state = StRunning;
}

void AIS::stop()
{
    qDebug("AIS::stop");
    m_state = StIdle;
}

bool AIS::handleMessage(const Message& cmd)
{
    if (MsgConfigureAIS::match(cmd))
    {
        MsgConfigureAIS& cfg = (MsgConfigureAIS&) cmd;
        qDebug() << "AIS::handleMessage: MsgConfigureAIS";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

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
        return true;
    }
    else
    {
        return false;
    }
}

QByteArray AIS::serialize() const
{
    return m_settings.serialize();
}

bool AIS::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureAIS *msg = MsgConfigureAIS::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureAIS *msg = MsgConfigureAIS::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void AIS::applySettings(const AISSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "AIS::applySettings:" << settings.getDebugString(settingsKeys, force) << force;

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
                settingsKeys.contains("reverseAPIAddress") ||
                settingsKeys.contains("reverseAPIPort") ||
                settingsKeys.contains("reverseAPIFeatureSetIndex") ||
                settingsKeys.contains("m_reverseAPIFeatureIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
   }
}

int AIS::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setAisSettings(new SWGSDRangel::SWGAISSettings());
    response.getAisSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int AIS::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    AISSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureAIS *msg = MsgConfigureAIS::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAIS *msgToGUI = MsgConfigureAIS::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

void AIS::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const AISSettings& settings)
{
    if (response.getAisSettings()->getTitle()) {
        *response.getAisSettings()->getTitle() = settings.m_title;
    } else {
        response.getAisSettings()->setTitle(new QString(settings.m_title));
    }

    response.getAisSettings()->setRgbColor(settings.m_rgbColor);
    response.getAisSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAisSettings()->getReverseApiAddress()) {
        *response.getAisSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAisSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAisSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAisSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getAisSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getAisSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getAisSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getAisSettings()->setRollupState(swgRollupState);
        }
    }

    if (!response.getAisSettings()->getVesselColumnIndexes()) {
        response.getAisSettings()->setVesselColumnIndexes(new QList<int>());
    }

    response.getAisSettings()->getVesselColumnIndexes()->clear();

    for (int i = 0; i < AIS_VESSEL_COLUMNS; i++) {
        response.getAisSettings()->getVesselColumnIndexes()->push_back(settings.m_vesselColumnIndexes[i]);
    }

    if (!response.getAisSettings()->getVesselColumnSizes()) {
        response.getAisSettings()->setVesselColumnSizes(new QList<int>());
    }

    response.getAisSettings()->getVesselColumnSizes()->clear();

    for (int i = 0; i < AIS_VESSEL_COLUMNS; i++) {
        response.getAisSettings()->getVesselColumnSizes()->push_back(settings.m_vesselColumnSizes[i]);
    }
}

void AIS::webapiUpdateFeatureSettings(
    AISSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getAisSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAisSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAisSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAisSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAisSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getAisSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getAisSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getAisSettings()->getRollupState());
    }

    if (featureSettingsKeys.contains("vesselColumnIndexes"))
    {
        const QList<int> *indexes = response.getAisSettings()->getVesselColumnIndexes();

        for (int i = 0; i < AIS_VESSEL_COLUMNS; i++) {
            settings.m_vesselColumnIndexes[i] = (*indexes)[i];
        }
    }

    if (featureSettingsKeys.contains("vesselColumnSizes"))
    {
        const QList<int> *indexes = response.getAisSettings()->getVesselColumnSizes();

        for (int i = 0; i < AIS_VESSEL_COLUMNS; i++) {
            settings.m_vesselColumnSizes[i] = (*indexes)[i];
        }
    }
}

void AIS::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const AISSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("AIS"));
    swgFeatureSettings->setAisSettings(new SWGSDRangel::SWGAISSettings());
    SWGSDRangel::SWGAISSettings *swgAISSettings = swgFeatureSettings->getAisSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("title") || force) {
        swgAISSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgAISSettings->setRgbColor(settings.m_rgbColor);
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

void AIS::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AIS::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AIS::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void AIS::scanAvailableChannels()
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

                if ((channel->getURI() == "sdrangel.channel.aisdemod") && !m_availableChannels.contains(channel))
                {
                    qDebug("AIS::scanAvailableChannels: register %d:%d (%p)", deviceSet->getIndex(), chi, channel);
                    ObjectPipe *pipe = messagePipes.registerProducerToConsumer(channel, this, "ais");
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
                        &AIS::handleMessagePipeToBeDeleted
                    );
                    m_availableChannels.insert(channel);
                }
            }
        }
    }
}

void AIS::handleChannelAdded(int deviceSetIndex, ChannelAPI *channel)
{
    qDebug("AIS::handleChannelAdded: deviceSetIndex: %d:%d channel: %s (%p)",
        deviceSetIndex, channel->getIndexInDeviceSet(), qPrintable(channel->getURI()), channel);
    std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();
    DeviceSet *deviceSet = deviceSets[deviceSetIndex];
    DSPDeviceSourceEngine *deviceSourceEngine =  deviceSet->m_deviceSourceEngine;

    if (deviceSourceEngine && (channel->getURI() == "sdrangel.channel.aisdemod"))
    {
        if (!m_availableChannels.contains(channel))
        {
            MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
            ObjectPipe *pipe = messagePipes.registerProducerToConsumer(channel, this, "ais");
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
                &AIS::handleMessagePipeToBeDeleted
            );
            m_availableChannels.insert(channel);
        }
    }
}

void AIS::handleMessagePipeToBeDeleted(int reason, QObject* object)
{
    if ((reason == 0) && m_availableChannels.contains((ChannelAPI*) object)) // producer (channel)
    {
        qDebug("AIS::handleMessagePipeToBeDeleted: removing channel at (%p)", object);
        m_availableChannels.remove((ChannelAPI*) object);
    }
}

void AIS::handleChannelMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}
