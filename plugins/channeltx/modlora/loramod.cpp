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
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGLoRaModReport.h"

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "util/db.h"

#include "loramodbaseband.h"
#include "loramod.h"

MESSAGE_CLASS_DEFINITION(LoRaMod::MsgConfigureLoRaMod, Message)
MESSAGE_CLASS_DEFINITION(LoRaMod::MsgReportPayloadTime, Message)

const QString LoRaMod::m_channelIdURI = "sdrangel.channeltx.modlora";
const QString LoRaMod::m_channelId = "LoRaMod";

LoRaMod::LoRaMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
	m_deviceAPI(deviceAPI),
    m_currentPayloadTime(0.0),
	m_settingsMutex(QMutex::Recursive),
	m_sampleRate(48000)
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new LoRaModBaseband();
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

LoRaMod::~LoRaMod()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void LoRaMod::start()
{
	qDebug("LoRaMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void LoRaMod::stop()
{
    qDebug("LoRaMod::stop");
	m_thread->exit();
	m_thread->wait();
}

void LoRaMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool LoRaMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureLoRaMod::match(cmd))
    {
        MsgConfigureLoRaMod& cfg = (MsgConfigureLoRaMod&) cmd;
        qDebug() << "LoRaMod::handleMessage: MsgConfigureLoRaMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "LoRaMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);

        // Forward to the GUI
        if (getMessageQueueToGUI())
        {
            DSPSignalNotification* repToGUI = new DSPSignalNotification(notif); // make a copy
            getMessageQueueToGUI()->push(repToGUI);
        }

        return true;
    }
	else
	{
		return false;
	}
}

void LoRaMod::applySettings(const LoRaModSettings& settings, bool force)
{
    qDebug() << "LoRaMod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_bandwidthIndex
            << " bandwidth: " << LoRaModSettings::bandwidths[settings.m_bandwidthIndex]
            << " m_channelMute: " << settings.m_channelMute
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

    LoRaModBaseband::MsgConfigureLoRaModPayload *payloadMsg = nullptr;
    std::vector<unsigned short> symbols;

    if ((settings.m_messageType == LoRaModSettings::MessageNone)
        && ((settings.m_messageType != m_settings.m_messageType) || force))
    {
        payloadMsg = LoRaModBaseband::MsgConfigureLoRaModPayload::create();
    }
    else if ((settings.m_messageType == LoRaModSettings::MessageBeacon)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_beaconMessage != m_settings.m_beaconMessage) || force))
    {
        m_encoder.encodeString(m_settings.m_beaconMessage, symbols);
        payloadMsg = LoRaModBaseband::MsgConfigureLoRaModPayload::create(symbols);
    }
    else if ((settings.m_messageType == LoRaModSettings::MessageCQ)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_cqMessage != m_settings.m_cqMessage) || force))
    {
        m_encoder.encodeString(m_settings.m_cqMessage, symbols);
        payloadMsg = LoRaModBaseband::MsgConfigureLoRaModPayload::create(symbols);
    }
    else if ((settings.m_messageType == LoRaModSettings::MessageReply)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_replyMessage != m_settings.m_replyMessage) || force))
    {
        m_encoder.encodeString(m_settings.m_replyMessage, symbols);
        payloadMsg = LoRaModBaseband::MsgConfigureLoRaModPayload::create(symbols);
    }
    else if ((settings.m_messageType == LoRaModSettings::MessageReport)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_reportMessage != m_settings.m_reportMessage) || force))
    {
        m_encoder.encodeString(m_settings.m_reportMessage, symbols);
        payloadMsg = LoRaModBaseband::MsgConfigureLoRaModPayload::create(symbols);
    }
    else if ((settings.m_messageType == LoRaModSettings::MessageReplyReport)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_replyReportMessage != m_settings.m_replyReportMessage) || force))
    {
        m_encoder.encodeString(m_settings.m_replyReportMessage, symbols);
        payloadMsg = LoRaModBaseband::MsgConfigureLoRaModPayload::create(symbols);
    }
    else if ((settings.m_messageType == LoRaModSettings::MessageRRR)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_rrrMessage != m_settings.m_rrrMessage) || force))
    {
        m_encoder.encodeString(m_settings.m_rrrMessage, symbols);
        payloadMsg = LoRaModBaseband::MsgConfigureLoRaModPayload::create(symbols);
    }
    else if ((settings.m_messageType == LoRaModSettings::Message73)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_73Message != m_settings.m_73Message) || force))
    {
        m_encoder.encodeString(m_settings.m_73Message, symbols);
        payloadMsg = LoRaModBaseband::MsgConfigureLoRaModPayload::create(symbols);
    }
    else if ((settings.m_messageType == LoRaModSettings::MessageQSOText)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_qsoTextMessage != m_settings.m_qsoTextMessage) || force))
    {
        m_encoder.encodeString(m_settings.m_qsoTextMessage, symbols);
        payloadMsg = LoRaModBaseband::MsgConfigureLoRaModPayload::create(symbols);
    }
    else if ((settings.m_messageType == LoRaModSettings::MessageText)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_textMessage != m_settings.m_textMessage) || force))
    {
        m_encoder.encodeString(m_settings.m_textMessage, symbols);
        payloadMsg = LoRaModBaseband::MsgConfigureLoRaModPayload::create(symbols);
    }
    else if ((settings.m_messageType == LoRaModSettings::MessageBytes)
        && ((settings.m_messageType != m_settings.m_messageType)
         || (settings.m_bytesMessage != m_settings.m_bytesMessage) || force))
    {
        m_encoder.encodeBytes(m_settings.m_bytesMessage, symbols);
        payloadMsg = LoRaModBaseband::MsgConfigureLoRaModPayload::create(symbols);
    }

    if (payloadMsg)
    {
        m_basebandSource->getInputMessageQueue()->push(payloadMsg);
        m_currentPayloadTime = (symbols.size()*(1<<settings.m_spreadFactor)*1000.0) / LoRaModSettings::bandwidths[settings.m_bandwidthIndex];

        if (getMessageQueueToGUI())
        {
            MsgReportPayloadTime *rpt = MsgReportPayloadTime::create(m_currentPayloadTime);
            getMessageQueueToGUI()->push(rpt);
        }
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

    LoRaModBaseband::MsgConfigureLoRaModBaseband *msg = LoRaModBaseband::MsgConfigureLoRaModBaseband::create(settings, force);
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

    m_settings = settings;
}

QByteArray LoRaMod::serialize() const
{
    return m_settings.serialize();
}

bool LoRaMod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureLoRaMod *msg = MsgConfigureLoRaMod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

int LoRaMod::webapiSettingsGet(
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setLoRaModSettings(new SWGSDRangel::SWGLoRaModSettings());
    response.getLoRaModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int LoRaMod::webapiSettingsPutPatch(
    bool force,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    LoRaModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureLoRaMod *msg = MsgConfigureLoRaMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureLoRaMod *msgToGUI = MsgConfigureLoRaMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void LoRaMod::webapiUpdateChannelSettings(
        LoRaModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getLoRaModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("bandwidthIndex")) {
        settings.m_bandwidthIndex = response.getLoRaModSettings()->getBandwidthIndex();
    }
    if (channelSettingsKeys.contains("spreadFactor")) {
        settings.m_spreadFactor = response.getLoRaModSettings()->getSpreadFactor();
    }
    if (channelSettingsKeys.contains("deBits")) {
        settings.m_deBits = response.getLoRaModSettings()->getDeBits();
    }
    if (channelSettingsKeys.contains("preambleChirps")) {
        settings.m_preambleChirps = response.getLoRaModSettings()->getPreambleChirps();
    }
    if (channelSettingsKeys.contains("quietMillis")) {
        settings.m_quietMillis = response.getLoRaModSettings()->getQuietMillis();
    }
    if (channelSettingsKeys.contains("syncWord")) {
        settings.m_syncWord = response.getLoRaModSettings()->getSyncWord();
    }
    if (channelSettingsKeys.contains("syncWord")) {
        settings.m_syncWord = response.getLoRaModSettings()->getSyncWord();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getLoRaModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("codingScheme")) {
        settings.m_codingScheme = (LoRaModSettings::CodingScheme) response.getLoRaModSettings()->getCodingScheme();
    }
    if (channelSettingsKeys.contains("nbParityBits")) {
        settings.m_nbParityBits = response.getLoRaModSettings()->getNbParityBits();
    }
    if (channelSettingsKeys.contains("hasCRC")) {
        settings.m_hasCRC = response.getLoRaModSettings()->getHasCrc() != 0;
    }
    if (channelSettingsKeys.contains("hasHeader")) {
        settings.m_hasHeader = response.getLoRaModSettings()->getHasHeader() != 0;
    }
    if (channelSettingsKeys.contains("myCall")) {
        settings.m_myCall = *response.getLoRaModSettings()->getMyCall();
    }
    if (channelSettingsKeys.contains("urCall")) {
        settings.m_urCall = *response.getLoRaModSettings()->getUrCall();
    }
    if (channelSettingsKeys.contains("myLoc")) {
        settings.m_myLoc = *response.getLoRaModSettings()->getMyLoc();
    }
    if (channelSettingsKeys.contains("myRpt")) {
        settings.m_myRpt = *response.getLoRaModSettings()->getMyRpt();
    }
    if (channelSettingsKeys.contains("messageType")) {
        settings.m_messageType = (LoRaModSettings::MessageType) response.getLoRaModSettings()->getMessageType();
    }
    if (channelSettingsKeys.contains("beaconMessage")) {
        settings.m_beaconMessage = *response.getLoRaModSettings()->getBeaconMessage();
    }
    if (channelSettingsKeys.contains("cqMessage")) {
        settings.m_cqMessage = *response.getLoRaModSettings()->getCqMessage();
    }
    if (channelSettingsKeys.contains("replyMessage")) {
        settings.m_replyMessage = *response.getLoRaModSettings()->getReplyMessage();
    }
    if (channelSettingsKeys.contains("reportMessage")) {
        settings.m_reportMessage = *response.getLoRaModSettings()->getReportMessage();
    }
    if (channelSettingsKeys.contains("replyReportMessage")) {
        settings.m_replyReportMessage = *response.getLoRaModSettings()->getReplyReportMessage();
    }
    if (channelSettingsKeys.contains("rrrMessage")) {
        settings.m_rrrMessage = *response.getLoRaModSettings()->getRrrMessage();
    }
    if (channelSettingsKeys.contains("message73")) {
        settings.m_73Message = *response.getLoRaModSettings()->getMessage73();
    }
    if (channelSettingsKeys.contains("qsoTextMessage")) {
        settings.m_qsoTextMessage = *response.getLoRaModSettings()->getQsoTextMessage();
    }
    if (channelSettingsKeys.contains("textMessage")) {
        settings.m_textMessage = *response.getLoRaModSettings()->getTextMessage();
    }
    if (channelSettingsKeys.contains("bytesMessage"))
    {
        const QList<QString *> *bytesStr = response.getLoRaModSettings()->getBytesMessage();
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
        settings.m_messageRepeat = response.getLoRaModSettings()->getMessageRepeat();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getLoRaModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getLoRaModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getLoRaModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getLoRaModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getLoRaModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getLoRaModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getLoRaModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getLoRaModSettings()->getReverseApiChannelIndex();
    }
}

int LoRaMod::webapiReportGet(
    SWGSDRangel::SWGChannelReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setLoRaModReport(new SWGSDRangel::SWGLoRaModReport());
    response.getLoRaModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void LoRaMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const LoRaModSettings& settings)
{
    response.getLoRaModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getLoRaModSettings()->setBandwidthIndex(settings.m_bandwidthIndex);
    response.getLoRaModSettings()->setSpreadFactor(settings.m_spreadFactor);
    response.getLoRaModSettings()->setDeBits(settings.m_deBits);
    response.getLoRaModSettings()->setPreambleChirps(settings.m_preambleChirps);
    response.getLoRaModSettings()->setQuietMillis(settings.m_quietMillis);
    response.getLoRaModSettings()->setSyncWord(settings.m_syncWord);
    response.getLoRaModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getLoRaModSettings()->setCodingScheme((int) settings.m_codingScheme);
    response.getLoRaModSettings()->setNbParityBits(settings.m_nbParityBits);
    response.getLoRaModSettings()->setHasCrc(settings.m_hasCRC ? 1 : 0);
    response.getLoRaModSettings()->setHasHeader(settings.m_hasHeader ? 1 : 0);

    if (response.getLoRaModSettings()->getMyCall()) {
        *response.getLoRaModSettings()->getMyCall() = settings.m_myCall;
    } else {
        response.getLoRaModSettings()->setMyCall(new QString(settings.m_myCall));
    }

    if (response.getLoRaModSettings()->getUrCall()) {
        *response.getLoRaModSettings()->getUrCall() = settings.m_urCall;
    } else {
        response.getLoRaModSettings()->setUrCall(new QString(settings.m_urCall));
    }

    if (response.getLoRaModSettings()->getMyLoc()) {
        *response.getLoRaModSettings()->getMyLoc() = settings.m_myLoc;
    } else {
        response.getLoRaModSettings()->setMyLoc(new QString(settings.m_myLoc));
    }

    if (response.getLoRaModSettings()->getMyRpt()) {
        *response.getLoRaModSettings()->getMyRpt() = settings.m_myRpt;
    } else {
        response.getLoRaModSettings()->setMyRpt(new QString(settings.m_myRpt));
    }

    response.getLoRaModSettings()->setMessageType((int) settings.m_messageType);

    if (response.getLoRaModSettings()->getBeaconMessage()) {
        *response.getLoRaModSettings()->getBeaconMessage() = settings.m_beaconMessage;
    } else {
        response.getLoRaModSettings()->setBeaconMessage(new QString(settings.m_beaconMessage));
    }

    if (response.getLoRaModSettings()->getCqMessage()) {
        *response.getLoRaModSettings()->getCqMessage() = settings.m_cqMessage;
    } else {
        response.getLoRaModSettings()->setCqMessage(new QString(settings.m_cqMessage));
    }

    if (response.getLoRaModSettings()->getReplyMessage()) {
        *response.getLoRaModSettings()->getReplyMessage() = settings.m_replyMessage;
    } else {
        response.getLoRaModSettings()->setReplyMessage(new QString(settings.m_replyMessage));
    }

    if (response.getLoRaModSettings()->getReportMessage()) {
        *response.getLoRaModSettings()->getReportMessage() = settings.m_reportMessage;
    } else {
        response.getLoRaModSettings()->setReportMessage(new QString(settings.m_reportMessage));
    }

    if (response.getLoRaModSettings()->getReplyReportMessage()) {
        *response.getLoRaModSettings()->getReplyReportMessage() = settings.m_replyReportMessage;
    } else {
        response.getLoRaModSettings()->setReplyReportMessage(new QString(settings.m_replyReportMessage));
    }

    if (response.getLoRaModSettings()->getRrrMessage()) {
        *response.getLoRaModSettings()->getRrrMessage() = settings.m_rrrMessage;
    } else {
        response.getLoRaModSettings()->setRrrMessage(new QString(settings.m_rrrMessage));
    }

    if (response.getLoRaModSettings()->getMessage73()) {
        *response.getLoRaModSettings()->getMessage73() = settings.m_73Message;
    } else {
        response.getLoRaModSettings()->setMessage73(new QString(settings.m_73Message));
    }

    if (response.getLoRaModSettings()->getQsoTextMessage()) {
        *response.getLoRaModSettings()->getQsoTextMessage() = settings.m_qsoTextMessage;
    } else {
        response.getLoRaModSettings()->setQsoTextMessage(new QString(settings.m_qsoTextMessage));
    }

    if (response.getLoRaModSettings()->getTextMessage()) {
        *response.getLoRaModSettings()->getTextMessage() = settings.m_textMessage;
    } else {
        response.getLoRaModSettings()->setTextMessage(new QString(settings.m_textMessage));
    }

    response.getLoRaModSettings()->setBytesMessage(new QList<QString *>);
    QList<QString *> *bytesStr = response.getLoRaModSettings()->getBytesMessage();

    for (QByteArray::const_iterator it = settings.m_bytesMessage.begin(); it != settings.m_bytesMessage.end(); ++it)
    {
        unsigned char b = *it;
        bytesStr->push_back(new QString(tr("%1").arg(b, 2, 16, QChar('0'))));
    }

    response.getLoRaModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getLoRaModSettings()->getTitle()) {
        *response.getLoRaModSettings()->getTitle() = settings.m_title;
    } else {
        response.getLoRaModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getLoRaModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getLoRaModSettings()->getReverseApiAddress()) {
        *response.getLoRaModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getLoRaModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getLoRaModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getLoRaModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getLoRaModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void LoRaMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getLoRaModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getLoRaModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
    float fourthsMs = ((1<<m_settings.m_spreadFactor) * 250.0) / LoRaModSettings::bandwidths[m_settings.m_bandwidthIndex];
    float controlMs = (4*m_settings.m_preambleChirps + 8 + 9) * fourthsMs; // preamble + sync word + SFD
    response.getLoRaModReport()->setPayloadTimeMs(m_currentPayloadTime);
    response.getLoRaModReport()->setTotalTimeMs(m_currentPayloadTime + controlMs);
    response.getLoRaModReport()->setSymbolTimeMs(4.0 * fourthsMs);
}

void LoRaMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const LoRaModSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("LoRaMod"));
    swgChannelSettings->setLoRaModSettings(new SWGSDRangel::SWGLoRaModSettings());
    SWGSDRangel::SWGLoRaModSettings *swgLoRaModSettings = swgChannelSettings->getLoRaModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgLoRaModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("bandwidthIndex") || force) {
        swgLoRaModSettings->setBandwidthIndex(settings.m_bandwidthIndex);
    }
    if (channelSettingsKeys.contains("spreadFactor") || force) {
        swgLoRaModSettings->setSpreadFactor(settings.m_spreadFactor);
    }
    if (channelSettingsKeys.contains("deBits") || force) {
        swgLoRaModSettings->setDeBits(settings.m_deBits);
    }
    if (channelSettingsKeys.contains("preambleChirps") || force) {
        swgLoRaModSettings->setPreambleChirps(settings.m_preambleChirps);
    }
    if (channelSettingsKeys.contains("quietMillis") || force) {
        swgLoRaModSettings->setQuietMillis(settings.m_quietMillis);
    }
    if (channelSettingsKeys.contains("syncWord") || force) {
        swgLoRaModSettings->setSyncWord(settings.m_syncWord);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgLoRaModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("codingScheme") || force) {
        swgLoRaModSettings->setCodingScheme((int) settings.m_codingScheme);
    }
    if (channelSettingsKeys.contains("nbParityBits") || force) {
        swgLoRaModSettings->setNbParityBits(settings.m_nbParityBits);
    }
    if (channelSettingsKeys.contains("hasCRC") || force) {
        swgLoRaModSettings->setHasCrc(settings.m_hasCRC ? 1 : 0);
    }
    if (channelSettingsKeys.contains("hasHeader") || force) {
        swgLoRaModSettings->setHasHeader(settings.m_hasHeader ? 1 : 0);
    }
    if (channelSettingsKeys.contains("myCall") || force) {
        swgLoRaModSettings->setMyCall(new QString(settings.m_myCall));
    }
    if (channelSettingsKeys.contains("urCall") || force) {
        swgLoRaModSettings->setUrCall(new QString(settings.m_urCall));
    }
    if (channelSettingsKeys.contains("myLoc") || force) {
        swgLoRaModSettings->setMyLoc(new QString(settings.m_myLoc));
    }
    if (channelSettingsKeys.contains("myRpt") || force) {
        swgLoRaModSettings->setMyRpt(new QString(settings.m_myRpt));
    }
    if (channelSettingsKeys.contains("messageType") || force) {
        swgLoRaModSettings->setMessageType((int) settings.m_messageType);
    }
    if (channelSettingsKeys.contains("beaconMessage") || force) {
        swgLoRaModSettings->setBeaconMessage(new QString(settings.m_beaconMessage));
    }
    if (channelSettingsKeys.contains("cqMessage") || force) {
        swgLoRaModSettings->setCqMessage(new QString(settings.m_cqMessage));
    }
    if (channelSettingsKeys.contains("replyMessage") || force) {
        swgLoRaModSettings->setReplyMessage(new QString(settings.m_replyMessage));
    }
    if (channelSettingsKeys.contains("reportMessage") || force) {
        swgLoRaModSettings->setReportMessage(new QString(settings.m_reportMessage));
    }
    if (channelSettingsKeys.contains("replyReportMessage") || force) {
        swgLoRaModSettings->setReplyReportMessage(new QString(settings.m_replyReportMessage));
    }
    if (channelSettingsKeys.contains("rrrMessage") || force) {
        swgLoRaModSettings->setRrrMessage(new QString(settings.m_rrrMessage));
    }
    if (channelSettingsKeys.contains("message73") || force) {
        swgLoRaModSettings->setMessage73(new QString(settings.m_73Message));
    }
    if (channelSettingsKeys.contains("qsoTextMessage") || force) {
        swgLoRaModSettings->setQsoTextMessage(new QString(settings.m_qsoTextMessage));
    }
    if (channelSettingsKeys.contains("textMessage") || force) {
        swgLoRaModSettings->setTextMessage(new QString(settings.m_textMessage));
    }

    if (channelSettingsKeys.contains("bytesMessage") || force)
    {
        swgLoRaModSettings->setBytesMessage(new QList<QString *>);
        QList<QString *> *bytesStr = swgLoRaModSettings-> getBytesMessage();

        for (QByteArray::const_iterator it = settings.m_bytesMessage.begin(); it != settings.m_bytesMessage.end(); ++it)
        {
            unsigned char b = *it;
            bytesStr->push_back(new QString(tr("%1").arg(b, 2, 16, QChar('0'))));
        }
    }

    if (channelSettingsKeys.contains("messageRepeat") || force) {
        swgLoRaModSettings->setMessageRepeat(settings.m_messageRepeat);
    }

    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgLoRaModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgLoRaModSettings->setTitle(new QString(settings.m_title));
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

void LoRaMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "LoRaMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("LoRaMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double LoRaMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void LoRaMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t LoRaMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

bool LoRaMod::getModulatorActive() const
{
    return m_basebandSource->getActive();
}
