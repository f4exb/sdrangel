///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#include "SWGFeatureActions.h"
#include "SWGDeviceState.h"

#include "dsp/dspcommands.h"
#include "dsp/datafifo.h"
#include "settings/serializable.h"
#include "channel/channelapi.h"
#include "maincore.h"

#include "denoiserworker.h"
#include "denoiser.h"

MESSAGE_CLASS_DEFINITION(Denoiser::MsgConfigureDenoiser, Message)
MESSAGE_CLASS_DEFINITION(Denoiser::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(Denoiser::MsgReportChannels, Message)
MESSAGE_CLASS_DEFINITION(Denoiser::MsgSelectChannel, Message)
MESSAGE_CLASS_DEFINITION(Denoiser::MsgReportSampleRate, Message)

const char* const Denoiser::m_featureIdURI = "sdrangel.feature.denoiser";
const char* const Denoiser::m_featureId = "Denoiser";

Denoiser::Denoiser(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_thread(nullptr),
    m_running(false),
    m_worker(nullptr),
    m_availableChannelOrFeatureHandler(DenoiserSettings::m_channelURIs),
    m_selectedChannel(nullptr),
    m_dataPipe(nullptr)
{
    qDebug("Denoiser::Denoiser: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "Denoiser error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Denoiser::networkManagerFinished
    );
    QObject::connect(
        &m_availableChannelOrFeatureHandler,
        &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged,
        this,
        &Denoiser::channelsOrFeaturesChanged
    );
    m_availableChannelOrFeatureHandler.scanAvailableChannelsAndFeatures();
}

Denoiser::~Denoiser()
{
    QObject::disconnect(
        &m_availableChannelOrFeatureHandler,
        &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged,
        this,
        &Denoiser::channelsOrFeaturesChanged
    );
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Denoiser::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running)
    {
        stop();
    }
}

void Denoiser::start()
{
    QMutexLocker m_lock(&m_mutex);

    if (m_running) {
        return;
    }

	qDebug("Denoiser::start");
    m_thread = new QThread();
    m_worker = new DenoiserWorker();
    m_worker->moveToThread(m_thread);

    QObject::connect(
        m_thread,
        &QThread::started,
        m_worker,
        &DenoiserWorker::startWork
    );
    QObject::connect(
        m_thread,
        &QThread::finished,
        m_worker,
        &QObject::deleteLater
    );
    QObject::connect(
        m_thread,
        &QThread::finished,
        m_thread,
        &QThread::deleteLater
    );

    m_worker->setMessageQueueToFeature(getInputMessageQueue());
    m_worker->startWork();
    m_state = StRunning;
    m_thread->start();

    DenoiserWorker::MsgConfigureDenoiserWorker *msg
        = DenoiserWorker::MsgConfigureDenoiserWorker::create(m_settings, QList<QString>(), true);
    m_worker->getInputMessageQueue()->push(msg);
    m_worker->applySampleRate(m_sampleRate);

    if (m_dataPipe)
    {
        DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);

        if (fifo)
        {
            DenoiserWorker::MsgConnectFifo *msg = DenoiserWorker::MsgConnectFifo::create(fifo, true);
            m_worker->getInputMessageQueue()->push(msg);
        }
    }

    m_running = true;
}

void Denoiser::stop()
{
    QMutexLocker m_lock(&m_mutex);

    if (!m_running) {
        return;
    }

    qDebug("Denoiser::stop");
    m_running = false;

    if (m_dataPipe)
    {
        DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);

        if (fifo)
        {
            DenoiserWorker::MsgConnectFifo *msg = DenoiserWorker::MsgConnectFifo::create(fifo, false);
            m_worker->getInputMessageQueue()->push(msg);
        }
    }

	m_worker->stopWork();
    m_state = StIdle;
	m_thread->quit();
	m_thread->wait();
}

double Denoiser::getMagSqAvg() const
{
    return m_running ? m_worker->getMagSqAvg() : 0.0;
}

bool Denoiser::handleMessage(const Message& cmd)
{
	if (MsgConfigureDenoiser::match(cmd))
	{
        MsgConfigureDenoiser& cfg = (MsgConfigureDenoiser&) cmd;
        qDebug() << "Denoiser::handleMessage: MsgConfigureDenoiser";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

		return true;
	}
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "Denoiser::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
    else if (MsgSelectChannel::match(cmd))
    {
        MsgSelectChannel& cfg = (MsgSelectChannel&) cmd;
        ChannelAPI *selectedChannel = cfg.getChannel();
        qDebug("Denoiser::handleMessage: MsgSelectChannel: %p %s",
            selectedChannel, qPrintable(selectedChannel->objectName()));
        setChannel(selectedChannel);
        MainCore::MsgChannelDemodQuery *msg = MainCore::MsgChannelDemodQuery::create();
        selectedChannel->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (MainCore::MsgChannelDemodReport::match(cmd))
    {
        qDebug() << "Denoiser::handleMessage: MainCore::MsgChannelDemodReport";
        MainCore::MsgChannelDemodReport& report = (MainCore::MsgChannelDemodReport&) cmd;

        if (report.getChannelAPI() == m_selectedChannel)
        {
            m_sampleRate = report.getSampleRate();

            if (m_running) {
                m_worker->applySampleRate(m_sampleRate);
            }

            if (m_dataPipe)
            {
                DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);

                if (fifo) {
                    fifo->setSize(2*m_sampleRate);
                }
            }

            if (getMessageQueueToGUI())
            {
                MsgReportSampleRate *msg = MsgReportSampleRate::create(m_sampleRate);
                getMessageQueueToGUI()->push(msg);
            }
        }

        return true;
    }
	else
	{
		return false;
	}
}

QByteArray Denoiser::serialize() const
{
    return m_settings.serialize();
}

bool Denoiser::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureDenoiser *msg = MsgConfigureDenoiser::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureDenoiser *msg = MsgConfigureDenoiser::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void Denoiser::applySettings(const DenoiserSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "Denoiser::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    if (m_running)
    {
        DenoiserWorker::MsgConfigureDenoiserWorker *msg = DenoiserWorker::MsgConfigureDenoiserWorker::create(
            settings, settingsKeys, force
        );
        m_worker->getInputMessageQueue()->push(msg);
    }

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

void Denoiser::channelsOrFeaturesChanged(const QStringList& renameFrom, const QStringList& renameTo)
{
    m_availableChannels = m_availableChannelOrFeatureHandler.getAvailableChannelOrFeatureList();
    notifyUpdate(renameFrom, renameTo);
}

void Denoiser::notifyUpdate(const QStringList& renameFrom, const QStringList& renameTo)
{
    if (getMessageQueueToGUI())
    {
        MsgReportChannels *msg = MsgReportChannels::create(renameFrom, renameTo);
        msg->getAvailableChannels() = m_availableChannels;
        getMessageQueueToGUI()->push(msg);
    }
}

void Denoiser::getAvailableChannelsReport()
{
    notifyUpdate(QStringList{}, QStringList{});
}


void Denoiser::setChannel(ChannelAPI *selectedChannel)
{
    if ((selectedChannel == m_selectedChannel) || (m_availableChannels.indexOfObject(selectedChannel) == -1)) {
        return;
    }

    MainCore *mainCore = MainCore::instance();

    if (m_selectedChannel)
    {
        ObjectPipe *pipe = mainCore->getDataPipes().unregisterProducerToConsumer(m_selectedChannel, this, "demod");
        DataFifo *fifo = qobject_cast<DataFifo*>(pipe->m_element);

        if ((fifo) && m_running)
        {
            DenoiserWorker::MsgConnectFifo *msg = DenoiserWorker::MsgConnectFifo::create(fifo, false);
            m_worker->getInputMessageQueue()->push(msg);
        }

        ObjectPipe *messagePipe = mainCore->getMessagePipes().unregisterProducerToConsumer(m_selectedChannel, this, "reportdemod");

        if (messagePipe)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(messagePipe->m_element);

            if (messageQueue) {
                disconnect(messageQueue, &MessageQueue::messageEnqueued, this, nullptr);  // Have to use nullptr, as slot is a lambda.
            }
        }
    }

    m_dataPipe = mainCore->getDataPipes().registerProducerToConsumer(selectedChannel, this, "demod");
    connect(m_dataPipe, SIGNAL(toBeDeleted(int, QObject*)), this, SLOT(handleDataPipeToBeDeleted(int, QObject*)));
    DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);

    if (fifo)
    {
        fifo->setSize(96000);

        if (m_running)
        {
            DenoiserWorker::MsgConnectFifo *msg = DenoiserWorker::MsgConnectFifo::create(fifo, true);
            m_worker->getInputMessageQueue()->push(msg);
        }
    }

    ObjectPipe *messagePipe = mainCore->getMessagePipes().registerProducerToConsumer(selectedChannel, this, "reportdemod");

    if (messagePipe)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(messagePipe->m_element);

        if (messageQueue)
        {
            QObject::connect(
                messageQueue,
                &MessageQueue::messageEnqueued,
                this,
                [=](){ this->handleChannelMessageQueue(messageQueue); },
                Qt::QueuedConnection
            );
        }
    }

    m_selectedChannel = selectedChannel;
}

int Denoiser::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int Denoiser::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setDenoiserSettings(new SWGSDRangel::SWGDenoiserSettings());
    response.getDenoiserSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int Denoiser::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    DenoiserSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureDenoiser *msg = MsgConfigureDenoiser::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    qDebug("Denoiser::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDenoiser *msgToGUI = MsgConfigureDenoiser::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

void Denoiser::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const DenoiserSettings& settings)
{
    if (response.getDenoiserSettings()->getTitle()) {
        *response.getDenoiserSettings()->getTitle() = settings.m_title;
    } else {
        response.getDenoiserSettings()->setTitle(new QString(settings.m_title));
    }

    response.getDenoiserSettings()->setRgbColor(settings.m_rgbColor);
    response.getDenoiserSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getDenoiserSettings()->getReverseApiAddress()) {
        *response.getDenoiserSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getDenoiserSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getDenoiserSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getDenoiserSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getDenoiserSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (response.getDenoiserSettings()->getFileRecordName()) {
        *response.getDenoiserSettings()->getFileRecordName() = settings.m_fileRecordName;
    } else {
        response.getDenoiserSettings()->setFileRecordName(new QString(settings.m_fileRecordName));
    }

    response.getDenoiserSettings()->setRecordToFile(settings.m_recordToFile ? 1 : 0);

    if (settings.m_rollupState)
    {
        if (response.getDenoiserSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getDenoiserSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getDenoiserSettings()->setRollupState(swgRollupState);
        }
    }
}

void Denoiser::webapiUpdateFeatureSettings(
    DenoiserSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getDenoiserSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getDenoiserSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getDenoiserSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getDenoiserSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getDenoiserSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getDenoiserSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getDenoiserSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getDenoiserSettings()->getRollupState());
    }
    if (featureSettingsKeys.contains("fileRecordName")) {
        settings.m_fileRecordName = *response.getDenoiserSettings()->getFileRecordName();
    }
    if (featureSettingsKeys.contains("recordToFile")) {
        settings.m_recordToFile = response.getDenoiserSettings()->getRecordToFile() != 0;
    }
}

void Denoiser::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const DenoiserSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("Denoiser"));
    swgFeatureSettings->setDenoiserSettings(new SWGSDRangel::SWGDenoiserSettings());
    SWGSDRangel::SWGDenoiserSettings *swgDenoiserSettings = swgFeatureSettings->getDenoiserSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("title") || force) {
        swgDenoiserSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgDenoiserSettings->setRgbColor(settings.m_rgbColor);
    }
    if (featureSettingsKeys.contains("fileRecordName")) {
        swgDenoiserSettings->setFileRecordName(new QString(settings.m_fileRecordName));
    }
    if (featureSettingsKeys.contains("recordToFile")) {
        swgDenoiserSettings->setRecordToFile(settings.m_recordToFile ? 1 : 0);
    }
    if (settings.m_rollupState && (featureSettingsKeys.contains("rollupState") || force)) {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgDenoiserSettings->setRollupState(swgRollupState);
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

void Denoiser::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "Denoiser::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("Denoiser::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void Denoiser::handleChannelMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void Denoiser::handleDataPipeToBeDeleted(int reason, QObject *object)
{
    qDebug("Denoiser::handleDataPipeToBeDeleted: %d %p", reason, object);

    if ((reason == 0) && (m_selectedChannel == object))
    {
        DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);

        if ((fifo) && m_running)
        {
            DenoiserWorker::MsgConnectFifo *msg = DenoiserWorker::MsgConnectFifo::create(fifo, false);
            m_worker->getInputMessageQueue()->push(msg);
        }

        m_selectedChannel = nullptr;
    }
}

int Denoiser::webapiActionsPost(
            const QStringList&,
            SWGSDRangel::SWGFeatureActions& query,
            QString& errorMessage) {

    MainCore* m_core = MainCore::instance();
    auto action = query.getDenoiserActions();
    if (action == nullptr) {
        errorMessage = QString("missing DenoiserActions in request");
        return 404;
    }

    auto deviceId = action->getDeviceId();
    auto chanId = action->getChannelId();

    ChannelAPI * chan = m_core->getChannel(deviceId, chanId);
    if (chan == nullptr) {
        errorMessage = QString("device(%1) or channel (%2) on the device does not exist").arg(deviceId).arg(chanId);
        return 404;
    }

    MsgSelectChannel *msg = MsgSelectChannel::create(chan);
    getInputMessageQueue()->push(msg);
    return 200;
}
