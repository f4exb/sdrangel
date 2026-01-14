///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2026 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include "inmarsatdemod.h"

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGInmarsatDemodSettings.h"
#include "SWGChannelReport.h"

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(InmarsatDemod::MsgConfigureInmarsatDemod, Message)

const char * const InmarsatDemod::m_channelIdURI = "sdrangel.channel.inmarsatdemod";
const char * const InmarsatDemod::m_channelId = "InmarsatDemod";

InmarsatDemod::InmarsatDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new InmarsatDemodBaseband(this);
    m_basebandSink->setMessageQueueToChannel(getInputMessageQueue());
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, QStringList(), true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &InmarsatDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &InmarsatDemod::handleIndexInDeviceSetChanged
    );
}

InmarsatDemod::~InmarsatDemod()
{
    qDebug("InmarsatDemod::~InmarsatDemod");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &InmarsatDemod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void InmarsatDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t InmarsatDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void InmarsatDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void InmarsatDemod::start()
{
    qDebug("InmarsatDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    InmarsatDemodBaseband::MsgConfigureInmarsatDemodBaseband *msg = InmarsatDemodBaseband::MsgConfigureInmarsatDemodBaseband::create(m_settings, QStringList(), true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void InmarsatDemod::stop()
{
    qDebug("InmarsatDemod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool InmarsatDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureInmarsatDemod::match(cmd))
    {
        const MsgConfigureInmarsatDemod& cfg = (const MsgConfigureInmarsatDemod&) cmd;
        qDebug() << "InmarsatDemod::handleMessage: MsgConfigureInmarsatDemod";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "InmarsatDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MainCore::MsgPacket::match(cmd))
    {
        // Forward to GUI
        const MainCore::MsgPacket& report = (const MainCore::MsgPacket&)cmd;
        if (getMessageQueueToGUI())
        {
            MainCore::MsgPacket *msg = new MainCore::MsgPacket(report);
            getMessageQueueToGUI()->push(msg);
        }

        // Forward to packet features
        QList<ObjectPipe*> packetsPipes;
        MainCore::instance()->getMessagePipes().getMessagePipes(this, "packets", packetsPipes);

        for (const auto& pipe : packetsPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgPacket *msg = new MainCore::MsgPacket(report);
            messageQueue->push(msg);
        }

        // Forward via UDP
        if (m_settings.m_udpEnabled)
        {
            qDebug() << "Forwarding to " << m_settings.m_udpAddress << ":" << m_settings.m_udpPort;
            m_udpSocket.writeDatagram(report.getPacket().data(), report.getPacket().size(),
                                      QHostAddress(m_settings.m_udpAddress), m_settings.m_udpPort);
        }

        // Write to log file
        if (m_logFile.isOpen())
        {
            m_logStream << report.getDateTime().date().toString() << ","
                << report.getDateTime().time().toString() << ","
                << report.getPacket().toHex() << "\n";
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "InmarsatDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

ScopeVis *InmarsatDemod::getScopeSink()
{
    return m_basebandSink->getScopeSink();
}

void InmarsatDemod::setCenterFrequency(qint64 frequency)
{
    InmarsatDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, {"inputFrequencyOffset"}, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureInmarsatDemod *msgToGUI = MsgConfigureInmarsatDemod::create(settings, {"inputFrequencyOffset"},  false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void InmarsatDemod::applySettings(const InmarsatDemodSettings& settings, const QStringList& settingsKeys, bool force)
{
    qDebug() << "InmarsatDemod::applySettings:"
        << settings.getDebugString(settingsKeys, force)
        << " force: " << force;

    if (settingsKeys.contains("streamIndex"))
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSinkAPI(this);
            m_deviceAPI->removeChannelSink(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSink(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSinkAPI(this);
            m_settings.m_streamIndex = settings.m_streamIndex; // make sure ChannelAPI::getStreamIndex() is consistent
            emit streamIndexChanged(settings.m_streamIndex);
        }
    }

    InmarsatDemodBaseband::MsgConfigureInmarsatDemodBaseband *msg = InmarsatDemodBaseband::MsgConfigureInmarsatDemodBaseband::create(settings, settingsKeys, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
            settingsKeys.contains("reverseAPIAddress") ||
            settingsKeys.contains("reverseAPIPort") ||
            settingsKeys.contains("reverseAPIDeviceIndex") ||
            settingsKeys.contains("reverseAPIChannelIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
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
                qDebug() << "InmarsatDemod::applySettings - Logging to: " << settings.m_logFilename;
                bool newFile = m_logFile.size() == 0;
                m_logStream.setDevice(&m_logFile);
                if (newFile)
                {
                    // Write header
                    m_logStream << "Date,Time,Data\n";
                }
            }
            else
            {
                qDebug() << "InmarsatDemod::applySettings - Unable to open log file: " << settings.m_logFilename;
            }
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void InmarsatDemod::sendSampleRateToDemodAnalyzer()
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
                InmarsatDemodSettings::CHANNEL_SAMPLE_RATE
            );
            messageQueue->push(msg);
        }
    }
}

QByteArray InmarsatDemod::serialize() const
{
    return m_settings.serialize();
}

bool InmarsatDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureInmarsatDemod *msg = MsgConfigureInmarsatDemod::create(m_settings, QStringList(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureInmarsatDemod *msg = MsgConfigureInmarsatDemod::create(m_settings, QStringList(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int InmarsatDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setInmarsatDemodSettings(new SWGSDRangel::SWGInmarsatDemodSettings());
    response.getInmarsatDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int InmarsatDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int InmarsatDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    InmarsatDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureInmarsatDemod *msg = MsgConfigureInmarsatDemod::create(settings, channelSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    qDebug("InmarsatDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureInmarsatDemod *msgToGUI = MsgConfigureInmarsatDemod::create(settings, channelSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int InmarsatDemod::webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setInmarsatDemodReport(new SWGSDRangel::SWGInmarsatDemodReport());
    response.getInmarsatDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void InmarsatDemod::webapiUpdateChannelSettings(
        InmarsatDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getInmarsatDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getInmarsatDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("rrcRolloff")) {
        settings.m_rrcRolloff = response.getInmarsatDemodSettings()->getRrcRolloff();
    }
    if (channelSettingsKeys.contains("pllBandwidth")) {
        settings.m_pllBW = response.getInmarsatDemodSettings()->getPllBandwidth();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getInmarsatDemodSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getInmarsatDemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getInmarsatDemodSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("logFilename")) {
        settings.m_logFilename = *response.getInmarsatDemodSettings()->getLogFilename();
    }
    if (channelSettingsKeys.contains("logEnabled")) {
        settings.m_logEnabled = response.getInmarsatDemodSettings()->getLogEnabled();
    }
    if (channelSettingsKeys.contains("useFileTime")) {
        settings.m_useFileTime = response.getInmarsatDemodSettings()->getUseFileTime();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getInmarsatDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getInmarsatDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getInmarsatDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getInmarsatDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getInmarsatDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getInmarsatDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getInmarsatDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getInmarsatDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getInmarsatDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getInmarsatDemodSettings()->getRollupState());
    }
}

void InmarsatDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const InmarsatDemodSettings& settings)
{
    response.getInmarsatDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getInmarsatDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getInmarsatDemodSettings()->setRrcRolloff(settings.m_rrcRolloff);
    response.getInmarsatDemodSettings()->setPllBandwidth(settings.m_pllBW);
    response.getInmarsatDemodSettings()->setUdpEnabled(settings.m_udpEnabled);
    response.getInmarsatDemodSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getInmarsatDemodSettings()->setUdpPort(settings.m_udpPort);
    response.getInmarsatDemodSettings()->setLogFilename(new QString(settings.m_logFilename));
    response.getInmarsatDemodSettings()->setLogEnabled(settings.m_logEnabled);
    response.getInmarsatDemodSettings()->setUseFileTime(settings.m_useFileTime);

    response.getInmarsatDemodSettings()->setRgbColor(settings.m_rgbColor);
    if (response.getInmarsatDemodSettings()->getTitle()) {
        *response.getInmarsatDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getInmarsatDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getInmarsatDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getInmarsatDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getInmarsatDemodSettings()->getReverseApiAddress()) {
        *response.getInmarsatDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getInmarsatDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getInmarsatDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getInmarsatDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getInmarsatDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getInmarsatDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getInmarsatDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getInmarsatDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getInmarsatDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getInmarsatDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getInmarsatDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void InmarsatDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getInmarsatDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getInmarsatDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
}

void InmarsatDemod::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const InmarsatDemodSettings& settings, bool force)
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

void InmarsatDemod::webapiFormatChannelSettings(
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const InmarsatDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("InmarsatDemod"));
    swgChannelSettings->setInmarsatDemodSettings(new SWGSDRangel::SWGInmarsatDemodSettings());
    SWGSDRangel::SWGInmarsatDemodSettings *swgInmarsatDemodSettings = swgChannelSettings->getInmarsatDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgInmarsatDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgInmarsatDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("rrcRolloff") || force) {
        swgInmarsatDemodSettings->setRrcRolloff(settings.m_rrcRolloff);
    }
    if (channelSettingsKeys.contains("pllBandwidth") || force) {
        swgInmarsatDemodSettings->setPllBandwidth(settings.m_pllBW);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgInmarsatDemodSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgInmarsatDemodSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgInmarsatDemodSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("logFilename") || force) {
        swgInmarsatDemodSettings->setLogFilename(new QString(settings.m_logFilename));
    }
    if (channelSettingsKeys.contains("logEnabled") || force) {
        swgInmarsatDemodSettings->setLogEnabled(settings.m_logEnabled);
    }
    if (channelSettingsKeys.contains("useFilTime") || force) {
        swgInmarsatDemodSettings->setUseFileTime(settings.m_useFileTime);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgInmarsatDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgInmarsatDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgInmarsatDemodSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgInmarsatDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgInmarsatDemodSettings->setRollupState(swgRollupState);
    }
}

void InmarsatDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "InmarsatDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("InmarsatDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void InmarsatDemod::handleIndexInDeviceSetChanged(int index)
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
