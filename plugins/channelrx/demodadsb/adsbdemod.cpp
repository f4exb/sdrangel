///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#define BOOST_CHRONO_HEADER_ONLY
#include <boost/chrono/chrono.hpp>

#include <stdio.h>
#include <complex.h>

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGADSBDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGADSBDemodReport.h"
#include "SWGTargetAzimuthElevation.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

#include "adsbdemod.h"
#include "adsbdemodworker.h"

MESSAGE_CLASS_DEFINITION(ADSBDemod::MsgConfigureADSBDemod, Message)
MESSAGE_CLASS_DEFINITION(ADSBDemod::MsgAircraftReport, Message)

const char* const ADSBDemod::m_channelIdURI = "sdrangel.channel.adsbdemod";
const char* const ADSBDemod::m_channelId = "ADSBDemod";

ADSBDemod::ADSBDemod(DeviceAPI *devieAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(devieAPI),
        m_basebandSampleRate(0),
        m_targetAzElValid(false),
        m_targetAzimuth(0.0f),
        m_targetElevation(0.0f)
{
    qDebug("ADSBDemod::ADSBDemod");
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSink = new ADSBDemodBaseband();
    m_basebandSink->moveToThread(m_thread);

    m_worker = new ADSBDemodWorker();
    m_basebandSink->setMessageQueueToWorker(m_worker->getInputMessageQueue());

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &ADSBDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &ADSBDemod::handleIndexInDeviceSetChanged
    );
}

ADSBDemod::~ADSBDemod()
{
    if (m_worker->isRunning()) {
        stop();
    }
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &ADSBDemod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);
    delete m_worker;
    delete m_basebandSink;
    delete m_thread;
}

void ADSBDemod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

uint32_t ADSBDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void ADSBDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void ADSBDemod::start()
{
    qDebug() << "ADSBDemod::start";

    if (m_basebandSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_basebandSampleRate);
    }

    m_worker->reset();
    m_worker->startWork();
    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread->start();

    ADSBDemodWorker::MsgConfigureADSBDemodWorker *msg = ADSBDemodWorker::MsgConfigureADSBDemodWorker::create(m_settings, true);
    m_worker->getInputMessageQueue()->push(msg);
}

void ADSBDemod::stop()
{
    qDebug() << "ADSBDemod::stop";
    m_basebandSink->stopWork();
    m_worker->stopWork();
    m_thread->exit();
    m_thread->wait();
}

bool ADSBDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureADSBDemod::match(cmd))
    {
        MsgConfigureADSBDemod& cfg = (MsgConfigureADSBDemod&) cmd;
        qDebug() << "ADSBDemod::handleMessage: MsgConfigureADSBDemod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "ADSBDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MsgAircraftReport::match(cmd))
    {
        MsgAircraftReport& msg = (MsgAircraftReport&) cmd;
        m_aircraftReport = msg.getReport();
        return true;
    }
    else
    {
        return false;
    }
}

void ADSBDemod::applySettings(const ADSBDemodSettings& settings, bool force)
{
    qDebug() << "ADSBDemod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_correlationThreshold != m_settings.m_correlationThreshold) || force) {
        reverseAPIKeys.append("correlationThreshold");
    }
    if ((settings.m_samplesPerBit != m_settings.m_samplesPerBit) || force) {
        reverseAPIKeys.append("samplesPerBit");
    }
    if ((settings.m_correlateFullPreamble != m_settings.m_correlateFullPreamble) || force) {
        reverseAPIKeys.append("correlateFullPreamble");
    }
    if ((settings.m_demodModeS != m_settings.m_demodModeS) || force) {
        reverseAPIKeys.append("demodModeS");
    }
    if ((settings.m_interpolatorPhaseSteps != m_settings.m_interpolatorPhaseSteps) || force) {
        reverseAPIKeys.append("interpolatorPhaseSteps");
    }
    if ((settings.m_interpolatorTapsPerPhase != m_settings.m_interpolatorTapsPerPhase) || force) {
        reverseAPIKeys.append("interpolatorTapsPerPhase");
    }
    if ((settings.m_removeTimeout != m_settings.m_removeTimeout) || force) {
        reverseAPIKeys.append("removeTimeout");
    }
    if ((settings.m_feedEnabled != m_settings.m_feedEnabled) || force) {
        reverseAPIKeys.append("feedEnabled");
    }
    if ((settings.m_exportClientEnabled != m_settings.m_exportClientEnabled) || force) {
        reverseAPIKeys.append("exportClientEnabled");
    }
    if ((settings.m_exportClientHost != m_settings.m_exportClientHost) || force) {
        reverseAPIKeys.append("exportClientHost");
    }
    if ((settings.m_exportClientPort != m_settings.m_exportClientPort) || force) {
        reverseAPIKeys.append("exportClientPort");
    }
    if ((settings.m_exportClientFormat != m_settings.m_exportClientFormat) || force) {
        reverseAPIKeys.append("exportClientFormat");
    }
     if ((settings.m_exportServerEnabled != m_settings.m_exportServerEnabled) || force) {
        reverseAPIKeys.append("exportServerEnabled");
    }
    if ((settings.m_exportServerPort != m_settings.m_exportServerPort) || force) {
        reverseAPIKeys.append("exportServerPort");
    }
   if ((settings.m_importEnabled != m_settings.m_importEnabled) || force) {
        reverseAPIKeys.append("importEnabled");
    }
    if ((settings.m_importHost != m_settings.m_importHost) || force) {
        reverseAPIKeys.append("importHost");
    }
    if ((settings.m_importUsername != m_settings.m_importUsername) || force) {
        reverseAPIKeys.append("importUsername");
    }
    if ((settings.m_importPassword != m_settings.m_importPassword) || force) {
        reverseAPIKeys.append("importPassword");
    }
    if ((settings.m_importParameters != m_settings.m_importParameters) || force) {
        reverseAPIKeys.append("importParameters");
    }
    if ((settings.m_importPeriod != m_settings.m_importPeriod) || force) {
        reverseAPIKeys.append("importPeriod");
    }
    if ((settings.m_importMinLatitude != m_settings.m_importMinLatitude) || force) {
        reverseAPIKeys.append("importMinLatitude");
    }
    if ((settings.m_importMaxLatitude != m_settings.m_importMaxLatitude) || force) {
        reverseAPIKeys.append("importMaxLatitude");
    }
    if ((settings.m_importMinLongitude != m_settings.m_importMinLongitude) || force) {
        reverseAPIKeys.append("importMinLongitude");
    }
    if ((settings.m_importMaxLongitude != m_settings.m_importMaxLongitude) || force) {
        reverseAPIKeys.append("importMaxLongitude");
    }
    if ((settings.m_logFilename != m_settings.m_logFilename) || force) {
        reverseAPIKeys.append("logFilename");
    }
    if ((settings.m_logEnabled != m_settings.m_logEnabled) || force) {
        reverseAPIKeys.append("logEnabled");
    }
    if ((settings.m_title != m_settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }

    if (m_settings.m_streamIndex != settings.m_streamIndex)
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSinkAPI(this);
            m_deviceAPI->removeChannelSink(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSink(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSinkAPI(this);
        }

        reverseAPIKeys.append("streamIndex");
    }

    ADSBDemodBaseband::MsgConfigureADSBDemodBaseband *msg = ADSBDemodBaseband::MsgConfigureADSBDemodBaseband::create(settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    ADSBDemodWorker::MsgConfigureADSBDemodWorker *workerMsg = ADSBDemodWorker::MsgConfigureADSBDemodWorker::create(settings, force);
    m_worker->getInputMessageQueue()->push(workerMsg);

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

void ADSBDemod::setCenterFrequency(qint64 frequency)
{
    ADSBDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureADSBDemod *msgToGUI = MsgConfigureADSBDemod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

QByteArray ADSBDemod::serialize() const
{
    return m_settings.serialize();
}

bool ADSBDemod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureADSBDemod *msg = MsgConfigureADSBDemod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

int ADSBDemod::webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setAdsbDemodSettings(new SWGSDRangel::SWGADSBDemodSettings());
    response.getAdsbDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int ADSBDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int ADSBDemod::webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage)
{
    (void) errorMessage;
    ADSBDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureADSBDemod *msg = MsgConfigureADSBDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureADSBDemod *msgToGUI = MsgConfigureADSBDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void ADSBDemod::webapiUpdateChannelSettings(
        ADSBDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getAdsbDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getAdsbDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("correlationThreshold")) {
        settings.m_correlationThreshold = response.getAdsbDemodSettings()->getCorrelationThreshold();
    }
    if (channelSettingsKeys.contains("samplesPerBit")) {
        settings.m_samplesPerBit = response.getAdsbDemodSettings()->getSamplesPerBit();
    }
    if (channelSettingsKeys.contains("correlateFullPreamble")) {
        settings.m_correlateFullPreamble = response.getAdsbDemodSettings()->getCorrelateFullPreamble() != 0;
    }
    if (channelSettingsKeys.contains("demodModeS")) {
        settings.m_demodModeS = response.getAdsbDemodSettings()->getDemodModeS() != 0;
    }
    if (channelSettingsKeys.contains("interpolatorPhaseSteps")) {
        settings.m_interpolatorPhaseSteps = response.getAdsbDemodSettings()->getInterpolatorPhaseSteps();
    }
    if (channelSettingsKeys.contains("interpolatorTapsPerPhase")) {
        settings.m_interpolatorTapsPerPhase = response.getAdsbDemodSettings()->getInterpolatorTapsPerPhase();
    }
    if (channelSettingsKeys.contains("removeTimeout")) {
        settings.m_removeTimeout = response.getAdsbDemodSettings()->getRemoveTimeout();
    }
    if (channelSettingsKeys.contains("feedEnabled")) {
        settings.m_feedEnabled = response.getAdsbDemodSettings()->getFeedEnabled() != 0;
    }
    if (channelSettingsKeys.contains("exportClientEnabled")) {
        settings.m_exportClientEnabled = response.getAdsbDemodSettings()->getExportClientEnabled() != 0;
    }
    if (channelSettingsKeys.contains("exportClientHost")) {
        settings.m_exportClientHost = *response.getAdsbDemodSettings()->getExportClientHost();
    }
    if (channelSettingsKeys.contains("exportClientPort")) {
        settings.m_exportClientPort = response.getAdsbDemodSettings()->getExportClientPort();
    }
    if (channelSettingsKeys.contains("exportClientFormat")) {
        settings.m_exportClientFormat = (ADSBDemodSettings::FeedFormat) response.getAdsbDemodSettings()->getExportClientFormat();
    }
    if (channelSettingsKeys.contains("exportServerEnabled")) {
        settings.m_exportServerEnabled = response.getAdsbDemodSettings()->getExportServerEnabled() != 0;
    }
    if (channelSettingsKeys.contains("exportServerPort")) {
        settings.m_exportServerPort = response.getAdsbDemodSettings()->getExportServerPort();
    }
    if (channelSettingsKeys.contains("importEnabled")) {
        settings.m_importEnabled = response.getAdsbDemodSettings()->getImportEnabled() != 0;
    }
    if (channelSettingsKeys.contains("importHost")) {
        settings.m_importHost = *response.getAdsbDemodSettings()->getImportHost();
    }
    if (channelSettingsKeys.contains("importUsername")) {
        settings.m_importUsername = *response.getAdsbDemodSettings()->getImportUsername();
    }
    if (channelSettingsKeys.contains("importPassword")) {
        settings.m_importPassword = *response.getAdsbDemodSettings()->getImportPassword();
    }
    if (channelSettingsKeys.contains("importParameters")) {
        settings.m_importParameters = *response.getAdsbDemodSettings()->getImportParameters();
    }
    if (channelSettingsKeys.contains("importPeriod")) {
        settings.m_importPeriod = response.getAdsbDemodSettings()->getImportPeriod();
    }
    if (channelSettingsKeys.contains("importMinLatitude")) {
        settings.m_importMinLatitude = *response.getAdsbDemodSettings()->getImportMinLatitude();
    }
    if (channelSettingsKeys.contains("importMaxLatitude")) {
        settings.m_importMaxLatitude = *response.getAdsbDemodSettings()->getImportMaxLatitude();
    }
    if (channelSettingsKeys.contains("importMinLongitude")) {
        settings.m_importMinLongitude = *response.getAdsbDemodSettings()->getImportMinLongitude();
    }
    if (channelSettingsKeys.contains("importMaxLongitude")) {
        settings.m_importMaxLongitude = *response.getAdsbDemodSettings()->getImportMaxLongitude();
    }
    if (channelSettingsKeys.contains("logFilename")) {
        settings.m_logFilename = *response.getAdsbDemodSettings()->getLogFilename();
    }
    if (channelSettingsKeys.contains("logEnabled")) {
        settings.m_logEnabled = response.getAdsbDemodSettings()->getLogEnabled();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAdsbDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getAdsbDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getAdsbDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAdsbDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAdsbDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAdsbDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAdsbDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getAdsbDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getAdsbDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getAdsbDemodSettings()->getRollupState());
    }
}

int ADSBDemod::webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setAdsbDemodReport(new SWGSDRangel::SWGADSBDemodReport());
    response.getAdsbDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void ADSBDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const ADSBDemodSettings& settings)
{
    response.getAdsbDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getAdsbDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getAdsbDemodSettings()->setCorrelationThreshold(settings.m_correlationThreshold);
    response.getAdsbDemodSettings()->setSamplesPerBit(settings.m_samplesPerBit);
    response.getAdsbDemodSettings()->setCorrelateFullPreamble(settings.m_correlateFullPreamble ? 1 : 0);
    response.getAdsbDemodSettings()->setDemodModeS(settings.m_demodModeS ? 1 : 0);
    response.getAdsbDemodSettings()->setInterpolatorPhaseSteps(settings.m_interpolatorPhaseSteps);
    response.getAdsbDemodSettings()->setInterpolatorTapsPerPhase(settings.m_interpolatorTapsPerPhase);
    response.getAdsbDemodSettings()->setRemoveTimeout(settings.m_removeTimeout);
    response.getAdsbDemodSettings()->setFeedEnabled(settings.m_feedEnabled ? 1 : 0);
    response.getAdsbDemodSettings()->setExportClientEnabled(settings.m_exportClientEnabled ? 1 : 0);
    response.getAdsbDemodSettings()->setExportClientHost(new QString(settings.m_exportClientHost));
    response.getAdsbDemodSettings()->setExportClientPort(settings.m_exportClientPort);
    response.getAdsbDemodSettings()->setExportClientFormat((int) settings.m_exportClientFormat);
    response.getAdsbDemodSettings()->setExportServerEnabled(settings.m_exportServerEnabled ? 1 : 0);
    response.getAdsbDemodSettings()->setExportServerPort(settings.m_exportServerPort);
    response.getAdsbDemodSettings()->setImportEnabled(settings.m_importEnabled ? 1 : 0);
    response.getAdsbDemodSettings()->setImportHost(new QString(settings.m_importHost));
    response.getAdsbDemodSettings()->setImportUsername(new QString(settings.m_importUsername));
    response.getAdsbDemodSettings()->setImportPassword(new QString(settings.m_importPassword));
    response.getAdsbDemodSettings()->setImportParameters(new QString(settings.m_importParameters));
    response.getAdsbDemodSettings()->setImportPeriod(settings.m_importPeriod);
    response.getAdsbDemodSettings()->setImportMinLatitude(new QString(settings.m_importMinLatitude));
    response.getAdsbDemodSettings()->setImportMaxLatitude(new QString(settings.m_importMaxLatitude));
    response.getAdsbDemodSettings()->setImportMinLongitude(new QString(settings.m_importMinLongitude));
    response.getAdsbDemodSettings()->setImportMaxLongitude(new QString(settings.m_importMaxLongitude));
    response.getAdsbDemodSettings()->setRgbColor(settings.m_rgbColor);
    response.getAdsbDemodSettings()->setLogFilename(new QString(settings.m_logFilename));
    response.getAdsbDemodSettings()->setLogEnabled(settings.m_logEnabled);

    if (response.getAdsbDemodSettings()->getTitle()) {
        *response.getAdsbDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getAdsbDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getAdsbDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getAdsbDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAdsbDemodSettings()->getReverseApiAddress()) {
        *response.getAdsbDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAdsbDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAdsbDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAdsbDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getAdsbDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getAdsbDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getAdsbDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getAdsbDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getAdsbDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getAdsbDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getAdsbDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void ADSBDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getAdsbDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getAdsbDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());

    if (m_targetAzElValid)
    {
        response.getAdsbDemodReport()->setTargetName(new QString(m_targetName));
        response.getAdsbDemodReport()->setTargetAzimuth(m_targetAzimuth);
        response.getAdsbDemodReport()->setTargetElevation(m_targetElevation);
        response.getAdsbDemodReport()->setTargetRange(m_targetRange);
    }

    QList<SWGSDRangel::SWGADSBDemodAircraftState *> *list = response.getAdsbDemodReport()->getAircraftState();
    for (const auto& report : m_aircraftReport)
    {
        SWGSDRangel::SWGADSBDemodAircraftState *aircraftState = new SWGSDRangel::SWGADSBDemodAircraftState();
        //aircraftState->setIcao(new QString(report.m_icao));
        aircraftState->setCallsign(new QString(report.m_callsign));
        aircraftState->setLatitude(report.m_latitude);
        aircraftState->setLongitude(report.m_longitude);
        aircraftState->setAltitude(report.m_altitude);
        aircraftState->setGroundSpeed(report.m_groundSpeed);
        list->append(aircraftState);
    }
}

void ADSBDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const ADSBDemodSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(0); // single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("ADSBDemod"));
    swgChannelSettings->setAdsbDemodSettings(new SWGSDRangel::SWGADSBDemodSettings());
    SWGSDRangel::SWGADSBDemodSettings *swgADSBDemodSettings = swgChannelSettings->getAdsbDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgADSBDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgADSBDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("correlationThreshold") || force) {
        swgADSBDemodSettings->setCorrelationThreshold(settings.m_correlationThreshold);
    }
    if (channelSettingsKeys.contains("samplesPerBit") || force) {
        swgADSBDemodSettings->setSamplesPerBit(settings.m_samplesPerBit);
    }
    if (channelSettingsKeys.contains("correlateFullPreamble") || force) {
        swgADSBDemodSettings->setCorrelateFullPreamble(settings.m_correlateFullPreamble ? 1 : 0);
    }
    if (channelSettingsKeys.contains("demodModeS") || force) {
        swgADSBDemodSettings->setDemodModeS(settings.m_demodModeS ? 1 : 0);
    }
    if (channelSettingsKeys.contains("interpolatorPhaseSteps") || force) {
        swgADSBDemodSettings->setInterpolatorPhaseSteps(settings.m_interpolatorPhaseSteps);
    }
    if (channelSettingsKeys.contains("interpolatorTapsPerPhase") || force) {
        swgADSBDemodSettings->setInterpolatorTapsPerPhase(settings.m_interpolatorTapsPerPhase);
    }
    if (channelSettingsKeys.contains("removeTimeout") || force) {
        swgADSBDemodSettings->setRemoveTimeout(settings.m_removeTimeout);
    }
    if (channelSettingsKeys.contains("feedEnabled") || force) {
        swgADSBDemodSettings->setFeedEnabled(settings.m_feedEnabled ? 1 : 0);
    }
    if (channelSettingsKeys.contains("exportClientEnabled") || force) {
        swgADSBDemodSettings->setExportClientEnabled(settings.m_exportClientEnabled ? 1 : 0);
    }
    if (channelSettingsKeys.contains("exportClientHost") || force) {
        swgADSBDemodSettings->setExportClientHost(new QString(settings.m_exportClientHost));
    }
    if (channelSettingsKeys.contains("exportClientPort") || force) {
        swgADSBDemodSettings->setExportClientPort(settings.m_exportClientPort);
    }
    if (channelSettingsKeys.contains("exportClientFormat") || force) {
        swgADSBDemodSettings->setExportClientFormat((int) settings.m_exportClientFormat);
    }
    if (channelSettingsKeys.contains("exportServerEnabled") || force) {
        swgADSBDemodSettings->setExportServerEnabled(settings.m_exportServerEnabled ? 1 : 0);
    }
    if (channelSettingsKeys.contains("exportServerPort") || force) {
        swgADSBDemodSettings->setExportServerPort(settings.m_exportServerPort);
    }
    if (channelSettingsKeys.contains("importEnabled") || force) {
        swgADSBDemodSettings->setImportEnabled(settings.m_importEnabled ? 1 : 0);
    }
    if (channelSettingsKeys.contains("importHost") || force) {
        swgADSBDemodSettings->setImportHost(new QString(settings.m_importHost));
    }
    if (channelSettingsKeys.contains("importUsername") || force) {
        swgADSBDemodSettings->setImportUsername(new QString(settings.m_importUsername));
    }
    if (channelSettingsKeys.contains("importPassword") || force) {
        swgADSBDemodSettings->setImportPassword(new QString(settings.m_importPassword));
    }
    if (channelSettingsKeys.contains("importParameters") || force) {
        swgADSBDemodSettings->setImportParameters(new QString(settings.m_importParameters));
    }
    if (channelSettingsKeys.contains("importPeriod") || force) {
        swgADSBDemodSettings->setImportPeriod(settings.m_importPeriod);
    }
    if (channelSettingsKeys.contains("importMinLatitude") || force) {
        swgADSBDemodSettings->setImportMinLatitude(new QString(settings.m_importMinLatitude));
    }
    if (channelSettingsKeys.contains("importMaxLatitude") || force) {
        swgADSBDemodSettings->setImportMaxLatitude(new QString(settings.m_importMaxLatitude));
    }
    if (channelSettingsKeys.contains("importMinLongitude") || force) {
        swgADSBDemodSettings->setImportMinLongitude(new QString(settings.m_importMinLongitude));
    }
    if (channelSettingsKeys.contains("importMaxLongitude") || force) {
        swgADSBDemodSettings->setImportMaxLongitude(new QString(settings.m_importMaxLongitude));
    }
    if (channelSettingsKeys.contains("logFilename") || force) {
        swgADSBDemodSettings->setLogFilename(new QString(settings.m_logFilename));
    }
    if (channelSettingsKeys.contains("logEnabled") || force) {
        swgADSBDemodSettings->setLogEnabled(settings.m_logEnabled);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgADSBDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgADSBDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgADSBDemodSettings->setStreamIndex(settings.m_streamIndex);
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgChannelSettings;
}

void ADSBDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "ADSBDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("ADSBDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void ADSBDemod::setTarget(const QString& name, float targetAzimuth, float targetElevation, float targetRange)
{
    m_targetAzimuth = targetAzimuth;
    m_targetElevation = targetElevation;
    m_targetRange = targetRange;
    m_targetName = name;
    m_targetAzElValid = true;

    // Send to Rotator Controllers
    QList<ObjectPipe*> rotatorPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "target", rotatorPipes);

    for (const auto& pipe : rotatorPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGTargetAzimuthElevation *swgTarget = new SWGSDRangel::SWGTargetAzimuthElevation();
        swgTarget->setName(new QString(name));
        swgTarget->setAzimuth(targetAzimuth);
        swgTarget->setElevation(targetElevation);
        messageQueue->push(MainCore::MsgTargetAzimuthElevation::create(this, swgTarget));
    }
}

void ADSBDemod::handleIndexInDeviceSetChanged(int index)
{
    if (index < 0) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
}
