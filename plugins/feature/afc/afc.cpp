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

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"
#include "SWGAFCReport.h"
#include "SWGDeviceState.h"
#include "SWGChannelSettings.h"

#include "dsp/dspengine.h"
#include "device/deviceset.h"
#include "channel/channelapi.h"
#include "maincore.h"

#include "afcworker.h"
#include "afc.h"

MESSAGE_CLASS_DEFINITION(AFC::MsgConfigureAFC, Message)
MESSAGE_CLASS_DEFINITION(AFC::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(AFC::MsgDeviceTrack, Message)
MESSAGE_CLASS_DEFINITION(AFC::MsgDevicesApply, Message)

const char* const AFC::m_featureIdURI = "sdrangel.feature.afc";
const char* const AFC::m_featureId = "AFC";

AFC::AFC(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_trackerDeviceSet(nullptr),
    m_trackedDeviceSet(nullptr),
    m_trackerIndexInDeviceSet(-1),
    m_trackerChannelAPI(nullptr)
{
    setObjectName(m_featureId);
    m_worker = new AFCWorker(webAPIAdapterInterface);
    m_state = StIdle;
    m_errorMessage = "AFC error";
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

AFC::~AFC()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    if (m_worker->isRunning()) {
        stop();
    }

    delete m_worker;
    removeTrackerFeatureReference();
    removeTrackedFeatureReferences();
}

void AFC::start()
{
	qDebug("AFC::start");

    m_worker->reset();
    m_worker->setMessageQueueToGUI(getMessageQueueToGUI());
    bool ok = m_worker->startWork();
    m_state = ok ? StRunning : StError;
    m_thread.start();

    AFCWorker::MsgConfigureAFCWorker *msg = AFCWorker::MsgConfigureAFCWorker::create(m_settings, true);
    m_worker->getInputMessageQueue()->push(msg);
}

void AFC::stop()
{
    qDebug("AFC::stop");
	m_worker->stopWork();
    m_state = StIdle;
	m_thread.quit();
	m_thread.wait();
}

bool AFC::handleMessage(const Message& cmd)
{
	if (MsgConfigureAFC::match(cmd))
	{
        MsgConfigureAFC& cfg = (MsgConfigureAFC&) cmd;
        qDebug() << "AFC::handleMessage: MsgConfigureAFC";
        applySettings(cfg.getSettings(), cfg.getForce());

		return true;
	}
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "AFC::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
    else if (MainCore::MsgChannelSettings::match(cmd))
    {
        MainCore::MsgChannelSettings& cfg = (MainCore::MsgChannelSettings&) cmd;
        SWGSDRangel::SWGChannelSettings *swgChannelSettings = cfg.getSWGSettings();
        QString *channelType  = swgChannelSettings->getChannelType();
        qDebug() << "AFC::handleMessage: MainCore::MsgChannelSettings: " << *channelType;

        if (m_worker->isRunning())
        {
            m_worker->getInputMessageQueue()->push(&cfg);
        }
        else
        {
            delete swgChannelSettings;
            return true;
        }
    }
    else if (MessagePipesCommon::MsgReportChannelDeleted::match(cmd))
    {
        qDebug() << "AFC::handleMessage: MessagePipesCommon::MsgReportChannelDeleted";
        MessagePipesCommon::MsgReportChannelDeleted& report = (MessagePipesCommon::MsgReportChannelDeleted&) cmd;
        const MessagePipesCommon::ChannelRegistrationKey& channelKey = report.getChannelRegistrationKey();
        MainCore::instance()->getMessagePipes().unregisterChannelToFeature(channelKey.m_key, this, "settings");

        return true;
    }
    else if (MsgDeviceTrack::match(cmd))
    {
        if (m_worker->isRunning())
        {
            AFCWorker::MsgDeviceTrack *msg = AFCWorker::MsgDeviceTrack::create();
            m_worker->getInputMessageQueue()->push(msg);
        }

        return true;
    }
    else if (MsgDevicesApply::match(cmd))
    {
        removeTrackerFeatureReference();
        trackerDeviceChange(m_settings.m_trackerDeviceSetIndex);
        removeTrackedFeatureReferences();
        trackedDeviceChange(m_settings.m_trackedDeviceSetIndex);

        if (m_worker->isRunning())
        {
            AFCWorker::MsgDevicesApply *msg = AFCWorker::MsgDevicesApply::create();
            m_worker->getInputMessageQueue()->push(msg);
        }

        return true;
    }

    return false;
}

QByteArray AFC::serialize() const
{
    return m_settings.serialize();
}

bool AFC::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureAFC *msg = MsgConfigureAFC::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureAFC *msg = MsgConfigureAFC::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void AFC::applySettings(const AFCSettings& settings, bool force)
{
    qDebug() << "AFC::applySettings:"
            << " m_title: " << settings.m_title
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_trackerDeviceSetIndex: " << settings.m_trackerDeviceSetIndex
            << " m_trackedDeviceSetIndex: " << settings.m_trackedDeviceSetIndex
            << " m_hasTargetFrequency: " << settings.m_hasTargetFrequency
            << " m_transverterTarget: " << settings.m_transverterTarget
            << " m_targetFrequency: " << settings.m_targetFrequency
            << " m_freqTolerance: " << settings.m_freqTolerance
            << " m_trackerAdjustPeriod:" << settings.m_trackerAdjustPeriod
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_title != settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((m_settings.m_rgbColor != settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((m_settings.m_trackerDeviceSetIndex != settings.m_trackerDeviceSetIndex) || force) {
        reverseAPIKeys.append("trackerDeviceSetIndex");
    }
    if ((m_settings.m_trackedDeviceSetIndex != settings.m_trackedDeviceSetIndex) || force) {
        reverseAPIKeys.append("trackedDeviceSetIndex");
    }
    if ((m_settings.m_hasTargetFrequency != settings.m_hasTargetFrequency) || force) {
        reverseAPIKeys.append("hasTargetFrequency");
    }
    if ((m_settings.m_transverterTarget != settings.m_transverterTarget) || force) {
        reverseAPIKeys.append("transverterTarget");
    }
    if ((m_settings.m_targetFrequency != settings.m_targetFrequency) || force) {
        reverseAPIKeys.append("targetFrequency");
    }
    if ((m_settings.m_freqTolerance != settings.m_freqTolerance) || force) {
        reverseAPIKeys.append("freqTolerance");
    }
    if ((m_settings.m_trackerAdjustPeriod != settings.m_trackerAdjustPeriod) || force) {
        reverseAPIKeys.append("trackerAdjustPeriod");
    }

    if ((m_settings.m_trackerDeviceSetIndex != settings.m_trackerDeviceSetIndex) || force)
    {
        removeTrackerFeatureReference();
        trackerDeviceChange(settings.m_trackerDeviceSetIndex);
    }

    if ((m_settings.m_trackedDeviceSetIndex != settings.m_trackedDeviceSetIndex) || force)
    {
        removeTrackedFeatureReferences();
        trackedDeviceChange(settings.m_trackedDeviceSetIndex);
    }

    AFCWorker::MsgConfigureAFCWorker *msg = AFCWorker::MsgConfigureAFCWorker::create(
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

int AFC::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int AFC::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setAfcSettings(new SWGSDRangel::SWGAFCSettings());
    response.getAfcSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int AFC::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    AFCSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureAFC *msg = MsgConfigureAFC::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("AFC::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAFC *msgToGUI = MsgConfigureAFC::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

int AFC::webapiReportGet(
    SWGSDRangel::SWGFeatureReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setAfcReport(new SWGSDRangel::SWGAFCReport());
    response.getAfcReport()->init();
    webapiFormatFeatureReport(response);
    return 200;
}

int AFC::webapiActionsPost(
    const QStringList& featureActionsKeys,
    SWGSDRangel::SWGFeatureActions& query,
    QString& errorMessage)
{
    SWGSDRangel::SWGAFCActions *swgAFCActions = query.getAfcActions();

    if (swgAFCActions)
    {
        if (featureActionsKeys.contains("deviceTrack"))
        {
            bool deviceTrack = swgAFCActions->getDeviceTrack() != 0;

            if (deviceTrack)
            {
                MsgDeviceTrack *msg = MsgDeviceTrack::create();
                getInputMessageQueue()->push(msg);
            }
        }

        if (featureActionsKeys.contains("devicesApply"))
        {
            bool devicesApply = swgAFCActions->getDevicesApply() != 0;

            if (devicesApply)
            {
                MsgDevicesApply *msg = MsgDevicesApply::create();
                getInputMessageQueue()->push(msg);
            }
        }

        return 202;
    }
    else
    {
        errorMessage = "Missing AFCActions in query";
        return 400;
    }
}

void AFC::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const AFCSettings& settings)
{
    if (response.getAfcSettings()->getTitle()) {
        *response.getAfcSettings()->getTitle() = settings.m_title;
    } else {
        response.getAfcSettings()->setTitle(new QString(settings.m_title));
    }

    response.getAfcSettings()->setRgbColor(settings.m_rgbColor);
    response.getAfcSettings()->setTrackerDeviceSetIndex(settings.m_trackerDeviceSetIndex);
    response.getAfcSettings()->setTrackedDeviceSetIndex(settings.m_trackedDeviceSetIndex);
    response.getAfcSettings()->setHasTargetFrequency(settings.m_hasTargetFrequency);
    response.getAfcSettings()->setTransverterTarget(settings.m_transverterTarget);
    response.getAfcSettings()->setTargetFrequency(settings.m_targetFrequency);
    response.getAfcSettings()->setFreqTolerance(settings.m_freqTolerance);
    response.getAfcSettings()->setTrackerAdjustPeriod(settings.m_trackerAdjustPeriod);

    response.getAfcSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAfcSettings()->getReverseApiAddress()) {
        *response.getAfcSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAfcSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAfcSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAfcSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getAfcSettings()->setReverseApiChannelIndex(settings.m_reverseAPIFeatureIndex);
}

void AFC::webapiUpdateFeatureSettings(
    AFCSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getAfcSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAfcSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("trackerDeviceSetIndex")) {
        settings.m_trackerDeviceSetIndex = response.getAfcSettings()->getTrackerDeviceSetIndex();
    }
    if (featureSettingsKeys.contains("trackedDeviceSetIndex")) {
        settings.m_trackedDeviceSetIndex = response.getAfcSettings()->getTrackedDeviceSetIndex();
    }
    if (featureSettingsKeys.contains("hasTargetFrequency")) {
        settings.m_hasTargetFrequency = response.getAfcSettings()->getHasTargetFrequency() != 0;
    }
    if (featureSettingsKeys.contains("hasTargetFrequency")) {
        settings.m_hasTargetFrequency = response.getAfcSettings()->getHasTargetFrequency() != 0;
    }
    if (featureSettingsKeys.contains("targetFrequency")) {
        settings.m_targetFrequency = response.getAfcSettings()->getTargetFrequency();
    }
    if (featureSettingsKeys.contains("freqTolerance")) {
        settings.m_freqTolerance = response.getAfcSettings()->getFreqTolerance();
    }
    if (featureSettingsKeys.contains("trackerAdjustPeriod")) {
        settings.m_trackerAdjustPeriod = response.getAfcSettings()->getTrackerAdjustPeriod();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAfcSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAfcSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAfcSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getAfcSettings()->getReverseApiDeviceIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getAfcSettings()->getReverseApiChannelIndex();
    }
}

void AFC::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response)
{
    response.getAfcReport()->setTrackerChannelIndex(m_trackerIndexInDeviceSet);
    response.getAfcReport()->setTrackerDeviceFrequency(m_worker->getTrackerDeviceFrequency());
    response.getAfcReport()->setTrackerChannelOffset(m_worker->getTrackerChannelOffset());
}

void AFC::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const AFCSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("AFC"));
    swgFeatureSettings->setAfcSettings(new SWGSDRangel::SWGAFCSettings());
    SWGSDRangel::SWGAFCSettings *swgAFCSettings = swgFeatureSettings->getAfcSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("title") || force) {
        swgAFCSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgAFCSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("trackerDeviceSetIndex") || force) {
        swgAFCSettings->setTrackerDeviceSetIndex(settings.m_trackerDeviceSetIndex);
    }
    if (channelSettingsKeys.contains("trackedDeviceSetIndex") || force) {
        swgAFCSettings->setTrackedDeviceSetIndex(settings.m_trackedDeviceSetIndex);
    }
    if (channelSettingsKeys.contains("hasTargetFrequency") || force) {
        swgAFCSettings->setHasTargetFrequency(settings.m_hasTargetFrequency ? 1 : 0);
    }
    if (channelSettingsKeys.contains("targetFrequency") || force) {
        swgAFCSettings->setTargetFrequency(settings.m_targetFrequency ? 1 : 0);
    }
    if (channelSettingsKeys.contains("targetFrequency") || force) {
        swgAFCSettings->setTargetFrequency(settings.m_targetFrequency);
    }
    if (channelSettingsKeys.contains("freqTolerance") || force) {
        swgAFCSettings->setFreqTolerance(settings.m_freqTolerance);
    }
    if (channelSettingsKeys.contains("trackerAdjustPeriod") || force) {
        swgAFCSettings->setTrackerAdjustPeriod(settings.m_trackerAdjustPeriod);
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

void AFC::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AFC::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AFC::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void AFC::trackerDeviceChange(int deviceIndex)
{
    MainCore *mainCore = MainCore::instance();
    m_trackerDeviceSet = mainCore->getDeviceSets()[deviceIndex];

    for (int i = 0; i < m_trackerDeviceSet->getNumberOfChannels(); i++)
    {
        ChannelAPI *channel = m_trackerDeviceSet->getChannelAt(i);

        if (channel->getURI() == "sdrangel.channel.freqtracker")
        {
            MessageQueue *messageQueue = mainCore->getMessagePipes().registerChannelToFeature(channel, this, "settings");
            QObject::connect(
                messageQueue,
                &MessageQueue::messageEnqueued,
                this,
                [=](){ this->handleChannelMessageQueue(messageQueue); },
                Qt::QueuedConnection
            );
            m_trackerChannelAPI = channel;
            break;
        }
    }
}

void AFC::trackedDeviceChange(int deviceIndex)
{
    MainCore *mainCore = MainCore::instance();
    m_trackedDeviceSet = mainCore->getDeviceSets()[deviceIndex];

    for (int i = 0; i < m_trackedDeviceSet->getNumberOfChannels(); i++)
    {
        ChannelAPI *channel = m_trackedDeviceSet->getChannelAt(i);

        if (channel->getURI() != "sdrangel.channel.freqtracker")
        {
            MessageQueue *messageQueue = mainCore->getMessagePipes().registerChannelToFeature(channel, this, "settings");
            QObject::connect(
                messageQueue,
                &MessageQueue::messageEnqueued,
                this,
                [=](){ this->handleChannelMessageQueue(messageQueue); },
                Qt::QueuedConnection
            );
            m_trackerIndexInDeviceSet = i;
        }
    }
}

void AFC::removeTrackerFeatureReference()
{
    if (m_trackerChannelAPI)
    {
        if (MainCore::instance()->existsChannel(m_trackerChannelAPI))
        {
            MessageQueue *messageQueue
                = MainCore::instance()->getMessagePipes().unregisterChannelToFeature(m_trackerChannelAPI, this, "settings");
            disconnect(messageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleChannelMessageQueue(MessageQueue*)));
        }
    }
}

void AFC::removeTrackedFeatureReferences()
{
    for (QList<ChannelAPI*>::iterator it = m_trackedChannelAPIs.begin(); it != m_trackedChannelAPIs.end(); ++it)
    {
        ChannelAPI *channel = *it;

        if (MainCore::instance()->existsChannel(channel)) {
            MainCore::instance()->getMessagePipes().unregisterChannelToFeature(channel, this, "settings");
        }
    }

    m_trackedChannelAPIs.clear();
}

void AFC::handleChannelMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}
