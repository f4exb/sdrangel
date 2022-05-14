///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <QTime>
#include <QDebug>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkDatagram>
#include <QNetworkReply>
#include <QBuffer>
#include <QUdpSocket>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

#include "aismodbaseband.h"
#include "aismod.h"

MESSAGE_CLASS_DEFINITION(AISMod::MsgConfigureAISMod, Message)
MESSAGE_CLASS_DEFINITION(AISMod::MsgReportData, Message)
MESSAGE_CLASS_DEFINITION(AISMod::MsgTx, Message)
MESSAGE_CLASS_DEFINITION(AISMod::MsgEncode, Message)
MESSAGE_CLASS_DEFINITION(AISMod::MsgTXPacketBytes, Message)
MESSAGE_CLASS_DEFINITION(AISMod::MsgTXPacketData, Message)

const char* const AISMod::m_channelIdURI = "sdrangel.channel.modais";
const char* const AISMod::m_channelId = "AISMod";

AISMod::AISMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF),
    m_settingsMutex(QMutex::Recursive),
    m_udpSocket(nullptr)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new AISModBaseband();
    m_basebandSource->setSpectrumSampleSink(&m_spectrumVis);
    m_basebandSource->setChannel(this);
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AISMod::networkManagerFinished
    );
}

AISMod::~AISMod()
{
    closeUDP();
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AISMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void AISMod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSourceAPI(this);
        m_deviceAPI->removeChannelSource(this);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSource(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

void AISMod::start()
{
    qDebug("AISMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void AISMod::stop()
{
    qDebug("AISMod::stop");
    m_thread->exit();
    m_thread->wait();
}

void AISMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

void AISMod::setCenterFrequency(qint64 frequency)
{
    AISModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAISMod *msgToGUI = MsgConfigureAISMod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

bool AISMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureAISMod::match(cmd))
    {
        MsgConfigureAISMod& cfg = (MsgConfigureAISMod&) cmd;
        qDebug() << "AISMod::handleMessage: MsgConfigureAISMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgTx::match(cmd))
    {
        // Forward a copy to baseband
        MsgTx* rep = new MsgTx((MsgTx&)cmd);
        qDebug() << "AISMod::handleMessage: MsgTx";
        m_basebandSource->getInputMessageQueue()->push(rep);

        return true;
    }
    else if (MsgEncode::match(cmd))
    {
        qDebug() << "AISMod::handleMessage: MsgEncode";
        encode();
        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "AISMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }
        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "AISMod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

ScopeVis *AISMod::getScopeSink()
{
    return m_basebandSource->getScopeSink();
}

// Convert decimal degrees to 1/10000 minutes
int AISMod::degToMinFracs(float decimal)
{
    return std::round(decimal * 60.0f * 10000.0f);
}

void AISMod::encode()
{
    unsigned char bytes[168/8];
    int mmsi;
    int latitude;
    int longitude;

    mmsi = m_settings.m_mmsi.toInt();

    latitude = degToMinFracs(m_settings.m_latitude);
    longitude = degToMinFracs(m_settings.m_longitude);

    if (m_settings.getMsgId() == 4)
    {
        // Base station report
        QDateTime currentDateTime = QDateTime::currentDateTimeUtc();
        QDate currentDate = currentDateTime.date();
        QTime currentTime = currentDateTime.time();

        int year = currentDate.year();
        int month = currentDate.month();
        int day = currentDate.day();
        int hour = currentTime.hour();
        int minute = currentTime.minute();
        int second = currentTime.second();

        bytes[0] = (m_settings.getMsgId() << 2); // Repeat indicator = 0
        bytes[1] = (mmsi >> 22) & 0xff;
        bytes[2] = (mmsi >> 14) & 0xff;
        bytes[3] = (mmsi >> 6) & 0xff;
        bytes[4] = ((mmsi & 0x3f) << 2) | ((year >> 12) & 0x3);
        bytes[5] = (year >> 4) & 0xff;
        bytes[6] = ((year & 0xf) << 4) | month;
        bytes[7] = (day << 3) | ((hour >> 2) & 0x7);
        bytes[8] = ((hour & 0x3) << 6) | minute;
        bytes[9] = (second << 2) | (0 << 1) | ((longitude >> 27) & 1);
        bytes[10] = (longitude >> 19) & 0xff;
        bytes[11] = (longitude >> 11) & 0xff;
        bytes[12] = (longitude >> 3) & 0xff;
        bytes[13] = ((longitude & 0x7) << 5) | ((latitude >> 22) & 0x1f);
        bytes[14] = (latitude >> 14) & 0xff;
        bytes[15] = (latitude >> 6) & 0xff;
        bytes[16] = ((latitude & 0x3f) << 2);
        bytes[17] = 0;
        bytes[18] = 0;
        bytes[19] = 0;
        bytes[20] = 0;
    }
    else
    {
        // Position report
        int status;
        int rateOfTurn = 0x80; // Not available as not currently in GUI
        int speedOverGround;
        int courseOverGround;
        int timestamp;

        timestamp = QDateTime::currentDateTimeUtc().time().second();

        if (m_settings.m_speed >= 102.2)
            speedOverGround = 1022;
        else
            speedOverGround = std::round(m_settings.m_speed * 10.0);

        courseOverGround = std::floor(m_settings.m_course * 10.0);

        if (m_settings.m_status == AISModSettings::StatusNotDefined) // Not defined (last in combo box)
            status = 15;
        else
            status = (int) m_settings.m_status;

        bytes[0] = (m_settings.getMsgId() << 2); // Repeat indicator = 0

        bytes[1] = (mmsi >> 22) & 0xff;
        bytes[2] = (mmsi >> 14) & 0xff;
        bytes[3] = (mmsi >> 6) & 0xff;
        bytes[4] = ((mmsi & 0x3f) << 2) | (status >> 2);

        bytes[5] = ((status & 0x3) << 6) | ((rateOfTurn >> 2) & 0x3f);
        bytes[6] = ((rateOfTurn & 0x3) << 6) | ((speedOverGround >> 4) & 0x3f);
        bytes[7] = ((speedOverGround & 0xf) << 4) | (0 << 3) | ((longitude >> 25) & 0x7); // Position accuracy = 0
        bytes[8] = (longitude >> 17) & 0xff;
        bytes[9] = (longitude >> 9) & 0xff;
        bytes[10] = (longitude >> 1) & 0xff;
        bytes[11] = ((longitude & 0x1) << 7) | ((latitude >> 20) & 0x7f);
        bytes[12] = (latitude >> 12) & 0xff;
        bytes[13] = (latitude >> 4) & 0xff;
        bytes[14] = ((latitude & 0xf) << 4) | ((courseOverGround >> 8) & 0xf);
        bytes[15] = courseOverGround & 0xff;
        bytes[16] = ((m_settings.m_heading >> 1) & 0xff);
        bytes[17] = ((m_settings.m_heading & 0x1) << 7) | ((timestamp & 0x3f) << 1);
        bytes[18] = 0;
        bytes[19] = 0;
        bytes[20] = 0;
    }

    QByteArray ba((const char *)bytes, sizeof(bytes));
    m_settings.m_data = ba.toHex();

    if (getMessageQueueToGUI())
    {
        MsgReportData *msg = MsgReportData::create(m_settings.m_data);
        getMessageQueueToGUI()->push(msg);
    }
}

void AISMod::applySettings(const AISModSettings& settings, bool force)
{
    qDebug() << "AISMod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_gain: " << settings.m_gain
            << " m_channelMute: " << settings.m_channelMute
            << " m_repeat: " << settings.m_repeat
            << " m_repeatDelay: " << settings.m_repeatDelay
            << " m_repeatCount: " << settings.m_repeatCount
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIAddress: " << settings.m_reverseAPIPort
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

    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        reverseAPIKeys.append("fmDeviation");
    }

    if ((settings.m_gain != m_settings.m_gain) || force) {
        reverseAPIKeys.append("gain");
    }

    if ((settings.m_channelMute != m_settings.m_channelMute) || force) {
        reverseAPIKeys.append("channelMute");
    }

    if ((settings.m_repeat != m_settings.m_repeat) || force) {
        reverseAPIKeys.append("repeat");
    }

    if ((settings.m_baud != m_settings.m_baud) || force) {
        reverseAPIKeys.append("baud");
    }

    if ((settings.m_repeatDelay != m_settings.m_repeatDelay) || force) {
        reverseAPIKeys.append("repeatDelay");
    }

    if ((settings.m_repeatCount != m_settings.m_repeatCount) || force) {
        reverseAPIKeys.append("repeatCount");
    }

    if ((settings.m_rampUpBits != m_settings.m_rampUpBits) || force) {
        reverseAPIKeys.append("rampUpBits");
    }

    if ((settings.m_rampDownBits != m_settings.m_rampDownBits) || force) {
        reverseAPIKeys.append("rampDownBits");
    }

    if ((settings.m_rampRange != m_settings.m_rampRange) || force) {
        reverseAPIKeys.append("rampRange");
    }

    if ((settings.m_rfNoise != m_settings.m_rfNoise) || force) {
        reverseAPIKeys.append("rfNoise");
    }

    if ((settings.m_writeToFile != m_settings.m_writeToFile) || force) {
        reverseAPIKeys.append("writeToFile");
    }

    if ((settings.m_msgType != m_settings.m_msgType) || force) {
        reverseAPIKeys.append("msgType");
    }

    if ((settings.m_mmsi != m_settings.m_mmsi) || force) {
        reverseAPIKeys.append("mmsi");
    }

    if ((settings.m_status != m_settings.m_status) || force) {
        reverseAPIKeys.append("status");
    }

    if ((settings.m_latitude != m_settings.m_latitude) || force) {
        reverseAPIKeys.append("latitude");
    }

    if ((settings.m_longitude != m_settings.m_longitude) || force) {
        reverseAPIKeys.append("longitude");
    }

    if ((settings.m_course != m_settings.m_course) || force) {
        reverseAPIKeys.append("course");
    }

    if ((settings.m_speed != m_settings.m_speed) || force) {
        reverseAPIKeys.append("speed");
    }

    if ((settings.m_heading != m_settings.m_heading) || force) {
        reverseAPIKeys.append("heading");
    }

    if ((settings.m_data != m_settings.m_data) || force) {
        reverseAPIKeys.append("data");
    }

    if ((settings.m_bt != m_settings.m_bt) || force) {
        reverseAPIKeys.append("bt");
    }

    if ((settings.m_symbolSpan != m_settings.m_symbolSpan) || force) {
        reverseAPIKeys.append("symbolSpan");
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

    if (   (settings.m_udpEnabled != m_settings.m_udpEnabled)
        || (settings.m_udpAddress != m_settings.m_udpAddress)
        || (settings.m_udpPort != m_settings.m_udpPort)
        || force)
    {
        if (settings.m_udpEnabled)
            openUDP(settings);
        else
            closeUDP();
    }

    if (m_settings.m_streamIndex != settings.m_streamIndex)
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSourceAPI(this);
            m_deviceAPI->removeChannelSource(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSource(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSourceAPI(this);
        }

        reverseAPIKeys.append("streamIndex");
    }

    AISModBaseband::MsgConfigureAISModBaseband *msg = AISModBaseband::MsgConfigureAISModBaseband::create(settings, force);
    m_basebandSource->getInputMessageQueue()->push(msg);

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, reverseAPIKeys, settings, force);
    }

    m_settings = settings;
}

QByteArray AISMod::serialize() const
{
    return m_settings.serialize();
}

bool AISMod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureAISMod *msg = MsgConfigureAISMod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

void AISMod::sendSampleRateToDemodAnalyzer()
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue* messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(
                this,
                AISModSettings::AISMOD_SAMPLE_RATE
            );
            messageQueue->push(msg);
        }
    }
}

int AISMod::webapiSettingsGet(
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setAisModSettings(new SWGSDRangel::SWGAISModSettings());
    response.getAisModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int AISMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int AISMod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    AISModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureAISMod *msg = MsgConfigureAISMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAISMod *msgToGUI = MsgConfigureAISMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void AISMod::webapiUpdateChannelSettings(
        AISModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getAisModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getAisModSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getAisModSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("gain")) {
        settings.m_gain = response.getAisModSettings()->getGain();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getAisModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("repeat")) {
        settings.m_repeat = response.getAisModSettings()->getRepeat() != 0;
    }
    if (channelSettingsKeys.contains("baud")) {
        settings.m_baud = response.getAisModSettings()->getBaud();
    }
    if (channelSettingsKeys.contains("repeatDelay")) {
        settings.m_repeatDelay = response.getAisModSettings()->getRepeatDelay();
    }
    if (channelSettingsKeys.contains("repeatCount")) {
        settings.m_repeatCount = response.getAisModSettings()->getRepeatCount();
    }
    if (channelSettingsKeys.contains("rampUpBits")) {
        settings.m_rampUpBits = response.getAisModSettings()->getRampUpBits();
    }
    if (channelSettingsKeys.contains("rampDownBits")) {
        settings.m_rampDownBits = response.getAisModSettings()->getRampDownBits();
    }
    if (channelSettingsKeys.contains("rampRange")) {
        settings.m_rampRange = response.getAisModSettings()->getRampRange();
    }
    if (channelSettingsKeys.contains("rfNoise")) {
        settings.m_rfNoise = response.getAisModSettings()->getRfNoise() != 0;
    }
    if (channelSettingsKeys.contains("writeToFile")) {
        settings.m_writeToFile = response.getAisModSettings()->getWriteToFile() != 0;
    }
    if (channelSettingsKeys.contains("msgType")) {
        settings.m_msgType = (AISModSettings::MsgType) response.getAisModSettings()->getMsgType();
    }
    if (channelSettingsKeys.contains("mmsi")) {
        settings.m_mmsi = *response.getAisModSettings()->getMmsi();
    }
    if (channelSettingsKeys.contains("status")) {
        settings.m_status = (AISModSettings::Status) response.getAisModSettings()->getStatus();
    }
    if (channelSettingsKeys.contains("latitude")) {
        settings.m_latitude = response.getAisModSettings()->getLatitude();
    }
    if (channelSettingsKeys.contains("longitude")) {
        settings.m_longitude = response.getAisModSettings()->getLongitude();
    }
    if (channelSettingsKeys.contains("course")) {
        settings.m_course = response.getAisModSettings()->getCourse();
    }
    if (channelSettingsKeys.contains("speed")) {
        settings.m_speed = response.getAisModSettings()->getSpeed();
    }
    if (channelSettingsKeys.contains("heading")) {
        settings.m_heading = response.getAisModSettings()->getHeading();
    }
    if (channelSettingsKeys.contains("data")) {
        settings.m_data = *response.getAisModSettings()->getData();
    }
    if (channelSettingsKeys.contains("bt")) {
        settings.m_bt = response.getAisModSettings()->getBt();
    }
    if (channelSettingsKeys.contains("symbolSpan")) {
        settings.m_symbolSpan = response.getAisModSettings()->getSymbolSpan();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAisModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getAisModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getAisModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAisModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAisModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAisModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAisModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getAisModSettings()->getReverseApiChannelIndex();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getAisModSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getAisModSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getAisModSettings()->getUdpPort();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getAisModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getAisModSettings()->getRollupState());
    }
}

int AISMod::webapiReportGet(
                SWGSDRangel::SWGChannelReport& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setAisModReport(new SWGSDRangel::SWGAISModReport());
    response.getAisModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

int AISMod::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGAISModActions *swgAISModActions = query.getAisModActions();

    if (swgAISModActions)
    {
        if (channelActionsKeys.contains("encode"))
        {
            if (swgAISModActions->getEncode() != 0)
            {
                AISMod::MsgEncode *msg = AISMod::MsgEncode::create();
                getInputMessageQueue()->push(msg);
            }

            return 202;
        }
        if (channelActionsKeys.contains("tx"))
        {
            if (swgAISModActions->getTx() != 0)
            {
                if (channelActionsKeys.contains("data") && (swgAISModActions->getData()))
                {
                    AISMod::MsgTXPacketData *msg = AISMod::MsgTXPacketData::create(*swgAISModActions->getData());
                    m_basebandSource->getInputMessageQueue()->push(msg);
                }
                else
                {
                    AISMod::MsgTx *msg = AISMod::MsgTx::create();
                    m_basebandSource->getInputMessageQueue()->push(msg);
                }
            }

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
        errorMessage = "Missing AISModActions in query";
        return 400;
    }
    return 400;
}

void AISMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const AISModSettings& settings)
{
    response.getAisModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getAisModSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getAisModSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getAisModSettings()->setGain(settings.m_gain);
    response.getAisModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getAisModSettings()->setBaud(settings.m_baud);
    response.getAisModSettings()->setRepeat(settings.m_repeat ? 1 : 0);
    response.getAisModSettings()->setRepeatDelay(settings.m_repeatDelay);
    response.getAisModSettings()->setRepeatCount(settings.m_repeatCount);
    response.getAisModSettings()->setRampUpBits(settings.m_rampUpBits);
    response.getAisModSettings()->setRampDownBits(settings.m_rampDownBits);
    response.getAisModSettings()->setRampRange(settings.m_rampRange);
    response.getAisModSettings()->setRfNoise(settings.m_rfNoise ? 1 : 0);
    response.getAisModSettings()->setWriteToFile(settings.m_writeToFile ? 1 : 0);
    response.getAisModSettings()->setMsgType((int) settings.m_msgType);

    if (response.getAisModSettings()->getMmsi()) {
        *response.getAisModSettings()->getMmsi() = settings.m_mmsi;
    } else {
        response.getAisModSettings()->setMmsi(new QString(settings.m_mmsi));
    }
    response.getAisModSettings()->setStatus((int) settings.m_status);
    response.getAisModSettings()->setLatitude(settings.m_latitude);
    response.getAisModSettings()->setLongitude(settings.m_longitude);
    response.getAisModSettings()->setCourse(settings.m_course);
    response.getAisModSettings()->setSpeed(settings.m_speed);
    response.getAisModSettings()->setHeading(settings.m_heading);

    if (response.getAisModSettings()->getData()) {
        *response.getAisModSettings()->getData() = settings.m_data;
    } else {
        response.getAisModSettings()->setData(new QString(settings.m_data));
    }

    response.getAisModSettings()->setBt(settings.m_bt);
    response.getAisModSettings()->setSymbolSpan(settings.m_symbolSpan);
    response.getAisModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getAisModSettings()->getTitle()) {
        *response.getAisModSettings()->getTitle() = settings.m_title;
    } else {
        response.getAisModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getAisModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAisModSettings()->getReverseApiAddress()) {
        *response.getAisModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAisModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAisModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAisModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getAisModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    response.getAisModSettings()->setUdpEnabled(settings.m_udpEnabled);
    response.getAisModSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getAisModSettings()->setUdpPort(settings.m_udpPort);

    if (settings.m_channelMarker)
    {
        if (response.getAisModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getAisModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getAisModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getAisModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getAisModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getAisModSettings()->setRollupState(swgRollupState);
        }
    }
}

void AISMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getAisModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getAisModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void AISMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const AISModSettings& settings, bool force)
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

void AISMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const AISModSettings& settings,
    bool force)
{
    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

        if (messageQueue)
        {
            SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
            webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);
            MainCore::MsgChannelSettings *msg = MainCore::MsgChannelSettings::create(
                this,
                channelSettingsKeys,
                swgChannelSettings,
                force
            );
            messageQueue->push(msg);
        }
    }
}

void AISMod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const AISModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setAisModSettings(new SWGSDRangel::SWGAISModSettings());
    SWGSDRangel::SWGAISModSettings *swgAISModSettings = swgChannelSettings->getAisModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgAISModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgAISModSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgAISModSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("gain") || force) {
        swgAISModSettings->setGain(settings.m_gain);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgAISModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("baud") || force) {
        swgAISModSettings->setBaud(settings.m_baud);
    }
    if (channelSettingsKeys.contains("repeat") || force) {
        swgAISModSettings->setRepeat(settings.m_repeat ? 1 : 0);
    }
    if (channelSettingsKeys.contains("repeatDelay") || force) {
        swgAISModSettings->setRepeatDelay(settings.m_repeatDelay);
    }
    if (channelSettingsKeys.contains("repeatCount") || force) {
        swgAISModSettings->setRepeatCount(settings.m_repeatCount);
    }
    if (channelSettingsKeys.contains("rampUpBits")) {
        swgAISModSettings->setRampUpBits(settings.m_rampUpBits);
    }
    if (channelSettingsKeys.contains("rampDownBits")) {
        swgAISModSettings->setRampDownBits(settings.m_rampDownBits);
    }
    if (channelSettingsKeys.contains("rampRange")) {
        swgAISModSettings->setRampRange(settings.m_rampRange);
    }
    if (channelSettingsKeys.contains("rfNoise")) {
        swgAISModSettings->setRfNoise(settings.m_rfNoise ? 1 : 0);
    }
    if (channelSettingsKeys.contains("writeToFile")) {
        swgAISModSettings->setWriteToFile(settings.m_writeToFile ? 1 : 0);
    }
    if (channelSettingsKeys.contains("msgType")) {
        swgAISModSettings->setMsgType((int) settings.m_msgType);
    }
    if (channelSettingsKeys.contains("mmsi")) {
        swgAISModSettings->setMmsi(new QString(settings.m_mmsi));
    }
    if (channelSettingsKeys.contains("status")) {
        swgAISModSettings->setStatus((int) settings.m_status);
    }
    if (channelSettingsKeys.contains("latitude")) {
        swgAISModSettings->setLatitude(settings.m_latitude);
    }
    if (channelSettingsKeys.contains("longitude")) {
        swgAISModSettings->setLongitude(settings.m_longitude);
    }
    if (channelSettingsKeys.contains("course")) {
        swgAISModSettings->setCourse(settings.m_course);
    }
    if (channelSettingsKeys.contains("speed")) {
        swgAISModSettings->setSpeed(settings.m_speed);
    }
    if (channelSettingsKeys.contains("heading")) {
        swgAISModSettings->setHeading(settings.m_heading);
    }
    if (channelSettingsKeys.contains("data")) {
        swgAISModSettings->setData(new QString(settings.m_data));
    }
    if (channelSettingsKeys.contains("bt")) {
        swgAISModSettings->setBt(settings.m_bt);
    }
    if (channelSettingsKeys.contains("symbolSpan")) {
        swgAISModSettings->setSymbolSpan(settings.m_symbolSpan);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgAISModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgAISModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgAISModSettings->setStreamIndex(settings.m_streamIndex);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgAISModSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgAISModSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgAISModSettings->setUdpPort(settings.m_udpPort);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgAISModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgAISModSettings->setRollupState(swgRollupState);
    }
}

void AISMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AISMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AISMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double AISMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void AISMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t AISMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

void AISMod::openUDP(const AISModSettings& settings)
{
    closeUDP();
    m_udpSocket = new QUdpSocket();
    if (!m_udpSocket->bind(QHostAddress(settings.m_udpAddress), settings.m_udpPort))
        qCritical() << "AISMod::openUDP: Failed to bind to port " << settings.m_udpAddress << ":" << settings.m_udpPort << ". Error: " << m_udpSocket->error();
    else
        qDebug() << "AISMod::openUDP: Listening for messages on " << settings.m_udpAddress << ":" << settings.m_udpPort;
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &AISMod::udpRx);
}

void AISMod::closeUDP()
{
    if (m_udpSocket != nullptr)
    {
        disconnect(m_udpSocket, &QUdpSocket::readyRead, this, &AISMod::udpRx);
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

void AISMod::udpRx()
{
    while (m_udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        MsgTXPacketBytes *msg = MsgTXPacketBytes::create(datagram.data());
        m_basebandSource->getInputMessageQueue()->push(msg);
    }
}
