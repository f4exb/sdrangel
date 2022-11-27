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

#include "radiosonde.h"

MESSAGE_CLASS_DEFINITION(Radiosonde::MsgConfigureRadiosonde, Message)

const char* const Radiosonde::m_featureIdURI = "sdrangel.feature.radiosonde";
const char* const Radiosonde::m_featureId = "Radiosonde";

Radiosonde::Radiosonde(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface)
{
    qDebug("Radiosonde::Radiosonde: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "Radiosonde error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Radiosonde::networkManagerFinished
    );
    scanAvailableChannels();
    QObject::connect(
        MainCore::instance(),
        &MainCore::channelAdded,
        this,
        &Radiosonde::handleChannelAdded
    );
}

Radiosonde::~Radiosonde()
{
    QObject::disconnect(
        MainCore::instance(),
        &MainCore::channelAdded,
        this,
        &Radiosonde::handleChannelAdded
    );
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Radiosonde::networkManagerFinished
    );
    delete m_networkManager;
}

void Radiosonde::start()
{
    qDebug("Radiosonde::start");
    m_state = StRunning;
}

void Radiosonde::stop()
{
    qDebug("Radiosonde::stop");
    m_state = StIdle;
}

bool Radiosonde::handleMessage(const Message& cmd)
{
    if (MsgConfigureRadiosonde::match(cmd))
    {
        MsgConfigureRadiosonde& cfg = (MsgConfigureRadiosonde&) cmd;
        qDebug() << "Radiosonde::handleMessage: MsgConfigureRadiosonde";
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

QByteArray Radiosonde::serialize() const
{
    return m_settings.serialize();
}

bool Radiosonde::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureRadiosonde *msg = MsgConfigureRadiosonde::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureRadiosonde *msg = MsgConfigureRadiosonde::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void Radiosonde::applySettings(const RadiosondeSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "Radiosonde::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    if (settingsKeys.contains("useReverseAPI"))
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

int Radiosonde::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setRadiosondeSettings(new SWGSDRangel::SWGRadiosondeSettings());
    response.getRadiosondeSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int Radiosonde::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    RadiosondeSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureRadiosonde *msg = MsgConfigureRadiosonde::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRadiosonde *msgToGUI = MsgConfigureRadiosonde::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

void Radiosonde::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const RadiosondeSettings& settings)
{
    if (response.getRadiosondeSettings()->getTitle()) {
        *response.getRadiosondeSettings()->getTitle() = settings.m_title;
    } else {
        response.getRadiosondeSettings()->setTitle(new QString(settings.m_title));
    }

    response.getRadiosondeSettings()->setRgbColor(settings.m_rgbColor);
    response.getRadiosondeSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRadiosondeSettings()->getReverseApiAddress()) {
        *response.getRadiosondeSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRadiosondeSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRadiosondeSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRadiosondeSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getRadiosondeSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getRadiosondeSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getRadiosondeSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getRadiosondeSettings()->setRollupState(swgRollupState);
        }
    }

    if (!response.getRadiosondeSettings()->getRadiosondesColumnIndexes()) {
        response.getRadiosondeSettings()->setRadiosondesColumnIndexes(new QList<int>());
    }

    response.getRadiosondeSettings()->getRadiosondesColumnIndexes()->clear();

    for (int i = 0; i < RADIOSONDES_COLUMNS; i++) {
        response.getRadiosondeSettings()->getRadiosondesColumnIndexes()->push_back(settings.m_radiosondesColumnIndexes[i]);
    }

    if (!response.getRadiosondeSettings()->getRadiosondesColumnSizes()) {
        response.getRadiosondeSettings()->setRadiosondesColumnSizes(new QList<int>());
    }

    response.getRadiosondeSettings()->getRadiosondesColumnSizes()->clear();

    for (int i = 0; i < RADIOSONDES_COLUMNS; i++) {
        response.getRadiosondeSettings()->getRadiosondesColumnSizes()->push_back(settings.m_radiosondesColumnSizes[i]);
    }
}

void Radiosonde::webapiUpdateFeatureSettings(
    RadiosondeSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getRadiosondeSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getRadiosondeSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRadiosondeSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRadiosondeSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRadiosondeSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getRadiosondeSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getRadiosondeSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getRadiosondeSettings()->getRollupState());
    }

    if (featureSettingsKeys.contains("radiosondesColumnIndexes"))
    {
        const QList<int> *indexes = response.getRadiosondeSettings()->getRadiosondesColumnIndexes();

        for (int i = 0; i < RADIOSONDES_COLUMNS; i++) {
            settings.m_radiosondesColumnIndexes[i] = (*indexes)[i];
        }
    }

    if (featureSettingsKeys.contains("vesselColumnSizes"))
    {
        const QList<int> *indexes = response.getRadiosondeSettings()->getRadiosondesColumnSizes();

        for (int i = 0; i < RADIOSONDES_COLUMNS; i++) {
            settings.m_radiosondesColumnSizes[i] = (*indexes)[i];
        }
    }
}

void Radiosonde::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const RadiosondeSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("Radiosonde"));
    swgFeatureSettings->setRadiosondeSettings(new SWGSDRangel::SWGRadiosondeSettings());
    SWGSDRangel::SWGRadiosondeSettings *swgRadiosondeSettings = swgFeatureSettings->getRadiosondeSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("title") || force) {
        swgRadiosondeSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgRadiosondeSettings->setRgbColor(settings.m_rgbColor);
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

void Radiosonde::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "Radiosonde::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("Radiosonde::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void Radiosonde::scanAvailableChannels()
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

                if ((channel->getURI() == "sdrangel.channel.radiosondedemod") && !m_availableChannels.contains(channel))
                {
                    qDebug("Radiosonde::scanAvailableChannels: register %d:%d (%p)", deviceSet->getIndex(), chi, channel);
                    ObjectPipe *pipe = messagePipes.registerProducerToConsumer(channel, this, "radiosonde");
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
                        &Radiosonde::handleMessagePipeToBeDeleted
                    );
                    m_availableChannels.insert(channel);
                }
            }
        }
    }
}

void Radiosonde::handleChannelAdded(int deviceSetIndex, ChannelAPI *channel)
{
    qDebug("Radiosonde::handleChannelAdded: deviceSetIndex: %d:%d channel: %s (%p)",
        deviceSetIndex, channel->getIndexInDeviceSet(), qPrintable(channel->getURI()), channel);
    std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();
    DeviceSet *deviceSet = deviceSets[deviceSetIndex];
    DSPDeviceSourceEngine *deviceSourceEngine =  deviceSet->m_deviceSourceEngine;

    if (deviceSourceEngine && (channel->getURI() == "sdrangel.channel.radiosondedemod"))
    {
        if (!m_availableChannels.contains(channel))
        {
            MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
            ObjectPipe *pipe = messagePipes.registerProducerToConsumer(channel, this, "radiosonde");
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
                &Radiosonde::handleMessagePipeToBeDeleted
            );
            m_availableChannels.insert(channel);
        }
    }
}

void Radiosonde::handleMessagePipeToBeDeleted(int reason, QObject* object)
{
    if ((reason == 0) && m_availableChannels.contains((ChannelAPI*) object)) // producer (channel)
    {
        qDebug("Radiosonde::handleMessagePipeToBeDeleted: removing channel at (%p)", object);
        m_availableChannels.remove((ChannelAPI*) object);
    }
}

void Radiosonde::handleChannelMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}
