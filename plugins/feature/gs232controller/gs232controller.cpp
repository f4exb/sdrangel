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

#include <cmath>

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QSerialPortInfo>

#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"
#include "SWGDeviceState.h"
#include "SWGTargetAzimuthElevation.h"

#include "dsp/dspengine.h"
#include "device/deviceset.h"
#include "feature/featureset.h"
#include "settings/serializable.h"
#include "maincore.h"

#include "gs232controller.h"
#include "gs232controllerworker.h"
#include "gs232controllerreport.h"

MESSAGE_CLASS_DEFINITION(GS232Controller::MsgConfigureGS232Controller, Message)
MESSAGE_CLASS_DEFINITION(GS232Controller::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(GS232Controller::MsgReportWorker, Message)
MESSAGE_CLASS_DEFINITION(GS232Controller::MsgReportAvailableChannelOrFeatures, Message)
MESSAGE_CLASS_DEFINITION(GS232Controller::MsgScanAvailableChannelOrFeatures, Message)

const char* const GS232Controller::m_featureIdURI = "sdrangel.feature.gs232controller";
const char* const GS232Controller::m_featureId = "GS232Controller";

GS232Controller::GS232Controller(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_thread(nullptr),
    m_worker(nullptr)
{
    qDebug("GS232Controller::GS232Controller: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "GS232Controller error";
    m_selectedPipe = nullptr;
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &GS232Controller::networkManagerFinished
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::featureAdded,
        this,
        &GS232Controller::handleFeatureAdded
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::channelAdded,
        this,
        &GS232Controller::handleChannelAdded
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::featureRemoved,
        this,
        &GS232Controller::handleFeatureRemoved
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::channelRemoved,
        this,
        &GS232Controller::handleChannelRemoved
    );
}

GS232Controller::~GS232Controller()
{
    QObject::disconnect(
        MainCore::instance(),
        &MainCore::channelRemoved,
        this,
        &GS232Controller::handleChannelRemoved
    );
    QObject::disconnect(
        MainCore::instance(),
        &MainCore::featureRemoved,
        this,
        &GS232Controller::handleFeatureRemoved
    );
    QObject::disconnect(
        MainCore::instance(),
        &MainCore::channelAdded,
        this,
        &GS232Controller::handleChannelAdded
    );
    QObject::disconnect(
        MainCore::instance(),
        &MainCore::featureAdded,
        this,
        &GS232Controller::handleFeatureAdded
    );
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &GS232Controller::networkManagerFinished
    );
    delete m_networkManager;
    stop();
}

void GS232Controller::start()
{
    qDebug("GS232Controller::start");

    m_thread = new QThread();
    m_worker = new GS232ControllerWorker();
    m_worker->moveToThread(m_thread);
    QObject::connect(m_thread, &QThread::started, m_worker, &GS232ControllerWorker::startWork);
    QObject::connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    QObject::connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);
    m_worker->setMessageQueueToFeature(getInputMessageQueue());
    m_thread->start();
    m_state = StRunning;

    GS232ControllerWorker::MsgConfigureGS232ControllerWorker *msg = GS232ControllerWorker::MsgConfigureGS232ControllerWorker::create(m_settings, true);
    m_worker->getInputMessageQueue()->push(msg);

    scanAvailableChannelsAndFeatures();
}

void GS232Controller::stop()
{
    qDebug("GS232Controller::stop");
    m_state = StIdle;
    if (m_thread)
    {
        m_thread->quit();
        m_thread->wait();
        m_thread = nullptr;
        m_worker = nullptr;
    }
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
    else if (MsgScanAvailableChannelOrFeatures::match(cmd))
    {
        scanAvailableChannelsAndFeatures();
        return true;
    }
    else if (GS232ControllerReport::MsgReportAzAl::match(cmd))
    {
        GS232ControllerReport::MsgReportAzAl& report = (GS232ControllerReport::MsgReportAzAl&) cmd;
        // Save state for Web report/getOnTarget
        m_currentAzimuth = report.getAzimuth();
        m_currentElevation = report.getElevation();
        // Forward to GUI
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new GS232ControllerReport::MsgReportAzAl(report));
        }
        return true;
    }
    else if (MainCore::MsgTargetAzimuthElevation::match(cmd))
    {
        // New source from another plugin
        if ((m_state == StRunning) && m_settings.m_track)
        {
            MainCore::MsgTargetAzimuthElevation& msg = (MainCore::MsgTargetAzimuthElevation&) cmd;
            // Is it from the selected pipe?
            if (msg.getPipeSource() == m_selectedPipe)
            {
                if (getMessageQueueToGUI())
                {
                    // Forward to GUI - which will then send us updated settings
                    getMessageQueueToGUI()->push(new MainCore::MsgTargetAzimuthElevation(msg));
                }
                else
                {
                    // No GUI, so save source - applySettings will propagate to worker
                    SWGSDRangel::SWGTargetAzimuthElevation *swgTarget = msg.getSWGTargetAzimuthElevation();
                    m_settings.m_azimuth = swgTarget->getAzimuth();
                    m_settings.m_elevation = swgTarget->getElevation();
                    applySettings(m_settings);
                }
            }
            else
                qDebug() << "GS232Controller::handleMessage: No match " << msg.getPipeSource() << " " << m_selectedPipe;
        }
        return true;
    }
    else
    {
        return false;
    }
}

// Calculate whether last received az/el was on target
bool GS232Controller::getOnTarget() const
{
    float targetAziumth, targetElevation;
    m_settings.calcTargetAzEl(targetAziumth, targetElevation);
    float readTolerance = m_settings.m_tolerance + 0.0f;
    bool onTarget =   (std::fabs(m_currentAzimuth - targetAziumth) < readTolerance)
                   && (std::fabs(m_currentElevation - targetElevation) < readTolerance);
    return onTarget;
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
            << " m_azimuthOffset: " << settings.m_azimuthOffset
            << " m_elevationOffset: " << settings.m_elevationOffset
            << " m_azimuthMin: " << settings.m_azimuthMin
            << " m_azimuthMax: " << settings.m_azimuthMax
            << " m_elevationMin: " << settings.m_elevationMin
            << " m_elevationMax: " << settings.m_elevationMax
            << " m_tolerance: " << settings.m_tolerance
            << " m_protocol: " << settings.m_protocol
            << " m_serialPort: " << settings.m_serialPort
            << " m_baudRate: " << settings.m_baudRate
            << " m_host: " << settings.m_host
            << " m_port: " << settings.m_port
            << " m_track: " << settings.m_track
            << " m_source: " << settings.m_source
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
    if ((m_settings.m_host != settings.m_host) || force) {
        reverseAPIKeys.append("host");
    }
    if ((m_settings.m_port != settings.m_port) || force) {
        reverseAPIKeys.append("port");
    }
    if ((m_settings.m_track != settings.m_track) || force) {
        reverseAPIKeys.append("track");
    }
    if ((m_settings.m_source != settings.m_source)
        || (!settings.m_source.isEmpty() && (m_selectedPipe == nullptr)) // Change in available pipes
        || force)
    {
        MainCore *mainCore = MainCore::instance();
        MessagePipes& messagePipes = mainCore->getMessagePipes();

        if (m_selectedPipe)
        {
            qDebug("GS232Controller::applySettings: unregister %s (%p)", qPrintable(m_selectedPipe->objectName()), m_selectedPipe);
            messagePipes.unregisterProducerToConsumer(m_selectedPipe, this, "target");
        }

        if (!settings.m_source.isEmpty())
        {
            QObject *object = nullptr;

            for (const auto& oval : m_availableChannelOrFeatures)
            {
                QString itemText = tr("%1%2:%3 %4")
                    .arg(oval.m_kind)
                    .arg(oval.m_superIndex)
                    .arg(oval.m_index)
                    .arg(oval.m_type);

                if (settings.m_source == itemText)
                {
                    object = m_availableChannelOrFeatures.key(oval);
                    break;
                }
            }

            if (object)
            {
                registerPipe(object);
                m_selectedPipe = object;
            }
            else
            {
                m_selectedPipe = nullptr;
                qDebug() << "GS232Controller::applySettings: No plugin corresponding to source " << settings.m_source;
            }
        }
        else
        {
            m_selectedPipe = nullptr;
        }

        reverseAPIKeys.append("source");
    }
    if ((m_settings.m_azimuthOffset != settings.m_azimuthOffset) || force) {
        reverseAPIKeys.append("azimuthOffset");
    }
    if ((m_settings.m_elevationOffset != settings.m_elevationOffset) || force) {
        reverseAPIKeys.append("elevationOffset");
    }
    if ((m_settings.m_azimuthMin != settings.m_azimuthMin) || force) {
        reverseAPIKeys.append("azimuthMin");
    }
    if ((m_settings.m_azimuthMax != settings.m_azimuthMax) || force) {
        reverseAPIKeys.append("azimuthMax");
    }
    if ((m_settings.m_elevationMin != settings.m_elevationMin) || force) {
        reverseAPIKeys.append("elevationMin");
    }
    if ((m_settings.m_tolerance != settings.m_tolerance) || force) {
        reverseAPIKeys.append("tolerance");
    }
    if ((m_settings.m_protocol != settings.m_protocol) || force) {
        reverseAPIKeys.append("m_protocol");
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
    if (m_worker) {
        m_worker->getInputMessageQueue()->push(msg);
    }

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

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureGS232Controller *msgToGUI = MsgConfigureGS232Controller::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

int GS232Controller::webapiReportGet(
    SWGSDRangel::SWGFeatureReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setGs232ControllerReport(new SWGSDRangel::SWGGS232ControllerReport());
    response.getGs232ControllerReport()->init();
    webapiFormatFeatureReport(response);
    return 200;
}

int GS232Controller::webapiActionsPost(
    const QStringList& featureActionsKeys,
    SWGSDRangel::SWGFeatureActions& query,
    QString& errorMessage)
{
    SWGSDRangel::SWGGS232ControllerActions *swgGS232ControllerActions = query.getGs232ControllerActions();

    if (swgGS232ControllerActions)
    {
        if (featureActionsKeys.contains("run"))
        {
            bool featureRun = swgGS232ControllerActions->getRun() != 0;
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
        errorMessage = "Missing GS232ControllerActions in query";
        return 400;
    }
}

void GS232Controller::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const GS232ControllerSettings& settings)
{
    response.getGs232ControllerSettings()->setAzimuth(settings.m_azimuth);
    response.getGs232ControllerSettings()->setElevation(settings.m_elevation);
    response.getGs232ControllerSettings()->setSerialPort(new QString(settings.m_serialPort));
    response.getGs232ControllerSettings()->setBaudRate(settings.m_baudRate);
    response.getGs232ControllerSettings()->setHost(new QString(settings.m_host));
    response.getGs232ControllerSettings()->setPort(settings.m_port);
    response.getGs232ControllerSettings()->setTrack(settings.m_track);
    response.getGs232ControllerSettings()->setSource(new QString(settings.m_source));
    response.getGs232ControllerSettings()->setAzimuthOffset(settings.m_azimuthOffset);
    response.getGs232ControllerSettings()->setElevationOffset(settings.m_elevationOffset);
    response.getGs232ControllerSettings()->setAzimuthMin(settings.m_azimuthMin);
    response.getGs232ControllerSettings()->setAzimuthMax(settings.m_azimuthMax);
    response.getGs232ControllerSettings()->setElevationMin(settings.m_elevationMin);
    response.getGs232ControllerSettings()->setElevationMax(settings.m_elevationMax);
    response.getGs232ControllerSettings()->setTolerance(settings.m_tolerance);
    response.getGs232ControllerSettings()->setProtocol(settings.m_protocol);

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
    response.getGs232ControllerSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getGs232ControllerSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getGs232ControllerSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getGs232ControllerSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getGs232ControllerSettings()->setRollupState(swgRollupState);
        }
    }
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
        settings.m_baudRate = response.getGs232ControllerSettings()->getBaudRate();
    }
    if (featureSettingsKeys.contains("host")) {
        settings.m_host = *response.getGs232ControllerSettings()->getHost();
    }
    if (featureSettingsKeys.contains("port")) {
        settings.m_port = response.getGs232ControllerSettings()->getPort();
    }
    if (featureSettingsKeys.contains("track")) {
        settings.m_track = response.getGs232ControllerSettings()->getTrack() != 0;
    }
    if (featureSettingsKeys.contains("source")) {
        settings.m_source = *response.getGs232ControllerSettings()->getSource();
    }
    if (featureSettingsKeys.contains("azimuthOffset")) {
        settings.m_azimuthOffset = response.getGs232ControllerSettings()->getAzimuthOffset();
    }
    if (featureSettingsKeys.contains("elevationOffset")) {
        settings.m_elevationOffset = response.getGs232ControllerSettings()->getElevationOffset();
    }
    if (featureSettingsKeys.contains("azimuthMin")) {
        settings.m_azimuthMin = response.getGs232ControllerSettings()->getAzimuthMin();
    }
    if (featureSettingsKeys.contains("azimuthMax")) {
        settings.m_azimuthMax = response.getGs232ControllerSettings()->getAzimuthMax();
    }
    if (featureSettingsKeys.contains("elevationMin")) {
        settings.m_elevationMin = response.getGs232ControllerSettings()->getElevationMin();
    }
    if (featureSettingsKeys.contains("elevationMax")) {
        settings.m_elevationMax = response.getGs232ControllerSettings()->getElevationMax();
    }
    if (featureSettingsKeys.contains("tolerance")) {
        settings.m_tolerance = response.getGs232ControllerSettings()->getTolerance();
    }
    if (featureSettingsKeys.contains("protocol")) {
        settings.m_protocol = (GS232ControllerSettings::Protocol)response.getGs232ControllerSettings()->getProtocol();
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
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getGs232ControllerSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getGs232ControllerSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getGs232ControllerSettings()->getRollupState());
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
    if (featureSettingsKeys.contains("host") || force) {
        swgGS232ControllerSettings->setHost(new QString(settings.m_host));
    }
    if (featureSettingsKeys.contains("port") || force) {
        swgGS232ControllerSettings->setPort(settings.m_port);
    }
    if (featureSettingsKeys.contains("track") || force) {
        swgGS232ControllerSettings->setTrack(settings.m_track);
    }
    if (featureSettingsKeys.contains("source") || force) {
        swgGS232ControllerSettings->setSource(new QString(settings.m_source));
    }
    if (featureSettingsKeys.contains("azimuthOffset") || force) {
        swgGS232ControllerSettings->setAzimuthOffset(settings.m_azimuthOffset);
    }
    if (featureSettingsKeys.contains("elevationOffset") || force) {
        swgGS232ControllerSettings->setElevationOffset(settings.m_elevationOffset);
    }
    if (featureSettingsKeys.contains("azimuthMin") || force) {
        swgGS232ControllerSettings->setAzimuthMin(settings.m_azimuthMin);
    }
    if (featureSettingsKeys.contains("azimuthMax") || force) {
        swgGS232ControllerSettings->setAzimuthMax(settings.m_azimuthMax);
    }
    if (featureSettingsKeys.contains("elevationMin") || force) {
        swgGS232ControllerSettings->setElevationMin(settings.m_elevationMin);
    }
    if (featureSettingsKeys.contains("elevationMax") || force) {
        swgGS232ControllerSettings->setElevationMax(settings.m_elevationMax);
    }
    if (featureSettingsKeys.contains("tolerance") || force) {
        swgGS232ControllerSettings->setTolerance(settings.m_tolerance);
    }
    if (featureSettingsKeys.contains("protocol") || force) {
        swgGS232ControllerSettings->setProtocol((int)settings.m_protocol);
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

void GS232Controller::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response)
{
    response.getGs232ControllerReport()->setSources(new QList<QString*>());

    for (const auto& item : m_availableChannelOrFeatures)
    {
        QString itemText = tr("%1%2:%3 %4")
            .arg(item.m_kind)
            .arg(item.m_superIndex)
            .arg(item.m_index)
            .arg(item.m_type);

        response.getGs232ControllerReport()->getSources()->append(new QString(itemText));
    }

    QList<QSerialPortInfo> serialPorts = QSerialPortInfo::availablePorts();
    QListIterator<QSerialPortInfo> i(serialPorts);
    response.getGs232ControllerReport()->setSerialPorts(new QList<QString*>());

    while (i.hasNext())
    {
        QSerialPortInfo info = i.next();
        response.getGs232ControllerReport()->getSerialPorts()->append(new QString(info.portName()));
    }

    float azimuth, elevation;
    m_settings.calcTargetAzEl(azimuth, elevation);
    response.getGs232ControllerReport()->setTargetAzimuth(azimuth);
    response.getGs232ControllerReport()->setTargetElevation(elevation);
    response.getGs232ControllerReport()->setCurrentAzimuth(m_currentAzimuth);
    response.getGs232ControllerReport()->setCurrentElevation(m_currentElevation);
    response.getGs232ControllerReport()->setOnTarget(getOnTarget());
    response.getGs232ControllerReport()->setRunningState(getState());
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

void GS232Controller::scanAvailableChannelsAndFeatures()
{
    qDebug("GS232Controller::scanAvailableChannelsAndFeatures");
    MainCore *mainCore = MainCore::instance();
    std::vector<FeatureSet*>& featureSets = mainCore->getFeatureeSets();
    m_availableChannelOrFeatures.clear();

    for (const auto& featureSet : featureSets)
    {
        for (int fei = 0; fei < featureSet->getNumberOfFeatures(); fei++)
        {
            Feature *feature = featureSet->getFeatureAt(fei);

            if (GS232ControllerSettings::m_pipeURIs.contains(feature->getURI()) && !m_availableChannelOrFeatures.contains(feature))
            {
                qDebug("GS232Controller::scanAvailableChannelsAndFeatures: store feature %d:%d %s (%p)",
                    featureSet->getIndex(), fei, qPrintable(feature->getURI()), feature);
                GS232ControllerSettings::AvailableChannelOrFeature availableItem =
                    GS232ControllerSettings::AvailableChannelOrFeature{"F", featureSet->getIndex(), fei, feature->getIdentifier()};
                m_availableChannelOrFeatures[feature] = availableItem;
            }
        }
    }

    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();

    for (const auto& deviceSet : deviceSets)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  deviceSet->m_deviceSourceEngine;

        if (deviceSourceEngine)
        {
            for (int chi = 0; chi < deviceSet->getNumberOfChannels(); chi++)
            {
                ChannelAPI *channel = deviceSet->getChannelAt(chi);

                if (GS232ControllerSettings::m_pipeURIs.contains(channel->getURI()) && !m_availableChannelOrFeatures.contains(channel))
                {
                    qDebug("GS232Controller::scanAvailableChannelsAndFeatures: store channel %d:%d %s (%p)",
                        deviceSet->getIndex(), chi, qPrintable(channel->getURI()), channel);
                    GS232ControllerSettings::AvailableChannelOrFeature availableItem =
                        GS232ControllerSettings::AvailableChannelOrFeature{"R", deviceSet->getIndex(), chi, channel->getIdentifier()};
                    m_availableChannelOrFeatures[channel] = availableItem;
                }
            }
        }
    }

    notifyUpdate();
}

void GS232Controller::notifyUpdate()
{
    if (getMessageQueueToGUI())
    {
        MsgReportAvailableChannelOrFeatures *msg = MsgReportAvailableChannelOrFeatures::create();
        msg->getItems() = m_availableChannelOrFeatures.values();
        getMessageQueueToGUI()->push(msg);
    }
}

void GS232Controller::handleFeatureAdded(int featureSetIndex, Feature *feature)
{
    qDebug("GS232Controller::handleFeatureAdded: featureSetIndex: %d:%d feature: %s (%p)",
        featureSetIndex, feature->getIndexInFeatureSet(), qPrintable(feature->getURI()), feature);
    FeatureSet *featureSet = MainCore::instance()->getFeatureeSets()[featureSetIndex];

    if (GS232ControllerSettings::m_pipeURIs.contains(feature->getURI()))
    {
        GS232ControllerSettings::AvailableChannelOrFeature availableItem =
            GS232ControllerSettings::AvailableChannelOrFeature{
                "F",
                featureSet->getIndex(),
                feature->getIndexInFeatureSet(),
                feature->getIdentifier()
            };
        m_availableChannelOrFeatures[feature] = availableItem;

        notifyUpdate();
    }
}

void GS232Controller::handleFeatureRemoved(int featureSetIndex, Feature *feature)
{
    qDebug("GS232Controller::handleFeatureRemoved: featureSetIndex: %d (%p)", featureSetIndex, feature);

    if (m_availableChannelOrFeatures.contains(feature))
    {
        m_availableChannelOrFeatures.remove(feature);
        notifyUpdate();
    }
}

void GS232Controller::handleChannelAdded(int deviceSetIndex, ChannelAPI *channel)
{
    qDebug("GS232Controller::handleChannelAdded: deviceSetIndex: %d:%d channel: %s (%p)",
        deviceSetIndex, channel->getIndexInDeviceSet(), qPrintable(channel->getURI()), channel);
    DeviceSet *deviceSet = MainCore::instance()->getDeviceSets()[deviceSetIndex];
    DSPDeviceSourceEngine *deviceSourceEngine =  deviceSet->m_deviceSourceEngine;

    if (deviceSourceEngine && GS232ControllerSettings::m_pipeURIs.contains(channel->getURI()))
    {
        GS232ControllerSettings::AvailableChannelOrFeature availableItem =
            GS232ControllerSettings::AvailableChannelOrFeature{
                "R",
                deviceSet->getIndex(),
                channel->getIndexInDeviceSet(),
                channel->getIdentifier()
            };
        m_availableChannelOrFeatures[channel] = availableItem;

        notifyUpdate();
    }
}

void GS232Controller::handleChannelRemoved(int deviceSetIndex, ChannelAPI *channel)
{
    qDebug("GS232Controller::handleChannelRemoved: deviceSetIndex: %d (%p)", deviceSetIndex, channel);

    if (m_availableChannelOrFeatures.contains(channel))
    {
        m_availableChannelOrFeatures.remove(channel);
        notifyUpdate();
    }
}

void GS232Controller::registerPipe(QObject *object)
{
    qDebug("GS232Controller::registerPipe: register %s (%p)", qPrintable(object->objectName()), object);
    MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
    ObjectPipe *pipe = messagePipes.registerProducerToConsumer(object, this, "target");
    MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
    QObject::connect(
        messageQueue,
        &MessageQueue::messageEnqueued,
        this,
        [=](){ this->handlePipeMessageQueue(messageQueue); },
        Qt::QueuedConnection
    );
    QObject::connect(
        pipe,
        &ObjectPipe::toBeDeleted,
        this,
        &GS232Controller::handleMessagePipeToBeDeleted
    );
}

void GS232Controller::handleMessagePipeToBeDeleted(int reason, QObject* object)
{
    if ((reason == 0) && m_availableChannelOrFeatures.contains(object)) // producer
    {
        qDebug("GS232Controller::handleMessagePipeToBeDeleted: removing channel or feature at (%p)", object);
        m_availableChannelOrFeatures.remove(object);
        notifyUpdate();
    }
}

void GS232Controller::handlePipeMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}
