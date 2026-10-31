///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include "acarsdemod.h"

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include <stdio.h>
#include <complex.h>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGACARSDemodSettings.h"
#include "SWGACARSDemodReport.h"
//#include "SWGACARSDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGMapItem.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

#include "acarsdemodworker.h"

MESSAGE_CLASS_DEFINITION(AcarsDemod::MsgConfigureAcarsDemod, Message)
MESSAGE_CLASS_DEFINITION(AcarsDemod::MsgVdl2Frame, Message)
MESSAGE_CLASS_DEFINITION(AcarsDemod::MsgAcarsPacket, MainCore::MsgPacket)
MESSAGE_CLASS_DEFINITION(AcarsDemod::MsgHfdlFrame, Message)
MESSAGE_CLASS_DEFINITION(AcarsDemod::MsgAeroFrame, Message)
MESSAGE_CLASS_DEFINITION(AcarsDemod::MsgProtocolChange, Message)

const char * const AcarsDemod::m_channelIdURI = "sdrangel.channel.acarsdemod";
const char * const AcarsDemod::m_channelId = "ACARSDemod";

AcarsDemod::AcarsDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new AcarsDemodBaseband(this);
    m_basebandSink->setFifoLabel(QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(getIndexInDeviceSet())
    );
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);

    // Decoded frames are processed by the worker on its own thread, for the
    // channel's lifetime, in both GUI and server builds
    m_workerThread = new QThread();
    m_worker = new AcarsDemodWorker(this);
    m_worker->moveToThread(m_workerThread);
    QObject::connect(m_workerThread, &QThread::started, m_worker, &AcarsDemodWorker::startWork);
    QObject::connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    QObject::connect(m_workerThread, &QThread::finished, m_workerThread, &QThread::deleteLater);
    m_workerThread->start();

    // The sink sends its frames straight to the worker
    m_basebandSink->setMessageQueueToChannel(m_worker->getInputMessageQueue());

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AcarsDemod::networkManagerFinished
    );
}

AcarsDemod::~AcarsDemod()
{
    qDebug("AcarsDemod::~AcarsDemod");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AcarsDemod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    // The sink has stopped, so nothing new reaches the worker - but frames it already
    // queued are still being processed, and those read the sink (getMagSq() for the feed
    // level). So the worker thread must finish, which also deletes the worker, before the
    // sink is deleted
    m_workerThread->quit();
    m_workerThread->wait();
    m_worker = nullptr;
    m_workerThread = nullptr;

    delete m_basebandSink;
}

void AcarsDemod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this, false);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

uint32_t AcarsDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void AcarsDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void AcarsDemod::start()
{
    qDebug("AcarsDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    AcarsDemodBaseband::MsgConfigureAcarsDemodBaseband *msg = AcarsDemodBaseband::MsgConfigureAcarsDemodBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void AcarsDemod::stop()
{
    qDebug("AcarsDemod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool AcarsDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureAcarsDemod::match(cmd))
    {
        MsgConfigureAcarsDemod& cfg = (MsgConfigureAcarsDemod&) cmd;
        qDebug() << "AcarsDemod::handleMessage: MsgConfigureAcarsDemod";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "AcarsDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to the worker, which needs the centre frequency for its outputs
        if (m_worker) {
            m_worker->getInputMessageQueue()->push(new DSPSignalNotification(notif));
        }
        // Forward to GUI if any
        if (m_guiMessageQueue)
        {
            rep = new DSPSignalNotification(notif);
            m_guiMessageQueue->push(rep);
        }

        return true;
    }
    else
    {
        return false;
    }
}

void AcarsDemod::setCenterFrequency(qint64 frequency)
{
    AcarsDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAcarsDemod *msgToGUI = MsgConfigureAcarsDemod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void AcarsDemod::applySettings(const AcarsDemodSettings& settings, bool force)
{
    qDebug() << "AcarsDemod::applySettings:"
            << " m_logEnabled: " << settings.m_logEnabled
            << " m_logFilename: " << settings.m_logFilename
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
    if ((settings.m_udpEnabled != m_settings.m_udpEnabled) || force) {
        reverseAPIKeys.append("udpEnabled");
    }
    if ((settings.m_udpAddress != m_settings.m_udpAddress) || force) {
        reverseAPIKeys.append("udpAddress");
    }
    if ((settings.m_udpPort != m_settings.m_udpPort) || force) {
        reverseAPIKeys.append("udpPort");
    }
    if ((settings.m_logFilename != m_settings.m_logFilename) || force) {
        reverseAPIKeys.append("logFilename");
    }
    if ((settings.m_logEnabled != m_settings.m_logEnabled) || force) {
        reverseAPIKeys.append("logEnabled");
    }
    if ((settings.m_mode != m_settings.m_mode) || force) {
        reverseAPIKeys.append("mode");
    }
    if ((settings.m_aeroChannel != m_settings.m_aeroChannel) || force) {
        reverseAPIKeys.append("aeroChannel");
    }
    if ((settings.m_iridiumBandwidth != m_settings.m_iridiumBandwidth) || force) {
        reverseAPIKeys.append("iridiumBandwidth");
    }
    if ((settings.m_correlationThreshold != m_settings.m_correlationThreshold) || force) {
        reverseAPIKeys.append("correlationThreshold");
    }
    if ((settings.m_filter != m_settings.m_filter) || force) {
        reverseAPIKeys.append("filter");
    }
    if ((settings.m_filterColumn != m_settings.m_filterColumn) || force) {
        reverseAPIKeys.append("filterColumn");
    }
    if ((settings.m_hideNoInfo != m_settings.m_hideNoInfo) || force) {
        reverseAPIKeys.append("hideNoInfo");
    }
    if ((settings.m_showDate != m_settings.m_showDate) || force) {
        reverseAPIKeys.append("showDate");
    }
    if ((settings.m_displayPhotos != m_settings.m_displayPhotos) || force) {
        reverseAPIKeys.append("displayPhotos");
    }
    if ((settings.m_displayChart != m_settings.m_displayChart) || force) {
        reverseAPIKeys.append("displayChart");
    }
    if ((settings.m_scopeCh1 != m_settings.m_scopeCh1) || force) {
        reverseAPIKeys.append("scopeCh1");
    }
    if ((settings.m_scopeCh2 != m_settings.m_scopeCh2) || force) {
        reverseAPIKeys.append("scopeCh2");
    }
    if ((settings.m_rgbColor != m_settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((settings.m_title != m_settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((settings.m_workspaceIndex != m_settings.m_workspaceIndex) || force) {
        reverseAPIKeys.append("workspaceIndex");
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

    AcarsDemodBaseband::MsgConfigureAcarsDemodBaseband *msg = AcarsDemodBaseband::MsgConfigureAcarsDemodBaseband::create(settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    // The worker handles the outputs (UDP, log, pipes) and, in later phases,
    // the higher layer decode
    if (m_worker) {
        m_worker->getInputMessageQueue()->push(AcarsDemodWorker::MsgConfigureWorker::create(settings, force));
    }

    m_settings = settings;
}

QByteArray AcarsDemod::serialize() const
{
    return m_settings.serialize();
}

bool AcarsDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureAcarsDemod *msg = MsgConfigureAcarsDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureAcarsDemod *msg = MsgConfigureAcarsDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

// NOTE: the API carries these settings in three directions - reading a PATCH,
// writing a GET, and sending an update to a reverse API - and each one now lists
// them out in full below. A setting added to AcarsDemodSettings has to be added to
// ALL THREE: webapiUpdateChannelSettings, webapiFormatChannelSettings and
// webapiFormatChannelSettings(keys, ...). Missing one leaves the setting silently
// unreadable, unreportable or unsynchronised rather than failing to build.

// The strings need their own handling: a generated setter takes ownership of what it is
// given, and the default constructor leaves every pointer null - so assigning in place
// without checking writes through nullptr
static void acarsApiSetString(SWGSDRangel::SWGACARSDemodSettings *swg, QString *held,
                              void (SWGSDRangel::SWGACARSDemodSettings::*setter)(QString *),
                              const QString& value)
{
    if (held) {
        *held = value;
    } else {
        (swg->*setter)(new QString(value));
    }
}

int AcarsDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAcarsDemodSettings(new SWGSDRangel::SWGACARSDemodSettings());
    response.getAcarsDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int AcarsDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    AcarsDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureAcarsDemod *msg = MsgConfigureAcarsDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue)      // forward to GUI if any
    {
        MsgConfigureAcarsDemod *msgToGUI = MsgConfigureAcarsDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);
    return 200;
}

int AcarsDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAcarsDemodReport(new SWGSDRangel::SWGACARSDemodReport());
    response.getAcarsDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void AcarsDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    SWGSDRangel::SWGACARSDemodReport *swg = response.getAcarsDemodReport();

    const double magsqAvg = getMagSq();
    swg->setChannelPowerDb(magsqAvg > 0.0 ? 10.0 * log10(magsqAvg) : -200.0);
    // Depends on the protocol, and in Aero on the rate tuned - see channelSampleRate()
    swg->setChannelSampleRate(AcarsDemodSink::channelSampleRate(m_settings));

    // Both are only meaningful in Aero mode, which is the only one with a continuous
    // carrier to average over. The other protocols leave them at zero.
    double evm = 0.0;
    double suRate = 0.0;
    getAeroQuality(evm, suRate);
    swg->setAeroEvm((float) evm);
    swg->setAeroSignalUnitRate((float) suRate);
}

void AcarsDemod::webapiUpdateChannelSettings(
        AcarsDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    SWGSDRangel::SWGACARSDemodSettings *swg = response.getAcarsDemodSettings();
    if (!swg) {
        return;
    }

    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = (qint32) swg->getInputFrequencyOffset();
    }
    // The ranges are the ones AcarsDemodSettings::deserialize() enforces, as the values go
    // to the same places - mode and aeroChannel index tables, rfBandwidth becomes a filter
    // cutoff. Deserialize falls back to the default; here a value out of range is ignored
    // and the current setting kept
    if (channelSettingsKeys.contains("mode"))
    {
        const int mode = swg->getMode();
        if ((mode >= 0) && (mode < (int) AcarsDemodSettings::ModeCount)) {
            settings.m_mode = mode;
        }
    }
    if (channelSettingsKeys.contains("aeroChannel"))
    {
        const int aeroChannel = swg->getAeroChannel();
        if ((aeroChannel >= 0) && (aeroChannel < AcarsAeroReceiver::SubmodeCount)) {
            settings.m_aeroChannel = aeroChannel;
        }
    }
    if (channelSettingsKeys.contains("iridiumBandwidth"))
    {
        const int iridiumBandwidth = swg->getIridiumBandwidth();
        if ((iridiumBandwidth >= 0) && (iridiumBandwidth < AcarsDemodSettings::IridiumScanWidths)) {
            settings.m_iridiumBandwidth = iridiumBandwidth;
        }
    }
    if (channelSettingsKeys.contains("rfBandwidth"))
    {
        const Real rfBandwidth = (Real) swg->getRfBandwidth();
        if ((rfBandwidth >= 1000.0f) && (rfBandwidth <= 40000.0f)) {
            settings.m_rfBandwidth = rfBandwidth;
        }
    }
    if (channelSettingsKeys.contains("correlationThreshold"))
    {
        const Real correlationThreshold = (Real) swg->getCorrelationThreshold();
        if ((correlationThreshold >= 0.0f) && (correlationThreshold <= 1.0f)) {
            settings.m_correlationThreshold = correlationThreshold;
        }
    }
    if (channelSettingsKeys.contains("filterColumn"))
    {
        const int filterColumn = swg->getFilterColumn();
        if ((filterColumn >= 0) && (filterColumn <= 3)) {
            settings.m_filterColumn = filterColumn;
        }
    }
    if (channelSettingsKeys.contains("hideNoInfo")) {
        settings.m_hideNoInfo = (bool) swg->getHideNoInfo();
    }
    if (channelSettingsKeys.contains("showDate")) {
        settings.m_showDate = (bool) swg->getShowDate();
    }
    if (channelSettingsKeys.contains("displayPhotos")) {
        settings.m_displayPhotos = (bool) swg->getDisplayPhotos();
    }
    if (channelSettingsKeys.contains("displayChart")) {
        settings.m_displayChart = (bool) swg->getDisplayChart();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = (bool) swg->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpPort"))
    {
        const int udpPort = swg->getUdpPort();
        if ((udpPort > 1023) && (udpPort < 65535)) {
            settings.m_udpPort = (uint16_t) udpPort;
        }
    }
    if (channelSettingsKeys.contains("logEnabled")) {
        settings.m_logEnabled = (bool) swg->getLogEnabled();
    }
    if (channelSettingsKeys.contains("scopeCh1")) {
        settings.m_scopeCh1 = (int) swg->getScopeCh1();
    }
    if (channelSettingsKeys.contains("scopeCh2")) {
        settings.m_scopeCh2 = (int) swg->getScopeCh2();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = (quint32) swg->getRgbColor();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = (int) swg->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = (bool) swg->getUseReverseApi();
    }
    if (channelSettingsKeys.contains("reverseAPIPort"))
    {
        const int reverseAPIPort = swg->getReverseApiPort();
        if ((reverseAPIPort > 1023) && (reverseAPIPort < 65535)) {
            settings.m_reverseAPIPort = (uint16_t) reverseAPIPort;
        }
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = (uint16_t) qBound(0, swg->getReverseApiDeviceIndex(), 99);
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = (uint16_t) qBound(0, swg->getReverseApiChannelIndex(), 99);
    }
    if (channelSettingsKeys.contains("workspaceIndex")) {
        settings.m_workspaceIndex = (int) swg->getWorkspaceIndex();
    }

    if (channelSettingsKeys.contains("filter") && swg->getFilter()) {
        settings.m_filter = *swg->getFilter();
    }
    if (channelSettingsKeys.contains("udpAddress") && swg->getUdpAddress()) {
        settings.m_udpAddress = *swg->getUdpAddress();
    }
    if (channelSettingsKeys.contains("logFilename") && swg->getLogFilename()) {
        settings.m_logFilename = *swg->getLogFilename();
    }
    if (channelSettingsKeys.contains("title") && swg->getTitle()) {
        settings.m_title = *swg->getTitle();
    }
    if (channelSettingsKeys.contains("reverseAPIAddress") && swg->getReverseApiAddress()) {
        settings.m_reverseAPIAddress = *swg->getReverseApiAddress();
    }
}

void AcarsDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const AcarsDemodSettings& settings)
{
    SWGSDRangel::SWGACARSDemodSettings *swg = response.getAcarsDemodSettings();

    swg->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    swg->setMode(settings.m_mode);
    swg->setAeroChannel(settings.m_aeroChannel);
    swg->setIridiumBandwidth(settings.m_iridiumBandwidth);
    swg->setRfBandwidth(settings.m_rfBandwidth);
    swg->setCorrelationThreshold(settings.m_correlationThreshold);
    swg->setFilterColumn(settings.m_filterColumn);
    swg->setHideNoInfo(settings.m_hideNoInfo);
    swg->setShowDate(settings.m_showDate);
    swg->setDisplayPhotos(settings.m_displayPhotos);
    swg->setDisplayChart(settings.m_displayChart);
    swg->setUdpEnabled(settings.m_udpEnabled);
    swg->setUdpPort(settings.m_udpPort);
    swg->setLogEnabled(settings.m_logEnabled);
    swg->setScopeCh1(settings.m_scopeCh1);
    swg->setScopeCh2(settings.m_scopeCh2);
    swg->setRgbColor(settings.m_rgbColor);
    swg->setStreamIndex(settings.m_streamIndex);
    swg->setUseReverseApi(settings.m_useReverseAPI);
    swg->setReverseApiPort(settings.m_reverseAPIPort);
    swg->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    swg->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
    swg->setWorkspaceIndex(settings.m_workspaceIndex);

    acarsApiSetString(swg, swg->getFilter(), &SWGSDRangel::SWGACARSDemodSettings::setFilter,
                      settings.m_filter);
    acarsApiSetString(swg, swg->getUdpAddress(), &SWGSDRangel::SWGACARSDemodSettings::setUdpAddress,
                      settings.m_udpAddress);
    acarsApiSetString(swg, swg->getLogFilename(), &SWGSDRangel::SWGACARSDemodSettings::setLogFilename,
                      settings.m_logFilename);
    acarsApiSetString(swg, swg->getTitle(), &SWGSDRangel::SWGACARSDemodSettings::setTitle,
                      settings.m_title);
    acarsApiSetString(swg, swg->getReverseApiAddress(), &SWGSDRangel::SWGACARSDemodSettings::setReverseApiAddress,
                      settings.m_reverseAPIAddress);
}

void AcarsDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const AcarsDemodSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);

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

// Only what changed, unless forced - and never the reverse API settings themselves,
// which describe this end of the link rather than the channel
void AcarsDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const AcarsDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0);     // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setAcarsDemodSettings(new SWGSDRangel::SWGACARSDemodSettings());
    SWGSDRangel::SWGACARSDemodSettings *swg = swgChannelSettings->getAcarsDemodSettings();

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swg->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("mode") || force) {
        swg->setMode(settings.m_mode);
    }
    if (channelSettingsKeys.contains("aeroChannel") || force) {
        swg->setAeroChannel(settings.m_aeroChannel);
    }
    if (channelSettingsKeys.contains("iridiumBandwidth") || force) {
        swg->setIridiumBandwidth(settings.m_iridiumBandwidth);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swg->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("correlationThreshold") || force) {
        swg->setCorrelationThreshold(settings.m_correlationThreshold);
    }
    if (channelSettingsKeys.contains("filterColumn") || force) {
        swg->setFilterColumn(settings.m_filterColumn);
    }
    if (channelSettingsKeys.contains("hideNoInfo") || force) {
        swg->setHideNoInfo(settings.m_hideNoInfo);
    }
    if (channelSettingsKeys.contains("showDate") || force) {
        swg->setShowDate(settings.m_showDate);
    }
    if (channelSettingsKeys.contains("displayPhotos") || force) {
        swg->setDisplayPhotos(settings.m_displayPhotos);
    }
    if (channelSettingsKeys.contains("displayChart") || force) {
        swg->setDisplayChart(settings.m_displayChart);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swg->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swg->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("logEnabled") || force) {
        swg->setLogEnabled(settings.m_logEnabled);
    }
    if (channelSettingsKeys.contains("scopeCh1") || force) {
        swg->setScopeCh1(settings.m_scopeCh1);
    }
    if (channelSettingsKeys.contains("scopeCh2") || force) {
        swg->setScopeCh2(settings.m_scopeCh2);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swg->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swg->setStreamIndex(settings.m_streamIndex);
    }
    if (channelSettingsKeys.contains("useReverseAPI") || force) {
        swg->setUseReverseApi(settings.m_useReverseAPI);
    }
    if (channelSettingsKeys.contains("reverseAPIPort") || force) {
        swg->setReverseApiPort(settings.m_reverseAPIPort);
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex") || force) {
        swg->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex") || force) {
        swg->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
    }
    if (channelSettingsKeys.contains("workspaceIndex") || force) {
        swg->setWorkspaceIndex(settings.m_workspaceIndex);
    }

    if (channelSettingsKeys.contains("filter") || force) {
        swg->setFilter(new QString(settings.m_filter));
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swg->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("logFilename") || force) {
        swg->setLogFilename(new QString(settings.m_logFilename));
    }
    if (channelSettingsKeys.contains("title") || force) {
        swg->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("reverseAPIAddress") || force) {
        swg->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }
}

void AcarsDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AcarsDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AcarsDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
