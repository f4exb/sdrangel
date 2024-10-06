///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#include "channel/channelapi.h"
#include "maincore.h"
#include "settings/serializable.h"

#include "morsedecoderworker.h"
#include "morsedecoder.h"

MESSAGE_CLASS_DEFINITION(MorseDecoder::MsgConfigureMorseDecoder, Message)
MESSAGE_CLASS_DEFINITION(MorseDecoder::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(MorseDecoder::MsgReportChannels, Message)
MESSAGE_CLASS_DEFINITION(MorseDecoder::MsgSelectChannel, Message)
MESSAGE_CLASS_DEFINITION(MorseDecoder::MsgReportSampleRate, Message)
MESSAGE_CLASS_DEFINITION(MorseDecoder::MsgReportText, Message)

const char* const MorseDecoder::m_featureIdURI = "sdrangel.feature.morsedecoder";
const char* const MorseDecoder::m_featureId = "MorseDecoder";

MorseDecoder::MorseDecoder(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_thread(nullptr),
    m_running(false),
    m_worker(nullptr),
    m_availableChannelOrFeatureHandler(MorseDecoderSettings::m_channelURIs),
    m_selectedChannel(nullptr),
    m_dataPipe(nullptr)
{
    qDebug("MorseDecoder::MorseDecoder: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "MorseDecoder error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &MorseDecoder::networkManagerFinished
    );
    QObject::connect(
        &m_availableChannelOrFeatureHandler,
        &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged,
        this,
        &MorseDecoder::channelsOrFeaturesChanged
    );
    m_availableChannelOrFeatureHandler.scanAvailableChannelsAndFeatures();
}

MorseDecoder::~MorseDecoder()
{
    QObject::disconnect(
        &m_availableChannelOrFeatureHandler,
        &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged,
        this,
        &MorseDecoder::channelsOrFeaturesChanged
    );
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &MorseDecoder::networkManagerFinished
    );
    delete m_networkManager;
    stop();
}

void MorseDecoder::start()
{
    QMutexLocker m_lock(&m_mutex);

    if (m_running) {
        return;
    }

	qDebug("MorseDecoder::start");
    m_thread = new QThread();
    m_worker = new MorseDecoderWorker();
    m_worker->moveToThread(m_thread);

    QObject::connect(
        m_thread,
        &QThread::started,
        m_worker,
        &MorseDecoderWorker::startWork
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

    m_worker->setScopeVis(&m_scopeVis);
    m_worker->setMessageQueueToFeature(getInputMessageQueue());
    m_worker->startWork();
    m_state = StRunning;
    m_thread->start();

    MorseDecoderWorker::MsgConfigureMorseDecoderWorker *msgConfigure
        = MorseDecoderWorker::MsgConfigureMorseDecoderWorker::create(m_settings, QList<QString>(), true);
    m_worker->getInputMessageQueue()->push(msgConfigure);

    MorseDecoderWorker::MsgConfigureSampleRate *msgSampleRate
        = MorseDecoderWorker::MsgConfigureSampleRate::create(m_sampleRate);
    m_worker->getInputMessageQueue()->push(msgSampleRate);

    if (m_dataPipe)
    {
        DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);

        if (fifo)
        {
            MorseDecoderWorker::MsgConnectFifo *msg = MorseDecoderWorker::MsgConnectFifo::create(fifo, true);
            m_worker->getInputMessageQueue()->push(msg);
        }
    }

    m_running = true;
}

void MorseDecoder::stop()
{
    QMutexLocker m_lock(&m_mutex);

    if (!m_running) {
        return;
    }

    qDebug("MorseDecoder::stop");
    m_running = false;

    if (m_dataPipe)
    {
        DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);

        if (fifo)
        {
            MorseDecoderWorker::MsgConnectFifo *msg = MorseDecoderWorker::MsgConnectFifo::create(fifo, false);
            m_worker->getInputMessageQueue()->push(msg);
        }
    }

	m_worker->stopWork();
    m_state = StIdle;
	m_thread->quit();
	m_thread->wait();
}

bool MorseDecoder::handleMessage(const Message& cmd)
{
	if (MsgConfigureMorseDecoder::match(cmd))
	{
        MsgConfigureMorseDecoder& cfg = (MsgConfigureMorseDecoder&) cmd;
        qDebug() << "MorseDecoder::handleMessage: MsgConfigureMorseDecoder";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

		return true;
	}
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "MorseDecoder::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

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
        qDebug("MorseDecoder::handleMessage: MsgSelectChannel: %p %s",
            selectedChannel, qPrintable(selectedChannel->objectName()));
        setChannel(selectedChannel);
        MainCore::MsgChannelDemodQuery *msg = MainCore::MsgChannelDemodQuery::create();
        selectedChannel->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (MainCore::MsgChannelDemodReport::match(cmd))
    {
        qDebug() << "MorseDecoder::handleMessage: MainCore::MsgChannelDemodReport";
        MainCore::MsgChannelDemodReport& report = (MainCore::MsgChannelDemodReport&) cmd;

        if (report.getChannelAPI() == m_selectedChannel)
        {
            m_sampleRate = report.getSampleRate();
            qDebug("MorseDecoder::handleMessage: MainCore::MsgChannelDemodReport: %d S/s", m_sampleRate);

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
    else if (MsgReportText::match(cmd))
    {
        MsgReportText& report = (MsgReportText&) cmd;

        // Write to log file
        if (m_logFile.isOpen())
        {
            // Format text
            m_logStream << MorseDecoderSettings::formatText(report.getText());
        }

        if (getMessageQueueToGUI())
        {
            MsgReportText *msg = new MsgReportText(report);
            getMessageQueueToGUI()->push(msg);
        }

        // Send via UDP
        if (m_settings.m_udpEnabled)
        {
            QByteArray bytes = MorseDecoderSettings::formatText(report.getText()).toUtf8();
            m_udpSocket.writeDatagram(
                bytes,
                bytes.size(),
                QHostAddress(m_settings.m_udpAddress), m_settings.m_udpPort
            );
        }

        return true;
    }
	else
	{
		return false;
	}
}

QByteArray MorseDecoder::serialize() const
{
    return m_settings.serialize();
}

bool MorseDecoder::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureMorseDecoder *msg = MsgConfigureMorseDecoder::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureMorseDecoder *msg = MsgConfigureMorseDecoder::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void MorseDecoder::applySettings(const MorseDecoderSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "MorseDecoder::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    if (m_running)
    {
        MorseDecoderWorker::MsgConfigureMorseDecoderWorker *msg = MorseDecoderWorker::MsgConfigureMorseDecoderWorker::create(
            settings, settingsKeys, force
        );
        m_worker->getInputMessageQueue()->push(msg);
    }

    if (settingsKeys.contains("logEnabled")
        || settingsKeys.contains("logFilename")
        || force)
    {
        if (m_logFile.isOpen())
        {
            m_logStream.flush();
            m_logFile.close();
        }
        if (settings.m_logEnabled && !settings.m_logFilename.isEmpty())
        {
            m_logFile.setFileName(settings.m_logFilename);
            if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            {
                qDebug() << "MorseDecoder::applySettings - Logging to: " << settings.m_logFilename;
                m_logStream.setDevice(&m_logFile);
            }
            else
            {
                qDebug() << "MorseDecoder::applySettings - Unable to open log file: " << settings.m_logFilename;
            }
        }
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

void MorseDecoder::channelsOrFeaturesChanged(const QStringList& renameFrom, const QStringList& renameTo)
{
    m_availableChannels = m_availableChannelOrFeatureHandler.getAvailableChannelOrFeatureList();
    notifyUpdate(renameFrom, renameTo);
}

void MorseDecoder::notifyUpdate(const QStringList& renameFrom, const QStringList& renameTo)
{
    if (getMessageQueueToGUI())
    {
        MsgReportChannels *msg = MsgReportChannels::create(renameFrom, renameTo);
        msg->getAvailableChannels() = m_availableChannels;
        getMessageQueueToGUI()->push(msg);
    }
}

void MorseDecoder::getAvailableChannelsReport()
{
    notifyUpdate(QStringList{}, QStringList{});
}

void MorseDecoder::setChannel(ChannelAPI *selectedChannel)
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
            MorseDecoderWorker::MsgConnectFifo *msg = MorseDecoderWorker::MsgConnectFifo::create(fifo, false);
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
            MorseDecoderWorker::MsgConnectFifo *msg = MorseDecoderWorker::MsgConnectFifo::create(fifo, true);
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

int MorseDecoder::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int MorseDecoder::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setMorseDecoderSettings(new SWGSDRangel::SWGMorseDecoderSettings());
    response.getMorseDecoderSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int MorseDecoder::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    MorseDecoderSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureMorseDecoder *msg = MsgConfigureMorseDecoder::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    qDebug("MorseDecoder::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureMorseDecoder *msgToGUI = MsgConfigureMorseDecoder::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

void MorseDecoder::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const MorseDecoderSettings& settings)
{
    if (response.getMorseDecoderSettings()->getTitle()) {
        *response.getMorseDecoderSettings()->getTitle() = settings.m_title;
    } else {
        response.getMorseDecoderSettings()->setTitle(new QString(settings.m_title));
    }

    response.getMorseDecoderSettings()->setRgbColor(settings.m_rgbColor);
    response.getMorseDecoderSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getMorseDecoderSettings()->getReverseApiAddress()) {
        *response.getMorseDecoderSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getMorseDecoderSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getMorseDecoderSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getMorseDecoderSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getMorseDecoderSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);
    response.getMorseDecoderSettings()->setUdpEnabled(settings.m_udpEnabled ? 1 : 0);

    if (response.getMorseDecoderSettings()->getUdpAddress()) {
        *response.getMorseDecoderSettings()->getUdpAddress() = settings.m_udpAddress;
    } else {
        response.getMorseDecoderSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    }

    response.getMorseDecoderSettings()->setUdpPort(settings.m_udpPort);

    if (response.getMorseDecoderSettings()->getLogFiledName()) {
        *response.getMorseDecoderSettings()->getLogFiledName() = settings.m_logFilename;
    } else {
        response.getMorseDecoderSettings()->setLogFiledName(new QString(settings.m_logFilename));
    }

    response.getMorseDecoderSettings()->setLogEnabled(settings.m_logEnabled ? 1 : 0);
    response.getMorseDecoderSettings()->setAuto(settings.m_auto ? 1 : 0);
    response.getMorseDecoderSettings()->setShowThreshold(settings.m_showThreshold ? 1 : 0);

    if (settings.m_scopeGUI)
    {
        if (response.getMorseDecoderSettings()->getScopeConfig())
        {
            settings.m_scopeGUI->formatTo(response.getMorseDecoderSettings()->getScopeConfig());
        }
        else
        {
            SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
            settings.m_scopeGUI->formatTo(swgGLScope);
            response.getMorseDecoderSettings()->setScopeConfig(swgGLScope);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getMorseDecoderSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getMorseDecoderSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getMorseDecoderSettings()->setRollupState(swgRollupState);
        }
    }
}

void MorseDecoder::webapiUpdateFeatureSettings(
    MorseDecoderSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getMorseDecoderSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getMorseDecoderSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getMorseDecoderSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getMorseDecoderSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getMorseDecoderSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getMorseDecoderSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getMorseDecoderSettings()->getReverseApiFeatureIndex();
    }
    if (featureSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getMorseDecoderSettings()->getUdpEnabled() != 0;
    }
    if (featureSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getMorseDecoderSettings()->getUdpAddress();
    }
    if (featureSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getMorseDecoderSettings()->getUdpPort();
    }
    if (featureSettingsKeys.contains("logFilename")) {
        settings.m_logFilename = *response.getMorseDecoderSettings()->getLogFiledName();
    }
    if (featureSettingsKeys.contains("logEnabled")) {
        settings.m_logEnabled = response.getMorseDecoderSettings()->getLogEnabled() != 0;
    }
    if (featureSettingsKeys.contains("auto")) {
        settings.m_auto = response.getMorseDecoderSettings()->getAuto() != 0;
    }
    if (featureSettingsKeys.contains("showThreshold")) {
        settings.m_showThreshold = response.getMorseDecoderSettings()->getShowThreshold() != 0;
    }
    if (settings.m_scopeGUI && featureSettingsKeys.contains("scopeGUI")) {
        settings.m_scopeGUI->updateFrom(featureSettingsKeys, response.getMorseDecoderSettings()->getScopeConfig());
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getMorseDecoderSettings()->getRollupState());
    }
}

void MorseDecoder::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const MorseDecoderSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("MorseDecoder"));
    swgFeatureSettings->setMorseDecoderSettings(new SWGSDRangel::SWGMorseDecoderSettings());
    SWGSDRangel::SWGMorseDecoderSettings *swgMorseDecoderSettings = swgFeatureSettings->getMorseDecoderSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("title") || force) {
        swgMorseDecoderSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgMorseDecoderSettings->setRgbColor(settings.m_rgbColor);
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

void MorseDecoder::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "MorseDecoder::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("MorseDecoder::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void MorseDecoder::handleChannelMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void MorseDecoder::handleDataPipeToBeDeleted(int reason, QObject *object)
{
    qDebug("MorseDecoder::handleDataPipeToBeDeleted: %d %p", reason, object);

    if ((reason == 0) && (m_selectedChannel == object))
    {
        DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);

        if ((fifo) && m_running)
        {
            MorseDecoderWorker::MsgConnectFifo *msg = MorseDecoderWorker::MsgConnectFifo::create(fifo, false);
            m_worker->getInputMessageQueue()->push(msg);
        }

        m_selectedChannel = nullptr;
    }
}

int MorseDecoder::webapiActionsPost(
            const QStringList&,
            SWGSDRangel::SWGFeatureActions& query,
            QString& errorMessage) {

    MainCore* m_core = MainCore::instance();
    auto action = query.getMorseDecoderActions();

    if (action == nullptr) {
        errorMessage = QString("missing MorseDecoderActions in request");
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
