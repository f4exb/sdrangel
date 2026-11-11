///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                    //
// Some code by AI                                                               //
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

#include "psk31demod.h"

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
#include "SWGPSK31DemodSettings.h"
#include "SWGChannelReport.h"

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(PSK31Demod::MsgConfigurePSK31Demod, Message)
MESSAGE_CLASS_DEFINITION(PSK31Demod::MsgCharacter, Message)
MESSAGE_CLASS_DEFINITION(PSK31Demod::MsgDemodReport, Message)

const char * const PSK31Demod::m_channelIdURI = "sdrangel.channel.psk31demod";
const char * const PSK31Demod::m_channelId = "PSK31Demod";

PSK31Demod::PSK31Demod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0),
        m_centerFrequency(0)
{
    setObjectName(m_channelId);
    m_thread.setObjectName("PSK31DemodBB");

    m_basebandSink = new PSK31DemodBaseband();
    m_basebandSink->setMessageQueueToChannel(getInputMessageQueue());
    m_basebandSink->moveToThread(&m_thread);

    applySettings(QStringList(), m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &PSK31Demod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &PSK31Demod::handleIndexInDeviceSetChanged
    );
}

PSK31Demod::~PSK31Demod()
{
    qDebug("PSK31Demod::~PSK31Demod");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &PSK31Demod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true, m_settings.m_streamIndex);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void PSK31Demod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this, false, m_settings.m_streamIndex);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this, m_settings.m_streamIndex);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

uint32_t PSK31Demod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void PSK31Demod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void PSK31Demod::start()
{
    qDebug("PSK31Demod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    PSK31DemodBaseband::MsgConfigurePSK31DemodBaseband *msg = PSK31DemodBaseband::MsgConfigurePSK31DemodBaseband::create(QStringList(), m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void PSK31Demod::stop()
{
    qDebug("PSK31Demod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool PSK31Demod::handleMessage(const Message& cmd)
{
    if (MsgConfigurePSK31Demod::match(cmd))
    {
        MsgConfigurePSK31Demod& cfg = (MsgConfigurePSK31Demod&) cmd;
        qDebug() << "PSK31Demod::handleMessage: MsgConfigurePSK31Demod";
        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "PSK31Demod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (PSK31Demod::MsgCharacter::match(cmd))
    {
        // Forward to GUI
        PSK31Demod::MsgCharacter& report = (PSK31Demod::MsgCharacter&)cmd;
        if (getMessageQueueToGUI())
        {
            PSK31Demod::MsgCharacter *msg = new PSK31Demod::MsgCharacter(report);
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
        if (m_logFile.isOpen() && !report.getCharacter().isEmpty())
        {
            const QChar character = report.getCharacter()[0];
            m_logStream << displayText(character, m_previousLogCharacter);
            m_logStream.flush();
            m_previousLogCharacter = character;
        }

        return true;
    }
    else if (PSK31Demod::MsgDemodReport::match(cmd))
    {
        PSK31Demod::MsgDemodReport& report = (PSK31Demod::MsgDemodReport&)cmd;
        if (getMessageQueueToGUI())
        {
            PSK31Demod::MsgDemodReport *msg = new PSK31Demod::MsgDemodReport(report);
            getMessageQueueToGUI()->push(msg);
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "PSK31Demod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

QString PSK31Demod::displayText(QChar character, QChar previousCharacter)
{
    if (character == '\r') {
        return QStringLiteral("\n");
    } else if (character == '\n') {
        return previousCharacter == '\r' ? QString() : QStringLiteral("\n");
    } else if ((character == '\t') || character.isPrint()) {
        return QString(character);
    } else {
        return QString();
    }
}

ScopeVis *PSK31Demod::getScopeSink()
{
    return m_basebandSink->getScopeSink();
}

void PSK31Demod::setCenterFrequency(qint64 frequency)
{
    PSK31DemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList({"inputFrequencyOffset"}), settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigurePSK31Demod *msgToGUI = MsgConfigurePSK31Demod::create(QStringList({"inputFrequencyOffset"}), settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void PSK31Demod::applySettings(const QStringList& settingsKeys, const PSK31DemodSettings& settings, bool force)
{
    PSK31DemodSettings validatedSettings = settings;
    validatedSettings.m_rfBandwidth = PSK31DemodSettings::validateRFBandwidth(settings.m_rfBandwidth);
    qDebug() << "PSK31Demod::applySettings:" << validatedSettings.getDebugString(settingsKeys, force);

    if (settingsKeys.contains("streamIndex") && (validatedSettings.m_streamIndex != m_settings.m_streamIndex))
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSinkAPI(this);
            m_deviceAPI->removeChannelSink(this, false, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSink(this, validatedSettings.m_streamIndex);
            m_deviceAPI->addChannelSinkAPI(this);
            m_settings.m_streamIndex = validatedSettings.m_streamIndex; // make sure ChannelAPI::getStreamIndex() is consistent
            emit streamIndexChanged(validatedSettings.m_streamIndex);
        }
    }

    PSK31DemodBaseband::MsgConfigurePSK31DemodBaseband *msg = PSK31DemodBaseband::MsgConfigurePSK31DemodBaseband::create(settingsKeys, validatedSettings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if (validatedSettings.m_useReverseAPI)
    {
        const bool fullUpdate = settingsKeys.contains("useReverseAPI") ||
                settingsKeys.contains("reverseAPIAddress") ||
                settingsKeys.contains("reverseAPIPort") ||
                settingsKeys.contains("reverseAPIDeviceIndex") ||
                settingsKeys.contains("reverseAPIChannelIndex");
        webapiReverseSendSettings(settingsKeys, validatedSettings, fullUpdate || force);
    }

    if ((settingsKeys.contains("logEnabled") && (validatedSettings.m_logEnabled != m_settings.m_logEnabled))
        || (settingsKeys.contains("logFilename") && (validatedSettings.m_logFilename != m_settings.m_logFilename))
        || force)
    {
        if (m_logFile.isOpen())
        {
            m_logStream.flush();
            m_logFile.close();
        }
        if (validatedSettings.m_logEnabled && !validatedSettings.m_logFilename.isEmpty())
        {
            m_logFile.setFileName(validatedSettings.m_logFilename);
            if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            {
                qDebug() << "PSK31Demod::applySettings - Logging to: " << validatedSettings.m_logFilename;
                m_logStream.setDevice(&m_logFile);
            }
            else
            {
                qDebug() << "PSK31Demod::applySettings - Unable to open log file: " << validatedSettings.m_logFilename;
            }
        }
    }

    if (force) {
        m_settings = validatedSettings;
    } else {
        m_settings.applySettings(settingsKeys, validatedSettings);
    }
}

void PSK31Demod::sendSampleRateToDemodAnalyzer()
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
                PSK31DemodSettings::PSK31DEMOD_CHANNEL_SAMPLE_RATE
            );
            messageQueue->push(msg);
        }
    }
}

QByteArray PSK31Demod::serialize() const
{
    return m_settings.serialize();
}

bool PSK31Demod::deserialize(const QByteArray& data)
{
    PSK31DemodSettings settings = m_settings;

    if (settings.deserialize(data))
    {
        MsgConfigurePSK31Demod *msg = MsgConfigurePSK31Demod::create(QStringList({"streamIndex"}), settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        settings.resetToDefaults();
        MsgConfigurePSK31Demod *msg = MsgConfigurePSK31Demod::create(QStringList({"streamIndex"}), settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int PSK31Demod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setPsk31DemodSettings(new SWGSDRangel::SWGPSK31DemodSettings());
    response.getPsk31DemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int PSK31Demod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int PSK31Demod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    PSK31DemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigurePSK31Demod *msg = MsgConfigurePSK31Demod::create(channelSettingsKeys, settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("PSK31Demod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigurePSK31Demod *msgToGUI = MsgConfigurePSK31Demod::create(channelSettingsKeys, settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int PSK31Demod::webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setPsk31DemodReport(new SWGSDRangel::SWGPSK31DemodReport());
    response.getPsk31DemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void PSK31Demod::webapiUpdateChannelSettings(
        PSK31DemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getPsk31DemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = PSK31DemodSettings::validateRFBandwidth(
            response.getPsk31DemodSettings()->getRfBandwidth());
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getPsk31DemodSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getPsk31DemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getPsk31DemodSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("logFilename")) {
        settings.m_logFilename = *response.getPsk31DemodSettings()->getLogFilename();
    }
    if (channelSettingsKeys.contains("logEnabled")) {
        settings.m_logEnabled = response.getPsk31DemodSettings()->getLogEnabled();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getPsk31DemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getPsk31DemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getPsk31DemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getPsk31DemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getPsk31DemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getPsk31DemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getPsk31DemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getPsk31DemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_scopeGUI && channelSettingsKeys.contains("scopeConfig")) {
        settings.m_scopeGUI->updateFrom(channelSettingsKeys, response.getPsk31DemodSettings()->getScopeConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getPsk31DemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getPsk31DemodSettings()->getRollupState());
    }
}

void PSK31Demod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const PSK31DemodSettings& settings)
{
    response.getPsk31DemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getPsk31DemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getPsk31DemodSettings()->setUdpEnabled(settings.m_udpEnabled);
    if (response.getPsk31DemodSettings()->getUdpAddress()) {
        *response.getPsk31DemodSettings()->getUdpAddress() = settings.m_udpAddress;
    } else {
        response.getPsk31DemodSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    }
    response.getPsk31DemodSettings()->setUdpPort(settings.m_udpPort);
    if (response.getPsk31DemodSettings()->getLogFilename()) {
        *response.getPsk31DemodSettings()->getLogFilename() = settings.m_logFilename;
    } else {
        response.getPsk31DemodSettings()->setLogFilename(new QString(settings.m_logFilename));
    }
    response.getPsk31DemodSettings()->setLogEnabled(settings.m_logEnabled);

    response.getPsk31DemodSettings()->setRgbColor(settings.m_rgbColor);
    if (response.getPsk31DemodSettings()->getTitle()) {
        *response.getPsk31DemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getPsk31DemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getPsk31DemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getPsk31DemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getPsk31DemodSettings()->getReverseApiAddress()) {
        *response.getPsk31DemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getPsk31DemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getPsk31DemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getPsk31DemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getPsk31DemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_scopeGUI)
    {
        if (response.getPsk31DemodSettings()->getScopeConfig())
        {
            settings.m_scopeGUI->formatTo(response.getPsk31DemodSettings()->getScopeConfig());
        }
        else
        {
            auto *swgGLScope = new SWGSDRangel::SWGGLScope();
            settings.m_scopeGUI->formatTo(swgGLScope);
            response.getPsk31DemodSettings()->setScopeConfig(swgGLScope);
        }
    }
    if (settings.m_channelMarker)
    {
        if (response.getPsk31DemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getPsk31DemodSettings()->getChannelMarker());
        }
        else
        {
            auto *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getPsk31DemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getPsk31DemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getPsk31DemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getPsk31DemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void PSK31Demod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getPsk31DemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getPsk31DemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
    response.getPsk31DemodReport()->setLocked(isLocked() ? 1 : 0);

    // Only meaningful when a signal is being received
    if (isLocked())
    {
        response.getPsk31DemodReport()->setFrequencyOffset(getFrequencyOffset());
        response.getPsk31DemodReport()->setSnr(getSNR());
    }
}

void PSK31Demod::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const PSK31DemodSettings& settings, bool force)
{
    auto *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
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

void PSK31Demod::webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const PSK31DemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    if (swgChannelSettings->getChannelType()) {
        *swgChannelSettings->getChannelType() = "PSK31Demod";
    } else {
        swgChannelSettings->setChannelType(new QString("PSK31Demod"));
    }
    swgChannelSettings->setPsk31DemodSettings(new SWGSDRangel::SWGPSK31DemodSettings());
    SWGSDRangel::SWGPSK31DemodSettings *swgPSK31DemodSettings = swgChannelSettings->getPsk31DemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgPSK31DemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgPSK31DemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgPSK31DemodSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        if (swgPSK31DemodSettings->getUdpAddress()) {
            *swgPSK31DemodSettings->getUdpAddress() = settings.m_udpAddress;
        } else {
            swgPSK31DemodSettings->setUdpAddress(new QString(settings.m_udpAddress));
        }
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgPSK31DemodSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("logFilename") || force) {
        if (swgPSK31DemodSettings->getLogFilename()) {
            *swgPSK31DemodSettings->getLogFilename() = settings.m_logFilename;
        } else {
            swgPSK31DemodSettings->setLogFilename(new QString(settings.m_logFilename));
        }
    }
    if (channelSettingsKeys.contains("logEnabled") || force) {
        swgPSK31DemodSettings->setLogEnabled(settings.m_logEnabled);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgPSK31DemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        if (swgPSK31DemodSettings->getTitle()) {
            *swgPSK31DemodSettings->getTitle() = settings.m_title;
        } else {
            swgPSK31DemodSettings->setTitle(new QString(settings.m_title));
        }
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgPSK31DemodSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_scopeGUI && (channelSettingsKeys.contains("scopeConfig") || force))
    {
        SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
        settings.m_scopeGUI->formatTo(swgGLScope);
        swgPSK31DemodSettings->setScopeConfig(swgGLScope);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgPSK31DemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgPSK31DemodSettings->setRollupState(swgRollupState);
    }
}

void PSK31Demod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "PSK31Demod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("PSK31Demod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void PSK31Demod::handleIndexInDeviceSetChanged(int index)
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
