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

#include <QTime>
#include <QDebug>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGChirpChatModReport.h"

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

#include "chirpchatmodbaseband.h"
#include "chirpchatmod.h"

MESSAGE_CLASS_DEFINITION(ChirpChatMod::MsgConfigureChirpChatMod, Message)
MESSAGE_CLASS_DEFINITION(ChirpChatMod::MsgReportPayloadTime, Message)

const char* const ChirpChatMod::m_channelIdURI = "sdrangel.channeltx.modchirpchat";
const char* const ChirpChatMod::m_channelId = "ChirpChatMod";

ChirpChatMod::ChirpChatMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
	m_deviceAPI(deviceAPI),
    m_currentPayloadTime(0.0),
	m_settingsMutex(QMutex::Recursive),
	m_sampleRate(48000),
        m_udpSocket(nullptr)
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new ChirpChatModBaseband();
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &ChirpChatMod::networkManagerFinished
    );
}

ChirpChatMod::~ChirpChatMod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &ChirpChatMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void ChirpChatMod::setDeviceAPI(DeviceAPI *deviceAPI)
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

void ChirpChatMod::start()
{
	qDebug("ChirpChatMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void ChirpChatMod::stop()
{
    qDebug("ChirpChatMod::stop");
	m_thread->exit();
	m_thread->wait();
}

void ChirpChatMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool ChirpChatMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureChirpChatMod::match(cmd))
    {
        MsgConfigureChirpChatMod& cfg = (MsgConfigureChirpChatMod&) cmd;
        qDebug() << "ChirpChatMod::handleMessage: MsgConfigureChirpChatMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "ChirpChatMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);

        // Forward to the GUI
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
	else
	{
		return false;
	}
}

void ChirpChatMod::setCenterFrequency(qint64 frequency)
{
    ChirpChatModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureChirpChatMod *msgToGUI = MsgConfigureChirpChatMod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void ChirpChatMod::applySettings(const ChirpChatModSettings& settings, bool force)
{
    qDebug() << "ChirpChatMod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_bandwidthIndex
            << " bandwidth: " << ChirpChatModSettings::bandwidths[settings.m_bandwidthIndex]
            << " m_channelMute: " << settings.m_channelMute
            << " m_beaconMessage: " << settings.m_beaconMessage
            << " m_cqMessage: " << settings.m_cqMessage
            << " m_replyMessage: " << settings.m_replyMessage
            << " m_reportMessage:" << settings.m_reportMessage
            << " m_replyReportMessage: " << settings.m_replyReportMessage
            << " m_rrrMessage: " << settings.m_rrrMessage
            << " m_73message: " << settings.m_73Message
            << " m_qsoTextMessage: " << settings.m_qsoTextMessage
            << " m_textMessage: " << settings.m_textMessage
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
    if ((settings.m_bandwidthIndex != m_settings.m_bandwidthIndex) || force) {
        reverseAPIKeys.append("bandwidthIndex");
    }
    if ((settings.m_channelMute != m_settings.m_channelMute) || force) {
        reverseAPIKeys.append("channelMute");
    }
    if ((settings.m_spreadFactor != m_settings.m_spreadFactor) || force) {
        reverseAPIKeys.append("spreadFactor");
    }
    if ((settings.m_deBits != m_settings.m_deBits) || force) {
        reverseAPIKeys.append("deBits");
    }

    if ((settings.m_spreadFactor != m_settings.m_spreadFactor)
     || (settings.m_deBits != m_settings.m_deBits) || force) {
        m_encoder.setNbSymbolBits(settings.m_spreadFactor, settings.m_deBits);
    }

    if ((settings.m_codingScheme != m_settings.m_codingScheme) || force)
    {
        reverseAPIKeys.append("codingScheme");
        m_encoder.setCodingScheme(settings.m_codingScheme);
    }

    if ((settings.m_nbParityBits != m_settings.m_nbParityBits || force))
    {
        reverseAPIKeys.append("nbParityBits");
        m_encoder.setLoRaParityBits(settings.m_nbParityBits);
    }

    if ((settings.m_hasCRC != m_settings.m_hasCRC) || force)
    {
        reverseAPIKeys.append("hasCRC");
        m_encoder.setLoRaHasCRC(settings.m_hasCRC);
    }

    if ((settings.m_hasHeader != m_settings.m_hasHeader) || force)
    {
        reverseAPIKeys.append("hasHeader");
        m_encoder.setLoRaHasHeader(settings.m_hasHeader);
    }

    if ((settings.m_messageType != m_settings.m_messageType) || force) {
        reverseAPIKeys.append("messageType");
    }
    if ((settings.m_beaconMessage != m_settings.m_beaconMessage) || force) {
        reverseAPIKeys.append("beaconMessage");
    }
    if ((settings.m_cqMessage != m_settings.m_cqMessage) || force) {
        reverseAPIKeys.append("cqMessage");
    }
    if ((settings.m_replyMessage != m_settings.m_replyMessage) || force) {
        reverseAPIKeys.append("replyMessage");
    }
    if ((settings.m_reportMessage != m_settings.m_reportMessage) || force) {
        reverseAPIKeys.append("reportMessage");
    }
    if ((settings.m_replyReportMessage != m_settings.m_replyReportMessage) || force) {
        reverseAPIKeys.append("replyReportMessage");
    }
    if ((settings.m_rrrMessage != m_settings.m_rrrMessage) || force) {
        reverseAPIKeys.append("rrrMessage");
    }
    if ((settings.m_73Message != m_settings.m_73Message) || force) {
        reverseAPIKeys.append("73Message");
    }
    if ((settings.m_qsoTextMessage != m_settings.m_qsoTextMessage) || force) {
        reverseAPIKeys.append("qsoTextMessage");
    }
    if ((settings.m_textMessage != m_settings.m_textMessage) || force) {
        reverseAPIKeys.append("textMessage");
    }
    if ((settings.m_bytesMessage != m_settings.m_bytesMessage) || force) {
        reverseAPIKeys.append("bytesMessage");
    }

    ChirpChatModBaseband::MsgConfigureChirpChatModPayload *payloadMsg = nullptr;
    std::vector<unsigned short> symbols;

    if ((settings.m_messageType == ChirpChatModSettings::MessageNone)
        && ((settings.m_messageType != m_settings.m_messageType) || force))
    {
        payloadMsg = ChirpChatModBaseband::MsgConfigureChirpChatModPayload::create();
    }
    else if ((settings.m_messageType == ChirpChatModSettings::MessageBeacon)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_beaconMessage != m_settings.m_beaconMessage) || force))
    {
        m_encoder.encodeString(settings.m_beaconMessage, symbols);
        payloadMsg = ChirpChatModBaseband::MsgConfigureChirpChatModPayload::create(symbols);
    }
    else if ((settings.m_messageType == ChirpChatModSettings::MessageCQ)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_cqMessage != m_settings.m_cqMessage) || force))
    {
        m_encoder.encodeString(settings.m_cqMessage, symbols);
        payloadMsg = ChirpChatModBaseband::MsgConfigureChirpChatModPayload::create(symbols);
    }
    else if ((settings.m_messageType == ChirpChatModSettings::MessageReply)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_replyMessage != m_settings.m_replyMessage) || force))
    {
        m_encoder.encodeString(settings.m_replyMessage, symbols);
        payloadMsg = ChirpChatModBaseband::MsgConfigureChirpChatModPayload::create(symbols);
    }
    else if ((settings.m_messageType == ChirpChatModSettings::MessageReport)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_reportMessage != m_settings.m_reportMessage) || force))
    {
        m_encoder.encodeString(settings.m_reportMessage, symbols);
        payloadMsg = ChirpChatModBaseband::MsgConfigureChirpChatModPayload::create(symbols);
    }
    else if ((settings.m_messageType == ChirpChatModSettings::MessageReplyReport)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_replyReportMessage != m_settings.m_replyReportMessage) || force))
    {
        m_encoder.encodeString(settings.m_replyReportMessage, symbols);
        payloadMsg = ChirpChatModBaseband::MsgConfigureChirpChatModPayload::create(symbols);
    }
    else if ((settings.m_messageType == ChirpChatModSettings::MessageRRR)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_rrrMessage != m_settings.m_rrrMessage) || force))
    {
        m_encoder.encodeString(settings.m_rrrMessage, symbols);
        payloadMsg = ChirpChatModBaseband::MsgConfigureChirpChatModPayload::create(symbols);
    }
    else if ((settings.m_messageType == ChirpChatModSettings::Message73)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_73Message != m_settings.m_73Message) || force))
    {
        m_encoder.encodeString(settings.m_73Message, symbols);
        payloadMsg = ChirpChatModBaseband::MsgConfigureChirpChatModPayload::create(symbols);
    }
    else if ((settings.m_messageType == ChirpChatModSettings::MessageQSOText)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_qsoTextMessage != m_settings.m_qsoTextMessage) || force))
    {
        m_encoder.encodeString(settings.m_qsoTextMessage, symbols);
        payloadMsg = ChirpChatModBaseband::MsgConfigureChirpChatModPayload::create(symbols);
    }
    else if ((settings.m_messageType == ChirpChatModSettings::MessageText)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_textMessage != m_settings.m_textMessage) || force))
    {
        m_encoder.encodeString(settings.m_textMessage, symbols);
        payloadMsg = ChirpChatModBaseband::MsgConfigureChirpChatModPayload::create(symbols);
    }
    else if ((settings.m_messageType == ChirpChatModSettings::MessageBytes)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_bytesMessage != m_settings.m_bytesMessage) || force))
    {
        m_encoder.encodeBytes(settings.m_bytesMessage, symbols);
        payloadMsg = ChirpChatModBaseband::MsgConfigureChirpChatModPayload::create(symbols);
    }

    if (payloadMsg)
    {
        m_basebandSource->getInputMessageQueue()->push(payloadMsg);
        m_currentPayloadTime = (symbols.size()*(1<<settings.m_spreadFactor)*1000.0) / ChirpChatModSettings::bandwidths[settings.m_bandwidthIndex];

        if (getMessageQueueToGUI())
        {
            MsgReportPayloadTime *rpt = MsgReportPayloadTime::create(m_currentPayloadTime);
            getMessageQueueToGUI()->push(rpt);
        }
    }

    if ((settings.m_messageRepeat != m_settings.m_messageRepeat) || force) {
        reverseAPIKeys.append("messageRepeat");
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

    ChirpChatModBaseband::MsgConfigureChirpChatModBaseband *msg =
        ChirpChatModBaseband::MsgConfigureChirpChatModBaseband::create(settings, force);
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

QByteArray ChirpChatMod::serialize() const
{
    return m_settings.serialize();
}

bool ChirpChatMod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureChirpChatMod *msg = MsgConfigureChirpChatMod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

int ChirpChatMod::webapiSettingsGet(
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setChirpChatModSettings(new SWGSDRangel::SWGChirpChatModSettings());
    response.getChirpChatModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int ChirpChatMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int ChirpChatMod::webapiSettingsPutPatch(
    bool force,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    ChirpChatModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureChirpChatMod *msg = MsgConfigureChirpChatMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureChirpChatMod *msgToGUI = MsgConfigureChirpChatMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void ChirpChatMod::webapiUpdateChannelSettings(
        ChirpChatModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getChirpChatModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("bandwidthIndex")) {
        settings.m_bandwidthIndex = response.getChirpChatModSettings()->getBandwidthIndex();
    }
    if (channelSettingsKeys.contains("spreadFactor")) {
        settings.m_spreadFactor = response.getChirpChatModSettings()->getSpreadFactor();
    }
    if (channelSettingsKeys.contains("deBits")) {
        settings.m_deBits = response.getChirpChatModSettings()->getDeBits();
    }
    if (channelSettingsKeys.contains("preambleChirps")) {
        settings.m_preambleChirps = response.getChirpChatModSettings()->getPreambleChirps();
    }
    if (channelSettingsKeys.contains("quietMillis")) {
        settings.m_quietMillis = response.getChirpChatModSettings()->getQuietMillis();
    }
    if (channelSettingsKeys.contains("syncWord")) {
        settings.m_syncWord = response.getChirpChatModSettings()->getSyncWord();
    }
    if (channelSettingsKeys.contains("syncWord")) {
        settings.m_syncWord = response.getChirpChatModSettings()->getSyncWord();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getChirpChatModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("codingScheme")) {
        settings.m_codingScheme = (ChirpChatModSettings::CodingScheme) response.getChirpChatModSettings()->getCodingScheme();
    }
    if (channelSettingsKeys.contains("nbParityBits")) {
        settings.m_nbParityBits = response.getChirpChatModSettings()->getNbParityBits();
    }
    if (channelSettingsKeys.contains("hasCRC")) {
        settings.m_hasCRC = response.getChirpChatModSettings()->getHasCrc() != 0;
    }
    if (channelSettingsKeys.contains("hasHeader")) {
        settings.m_hasHeader = response.getChirpChatModSettings()->getHasHeader() != 0;
    }
    if (channelSettingsKeys.contains("myCall")) {
        settings.m_myCall = *response.getChirpChatModSettings()->getMyCall();
    }
    if (channelSettingsKeys.contains("urCall")) {
        settings.m_urCall = *response.getChirpChatModSettings()->getUrCall();
    }
    if (channelSettingsKeys.contains("myLoc")) {
        settings.m_myLoc = *response.getChirpChatModSettings()->getMyLoc();
    }
    if (channelSettingsKeys.contains("myRpt")) {
        settings.m_myRpt = *response.getChirpChatModSettings()->getMyRpt();
    }
    if (channelSettingsKeys.contains("messageType")) {
        settings.m_messageType = (ChirpChatModSettings::MessageType) response.getChirpChatModSettings()->getMessageType();
    }
    if (channelSettingsKeys.contains("beaconMessage")) {
        settings.m_beaconMessage = *response.getChirpChatModSettings()->getBeaconMessage();
    }
    if (channelSettingsKeys.contains("cqMessage")) {
        settings.m_cqMessage = *response.getChirpChatModSettings()->getCqMessage();
    }
    if (channelSettingsKeys.contains("replyMessage")) {
        settings.m_replyMessage = *response.getChirpChatModSettings()->getReplyMessage();
    }
    if (channelSettingsKeys.contains("reportMessage")) {
        settings.m_reportMessage = *response.getChirpChatModSettings()->getReportMessage();
    }
    if (channelSettingsKeys.contains("replyReportMessage")) {
        settings.m_replyReportMessage = *response.getChirpChatModSettings()->getReplyReportMessage();
    }
    if (channelSettingsKeys.contains("rrrMessage")) {
        settings.m_rrrMessage = *response.getChirpChatModSettings()->getRrrMessage();
    }
    if (channelSettingsKeys.contains("message73")) {
        settings.m_73Message = *response.getChirpChatModSettings()->getMessage73();
    }
    if (channelSettingsKeys.contains("qsoTextMessage")) {
        settings.m_qsoTextMessage = *response.getChirpChatModSettings()->getQsoTextMessage();
    }
    if (channelSettingsKeys.contains("textMessage")) {
        settings.m_textMessage = *response.getChirpChatModSettings()->getTextMessage();
    }
    if (channelSettingsKeys.contains("bytesMessage"))
    {
        const QList<QString *> *bytesStr = response.getChirpChatModSettings()->getBytesMessage();
        settings.m_bytesMessage.clear();

        for (QList<QString *>::const_iterator it = bytesStr->begin(); it != bytesStr->end(); ++it)
        {
            bool bStatus = false;
            unsigned int byteInt = (**it).toUInt(&bStatus, 16);

            if (bStatus) {
                settings.m_bytesMessage.append((char) (byteInt % 256));
            }
        }
    }
    if (channelSettingsKeys.contains("messageRepeat")) {
        settings.m_messageRepeat = response.getChirpChatModSettings()->getMessageRepeat();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getChirpChatModSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getChirpChatModSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getChirpChatModSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getChirpChatModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getChirpChatModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getChirpChatModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getChirpChatModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getChirpChatModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getChirpChatModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getChirpChatModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getChirpChatModSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getChirpChatModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getChirpChatModSettings()->getChannelMarker());
    }
}

int ChirpChatMod::webapiReportGet(
    SWGSDRangel::SWGChannelReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setChirpChatModReport(new SWGSDRangel::SWGChirpChatModReport());
    response.getChirpChatModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void ChirpChatMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const ChirpChatModSettings& settings)
{
    response.getChirpChatModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getChirpChatModSettings()->setBandwidthIndex(settings.m_bandwidthIndex);
    response.getChirpChatModSettings()->setSpreadFactor(settings.m_spreadFactor);
    response.getChirpChatModSettings()->setDeBits(settings.m_deBits);
    response.getChirpChatModSettings()->setPreambleChirps(settings.m_preambleChirps);
    response.getChirpChatModSettings()->setQuietMillis(settings.m_quietMillis);
    response.getChirpChatModSettings()->setSyncWord(settings.m_syncWord);
    response.getChirpChatModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getChirpChatModSettings()->setCodingScheme((int) settings.m_codingScheme);
    response.getChirpChatModSettings()->setNbParityBits(settings.m_nbParityBits);
    response.getChirpChatModSettings()->setHasCrc(settings.m_hasCRC ? 1 : 0);
    response.getChirpChatModSettings()->setHasHeader(settings.m_hasHeader ? 1 : 0);

    if (response.getChirpChatModSettings()->getMyCall()) {
        *response.getChirpChatModSettings()->getMyCall() = settings.m_myCall;
    } else {
        response.getChirpChatModSettings()->setMyCall(new QString(settings.m_myCall));
    }

    if (response.getChirpChatModSettings()->getUrCall()) {
        *response.getChirpChatModSettings()->getUrCall() = settings.m_urCall;
    } else {
        response.getChirpChatModSettings()->setUrCall(new QString(settings.m_urCall));
    }

    if (response.getChirpChatModSettings()->getMyLoc()) {
        *response.getChirpChatModSettings()->getMyLoc() = settings.m_myLoc;
    } else {
        response.getChirpChatModSettings()->setMyLoc(new QString(settings.m_myLoc));
    }

    if (response.getChirpChatModSettings()->getMyRpt()) {
        *response.getChirpChatModSettings()->getMyRpt() = settings.m_myRpt;
    } else {
        response.getChirpChatModSettings()->setMyRpt(new QString(settings.m_myRpt));
    }

    response.getChirpChatModSettings()->setMessageType((int) settings.m_messageType);

    if (response.getChirpChatModSettings()->getBeaconMessage()) {
        *response.getChirpChatModSettings()->getBeaconMessage() = settings.m_beaconMessage;
    } else {
        response.getChirpChatModSettings()->setBeaconMessage(new QString(settings.m_beaconMessage));
    }

    if (response.getChirpChatModSettings()->getCqMessage()) {
        *response.getChirpChatModSettings()->getCqMessage() = settings.m_cqMessage;
    } else {
        response.getChirpChatModSettings()->setCqMessage(new QString(settings.m_cqMessage));
    }

    if (response.getChirpChatModSettings()->getReplyMessage()) {
        *response.getChirpChatModSettings()->getReplyMessage() = settings.m_replyMessage;
    } else {
        response.getChirpChatModSettings()->setReplyMessage(new QString(settings.m_replyMessage));
    }

    if (response.getChirpChatModSettings()->getReportMessage()) {
        *response.getChirpChatModSettings()->getReportMessage() = settings.m_reportMessage;
    } else {
        response.getChirpChatModSettings()->setReportMessage(new QString(settings.m_reportMessage));
    }

    if (response.getChirpChatModSettings()->getReplyReportMessage()) {
        *response.getChirpChatModSettings()->getReplyReportMessage() = settings.m_replyReportMessage;
    } else {
        response.getChirpChatModSettings()->setReplyReportMessage(new QString(settings.m_replyReportMessage));
    }

    if (response.getChirpChatModSettings()->getRrrMessage()) {
        *response.getChirpChatModSettings()->getRrrMessage() = settings.m_rrrMessage;
    } else {
        response.getChirpChatModSettings()->setRrrMessage(new QString(settings.m_rrrMessage));
    }

    if (response.getChirpChatModSettings()->getMessage73()) {
        *response.getChirpChatModSettings()->getMessage73() = settings.m_73Message;
    } else {
        response.getChirpChatModSettings()->setMessage73(new QString(settings.m_73Message));
    }

    if (response.getChirpChatModSettings()->getQsoTextMessage()) {
        *response.getChirpChatModSettings()->getQsoTextMessage() = settings.m_qsoTextMessage;
    } else {
        response.getChirpChatModSettings()->setQsoTextMessage(new QString(settings.m_qsoTextMessage));
    }

    if (response.getChirpChatModSettings()->getTextMessage()) {
        *response.getChirpChatModSettings()->getTextMessage() = settings.m_textMessage;
    } else {
        response.getChirpChatModSettings()->setTextMessage(new QString(settings.m_textMessage));
    }

    response.getChirpChatModSettings()->setBytesMessage(new QList<QString *>);
    QList<QString *> *bytesStr = response.getChirpChatModSettings()->getBytesMessage();

    for (QByteArray::const_iterator it = settings.m_bytesMessage.begin(); it != settings.m_bytesMessage.end(); ++it)
    {
        unsigned char b = *it;
        bytesStr->push_back(new QString(tr("%1").arg(b, 2, 16, QChar('0'))));
    }

    response.getChirpChatModSettings()->setMessageRepeat(settings.m_messageRepeat);
    response.getChirpChatModSettings()->setUdpEnabled(settings.m_udpEnabled);
    response.getChirpChatModSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getChirpChatModSettings()->setUdpPort(settings.m_udpPort);

    response.getChirpChatModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getChirpChatModSettings()->getTitle()) {
        *response.getChirpChatModSettings()->getTitle() = settings.m_title;
    } else {
        response.getChirpChatModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getChirpChatModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getChirpChatModSettings()->getReverseApiAddress()) {
        *response.getChirpChatModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getChirpChatModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getChirpChatModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getChirpChatModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getChirpChatModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getChirpChatModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getChirpChatModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getChirpChatModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getChirpChatModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getChirpChatModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getChirpChatModSettings()->setRollupState(swgRollupState);
        }
    }
}

void ChirpChatMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getChirpChatModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getChirpChatModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
    float fourthsMs = ((1<<m_settings.m_spreadFactor) * 250.0) / ChirpChatModSettings::bandwidths[m_settings.m_bandwidthIndex];
    float controlMs = (4*m_settings.m_preambleChirps + 8 + 9) * fourthsMs; // preamble + sync word + SFD
    response.getChirpChatModReport()->setPayloadTimeMs(m_currentPayloadTime);
    response.getChirpChatModReport()->setTotalTimeMs(m_currentPayloadTime + controlMs);
    response.getChirpChatModReport()->setSymbolTimeMs(4.0 * fourthsMs);
    response.getChirpChatModReport()->setPlaying(getModulatorActive() ? 1 : 0);
}

void ChirpChatMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const ChirpChatModSettings& settings, bool force)
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

void ChirpChatMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const ChirpChatModSettings& settings,
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

void ChirpChatMod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const ChirpChatModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setChirpChatModSettings(new SWGSDRangel::SWGChirpChatModSettings());
    SWGSDRangel::SWGChirpChatModSettings *swgChirpChatModSettings = swgChannelSettings->getChirpChatModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgChirpChatModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("bandwidthIndex") || force) {
        swgChirpChatModSettings->setBandwidthIndex(settings.m_bandwidthIndex);
    }
    if (channelSettingsKeys.contains("spreadFactor") || force) {
        swgChirpChatModSettings->setSpreadFactor(settings.m_spreadFactor);
    }
    if (channelSettingsKeys.contains("deBits") || force) {
        swgChirpChatModSettings->setDeBits(settings.m_deBits);
    }
    if (channelSettingsKeys.contains("preambleChirps") || force) {
        swgChirpChatModSettings->setPreambleChirps(settings.m_preambleChirps);
    }
    if (channelSettingsKeys.contains("quietMillis") || force) {
        swgChirpChatModSettings->setQuietMillis(settings.m_quietMillis);
    }
    if (channelSettingsKeys.contains("syncWord") || force) {
        swgChirpChatModSettings->setSyncWord(settings.m_syncWord);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgChirpChatModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("codingScheme") || force) {
        swgChirpChatModSettings->setCodingScheme((int) settings.m_codingScheme);
    }
    if (channelSettingsKeys.contains("nbParityBits") || force) {
        swgChirpChatModSettings->setNbParityBits(settings.m_nbParityBits);
    }
    if (channelSettingsKeys.contains("hasCRC") || force) {
        swgChirpChatModSettings->setHasCrc(settings.m_hasCRC ? 1 : 0);
    }
    if (channelSettingsKeys.contains("hasHeader") || force) {
        swgChirpChatModSettings->setHasHeader(settings.m_hasHeader ? 1 : 0);
    }
    if (channelSettingsKeys.contains("myCall") || force) {
        swgChirpChatModSettings->setMyCall(new QString(settings.m_myCall));
    }
    if (channelSettingsKeys.contains("urCall") || force) {
        swgChirpChatModSettings->setUrCall(new QString(settings.m_urCall));
    }
    if (channelSettingsKeys.contains("myLoc") || force) {
        swgChirpChatModSettings->setMyLoc(new QString(settings.m_myLoc));
    }
    if (channelSettingsKeys.contains("myRpt") || force) {
        swgChirpChatModSettings->setMyRpt(new QString(settings.m_myRpt));
    }
    if (channelSettingsKeys.contains("messageType") || force) {
        swgChirpChatModSettings->setMessageType((int) settings.m_messageType);
    }
    if (channelSettingsKeys.contains("beaconMessage") || force) {
        swgChirpChatModSettings->setBeaconMessage(new QString(settings.m_beaconMessage));
    }
    if (channelSettingsKeys.contains("cqMessage") || force) {
        swgChirpChatModSettings->setCqMessage(new QString(settings.m_cqMessage));
    }
    if (channelSettingsKeys.contains("replyMessage") || force) {
        swgChirpChatModSettings->setReplyMessage(new QString(settings.m_replyMessage));
    }
    if (channelSettingsKeys.contains("reportMessage") || force) {
        swgChirpChatModSettings->setReportMessage(new QString(settings.m_reportMessage));
    }
    if (channelSettingsKeys.contains("replyReportMessage") || force) {
        swgChirpChatModSettings->setReplyReportMessage(new QString(settings.m_replyReportMessage));
    }
    if (channelSettingsKeys.contains("rrrMessage") || force) {
        swgChirpChatModSettings->setRrrMessage(new QString(settings.m_rrrMessage));
    }
    if (channelSettingsKeys.contains("message73") || force) {
        swgChirpChatModSettings->setMessage73(new QString(settings.m_73Message));
    }
    if (channelSettingsKeys.contains("qsoTextMessage") || force) {
        swgChirpChatModSettings->setQsoTextMessage(new QString(settings.m_qsoTextMessage));
    }
    if (channelSettingsKeys.contains("textMessage") || force) {
        swgChirpChatModSettings->setTextMessage(new QString(settings.m_textMessage));
    }

    if (channelSettingsKeys.contains("bytesMessage") || force)
    {
        swgChirpChatModSettings->setBytesMessage(new QList<QString *>);
        QList<QString *> *bytesStr = swgChirpChatModSettings-> getBytesMessage();

        for (QByteArray::const_iterator it = settings.m_bytesMessage.begin(); it != settings.m_bytesMessage.end(); ++it)
        {
            unsigned char b = *it;
            bytesStr->push_back(new QString(tr("%1").arg(b, 2, 16, QChar('0'))));
        }
    }

    if (channelSettingsKeys.contains("messageRepeat") || force) {
        swgChirpChatModSettings->setMessageRepeat(settings.m_messageRepeat);
    }

    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgChirpChatModSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgChirpChatModSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgChirpChatModSettings->setUdpPort(settings.m_udpPort);
    }

    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgChirpChatModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgChirpChatModSettings->setTitle(new QString(settings.m_title));
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgChirpChatModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgChirpChatModSettings->setRollupState(swgRollupState);
    }
}

void ChirpChatMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "ChirpChatMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("ChirpChatMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double ChirpChatMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void ChirpChatMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t ChirpChatMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

bool ChirpChatMod::getModulatorActive() const
{
    return m_basebandSource->getActive();
}

void ChirpChatMod::openUDP(const ChirpChatModSettings& settings)
{
    closeUDP();
    m_udpSocket = new QUdpSocket();
    if (!m_udpSocket->bind(QHostAddress(settings.m_udpAddress), settings.m_udpPort))
        qCritical() << "ChirpChatMod::openUDP: Failed to bind to port " << settings.m_udpAddress << ":" << settings.m_udpPort << ". Error: " << m_udpSocket->error();
    else
        qDebug() << "ChirpChatMod::openUDP: Listening for packets on " << settings.m_udpAddress << ":" << settings.m_udpPort;
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &ChirpChatMod::udpRx);
}

void ChirpChatMod::closeUDP()
{
    if (m_udpSocket != nullptr)
    {
        disconnect(m_udpSocket, &QUdpSocket::readyRead, this, &ChirpChatMod::udpRx);
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

void ChirpChatMod::udpRx()
{
    while (m_udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        ChirpChatModBaseband::MsgConfigureChirpChatModPayload *payloadMsg = nullptr;
        std::vector<unsigned short> symbols;

        m_encoder.encodeBytes(datagram.data(), symbols);
        payloadMsg = ChirpChatModBaseband::MsgConfigureChirpChatModPayload::create(symbols);

        if (payloadMsg)
        {
            m_basebandSource->getInputMessageQueue()->push(payloadMsg);
            m_currentPayloadTime = (symbols.size()*(1<<m_settings.m_spreadFactor)*1000.0) / ChirpChatModSettings::bandwidths[m_settings.m_bandwidthIndex];

            if (getMessageQueueToGUI())
            {
                MsgReportPayloadTime *rpt = MsgReportPayloadTime::create(m_currentPayloadTime);
                getMessageQueueToGUI()->push(rpt);
            }
        }
    }
}
