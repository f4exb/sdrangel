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

#include "rttydemod.h"

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
#include "SWGRTTYDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGMapItem.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(RttyDemod::MsgConfigureRttyDemod, Message)
MESSAGE_CLASS_DEFINITION(RttyDemod::MsgCharacter, Message)
MESSAGE_CLASS_DEFINITION(RttyDemod::MsgModeEstimate, Message)

const char * const RttyDemod::m_channelIdURI = "sdrangel.channel.rttydemod";
const char * const RttyDemod::m_channelId = "RTTYDemod";

RttyDemod::RttyDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new RttyDemodBaseband(this);
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
        &RttyDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &RttyDemod::handleIndexInDeviceSetChanged
    );
}

RttyDemod::~RttyDemod()
{
    qDebug("RttyDemod::~RttyDemod");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RttyDemod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void RttyDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t RttyDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void RttyDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void RttyDemod::start()
{
    qDebug("RttyDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    RttyDemodBaseband::MsgConfigureRttyDemodBaseband *msg = RttyDemodBaseband::MsgConfigureRttyDemodBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void RttyDemod::stop()
{
    qDebug("RttyDemod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool RttyDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureRttyDemod::match(cmd))
    {
        MsgConfigureRttyDemod& cfg = (MsgConfigureRttyDemod&) cmd;
        qDebug() << "RttyDemod::handleMessage: MsgConfigureRttyDemod";
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
        qDebug() << "RttyDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (RttyDemod::MsgCharacter::match(cmd))
    {
        // Forward to GUI
        RttyDemod::MsgCharacter& report = (RttyDemod::MsgCharacter&)cmd;
        if (getMessageQueueToGUI())
        {
            RttyDemod::MsgCharacter *msg = new RttyDemod::MsgCharacter(report);
            getMessageQueueToGUI()->push(msg);
        }

        // Forward via UDP
        if (m_settings.m_udpEnabled)
        {
            QByteArray bytes = report.getCharacter().toUtf8();
            m_udpSocket.writeDatagram(bytes, bytes.size(),
                                      QHostAddress(m_settings.m_udpAddress), m_settings.m_udpPort);
        }

        // Write to log file
        if (m_logFile.isOpen()) {
            m_logStream << report.getCharacter();
        }

        return true;
    }
    else if (RttyDemod::MsgModeEstimate::match(cmd))
    {
        // Forward to GUI
        RttyDemod::MsgModeEstimate& report = (RttyDemod::MsgModeEstimate&)cmd;
        if (getMessageQueueToGUI())
        {
            RttyDemod::MsgModeEstimate *msg = new RttyDemod::MsgModeEstimate(report);
            getMessageQueueToGUI()->push(msg);
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "RttyDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

ScopeVis *RttyDemod::getScopeSink()
{
    return m_basebandSink->getScopeSink();
}

void RttyDemod::setCenterFrequency(qint64 frequency)
{
    RttyDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRttyDemod *msgToGUI = MsgConfigureRttyDemod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void RttyDemod::applySettings(const RttyDemodSettings& settings, bool force)
{
    qDebug() << "RttyDemod::applySettings:"
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
    if ((settings.m_baudRate != m_settings.m_baudRate) || force) {
        reverseAPIKeys.append("baudRate");
    }
    if ((settings.m_frequencyShift != m_settings.m_frequencyShift) || force) {
        reverseAPIKeys.append("frequencyShift");
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
    if ((settings.m_characterSet != m_settings.m_characterSet) || force) {
        reverseAPIKeys.append("characterSet");
    }
    if ((settings.m_suppressCRLF != m_settings.m_suppressCRLF) || force) {
        reverseAPIKeys.append("suppressCRLF");
    }
    if ((settings.m_unshiftOnSpace != m_settings.m_unshiftOnSpace) || force) {
        reverseAPIKeys.append("unshiftOnSpace");
    }
    if ((settings.m_msbFirst != m_settings.m_msbFirst) || force) {
        reverseAPIKeys.append("msbFirst");
    }
    if ((settings.m_spaceHigh != m_settings.m_spaceHigh) || force) {
        reverseAPIKeys.append("spaceHigh");
    }
    if ((settings.m_squelch != m_settings.m_squelch) || force) {
        reverseAPIKeys.append("squelch");
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

    RttyDemodBaseband::MsgConfigureRttyDemodBaseband *msg = RttyDemodBaseband::MsgConfigureRttyDemodBaseband::create(settings, force);
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
                qDebug() << "RttyDemod::applySettings - Logging to: " << settings.m_logFilename;
                m_logStream.setDevice(&m_logFile);
            }
            else
            {
                qDebug() << "RttyDemod::applySettings - Unable to open log file: " << settings.m_logFilename;
            }
        }
    }

    m_settings = settings;
}

void RttyDemod::sendSampleRateToDemodAnalyzer()
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
                RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE
            );
            messageQueue->push(msg);
        }
    }
}

QByteArray RttyDemod::serialize() const
{
    return m_settings.serialize();
}

bool RttyDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureRttyDemod *msg = MsgConfigureRttyDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureRttyDemod *msg = MsgConfigureRttyDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int RttyDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRttyDemodSettings(new SWGSDRangel::SWGRTTYDemodSettings());
    response.getRttyDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int RttyDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int RttyDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    RttyDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureRttyDemod *msg = MsgConfigureRttyDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("RttyDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRttyDemod *msgToGUI = MsgConfigureRttyDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int RttyDemod::webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setRttyDemodReport(new SWGSDRangel::SWGRTTYDemodReport());
    response.getRttyDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void RttyDemod::webapiUpdateChannelSettings(
        RttyDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getRttyDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getRttyDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("baudRate")) {
        settings.m_baudRate = response.getRttyDemodSettings()->getBaudRate();
    }
    if (channelSettingsKeys.contains("frequencyShift")) {
        settings.m_frequencyShift = response.getRttyDemodSettings()->getFrequencyShift();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getRttyDemodSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getRttyDemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getRttyDemodSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("characterSet")) {
        settings.m_characterSet = (Baudot::CharacterSet)response.getRttyDemodSettings()->getCharacterSet();
    }
    if (channelSettingsKeys.contains("suppressCRLF")) {
        settings.m_suppressCRLF = response.getRttyDemodSettings()->getSuppressCrlf();
    }
    if (channelSettingsKeys.contains("unshiftOnSpace")) {
        settings.m_unshiftOnSpace = response.getRttyDemodSettings()->getUnshiftOnSpace();
    }
    if (channelSettingsKeys.contains("msbFirst")) {
        settings.m_msbFirst = response.getRttyDemodSettings()->getMsbFirst();
    }
    if (channelSettingsKeys.contains("spaceHigh")) {
        settings.m_spaceHigh = response.getRttyDemodSettings()->getSpaceHigh();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getRttyDemodSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("logFilename")) {
        settings.m_logFilename = *response.getAdsbDemodSettings()->getLogFilename();
    }
    if (channelSettingsKeys.contains("logEnabled")) {
        settings.m_logEnabled = response.getAdsbDemodSettings()->getLogEnabled();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getRttyDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getRttyDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getRttyDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRttyDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRttyDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRttyDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getRttyDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getRttyDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_scopeGUI && channelSettingsKeys.contains("scopeConfig")) {
        settings.m_scopeGUI->updateFrom(channelSettingsKeys, response.getRttyDemodSettings()->getScopeConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getRttyDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getRttyDemodSettings()->getRollupState());
    }
}

void RttyDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const RttyDemodSettings& settings)
{
    response.getRttyDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getRttyDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getRttyDemodSettings()->setBaudRate(settings.m_baudRate);
    response.getRttyDemodSettings()->setFrequencyShift(settings.m_frequencyShift);
    response.getRttyDemodSettings()->setUdpEnabled(settings.m_udpEnabled);
    response.getRttyDemodSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getRttyDemodSettings()->setUdpPort(settings.m_udpPort);
    response.getRttyDemodSettings()->setCharacterSet(settings.m_characterSet);
    response.getRttyDemodSettings()->setSuppressCrlf(settings.m_suppressCRLF);
    response.getRttyDemodSettings()->setUnshiftOnSpace(settings.m_unshiftOnSpace);
    response.getRttyDemodSettings()->setMsbFirst(settings.m_msbFirst);
    response.getRttyDemodSettings()->setSpaceHigh(settings.m_spaceHigh);
    response.getRttyDemodSettings()->setSquelch(settings.m_squelch);
    response.getRttyDemodSettings()->setLogFilename(new QString(settings.m_logFilename));
    response.getRttyDemodSettings()->setLogEnabled(settings.m_logEnabled);

    response.getRttyDemodSettings()->setRgbColor(settings.m_rgbColor);
    if (response.getRttyDemodSettings()->getTitle()) {
        *response.getRttyDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getRttyDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getRttyDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getRttyDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRttyDemodSettings()->getReverseApiAddress()) {
        *response.getRttyDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRttyDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRttyDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRttyDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getRttyDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_scopeGUI)
    {
        if (response.getRttyDemodSettings()->getScopeConfig())
        {
            settings.m_scopeGUI->formatTo(response.getRttyDemodSettings()->getScopeConfig());
        }
        else
        {
            SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
            settings.m_scopeGUI->formatTo(swgGLScope);
            response.getRttyDemodSettings()->setScopeConfig(swgGLScope);
        }
    }
    if (settings.m_channelMarker)
    {
        if (response.getRttyDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getRttyDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getRttyDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getRttyDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getRttyDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getRttyDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void RttyDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getRttyDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getRttyDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
}

void RttyDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const RttyDemodSettings& settings, bool force)
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

void RttyDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const RttyDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("RttyDemod"));
    swgChannelSettings->setRttyDemodSettings(new SWGSDRangel::SWGRTTYDemodSettings());
    SWGSDRangel::SWGRTTYDemodSettings *swgRttyDemodSettings = swgChannelSettings->getRttyDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgRttyDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgRttyDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("baudRate") || force) {
        swgRttyDemodSettings->setBaudRate(settings.m_baudRate);
    }
    if (channelSettingsKeys.contains("frequencyShift") || force) {
        swgRttyDemodSettings->setFrequencyShift(settings.m_frequencyShift);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgRttyDemodSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgRttyDemodSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgRttyDemodSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("characterSet") || force) {
        swgRttyDemodSettings->setCharacterSet(settings.m_characterSet);
    }
    if (channelSettingsKeys.contains("suppressCRLF") || force) {
        swgRttyDemodSettings->setSuppressCrlf(settings.m_suppressCRLF);
    }
    if (channelSettingsKeys.contains("unshiftOnSpace") || force) {
        swgRttyDemodSettings->setUnshiftOnSpace(settings.m_unshiftOnSpace);
    }
    if (channelSettingsKeys.contains("msbFirst") || force) {
        swgRttyDemodSettings->setMsbFirst(settings.m_msbFirst);
    }
    if (channelSettingsKeys.contains("spaceHigh") || force) {
        swgRttyDemodSettings->setSpaceHigh(settings.m_spaceHigh);
    }
    if (channelSettingsKeys.contains("squelch") || force) {
        swgRttyDemodSettings->setSquelch(settings.m_squelch);
    }
    if (channelSettingsKeys.contains("logFilename") || force) {
        swgRttyDemodSettings->setLogFilename(new QString(settings.m_logFilename));
    }
    if (channelSettingsKeys.contains("logEnabled") || force) {
        swgRttyDemodSettings->setLogEnabled(settings.m_logEnabled);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgRttyDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgRttyDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgRttyDemodSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_scopeGUI && (channelSettingsKeys.contains("scopeConfig") || force))
    {
        SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
        settings.m_scopeGUI->formatTo(swgGLScope);
        swgRttyDemodSettings->setScopeConfig(swgGLScope);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgRttyDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgRttyDemodSettings->setRollupState(swgRollupState);
    }
}

void RttyDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "RttyDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("RttyDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void RttyDemod::handleIndexInDeviceSetChanged(int index)
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

