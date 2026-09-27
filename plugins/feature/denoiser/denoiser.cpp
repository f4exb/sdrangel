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
#include <QJsonDocument>
#include <QJsonObject>

#include "SWGFeatureSettings.h"
#include "SWGFeatureActions.h"
#include "SWGDeviceState.h"

#include "dsp/dspcommands.h"
#include "dsp/datafifo.h"
#include "settings/serializable.h"
#include "channel/channelapi.h"
#include "maincore.h"

#include "denoiserworker.h"
#include "vst3effect.h"
#include "denoiser.h"

MESSAGE_CLASS_DEFINITION(Denoiser::MsgConfigureDenoiser, Message)
MESSAGE_CLASS_DEFINITION(Denoiser::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(Denoiser::MsgReportChannels, Message)
MESSAGE_CLASS_DEFINITION(Denoiser::MsgSelectChannel, Message)
MESSAGE_CLASS_DEFINITION(Denoiser::MsgReportSampleRate, Message)

namespace {

QString vst3ParametersJson(const QMap<quint32, double>& values)
{
    QJsonObject object;

    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        object.insert(QString::number(it.key()), it.value());
    }

    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QMap<quint32, double> parseVst3Parameters(const QString& json)
{
    QMap<quint32, double> values;
    const QJsonObject object = QJsonDocument::fromJson(json.toUtf8()).object();

    for (auto it = object.begin(); it != object.end(); ++it) 
    {
        bool validId = false;
        const quint32 id = it.key().toUInt(&validId);
        const double value = it.value().toDouble(-1.0);
        if (validId && value >= 0.0 && value <= 1.0) values.insert(id, value);
    }

    return values;
}

}

const char* const Denoiser::m_featureIdURI = "sdrangel.feature.denoiser";
const char* const Denoiser::m_featureId = "Denoiser";

Denoiser::Denoiser(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_thread(nullptr),
    m_running(false),
    m_worker(nullptr),
    m_availableChannelOrFeatureHandler(DenoiserSettings::m_channelURIs),
    m_selectedChannel(nullptr),
    m_dataPipe(nullptr),
    m_sampleRate(0)
{
    qDebug("Denoiser::Denoiser: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    setState(StIdle);
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

    useDefaultSampleRate();

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
    m_worker->setDemodProducer(this);
    m_worker->startWork();
    setState(StRunning);
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

    if (m_levelMeter) {
        connect(m_worker, SIGNAL(levelChanged(qreal, qreal, int)), m_levelMeter, SLOT(levelChanged(qreal, qreal, int)));
    }


    m_running = true;
    configureVst3();
    if (m_settings.m_enableDenoiser &&
        m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_Nvidia)
    {
        QString error;
        if (!NvidiaAudioEffects::isAvailable(error))
        {
            m_errorMessage = error + QStringLiteral(" Audio is passing through.");
            qWarning() << "Denoiser:" << m_errorMessage;
            setState(StError);
        }
    }
}

void Denoiser::stop()
{
    QMutexLocker m_lock(&m_mutex);

    if (!m_running) {
        return;
    }

    qDebug("Denoiser::stop");
    m_running = false;
    m_worker->setVst3Effect(nullptr);
    m_vst3Effect.reset();

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
    setState(StIdle);
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
    else if (DenoiserWorker::MsgReportNvidiaStatus::match(cmd))
    {
        const auto& report = static_cast<const DenoiserWorker::MsgReportNvidiaStatus&>(cmd);
        if (m_running && m_settings.m_enableDenoiser &&
            m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_Nvidia)
        {
            if (report.getError().isEmpty()) {
                setState(StRunning);
            } else {
                m_errorMessage = report.getError();
                setState(StError);
            }
        }
        return true;
    }
    else if (DenoiserWorker::MsgReportVst3Status::match(cmd))
    {
        const auto& report = static_cast<const DenoiserWorker::MsgReportVst3Status&>(cmd);
        if (m_running && m_settings.m_enableDenoiser &&
            m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_Vst3)
        {
            m_errorMessage = report.getError();
            setState(StError);
        }
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
        QObject *selectedChannel = cfg.getChannel();
        if (!selectedChannel) {
            return true;
        }
        qDebug("Denoiser::handleMessage: MsgSelectChannel: %p %s",
            selectedChannel, qPrintable(selectedChannel->objectName()));
        setChannel(selectedChannel);
        if (m_selectedChannel == selectedChannel) {
            querySelectedSource();
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        reportDemodSampleRate();
        return true;
    }
    else if (MainCore::MsgChannelDemodReport::match(cmd))
    {
        qDebug() << "Denoiser::handleMessage: MainCore::MsgChannelDemodReport";
        MainCore::MsgChannelDemodReport& report = (MainCore::MsgChannelDemodReport&) cmd;

        if (report.getChannelAPI() == m_selectedChannel) {
            applyReportedSampleRate(report.getSampleRate());
        }

        return true;
    }
    else if (MainCore::MsgFeatureDemodReport::match(cmd))
    {
        const auto& report = static_cast<const MainCore::MsgFeatureDemodReport&>(cmd);
        if (report.getFeature() == m_selectedChannel) {
            applyReportedSampleRate(report.getSampleRate());
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
                settingsKeys.contains("reverseAPIFeatureIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    const auto previousParameters = m_settings.m_vst3Parameters;
    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (force || settingsKeys.contains("selectedSource")) {
        restoreSelectedSource();
    }

    if (m_running &&
        (force || settingsKeys.contains("denoiserType") || settingsKeys.contains("enableDenoiser") ||
         settingsKeys.contains("nvidiaIntensity") || settingsKeys.contains("nvidiaVad")))
    {
        QString error;
        if (m_settings.m_enableDenoiser &&
            m_settings.m_denoiserType == DenoiserSettings::DenoiserType::DenoiserType_Nvidia &&
            !NvidiaAudioEffects::isAvailable(error))
        {
            m_errorMessage = error + QStringLiteral(" Audio is passing through.");
            qWarning() << "Denoiser:" << m_errorMessage;
            setState(StError);
        }
        else if (m_state == StError)
        {
            setState(StRunning);
        }
    }
    if (m_running && (force || settingsKeys.contains("denoiserType") ||
        settingsKeys.contains("enableDenoiser") ||
        settingsKeys.contains("vst3ModulePath") || settingsKeys.contains("vst3ClassId") ||
        settingsKeys.contains("vst3State")))
    {
        configureVst3();
    } 
    else if (m_vst3Effect && settingsKeys.contains("vst3Parameters")) 
    {
        for (auto it = m_settings.m_vst3Parameters.cbegin(); it != m_settings.m_vst3Parameters.cend(); ++it) {
            if (!previousParameters.contains(it.key()) || previousParameters.value(it.key()) != it.value()) {
                m_vst3Effect->setParameter(it.key(), it.value());
            }
        }
    }
}

QVector<Vst3ParameterInfo> Denoiser::vst3Parameters() const
{
    return m_vst3Effect ? m_vst3Effect->parameters() : QVector<Vst3ParameterInfo>();
}

bool Denoiser::hasVst3Effect() const
{
    return m_vst3Effect != nullptr;
}

QString Denoiser::vst3ParameterText(quint32 id, double value) const
{
    return m_vst3Effect ? m_vst3Effect->parameterText(id, value) : QString::number(value, 'f', 3);
}

void Denoiser::configureVst3()
{
    if (!m_worker) {
        return;
    }
    m_worker->setVst3Effect(nullptr);
    m_vst3Effect.reset();
    if (!m_running || !m_settings.m_enableDenoiser ||
        m_settings.m_denoiserType != DenoiserSettings::DenoiserType::DenoiserType_Vst3) 
    {
        return;
    }
    if (m_sampleRate <= 0) {
        return;
    }
    if (m_settings.m_vst3ModulePath.isEmpty() || m_settings.m_vst3ClassId.size() != 16) 
    {
        m_errorMessage = QStringLiteral("Select a VST3 audio effect");
        setState(StError);
        return;
    }
    auto effect = std::make_unique<Vst3Effect>();
    QString error;
    if (!effect->open(m_settings.m_vst3ModulePath, m_settings.m_vst3ClassId, m_sampleRate, 2, error, m_settings.m_vst3State))
    {
        m_errorMessage = QStringLiteral("VST3: %1. Audio is passing through.").arg(error);
        setState(StError);
        qWarning() << m_errorMessage;
        return;
    }
    for (auto it = m_settings.m_vst3Parameters.cbegin(); it != m_settings.m_vst3Parameters.cend(); ++it) {
        effect->setParameter(it.key(), it.value());
    }
    m_worker->setVst3Effect(effect.get());
    m_vst3Effect = std::move(effect);
    if (m_state == StError) {
        setState(StRunning);
    }
}

void Denoiser::useDefaultSampleRate()
{
    if (!m_selectedChannel || m_sampleRate > 0) {
        return;
    }

    m_sampleRate = 48000;

    if (m_dataPipe) 
    {
        DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);
        if (fifo) {
            fifo->setSize(2 * m_sampleRate);
        }
    }

    if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(MsgReportSampleRate::create(m_sampleRate));
    }
    reportDemodSampleRate();
}

void Denoiser::applyReportedSampleRate(int sampleRate)
{
    if (sampleRate <= 0) return;

    const bool sampleRateChanged = m_sampleRate != sampleRate;
    m_sampleRate = sampleRate;

    if (m_running && (sampleRateChanged || !m_vst3Effect ||
        m_settings.m_denoiserType != DenoiserSettings::DenoiserType::DenoiserType_Vst3))
    {
        m_worker->applySampleRate(m_sampleRate);
        configureVst3();
    }

    if (m_dataPipe)
    {
        DataFifo *fifo = qobject_cast<DataFifo*>(m_dataPipe->m_element);
        if (fifo) fifo->setSize(2 * m_sampleRate);
    }

    if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(MsgReportSampleRate::create(m_sampleRate));
    }
    if (sampleRateChanged) reportDemodSampleRate();
}

void Denoiser::reportDemodSampleRate()
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    for (const auto& pipe : pipes)
    {
        MessageQueue *queue = qobject_cast<MessageQueue*>(pipe->m_element);
        if (queue) {
            queue->push(MainCore::MsgFeatureDemodReport::create(this, m_sampleRate));
        }
    }
}

void Denoiser::channelsOrFeaturesChanged(const QStringList& renameFrom, const QStringList& renameTo)
{
    m_availableChannels.clear();
    for (const auto& source : m_availableChannelOrFeatureHandler.getAvailableChannelOrFeatureList()) {
        if (source.m_object != this) {
            m_availableChannels.append(source);
        }
    }

    for (int i = 0; i < renameFrom.size() && i < renameTo.size(); ++i)
    {
        if (m_settings.m_selectedSource == renameFrom.at(i)) {
            m_settings.m_selectedSource = renameTo.at(i);
        }
    }

    if (!m_settings.m_selectedSource.isEmpty()) {
        restoreSelectedSource();
    }
    notifyUpdate(renameFrom, renameTo);
}

void Denoiser::notifyUpdate(const QStringList& renameFrom, const QStringList& renameTo, bool autoSelect)
{
    if (getMessageQueueToGUI())
    {
        MsgReportChannels *msg = MsgReportChannels::create(renameFrom, renameTo, m_selectedChannel, autoSelect);
        msg->getAvailableChannels() = m_availableChannels;
        getMessageQueueToGUI()->push(msg);
    }
}

void Denoiser::getAvailableChannelsReport()
{
    notifyUpdate(QStringList{}, QStringList{});
}


void Denoiser::querySelectedSource()
{
    if (auto *channel = qobject_cast<ChannelAPI*>(m_selectedChannel)) {
        channel->getInputMessageQueue()->push(MainCore::MsgChannelDemodQuery::create());
    } else if (auto *feature = qobject_cast<Feature*>(m_selectedChannel)) {
        feature->getInputMessageQueue()->push(MainCore::MsgChannelDemodQuery::create());
    }
}

void Denoiser::restoreSelectedSource()
{
    if (m_settings.m_selectedSource.isEmpty())
    {
        setChannel(nullptr);
        return;
    }

    const int index = m_availableChannels.indexOfLongId(m_settings.m_selectedSource);
    if (index < 0)
    {
        setChannel(nullptr, true);
        return;
    }

    QObject *source = m_availableChannels.at(index).m_object;
    if (source != m_selectedChannel)
    {
        setChannel(source);
        if (source == m_selectedChannel) 
        {
            querySelectedSource();
        } 
        else 
        {
            const int currentIndex = m_availableChannels.indexOfObject(m_selectedChannel);
            m_settings.m_selectedSource = currentIndex >= 0 ? m_availableChannels.at(currentIndex).getLongId() : QString();
        }
    }
}

void Denoiser::setChannel(QObject *selectedChannel, bool preserveSavedSource)
{
    if (selectedChannel == m_selectedChannel) {
        return;
    }

    const int selectedIndex = selectedChannel ? m_availableChannels.indexOfObject(selectedChannel) : -1;
    if (selectedChannel && selectedIndex < 0) {
        return;
    }

    QObject *upstream = selectedChannel;
    while (auto *denoiser = qobject_cast<Denoiser*>(upstream))
    {
        if (denoiser == this) 
        {
            qWarning() << "Denoiser: rejecting a cyclic denoiser connection";
            notifyUpdate(QStringList{}, QStringList{}, false);
            return;
        }
        upstream = denoiser->m_selectedChannel;
    }

    m_sampleRate = 0;
    if (m_running) 
    {
        m_worker->setVst3Effect(nullptr);
        m_vst3Effect.reset();
        m_worker->applySampleRate(0);
    }
    if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(MsgReportSampleRate::create(0));
    }

    MainCore *mainCore = MainCore::instance();

    if (m_selectedChannel)
    {
        ObjectPipe *pipe = mainCore->getDataPipes().unregisterProducerToConsumer(m_selectedChannel, this, "demod");
        DataFifo *fifo = pipe ? qobject_cast<DataFifo*>(pipe->m_element) : nullptr;

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

    m_dataPipe = nullptr;
    m_selectedChannel = nullptr;

    if (!selectedChannel)
    {
        if (!preserveSavedSource) {
            m_settings.m_selectedSource.clear();
        }
        return;
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
    m_settings.m_selectedSource = m_availableChannels.at(selectedIndex).getLongId();
    useDefaultSampleRate();
    if (m_running) 
    {
        m_worker->applySampleRate(m_sampleRate);
        configureVst3();
    }
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
    response.getDenoiserSettings()->setDenoiserType(static_cast<int>(settings.m_denoiserType));
    response.getDenoiserSettings()->setEnableDenoiser(settings.m_enableDenoiser ? 1 : 0);
    response.getDenoiserSettings()->setNvidiaIntensity(settings.m_nvidiaIntensity);
    response.getDenoiserSettings()->setNvidiaVad(settings.m_nvidiaVad ? 1 : 0);
    *response.getDenoiserSettings()->getVst3ModulePath() = settings.m_vst3ModulePath;
    *response.getDenoiserSettings()->getVst3ClassId() = QString::fromLatin1(settings.m_vst3ClassId.toHex());
    *response.getDenoiserSettings()->getVst3Parameters() = vst3ParametersJson(settings.m_vst3Parameters);
    *response.getDenoiserSettings()->getVst3State() = QString::fromLatin1(settings.m_vst3State.toBase64());
    response.getDenoiserSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getDenoiserSettings()->setVolumeTenths(settings.m_volumeTenths);
    if (response.getDenoiserSettings()->getAudioDeviceName()) {
        *response.getDenoiserSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getDenoiserSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
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
    if (featureSettingsKeys.contains("denoiserType")) 
    {
        const int type = response.getDenoiserSettings()->getDenoiserType();
        if (type >= 0 && type <= 3) {
            settings.m_denoiserType = static_cast<DenoiserSettings::DenoiserType>(type);
        }
    }
    if (featureSettingsKeys.contains("enableDenoiser")) {
        settings.m_enableDenoiser = response.getDenoiserSettings()->getEnableDenoiser() != 0;
    }
    if (featureSettingsKeys.contains("nvidiaIntensity")) {
        settings.m_nvidiaIntensity = qBound(0, response.getDenoiserSettings()->getNvidiaIntensity(), 100);
    }
    if (featureSettingsKeys.contains("nvidiaVad")) {
        settings.m_nvidiaVad = response.getDenoiserSettings()->getNvidiaVad() != 0;
    }
    if (featureSettingsKeys.contains("vst3ModulePath") && response.getDenoiserSettings()->getVst3ModulePath()) {
        settings.m_vst3ModulePath = *response.getDenoiserSettings()->getVst3ModulePath();
    }
    if (featureSettingsKeys.contains("vst3ClassId") && response.getDenoiserSettings()->getVst3ClassId()) {
        settings.m_vst3ClassId = QByteArray::fromHex(response.getDenoiserSettings()->getVst3ClassId()->toLatin1());
    }
    if (featureSettingsKeys.contains("vst3Parameters") && response.getDenoiserSettings()->getVst3Parameters()) {
        settings.m_vst3Parameters = parseVst3Parameters(*response.getDenoiserSettings()->getVst3Parameters());
    }
    if (featureSettingsKeys.contains("vst3State") && response.getDenoiserSettings()->getVst3State()) {
        settings.m_vst3State = QByteArray::fromBase64(response.getDenoiserSettings()->getVst3State()->toLatin1());
    }
    if (featureSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getDenoiserSettings()->getAudioMute() != 0;
    }
    if (featureSettingsKeys.contains("volumeTenths")) {
        settings.m_volumeTenths = response.getDenoiserSettings()->getVolumeTenths();
    }
    if (featureSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getDenoiserSettings()->getAudioDeviceName();
    }
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

    if (featureSettingsKeys.contains("useReverseAPI") || force) {
        swgDenoiserSettings->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);
    }
    if (featureSettingsKeys.contains("reverseAPIAddress") || force) {
        swgDenoiserSettings->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }
    if (featureSettingsKeys.contains("reverseAPIPort") || force) {
        swgDenoiserSettings->setReverseApiPort(settings.m_reverseAPIPort);
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex") || force) {
        swgDenoiserSettings->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex") || force) {
        swgDenoiserSettings->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);
    }
    if (featureSettingsKeys.contains("denoiserType") || force) {
        swgDenoiserSettings->setDenoiserType(static_cast<int>(settings.m_denoiserType));
    }
    if (featureSettingsKeys.contains("enableDenoiser") || force) {
        swgDenoiserSettings->setEnableDenoiser(settings.m_enableDenoiser ? 1 : 0);
    }
    if (featureSettingsKeys.contains("nvidiaIntensity") || force) {
        swgDenoiserSettings->setNvidiaIntensity(settings.m_nvidiaIntensity);
    }
    if (featureSettingsKeys.contains("nvidiaVad") || force) {
        swgDenoiserSettings->setNvidiaVad(settings.m_nvidiaVad ? 1 : 0);
    }
    if (featureSettingsKeys.contains("vst3ModulePath") || force) {
        swgDenoiserSettings->setVst3ModulePath(new QString(settings.m_vst3ModulePath));
    }
    if (featureSettingsKeys.contains("vst3ClassId") || force) {
        swgDenoiserSettings->setVst3ClassId(new QString(QString::fromLatin1(settings.m_vst3ClassId.toHex())));
    }
    if (featureSettingsKeys.contains("vst3Parameters") || force) {
        swgDenoiserSettings->setVst3Parameters(new QString(vst3ParametersJson(settings.m_vst3Parameters)));
    }
    if (featureSettingsKeys.contains("vst3State") || force) {
        swgDenoiserSettings->setVst3State(new QString(QString::fromLatin1(settings.m_vst3State.toBase64())));
    }
    if (featureSettingsKeys.contains("audioMute") || force) {
        swgDenoiserSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (featureSettingsKeys.contains("volumeTenths") || force) {
        swgDenoiserSettings->setVolumeTenths(settings.m_volumeTenths);
    }
    if (featureSettingsKeys.contains("audioDeviceName") || force) {
        swgDenoiserSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
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
        m_dataPipe = nullptr; // The pipe and its FIFO are freed by the pipes GC.
        m_sampleRate = 0;
        if (m_running) 
        {
            m_worker->setVst3Effect(nullptr);
            m_vst3Effect.reset();
            m_worker->applySampleRate(0);
        }
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(MsgReportSampleRate::create(0));
        }
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
