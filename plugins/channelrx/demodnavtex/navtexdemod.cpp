///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include "navtexdemod.h"

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include <stdio.h>
#include <complex.h>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGNavtexDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGMapItem.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(NavtexDemod::MsgConfigureNavtexDemod, Message)
MESSAGE_CLASS_DEFINITION(NavtexDemod::MsgCharacter, Message)
MESSAGE_CLASS_DEFINITION(NavtexDemod::MsgMessage, Message)

const char * const NavtexDemod::m_channelIdURI = "sdrangel.channel.navtexdemod";
const char * const NavtexDemod::m_channelId = "NavtexDemod";

NavtexDemod::NavtexDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new NavtexDemodBaseband(this);
    m_basebandSink->setMessageQueueToChannel(getInputMessageQueue());
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &NavtexDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &NavtexDemod::handleIndexInDeviceSetChanged
    );
}

NavtexDemod::~NavtexDemod()
{
    qDebug("NavtexDemod::~NavtexDemod");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &NavtexDemod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void NavtexDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t NavtexDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void NavtexDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void NavtexDemod::start()
{
    qDebug("NavtexDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    NavtexDemodBaseband::MsgConfigureNavtexDemodBaseband *msg = NavtexDemodBaseband::MsgConfigureNavtexDemodBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void NavtexDemod::stop()
{
    qDebug("NavtexDemod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool NavtexDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureNavtexDemod::match(cmd))
    {
        MsgConfigureNavtexDemod& cfg = (MsgConfigureNavtexDemod&) cmd;
        qDebug() << "NavtexDemod::handleMessage: MsgConfigureNavtexDemod";
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
        qDebug() << "NavtexDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (NavtexDemod::MsgCharacter::match(cmd))
    {
        // Forward to GUI
        NavtexDemod::MsgCharacter& report = (NavtexDemod::MsgCharacter&)cmd;
        if (getMessageQueueToGUI())
        {
            NavtexDemod::MsgCharacter *msg = new NavtexDemod::MsgCharacter(report);
            getMessageQueueToGUI()->push(msg);
        }

        return true;
    }
    else if (NavtexDemod::MsgMessage::match(cmd))
    {
        // Forward to GUI
        NavtexDemod::MsgMessage& report = (NavtexDemod::MsgMessage&)cmd;
        if (getMessageQueueToGUI())
        {
            NavtexDemod::MsgMessage *msg = new NavtexDemod::MsgMessage(report);
            getMessageQueueToGUI()->push(msg);
        }

        // Forward via UDP
        if (m_settings.m_udpEnabled)
        {
            qDebug() << "Forwarding to " << m_settings.m_udpAddress << ":" << m_settings.m_udpPort;
            QByteArray bytes = report.getMessage().m_message.toUtf8();
            m_udpSocket.writeDatagram(bytes, bytes.size(),
                                      QHostAddress(m_settings.m_udpAddress), m_settings.m_udpPort);
        }

        // Write to log file
        if (m_logFile.isOpen())
        {
            const NavtexMessage &navtexMsg = report.getMessage();

            if (navtexMsg.m_valid)
            {
                m_logStream << navtexMsg.m_dateTime.date().toString() << ","
                    << navtexMsg.m_dateTime.time().toString() << ","
                    << navtexMsg.m_stationId << ","
                    << navtexMsg.m_typeId << ","
                    << navtexMsg.m_id << ","
                    << "\"" << navtexMsg.m_message << "\","
                    << report.getErrors() << ","
                    << report.getRSSI()
                    << "\n";
            }
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "NavtexDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

ScopeVis *NavtexDemod::getScopeSink()
{
    return m_basebandSink->getScopeSink();
}

void NavtexDemod::setCenterFrequency(qint64 frequency)
{
    NavtexDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureNavtexDemod *msgToGUI = MsgConfigureNavtexDemod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void NavtexDemod::applySettings(const NavtexDemodSettings& settings, bool force)
{
    qDebug() << "NavtexDemod::applySettings:"
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
    if ((settings.m_navArea != m_settings.m_navArea) || force) {
        reverseAPIKeys.append("navArea");
    }
    if ((settings.m_filterStation != m_settings.m_filterStation) || force) {
        reverseAPIKeys.append("filterStation");
    }
    if ((settings.m_filterType != m_settings.m_filterType) || force) {
        reverseAPIKeys.append("filterType");
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

    NavtexDemodBaseband::MsgConfigureNavtexDemodBaseband *msg = NavtexDemodBaseband::MsgConfigureNavtexDemodBaseband::create(settings, force);
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

    if ((settings.m_logEnabled != m_settings.m_logEnabled)
        || (settings.m_logFilename != m_settings.m_logFilename)
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
                qDebug() << "NavtexDemod::applySettings - Logging to: " << settings.m_logFilename;
                bool newFile = m_logFile.size() == 0;
                m_logStream.setDevice(&m_logFile);
                if (newFile)
                {
                    // Write header
                    m_logStream << "Date,Time,SID,TID,MID,Message,Errors,RSSI\n";
                }
            }
            else
            {
                qDebug() << "NavtexDemod::applySettings - Unable to open log file: " << settings.m_logFilename;
            }
        }
    }

    m_settings = settings;
}

void NavtexDemod::sendSampleRateToDemodAnalyzer()
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(
                this,
                NavtexDemodSettings::NAVTEXDEMOD_CHANNEL_SAMPLE_RATE
            );
            messageQueue->push(msg);
        }
    }
}

QByteArray NavtexDemod::serialize() const
{
    return m_settings.serialize();
}

bool NavtexDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureNavtexDemod *msg = MsgConfigureNavtexDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureNavtexDemod *msg = MsgConfigureNavtexDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int NavtexDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setNavtexDemodSettings(new SWGSDRangel::SWGNavtexDemodSettings());
    response.getNavtexDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int NavtexDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int NavtexDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    NavtexDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureNavtexDemod *msg = MsgConfigureNavtexDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("NavtexDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureNavtexDemod *msgToGUI = MsgConfigureNavtexDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int NavtexDemod::webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setNavtexDemodReport(new SWGSDRangel::SWGNavtexDemodReport());
    response.getNavtexDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void NavtexDemod::webapiUpdateChannelSettings(
        NavtexDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getNavtexDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getNavtexDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("navArea")) {
        settings.m_navArea = response.getNavtexDemodSettings()->getNavArea();
    }
    if (channelSettingsKeys.contains("filterStation")) {
        settings.m_filterStation = *response.getNavtexDemodSettings()->getFilterStation();
    }
    if (channelSettingsKeys.contains("filterType")) {
        settings.m_filterType = *response.getNavtexDemodSettings()->getFilterType();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getNavtexDemodSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getNavtexDemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getNavtexDemodSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("logFilename")) {
        settings.m_logFilename = *response.getAdsbDemodSettings()->getLogFilename();
    }
    if (channelSettingsKeys.contains("logEnabled")) {
        settings.m_logEnabled = response.getAdsbDemodSettings()->getLogEnabled();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getNavtexDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getNavtexDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getNavtexDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getNavtexDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getNavtexDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getNavtexDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getNavtexDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getNavtexDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_scopeGUI && channelSettingsKeys.contains("scopeConfig")) {
        settings.m_scopeGUI->updateFrom(channelSettingsKeys, response.getNavtexDemodSettings()->getScopeConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getNavtexDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getNavtexDemodSettings()->getRollupState());
    }
}

void NavtexDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const NavtexDemodSettings& settings)
{
    response.getNavtexDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getNavtexDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getNavtexDemodSettings()->setNavArea(settings.m_navArea);
    response.getNavtexDemodSettings()->setFilterStation(new QString(settings.m_filterStation));
    response.getNavtexDemodSettings()->setFilterType(new QString(settings.m_filterType));
    response.getNavtexDemodSettings()->setUdpEnabled(settings.m_udpEnabled);
    response.getNavtexDemodSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getNavtexDemodSettings()->setUdpPort(settings.m_udpPort);
    response.getNavtexDemodSettings()->setLogFilename(new QString(settings.m_logFilename));
    response.getNavtexDemodSettings()->setLogEnabled(settings.m_logEnabled);

    response.getNavtexDemodSettings()->setRgbColor(settings.m_rgbColor);
    if (response.getNavtexDemodSettings()->getTitle()) {
        *response.getNavtexDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getNavtexDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getNavtexDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getNavtexDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getNavtexDemodSettings()->getReverseApiAddress()) {
        *response.getNavtexDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getNavtexDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getNavtexDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getNavtexDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getNavtexDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_scopeGUI)
    {
        if (response.getNavtexDemodSettings()->getScopeConfig())
        {
            settings.m_scopeGUI->formatTo(response.getNavtexDemodSettings()->getScopeConfig());
        }
        else
        {
            SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
            settings.m_scopeGUI->formatTo(swgGLScope);
            response.getNavtexDemodSettings()->setScopeConfig(swgGLScope);
        }
    }
    if (settings.m_channelMarker)
    {
        if (response.getNavtexDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getNavtexDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getNavtexDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getNavtexDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getNavtexDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getNavtexDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void NavtexDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getNavtexDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getNavtexDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
}

void NavtexDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const NavtexDemodSettings& settings, bool force)
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

void NavtexDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const NavtexDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("NavtexDemod"));
    swgChannelSettings->setNavtexDemodSettings(new SWGSDRangel::SWGNavtexDemodSettings());
    SWGSDRangel::SWGNavtexDemodSettings *swgNavtexDemodSettings = swgChannelSettings->getNavtexDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgNavtexDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgNavtexDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("navArea") || force) {
        swgNavtexDemodSettings->setNavArea(settings.m_navArea);
    }
    if (channelSettingsKeys.contains("filterStation") || force) {
        swgNavtexDemodSettings->setFilterStation(new QString(settings.m_filterStation));
    }
    if (channelSettingsKeys.contains("filterType") || force) {
        swgNavtexDemodSettings->setFilterType(new QString(settings.m_filterType));
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgNavtexDemodSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgNavtexDemodSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgNavtexDemodSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("logFilename") || force) {
        swgNavtexDemodSettings->setLogFilename(new QString(settings.m_logFilename));
    }
    if (channelSettingsKeys.contains("logEnabled") || force) {
        swgNavtexDemodSettings->setLogEnabled(settings.m_logEnabled);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgNavtexDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgNavtexDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgNavtexDemodSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_scopeGUI && (channelSettingsKeys.contains("scopeConfig") || force))
    {
        SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
        settings.m_scopeGUI->formatTo(swgGLScope);
        swgNavtexDemodSettings->setScopeConfig(swgGLScope);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgNavtexDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgNavtexDemodSettings->setRollupState(swgRollupState);
    }
}

void NavtexDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "NavtexDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("NavtexDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void NavtexDemod::handleIndexInDeviceSetChanged(int index)
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

