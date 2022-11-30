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
#include "SWGDeviceState.h"
#include "SWGChannelReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/devicesamplesource.h"
#include "device/deviceset.h"
#include "channel/channelapi.h"
#include "settings/serializable.h"
#include "maincore.h"

#include "vorlocalizerreport.h"
#include "vorlocalizerworker.h"
#include "vorlocalizer.h"

MESSAGE_CLASS_DEFINITION(VORLocalizer::MsgConfigureVORLocalizer, Message)
MESSAGE_CLASS_DEFINITION(VORLocalizer::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(VORLocalizer::MsgAddVORChannel, Message)
MESSAGE_CLASS_DEFINITION(VORLocalizer::MsgRemoveVORChannel, Message)
MESSAGE_CLASS_DEFINITION(VORLocalizer::MsgRefreshChannels, Message)

const char* const VORLocalizer::m_featureIdURI = "sdrangel.feature.vorlocalizer";
const char* const VORLocalizer::m_featureId = "VORLocalizer";

VORLocalizer::VORLocalizer(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_thread(nullptr),
    m_worker(nullptr),
    m_running(false)
{
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "VORLocalizer error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &VORLocalizer::networkManagerFinished
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::channelAdded,
        this,
        &VORLocalizer::handleChannelAdded
    );
}

VORLocalizer::~VORLocalizer()
{
    QObject::disconnect(
        MainCore::instance(),
        &MainCore::channelAdded,
        this,
        &VORLocalizer::handleChannelAdded
    );
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &VORLocalizer::networkManagerFinished
    );
    delete m_networkManager;
    stop();
}

void VORLocalizer::start()
{
    QMutexLocker m_lock(&m_mutex);

    if (m_running) {
        return;
    }

	qDebug("VORLocalizer::start");
    m_thread = new QThread();
    m_worker = new VorLocalizerWorker(getWebAPIAdapterInterface());
    m_worker->moveToThread(m_thread);

    QObject::connect(
        m_thread,
        &QThread::started,
        m_worker,
        &VorLocalizerWorker::startWork
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
    m_worker->setAvailableChannels(&m_availableChannels);
    m_worker->startWork();
    m_state = StRunning;
    m_thread->start();

    VorLocalizerWorker::MsgConfigureVORLocalizerWorker *msg = VorLocalizerWorker::MsgConfigureVORLocalizerWorker::create(m_settings, QList<QString>(), true);
    m_worker->getInputMessageQueue()->push(msg);

    m_running = true;
}

void VORLocalizer::stop()
{
    QMutexLocker m_lock(&m_mutex);

    if (!m_running) {
        return;
    }

    qDebug("VORLocalizer::stop");
    m_running = false;
	m_worker->stopWork();
    m_state = StIdle;
	m_thread->quit();
	m_thread->wait();
}

bool VORLocalizer::handleMessage(const Message& cmd)
{
	if (MsgConfigureVORLocalizer::match(cmd))
	{
        MsgConfigureVORLocalizer& cfg = (MsgConfigureVORLocalizer&) cmd;
        qDebug() << "VORLocalizer::handleMessage: MsgConfigureVORLocalizer";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

		return true;
	}
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "VORLocalizer::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
    else if (MsgRefreshChannels::match(cmd))
    {
        qDebug() << "VORLocalizer::handleMessage: MsgRefreshChannels";
        updateChannels();

        return true;
    }
	else if (MainCore::MsgChannelReport::match(cmd))
    {
        MainCore::MsgChannelReport& report = (MainCore::MsgChannelReport&) cmd;
        SWGSDRangel::SWGChannelReport* swgChannelReport = report.getSWGReport();
        QString *channelType  = swgChannelReport->getChannelType();

        if (*channelType == "VORDemod")
        {
            SWGSDRangel::SWGVORDemodReport *swgVORDemodReport = swgChannelReport->getVorDemodReport();
            int navId = swgVORDemodReport->getNavId();

            if (navId < 0) { // disregard message for unallocated channels
                return true;
            }

            bool singlePlan = (m_vorSinglePlans.contains(navId)) && !m_settings.m_forceRRAveraging ?
                m_vorSinglePlans[navId] :
                false;

            // qDebug() << "VORLocalizer::handleMessage: MainCore::MsgChannelReport(VORDemod): "
            //     << "navId:" << navId
            //     << "singlePlanProvided" << m_vorSinglePlans.contains(navId)
            //     << "singlePlan:" << singlePlan;

            if (m_vorChannelReports.contains(navId))
            {
                m_vorChannelReports[navId].m_radial = swgVORDemodReport->getRadial();
                m_vorChannelReports[navId].m_refMag = swgVORDemodReport->getRefMag();
                m_vorChannelReports[navId].m_varMag = swgVORDemodReport->getVarMag();
                m_vorChannelReports[navId].m_validRadial = swgVORDemodReport->getValidRadial() != 0;
                m_vorChannelReports[navId].m_validRefMag = swgVORDemodReport->getValidRefMag() != 0;
                m_vorChannelReports[navId].m_validVarMag = swgVORDemodReport->getValidVarMag() != 0;
                m_vorChannelReports[navId].m_morseIdent = *swgVORDemodReport->getMorseIdent();
            }
            else
            {
                m_vorChannelReports[navId] = VORChannelReport{
                    swgVORDemodReport->getRadial(),
                    swgVORDemodReport->getRefMag(),
                    swgVORDemodReport->getVarMag(),
                    AverageUtil<float, double>(),
                    AverageUtil<float, double>(),
                    AverageUtil<float, double>(),
                    swgVORDemodReport->getValidRadial() != 0,
                    swgVORDemodReport->getValidRefMag() != 0,
                    swgVORDemodReport->getValidVarMag() != 0,
                    *swgVORDemodReport->getMorseIdent()
                };
            }

            if (m_vorChannelReports[navId].m_validRadial) {
                m_vorChannelReports[navId].m_radialAvg(swgVORDemodReport->getRadial());
            }
            if (m_vorChannelReports[navId].m_validRefMag) {
                m_vorChannelReports[navId].m_refMagAvg(swgVORDemodReport->getRefMag());
            }
            if (m_vorChannelReports[navId].m_validVarMag) {
                m_vorChannelReports[navId].m_varMagAvg(swgVORDemodReport->getVarMag());
            }

            if (getMessageQueueToGUI())
            {
                float radial = ((m_vorChannelReports[navId].m_radialAvg.getNumSamples() == 0) || singlePlan) ?
                    m_vorChannelReports[navId].m_radial :
                    m_vorChannelReports[navId].m_radialAvg.instantAverage();
                float refMag = ((m_vorChannelReports[navId].m_refMagAvg.getNumSamples() == 0) || singlePlan) ?
                    m_vorChannelReports[navId].m_refMag :
                    m_vorChannelReports[navId].m_refMagAvg.instantAverage();
                float varMag = ((m_vorChannelReports[navId].m_varMagAvg.getNumSamples() == 0) || singlePlan) ?
                    m_vorChannelReports[navId].m_varMag :
                    m_vorChannelReports[navId].m_varMagAvg.instantAverage();
                bool validRadial = singlePlan ? m_vorChannelReports[navId].m_validRadial :
                    m_vorChannelReports[navId].m_radialAvg.getNumSamples() != 0 || m_vorChannelReports[navId].m_validRadial;
                bool validRefMag = singlePlan ? m_vorChannelReports[navId].m_validRefMag :
                    m_vorChannelReports[navId].m_refMagAvg.getNumSamples() != 0 || m_vorChannelReports[navId].m_validRefMag;
                bool validVarMag = singlePlan ? m_vorChannelReports[navId].m_validVarMag :
                    m_vorChannelReports[navId].m_varMagAvg.getNumSamples() != 0 || m_vorChannelReports[navId].m_validVarMag;
                VORLocalizerReport::MsgReportRadial *msgRadial = VORLocalizerReport::MsgReportRadial::create(
                    navId,
                    radial,
                    refMag,
                    varMag,
                    validRadial,
                    validRefMag,
                    validVarMag
                );
                getMessageQueueToGUI()->push(msgRadial);
                VORLocalizerReport::MsgReportIdent *msgIdent = VORLocalizerReport::MsgReportIdent::create(
                    navId,
                    m_vorChannelReports[navId].m_morseIdent
                );
                getMessageQueueToGUI()->push(msgIdent);
            }
        }

        return true;
    }
    else if (VORLocalizerReport::MsgReportServiceddVORs::match(cmd))
    {
        qDebug() << "VORLocalizer::handleMessage: MsgReportServiceddVORs";
        VORLocalizerReport::MsgReportServiceddVORs& report = (VORLocalizerReport::MsgReportServiceddVORs&) cmd;
        std::vector<int>& vorNavIds = report.getNavIds();
        m_vorSinglePlans = report.getSinglePlans();

        for (std::vector<int>::const_iterator it = vorNavIds.begin(); it != vorNavIds.end(); ++it)
        {
            m_vorChannelReports[*it].m_radialAvg.reset();
            m_vorChannelReports[*it].m_refMagAvg.reset();
            m_vorChannelReports[*it].m_varMagAvg.reset();
        }

        if (getMessageQueueToGUI())
        {
            VORLocalizerReport::MsgReportServiceddVORs *msgToGUI = VORLocalizerReport::MsgReportServiceddVORs::create();
            msgToGUI->getNavIds() = vorNavIds;
            getMessageQueueToGUI()->push(msgToGUI);
        }

        return true;
    }
    else
	{
		return false;
	}
}

void VORLocalizer::handleMessagePipeToBeDeleted(int reason, QObject* object)
{
    if ((reason == 0) && m_availableChannels.contains((ChannelAPI*) object)) // producer (channel)
    {
        m_availableChannels.remove((ChannelAPI*) object);
        updateChannels();
    }
}

QByteArray VORLocalizer::serialize() const
{
    return m_settings.serialize();
}

bool VORLocalizer::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureVORLocalizer *msg = MsgConfigureVORLocalizer::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureVORLocalizer *msg = MsgConfigureVORLocalizer::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void VORLocalizer::applySettings(const VORLocalizerSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "VORLocalizer::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    if (m_running)
    {
        VorLocalizerWorker::MsgConfigureVORLocalizerWorker *msg = VorLocalizerWorker::MsgConfigureVORLocalizerWorker::create(
            settings, settingsKeys, force
        );
        m_worker->getInputMessageQueue()->push(msg);
    }

    if (settingsKeys.contains("useReverseAPI"))
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
                settingsKeys.contains("reverseAPIAddress") ||
                settingsKeys.contains("reverseAPIPort") ||
                settingsKeys.contains("reverseAPIFeatureSetIndex") ||
                settingsKeys.contains("m_reverseAPIFeatureIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

void VORLocalizer::updateChannels()
{
    MainCore *mainCore = MainCore::instance();
    MessagePipes& messagePipes = mainCore->getMessagePipes();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
    m_availableChannels.clear();

    for (const auto& deviceSet : deviceSets)
    {
        DSPDeviceSourceEngine *deviceSourceEngine = deviceSet->m_deviceSourceEngine;

        if (deviceSourceEngine)
        {
            DeviceSampleSource *deviceSource = deviceSourceEngine->getSource();
            quint64 deviceCenterFrequency = deviceSource->getCenterFrequency();
            int basebandSampleRate = deviceSource->getSampleRate();

            for (int chi = 0; chi < deviceSet->getNumberOfChannels(); chi++)
            {
                ChannelAPI *channel = deviceSet->getChannelAt(chi);

                if (channel->getURI() == "sdrangel.channel.vordemod")
                {
                    if (!m_availableChannels.contains(channel))
                    {
                        ObjectPipe *pipe = messagePipes.registerProducerToConsumer(channel, this, "report");
                        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                        QObject::connect(
                            messageQueue,
                            &MessageQueue::messageEnqueued,
                            this,
                            [=](){ this->handleChannelMessageQueue(messageQueue); },
                            Qt::QueuedConnection
                        );
                        connect(pipe, SIGNAL(toBeDeleted(int, QObject*)), this, SLOT(handleMessagePipeToBeDeleted(int, QObject*)));
                    }

                    VORLocalizerSettings::AvailableChannel availableChannel =
                        VORLocalizerSettings::AvailableChannel{deviceSet->getIndex(), chi, channel, deviceCenterFrequency, basebandSampleRate, -1};
                    m_availableChannels[channel] = availableChannel;
                }
            }
        }
    }

    notifyUpdateChannels();
}

int VORLocalizer::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int VORLocalizer::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setVorLocalizerSettings(new SWGSDRangel::SWGVORLocalizerSettings());
    response.getVorLocalizerSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int VORLocalizer::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    VORLocalizerSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureVORLocalizer *msg = MsgConfigureVORLocalizer::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    qDebug("VORLocalizer::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureVORLocalizer *msgToGUI = MsgConfigureVORLocalizer::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

int VORLocalizer::webapiReportGet(
    SWGSDRangel::SWGFeatureReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setVorLocalizerReport(new SWGSDRangel::SWGVORLocalizerReport());
    response.getVorLocalizerReport()->init();
    webapiFormatFeatureReport(response);
    return 200;
}

void VORLocalizer::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response)
{
    response.getVorLocalizerReport()->setRunningState(getState());
}

int VORLocalizer::webapiActionsPost(
    const QStringList& featureActionsKeys,
    SWGSDRangel::SWGFeatureActions& query,
    QString& errorMessage)
{
    SWGSDRangel::SWGVORLocalizerActions *swgVORLocalizerActions = query.getVorLocalizerActions();

    if (swgVORLocalizerActions)
    {
        if (featureActionsKeys.contains("run"))
        {
            bool featureRun = swgVORLocalizerActions->getRun() != 0;
            MsgStartStop *msg = MsgStartStop::create(featureRun);
            getInputMessageQueue()->push(msg);
            return 202;
        }
        else
        {
            errorMessage = "Unknown action";
            return 400;
        }
    }
    else
    {
        errorMessage = "Missing VORLocalizerActions in query";
        return 400;
    }
}

void VORLocalizer::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const VORLocalizerSettings& settings)
{
    if (response.getVorLocalizerSettings()->getTitle()) {
        *response.getVorLocalizerSettings()->getTitle() = settings.m_title;
    } else {
        response.getVorLocalizerSettings()->setTitle(new QString(settings.m_title));
    }

    response.getVorLocalizerSettings()->setRgbColor(settings.m_rgbColor);
    response.getVorLocalizerSettings()->setMagDecAdjust(settings.m_magDecAdjust);
    response.getVorLocalizerSettings()->setRrTime(settings.m_rrTime);
    response.getVorLocalizerSettings()->setForceRrAveraging(settings.m_forceRRAveraging ? 1 : 0);
    response.getVorLocalizerSettings()->setCenterShift(settings.m_centerShift);

    response.getVorLocalizerSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getVorLocalizerSettings()->getReverseApiAddress()) {
        *response.getVorLocalizerSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getVorLocalizerSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getVorLocalizerSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getVorLocalizerSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getVorLocalizerSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getVorLocalizerSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getVorLocalizerSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getVorLocalizerSettings()->setRollupState(swgRollupState);
        }
    }

    if (!response.getVorLocalizerSettings()->getColumnIndexes()) {
        response.getVorLocalizerSettings()->setColumnIndexes(new QList<int>());
    }

    response.getVorLocalizerSettings()->getColumnIndexes()->clear();

    for (int i = 0; i < VORLocalizerSettings::VORDEMOD_COLUMNS; i++) {
        response.getVorLocalizerSettings()->getColumnIndexes()->push_back(settings.m_columnIndexes[i]);
    }

    if (!response.getVorLocalizerSettings()->getColumnSizes()) {
        response.getVorLocalizerSettings()->setColumnSizes(new QList<int>());
    }

    response.getVorLocalizerSettings()->getColumnSizes()->clear();

    for (int i = 0; i < VORLocalizerSettings::VORDEMOD_COLUMNS; i++) {
        response.getVorLocalizerSettings()->getColumnSizes()->push_back(settings.m_columnSizes[i]);
    }
}

void VORLocalizer::webapiUpdateFeatureSettings(
    VORLocalizerSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getVorLocalizerSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getVorLocalizerSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("magDecAdjust")) {
        settings.m_magDecAdjust = response.getVorLocalizerSettings()->getMagDecAdjust();
    }
    if (featureSettingsKeys.contains("rrTime")) {
        settings.m_rrTime = response.getVorLocalizerSettings()->getRrTime();
    }
    if (featureSettingsKeys.contains("forceRRAveraging")) {
        settings.m_forceRRAveraging = response.getVorLocalizerSettings()->getForceRrAveraging() != 0;
    }
    if (featureSettingsKeys.contains("centerShift")) {
        settings.m_centerShift = response.getVorLocalizerSettings()->getCenterShift();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getVorLocalizerSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getVorLocalizerSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getVorLocalizerSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getVorLocalizerSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getVorLocalizerSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getVorLocalizerSettings()->getRollupState());
    }

    if (featureSettingsKeys.contains("columnIndexes"))
    {
        const QList<int> *indexes = response.getVorLocalizerSettings()->getColumnIndexes();

        for (int i = 0; i < VORLocalizerSettings::VORDEMOD_COLUMNS; i++) {
            settings.m_columnIndexes[i] = (*indexes)[i];
        }
    }

    if (featureSettingsKeys.contains("columnSizes"))
    {
        const QList<int> *indexes = response.getVorLocalizerSettings()->getColumnSizes();

        for (int i = 0; i < VORLocalizerSettings::VORDEMOD_COLUMNS; i++) {
            settings.m_columnSizes[i] = (*indexes)[i];
        }
    }
}

void VORLocalizer::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const VORLocalizerSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("VORLocalizer"));
    swgFeatureSettings->setVorLocalizerSettings(new SWGSDRangel::SWGVORLocalizerSettings());
    SWGSDRangel::SWGVORLocalizerSettings *swgVORLocalizerSettings = swgFeatureSettings->getVorLocalizerSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("title") || force) {
        swgVORLocalizerSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgVORLocalizerSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("magDecAdjust") || force) {
        swgVORLocalizerSettings->setMagDecAdjust(settings.m_magDecAdjust);
    }
    if (channelSettingsKeys.contains("rrTime") || force) {
        swgVORLocalizerSettings->setRrTime(settings.m_rrTime);
    }
    if (channelSettingsKeys.contains("forceRRAveraging") || force) {
        swgVORLocalizerSettings->setForceRrAveraging(settings.m_forceRRAveraging ? 1 : 0);
    }
    if (channelSettingsKeys.contains("centerShift") || force) {
        swgVORLocalizerSettings->setCenterShift(settings.m_centerShift);
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

void VORLocalizer::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "VORLocalizer::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("VORLocalizer::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void VORLocalizer::handleChannelMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void VORLocalizer::handleChannelAdded(int deviceSetIndex, ChannelAPI *channel)
{
    qDebug(" VORLocalizer::handleChannelAdded: deviceSetIndex: %d channel: %s", deviceSetIndex, qPrintable(channel->getURI()));
    DeviceSet *deviceSet = MainCore::instance()->getDeviceSets()[deviceSetIndex];
    DSPDeviceSourceEngine *deviceSourceEngine =  deviceSet->m_deviceSourceEngine;

    if (deviceSourceEngine && (channel->getURI() == "sdrangel.channel.vordemod"))
    {
        DeviceSampleSource *deviceSource = deviceSourceEngine->getSource();
        quint64 deviceCenterFrequency = deviceSource->getCenterFrequency();
        int basebandSampleRate = deviceSource->getSampleRate();
        int chi = channel->getIndexInDeviceSet();

        if (!m_availableChannels.contains(channel))
        {
            MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
            ObjectPipe *pipe = messagePipes.registerProducerToConsumer(channel, this, "report");
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
                &VORLocalizer::handleMessagePipeToBeDeleted
            );
        }

        VORLocalizerSettings::AvailableChannel availableChannel =
            VORLocalizerSettings::AvailableChannel{deviceSetIndex, chi, channel, deviceCenterFrequency, basebandSampleRate, -1};
        m_availableChannels[channel] = availableChannel;

        notifyUpdateChannels();
    }
}

void VORLocalizer::notifyUpdateChannels()
{
    if (getMessageQueueToGUI())
    {
        VORLocalizerReport::MsgReportChannels *msgToGUI = VORLocalizerReport::MsgReportChannels::create();
        std::vector<VORLocalizerReport::MsgReportChannels::Channel>& msgChannels = msgToGUI->getChannels();
        QHash<ChannelAPI*, VORLocalizerSettings::AvailableChannel>::iterator it = m_availableChannels.begin();

        for (; it != m_availableChannels.end(); ++it)
        {
            VORLocalizerReport::MsgReportChannels::Channel msgChannel =
                VORLocalizerReport::MsgReportChannels::Channel{
                    it->m_deviceSetIndex,
                    it->m_channelIndex
                };
            msgChannels.push_back(msgChannel);
        }

        getMessageQueueToGUI()->push(msgToGUI);
    }

    if (m_running)
    {
        VorLocalizerWorker::MsgRefreshChannels *msgToWorker = VorLocalizerWorker::MsgRefreshChannels::create();
        m_worker->getInputMessageQueue()->push(msgToWorker);
    }
}
