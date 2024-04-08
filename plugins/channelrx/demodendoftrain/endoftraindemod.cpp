///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include "endoftraindemod.h"
#include "endoftrainpacket.h"

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGEndOfTrainDemodSettings.h"
#include "SWGChannelReport.h"

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(EndOfTrainDemod::MsgConfigureEndOfTrainDemod, Message)

const char * const EndOfTrainDemod::m_channelIdURI = "sdrangel.channel.endoftraindemod";
const char * const EndOfTrainDemod::m_channelId = "EndOfTrainDemod";

EndOfTrainDemod::EndOfTrainDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new EndOfTrainDemodBaseband(this);
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
        &EndOfTrainDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &EndOfTrainDemod::handleIndexInDeviceSetChanged
    );
}

EndOfTrainDemod::~EndOfTrainDemod()
{
    qDebug("EndOfTrainDemod::~EndOfTrainDemod");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &EndOfTrainDemod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void EndOfTrainDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t EndOfTrainDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void EndOfTrainDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void EndOfTrainDemod::start()
{
    qDebug("EndOfTrainDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    EndOfTrainDemodBaseband::MsgConfigureEndOfTrainDemodBaseband *msg = EndOfTrainDemodBaseband::MsgConfigureEndOfTrainDemodBaseband::create(m_settings, QStringList(), true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void EndOfTrainDemod::stop()
{
    qDebug("EndOfTrainDemod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool EndOfTrainDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureEndOfTrainDemod::match(cmd))
    {
        MsgConfigureEndOfTrainDemod& cfg = (MsgConfigureEndOfTrainDemod&) cmd;
        qDebug() << "EndOfTrainDemod::handleMessage: MsgConfigureEndOfTrainDemod";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "EndOfTrainDemod::handleMessage: DSPSignalNotification";
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
        MainCore::MsgPacket& report = (MainCore::MsgPacket&)cmd;
        if (getMessageQueueToGUI())
        {
            MainCore::MsgPacket *msg = new MainCore::MsgPacket(report);
            getMessageQueueToGUI()->push(msg);
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
            EndOfTrainPacket packet;

            if (packet.decode(report.getPacket()))
            {
                m_logStream << report.getDateTime().date().toString() << ","
                    << report.getDateTime().time().toString() << ","
                    << report.getPacket().toHex() << ","
                    << packet.m_chainingBits << ","
                    << packet.m_batteryCondition << ","
                    << packet.m_type << ","
                    << packet.m_address << ","
                    << packet.m_pressure << ","
                    << packet.m_batteryCharge << ","
                    << packet.m_discretionary << ","
                    << packet.m_valveCircuitStatus << ","
                    << packet.m_confirmation << ","
                    << packet.m_turbine << ","
                    << packet.m_markerLightBatteryCondition << ","
                    << packet.m_markerLightStatus << ","
                    << packet.m_crcValid << "\n";
            }
            else
            {
                m_logStream << report.getDateTime().date().toString() << ","
                    << report.getDateTime().time().toString() << ","
                    << report.getPacket().toHex() << "\n";
            }
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "EndOfTrainDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

ScopeVis *EndOfTrainDemod::getScopeSink()
{
    return m_basebandSink->getScopeSink();
}

void EndOfTrainDemod::setCenterFrequency(qint64 frequency)
{
    EndOfTrainDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, {"inputFrequencyOffset"}, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureEndOfTrainDemod *msgToGUI = MsgConfigureEndOfTrainDemod::create(settings, {"inputFrequencyOffset"}, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void EndOfTrainDemod::applySettings(const EndOfTrainDemodSettings& settings, const QStringList& settingsKeys, bool force)
{
    qDebug() << "EndOfTrainDemod::applySettings:"
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

    EndOfTrainDemodBaseband::MsgConfigureEndOfTrainDemodBaseband *msg = EndOfTrainDemodBaseband::MsgConfigureEndOfTrainDemodBaseband::create(settings, settingsKeys, force);
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
                qDebug() << "EndOfTrainDemod::applySettings - Logging to: " << settings.m_logFilename;
                bool newFile = m_logFile.size() == 0;
                m_logStream.setDevice(&m_logFile);
                if (newFile)
                {
                    // Write header
                    m_logStream << "Date,Time,Data,Chaining Bits,Battery Condition,Message Type,Address,Pressure,Battery Charge,Discretionary,Valve Circuit Status,Confidence Indicator,Turbine,Motion,Marker Battery Light Condition,Marker Light Status, Arm Status,CRC Valid\n";
                }
            }
            else
            {
                qDebug() << "EndOfTrainDemod::applySettings - Unable to open log file: " << settings.m_logFilename;
            }
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void EndOfTrainDemod::sendSampleRateToDemodAnalyzer()
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
                EndOfTrainDemodSettings::CHANNEL_SAMPLE_RATE
            );
            messageQueue->push(msg);
        }
    }
}

QByteArray EndOfTrainDemod::serialize() const
{
    return m_settings.serialize();
}

bool EndOfTrainDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureEndOfTrainDemod *msg = MsgConfigureEndOfTrainDemod::create(m_settings,  QStringList(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureEndOfTrainDemod *msg = MsgConfigureEndOfTrainDemod::create(m_settings,  QStringList(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int EndOfTrainDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setEndOfTrainDemodSettings(new SWGSDRangel::SWGEndOfTrainDemodSettings());
    response.getEndOfTrainDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int EndOfTrainDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int EndOfTrainDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    EndOfTrainDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureEndOfTrainDemod *msg = MsgConfigureEndOfTrainDemod::create(settings, channelSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    qDebug("EndOfTrainDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureEndOfTrainDemod *msgToGUI = MsgConfigureEndOfTrainDemod::create(settings, channelSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int EndOfTrainDemod::webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setEndOfTrainDemodReport(new SWGSDRangel::SWGEndOfTrainDemodReport());
    response.getEndOfTrainDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void EndOfTrainDemod::webapiUpdateChannelSettings(
        EndOfTrainDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getEndOfTrainDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getEndOfTrainDemodSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getEndOfTrainDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getEndOfTrainDemodSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getEndOfTrainDemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getEndOfTrainDemodSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("logFilename")) {
        settings.m_logFilename = *response.getEndOfTrainDemodSettings()->getLogFilename();
    }
    if (channelSettingsKeys.contains("logEnabled")) {
        settings.m_logEnabled = response.getEndOfTrainDemodSettings()->getLogEnabled();
    }
    if (channelSettingsKeys.contains("useFileTime")) {
        settings.m_useFileTime = response.getEndOfTrainDemodSettings()->getUseFileTime();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getEndOfTrainDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getEndOfTrainDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getEndOfTrainDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getEndOfTrainDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getEndOfTrainDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getEndOfTrainDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getEndOfTrainDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getEndOfTrainDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getEndOfTrainDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getEndOfTrainDemodSettings()->getRollupState());
    }
}

void EndOfTrainDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const EndOfTrainDemodSettings& settings)
{
    response.getEndOfTrainDemodSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getEndOfTrainDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getEndOfTrainDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getEndOfTrainDemodSettings()->setUdpEnabled(settings.m_udpEnabled);
    response.getEndOfTrainDemodSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getEndOfTrainDemodSettings()->setUdpPort(settings.m_udpPort);
    response.getEndOfTrainDemodSettings()->setLogFilename(new QString(settings.m_logFilename));
    response.getEndOfTrainDemodSettings()->setLogEnabled(settings.m_logEnabled);
    response.getEndOfTrainDemodSettings()->setUseFileTime(settings.m_useFileTime);

    response.getEndOfTrainDemodSettings()->setRgbColor(settings.m_rgbColor);
    if (response.getEndOfTrainDemodSettings()->getTitle()) {
        *response.getEndOfTrainDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getEndOfTrainDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getEndOfTrainDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getEndOfTrainDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getEndOfTrainDemodSettings()->getReverseApiAddress()) {
        *response.getEndOfTrainDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getEndOfTrainDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getEndOfTrainDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getEndOfTrainDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getEndOfTrainDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getEndOfTrainDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getEndOfTrainDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getEndOfTrainDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getEndOfTrainDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getEndOfTrainDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getEndOfTrainDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void EndOfTrainDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getEndOfTrainDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getEndOfTrainDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
}

void EndOfTrainDemod::webapiReverseSendSettings(const QStringList& channelSettingsKeys, const EndOfTrainDemodSettings& settings, bool force)
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

void EndOfTrainDemod::webapiFormatChannelSettings(
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const EndOfTrainDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("EndOfTrainDemod"));
    swgChannelSettings->setEndOfTrainDemodSettings(new SWGSDRangel::SWGEndOfTrainDemodSettings());
    SWGSDRangel::SWGEndOfTrainDemodSettings *swgEndOfTrainDemodSettings = swgChannelSettings->getEndOfTrainDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgEndOfTrainDemodSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgEndOfTrainDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgEndOfTrainDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgEndOfTrainDemodSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgEndOfTrainDemodSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgEndOfTrainDemodSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("logFilename") || force) {
        swgEndOfTrainDemodSettings->setLogFilename(new QString(settings.m_logFilename));
    }
    if (channelSettingsKeys.contains("logEnabled") || force) {
        swgEndOfTrainDemodSettings->setLogEnabled(settings.m_logEnabled);
    }
    if (channelSettingsKeys.contains("useFileTime") || force) {
        swgEndOfTrainDemodSettings->setUseFileTime(settings.m_useFileTime);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgEndOfTrainDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgEndOfTrainDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgEndOfTrainDemodSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgEndOfTrainDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgEndOfTrainDemodSettings->setRollupState(swgRollupState);
    }
}

void EndOfTrainDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "EndOfTrainDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("EndOfTrainDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void EndOfTrainDemod::handleIndexInDeviceSetChanged(int index)
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
