///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2021 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"
#include "SWGChirpChatModReport.h"
#include "SWGMeshcoreModActions.h"

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"
#include "channel/channelwebapiutils.h"

#include "meshcorepacket.h"
#include "meshcoremodbaseband.h"
#include "meshcoremod.h"

MESSAGE_CLASS_DEFINITION(MeshcoreMod::MsgConfigureMeshcoreMod, Message)
MESSAGE_CLASS_DEFINITION(MeshcoreMod::MsgReportPayloadTime, Message)
MESSAGE_CLASS_DEFINITION(MeshcoreMod::MsgSendMessage, Message)

const char* const MeshcoreMod::m_channelIdURI = "sdrangel.channeltx.modmeshcore";
const char* const MeshcoreMod::m_channelId = "MeshcoreMod";

MeshcoreMod::MeshcoreMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
	m_deviceAPI(deviceAPI),
    m_currentPayloadTime(0.0),
	m_sampleRate(48000),
        m_udpSocket(nullptr)
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new MeshcoreModBaseband();
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &MeshcoreMod::networkManagerFinished
    );
}

MeshcoreMod::~MeshcoreMod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &MeshcoreMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this, true);
    stop();
    delete m_basebandSource;
    delete m_thread;
}

void MeshcoreMod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSourceAPI(this);
        m_deviceAPI->removeChannelSource(this, false);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSource(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

void MeshcoreMod::start()
{
	qDebug("MeshcoreMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void MeshcoreMod::stop()
{
    qDebug("MeshcoreMod::stop");
	m_thread->exit();
	m_thread->wait();
}

void MeshcoreMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool MeshcoreMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureMeshcoreMod::match(cmd))
    {
        MsgConfigureMeshcoreMod& cfg = (MsgConfigureMeshcoreMod&) cmd;
        qDebug() << "MeshcoreMod::handleMessage: MsgConfigureMeshcoreMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgSendMessage::match(cmd))
    {
        qDebug() << "MeshcoreMod::handleMessage: MsgSendMessage";
        sendCurrentSettingsMessage();
        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "MeshcoreMod::handleMessage: DSPSignalNotification";
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

void MeshcoreMod::sendMessage()
{
    m_inputMessageQueue.push(MsgSendMessage::create());
}

void MeshcoreMod::sendCurrentSettingsMessage()
{
    MeshcoreModBaseband::MsgConfigureMeshcoreModPayload *payloadMsg = nullptr;

    m_symbols.clear();
    m_encoder.encode(m_settings, m_symbols);
    payloadMsg = MeshcoreModBaseband::MsgConfigureMeshcoreModPayload::create(m_symbols);

    if (payloadMsg)
    {
        m_basebandSource->getInputMessageQueue()->push(payloadMsg);
        m_currentPayloadTime = (m_symbols.size()*(1<<m_settings.m_spreadFactor)*1000.0) / MeshcoreModSettings::bandwidths[m_settings.m_bandwidthIndex];

        if (getMessageQueueToGUI())
        {
            MsgReportPayloadTime *rpt = MsgReportPayloadTime::create(m_currentPayloadTime, m_symbols.size());
            getMessageQueueToGUI()->push(rpt);
        }
    }
}

void MeshcoreMod::setCenterFrequency(qint64 frequency)
{
    MeshcoreModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureMeshcoreMod *msgToGUI = MsgConfigureMeshcoreMod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void MeshcoreMod::applySettings(const MeshcoreModSettings& incomingSettings, bool force)
{
    MeshcoreModSettings settings(incomingSettings);

    qDebug() << "MeshcoreMod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_bandwidthIndex
            << " bandwidth: " << MeshcoreModSettings::bandwidths[settings.m_bandwidthIndex]
            << " m_channelMute: " << settings.m_channelMute
            << " m_textMessage: " << settings.m_textMessage
            << " m_bytesMessage: " << settings.m_bytesMessage.toHex()
            << " m_spreadFactor: " << settings.m_spreadFactor
            << " m_deBits: " << settings.m_deBits
            << " m_codingScheme: " << MeshcoreModSettings::m_codingScheme
            << " m_nbParityBits: " << settings.m_nbParityBits
            << " m_hasCRC: " << MeshcoreModSettings::m_hasCRC
            << " m_hasHeader: " << MeshcoreModSettings::m_hasHeader
            << " m_messageType: " << settings.m_messageType
            << " m_preambleChirps: " << settings.m_preambleChirps
            << " m_quietMillis: " << settings.m_quietMillis
            << " m_messageRepeat: " << settings.m_messageRepeat
            << " m_udpEnabled: " << settings.m_udpEnabled
            << " m_udpAddress: " << settings.m_udpAddress
            << " m_udpPort: " << settings.m_udpPort
            << " m_syncWord: " << settings.m_syncWord
            << " m_invertRamps: " << settings.m_invertRamps
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

    if ((settings.m_spreadFactor != m_settings.m_spreadFactor)
     || (settings.m_bandwidthIndex != m_settings.m_bandwidthIndex) || force)
    {
        if (getMessageQueueToGUI())
        {
            m_currentPayloadTime = (m_symbols.size()*(1<<settings.m_spreadFactor)*1000.0) / MeshcoreModSettings::bandwidths[settings.m_bandwidthIndex];
            MsgReportPayloadTime *rpt = MsgReportPayloadTime::create(m_currentPayloadTime, m_symbols.size());
            getMessageQueueToGUI()->push(rpt);
        }
    }

    if ((settings.m_nbParityBits != m_settings.m_nbParityBits || force))
    {
        reverseAPIKeys.append("nbParityBits");
        m_encoder.setLoRaParityBits(settings.m_nbParityBits);
    }

    if ((settings.m_textMessage != m_settings.m_textMessage) || force) {
        reverseAPIKeys.append("textMessage");
    }
    if ((settings.m_bytesMessage != m_settings.m_bytesMessage) || force) {
        reverseAPIKeys.append("bytesMessage");
    }
    if ((settings.m_preambleChirps != m_settings.m_preambleChirps) || force) {
        reverseAPIKeys.append("preambleChirps");
    }
    if ((settings.m_quietMillis != m_settings.m_quietMillis) || force) {
        reverseAPIKeys.append("quietMillis");
    }
    if ((settings.m_invertRamps != m_settings.m_invertRamps) || force) {
        reverseAPIKeys.append("invertRamps");
    }
    if ((settings.m_syncWord != m_settings.m_syncWord) || force) {
        reverseAPIKeys.append("syncWord");
    }

    MeshcoreModBaseband::MsgConfigureMeshcoreModPayload *payloadMsg = nullptr;

    const bool reencodePayload = force
        || settings.m_textMessage != m_settings.m_textMessage
        || settings.m_messageType != m_settings.m_messageType
        || settings.m_meshIdentityPath != m_settings.m_meshIdentityPath
        || settings.m_meshNodeName != m_settings.m_meshNodeName
        || settings.m_meshAdvertLocationEnabled != m_settings.m_meshAdvertLocationEnabled
        || settings.m_meshAdvertLat != m_settings.m_meshAdvertLat
        || settings.m_meshAdvertLon != m_settings.m_meshAdvertLon
        || settings.m_meshDestPubKeyHex != m_settings.m_meshDestPubKeyHex
        || settings.m_meshGroupChannelName != m_settings.m_meshGroupChannelName
        || settings.m_meshGroupChannelPskHex != m_settings.m_meshGroupChannelPskHex
        || settings.m_meshAckMsgHashHex != m_settings.m_meshAckMsgHashHex;

    if (reencodePayload)
    {
        m_symbols.clear();
        m_encoder.encode(settings, m_symbols);
        payloadMsg = MeshcoreModBaseband::MsgConfigureMeshcoreModPayload::create(m_symbols);
    }

    if (payloadMsg)
    {
        m_basebandSource->getInputMessageQueue()->push(payloadMsg);
        m_currentPayloadTime = (m_symbols.size()*(1<<settings.m_spreadFactor)*1000.0) / MeshcoreModSettings::bandwidths[settings.m_bandwidthIndex];

        if (getMessageQueueToGUI())
        {
            MsgReportPayloadTime *rpt = MsgReportPayloadTime::create(m_currentPayloadTime, m_symbols.size());
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
            m_deviceAPI->removeChannelSource(this, false, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSource(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSourceAPI(this);
            m_settings.m_streamIndex = settings.m_streamIndex; // make sure ChannelAPI::getStreamIndex() is consistent
            emit streamIndexChanged(settings.m_streamIndex);
        }

        reverseAPIKeys.append("streamIndex");
    }

    MeshcoreModBaseband::MsgConfigureMeshcoreModBaseband *msg =
        MeshcoreModBaseband::MsgConfigureMeshcoreModBaseband::create(settings, force);
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

QByteArray MeshcoreMod::serialize() const
{
    return m_settings.serialize();
}

bool MeshcoreMod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureMeshcoreMod *msg = MsgConfigureMeshcoreMod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

int MeshcoreMod::webapiSettingsGet(
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setMeshcoreModSettings(new SWGSDRangel::SWGMeshcoreModSettings());
    response.getMeshcoreModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int MeshcoreMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int MeshcoreMod::webapiSettingsPutPatch(
    bool force,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    MeshcoreModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureMeshcoreMod *msg = MsgConfigureMeshcoreMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureMeshcoreMod *msgToGUI = MsgConfigureMeshcoreMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void MeshcoreMod::webapiUpdateChannelSettings(
        MeshcoreModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getMeshcoreModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("bandwidthIndex")) {
        settings.m_bandwidthIndex = response.getMeshcoreModSettings()->getBandwidthIndex();
    }
    if (channelSettingsKeys.contains("spreadFactor")) {
        settings.m_spreadFactor = response.getMeshcoreModSettings()->getSpreadFactor();
    }
    if (channelSettingsKeys.contains("deBits")) {
        settings.m_deBits = response.getMeshcoreModSettings()->getDeBits();
    }
    if (channelSettingsKeys.contains("preambleChirps")) {
        settings.m_preambleChirps = response.getMeshcoreModSettings()->getPreambleChirps();
    }
    if (channelSettingsKeys.contains("quietMillis")) {
        settings.m_quietMillis = response.getMeshcoreModSettings()->getQuietMillis();
    }
    if (channelSettingsKeys.contains("syncWord")) {
        settings.m_syncWord = response.getMeshcoreModSettings()->getSyncWord();
    }
    if (channelSettingsKeys.contains("syncWord")) {
        settings.m_syncWord = response.getMeshcoreModSettings()->getSyncWord();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getMeshcoreModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("nbParityBits")) {
        settings.m_nbParityBits = response.getMeshcoreModSettings()->getNbParityBits();
    }
    if (channelSettingsKeys.contains("textMessage")) {
        settings.m_textMessage = *response.getMeshcoreModSettings()->getTextMessage();
    }
    if (channelSettingsKeys.contains("messageRepeat")) {
        settings.m_messageRepeat = response.getMeshcoreModSettings()->getMessageRepeat();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getMeshcoreModSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getMeshcoreModSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getMeshcoreModSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("invertRamps")) {
        settings.m_invertRamps = response.getMeshcoreModSettings()->getInvertRamps();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getMeshcoreModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getMeshcoreModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getMeshcoreModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getMeshcoreModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getMeshcoreModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getMeshcoreModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getMeshcoreModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getMeshcoreModSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getMeshcoreModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getMeshcoreModSettings()->getRollupState());
    }
}

int MeshcoreMod::webapiReportGet(
    SWGSDRangel::SWGChannelReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setMeshcoreModReport(new SWGSDRangel::SWGMeshcoreModReport());
    response.getMeshcoreModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

int MeshcoreMod::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGMeshcoreModActions *swgMeshcoreModActions = query.getMeshcoreModActions();

    if (swgMeshcoreModActions)
    {
        if (channelActionsKeys.contains("sendNow") && (swgMeshcoreModActions->getSendNow() != 0))
        {
            sendMessage();
        }
        return 202;
    }
    else
    {
        errorMessage = "Missing MeshcoreModActions in query";
        return 400;
    }
}

void MeshcoreMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const MeshcoreModSettings& settings)
{
    response.getMeshcoreModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getMeshcoreModSettings()->setBandwidthIndex(settings.m_bandwidthIndex);
    response.getMeshcoreModSettings()->setSpreadFactor(settings.m_spreadFactor);
    response.getMeshcoreModSettings()->setDeBits(settings.m_deBits);
    response.getMeshcoreModSettings()->setPreambleChirps(settings.m_preambleChirps);
    response.getMeshcoreModSettings()->setQuietMillis(settings.m_quietMillis);
    response.getMeshcoreModSettings()->setSyncWord(settings.m_syncWord);
    response.getMeshcoreModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getMeshcoreModSettings()->setNbParityBits(settings.m_nbParityBits);

    if (response.getMeshcoreModSettings()->getTextMessage()) {
        *response.getMeshcoreModSettings()->getTextMessage() = settings.m_textMessage;
    } else {
        response.getMeshcoreModSettings()->setTextMessage(new QString(settings.m_textMessage));
    }

    response.getMeshcoreModSettings()->setMessageRepeat(settings.m_messageRepeat);
    response.getMeshcoreModSettings()->setUdpEnabled(settings.m_udpEnabled);
    if (response.getMeshcoreModSettings()->getUdpAddress()) {
        *response.getMeshcoreModSettings()->getUdpAddress() = settings.m_udpAddress;
    } else {
        response.getMeshcoreModSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    }
    response.getMeshcoreModSettings()->setUdpPort(settings.m_udpPort);
    response.getMeshcoreModSettings()->setInvertRamps(settings.m_invertRamps ? 1 : 0);

    response.getMeshcoreModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getMeshcoreModSettings()->getTitle()) {
        *response.getMeshcoreModSettings()->getTitle() = settings.m_title;
    } else {
        response.getMeshcoreModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getMeshcoreModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getMeshcoreModSettings()->getReverseApiAddress()) {
        *response.getMeshcoreModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getMeshcoreModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getMeshcoreModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getMeshcoreModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getMeshcoreModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getMeshcoreModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getMeshcoreModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getMeshcoreModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getMeshcoreModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getMeshcoreModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getMeshcoreModSettings()->setRollupState(swgRollupState);
        }
    }
}

void MeshcoreMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getMeshcoreModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getMeshcoreModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
    float fourthsMs = ((1<<m_settings.m_spreadFactor) * 250.0) / MeshcoreModSettings::bandwidths[m_settings.m_bandwidthIndex];
    float controlMs = (4*m_settings.m_preambleChirps + 8 + 9) * fourthsMs; // preamble + sync word + SFD
    response.getMeshcoreModReport()->setPayloadTimeMs(m_currentPayloadTime);
    response.getMeshcoreModReport()->setTotalTimeMs(m_currentPayloadTime + controlMs);
    response.getMeshcoreModReport()->setSymbolTimeMs(4.0 * fourthsMs);
    response.getMeshcoreModReport()->setPlaying(getModulatorActive() ? 1 : 0);
}

void MeshcoreMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const MeshcoreModSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);

    const QUrl channelSettingsURL = ChannelWebAPIUtils::buildChannelSettingsURL(
        settings.m_reverseAPIAddress,
        settings.m_reverseAPIPort,
        settings.m_reverseAPIDeviceIndex,
        settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(channelSettingsURL);
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

void MeshcoreMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const MeshcoreModSettings& settings,
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

void MeshcoreMod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const MeshcoreModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    if (swgChannelSettings->getChannelType()) {
        *swgChannelSettings->getChannelType() = m_channelId;
    } else {
        swgChannelSettings->setChannelType(new QString(m_channelId));
    }
    swgChannelSettings->setMeshcoreModSettings(new SWGSDRangel::SWGMeshcoreModSettings());
    SWGSDRangel::SWGMeshcoreModSettings *swgMeshcoreModSettings = swgChannelSettings->getMeshcoreModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgMeshcoreModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("bandwidthIndex") || force) {
        swgMeshcoreModSettings->setBandwidthIndex(settings.m_bandwidthIndex);
    }
    if (channelSettingsKeys.contains("spreadFactor") || force) {
        swgMeshcoreModSettings->setSpreadFactor(settings.m_spreadFactor);
    }
    if (channelSettingsKeys.contains("deBits") || force) {
        swgMeshcoreModSettings->setDeBits(settings.m_deBits);
    }
    if (channelSettingsKeys.contains("preambleChirps") || force) {
        swgMeshcoreModSettings->setPreambleChirps(settings.m_preambleChirps);
    }
    if (channelSettingsKeys.contains("quietMillis") || force) {
        swgMeshcoreModSettings->setQuietMillis(settings.m_quietMillis);
    }
    if (channelSettingsKeys.contains("syncWord") || force) {
        swgMeshcoreModSettings->setSyncWord(settings.m_syncWord);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgMeshcoreModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("nbParityBits") || force) {
        swgMeshcoreModSettings->setNbParityBits(settings.m_nbParityBits);
    }
    if (channelSettingsKeys.contains("textMessage") || force) {
        if (swgMeshcoreModSettings->getTextMessage()) {
            *swgMeshcoreModSettings->getTextMessage() = settings.m_textMessage;
        } else {
            swgMeshcoreModSettings->setTextMessage(new QString(settings.m_textMessage));
        }
    }

    if (channelSettingsKeys.contains("messageRepeat") || force) {
        swgMeshcoreModSettings->setMessageRepeat(settings.m_messageRepeat);
    }

    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgMeshcoreModSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        if (swgMeshcoreModSettings->getUdpAddress()) {
            *swgMeshcoreModSettings->getUdpAddress() = settings.m_udpAddress;
        } else {
            swgMeshcoreModSettings->setUdpAddress(new QString(settings.m_udpAddress));
        }
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgMeshcoreModSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("invertRamps") || force) {
        swgMeshcoreModSettings->setInvertRamps(settings.m_invertRamps ? 1 : 0);
    }

    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgMeshcoreModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        if (swgMeshcoreModSettings->getTitle()) {
            *swgMeshcoreModSettings->getTitle() = settings.m_title;
        } else {
            swgMeshcoreModSettings->setTitle(new QString(settings.m_title));
        }
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgMeshcoreModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgMeshcoreModSettings->setRollupState(swgRollupState);
    }
}

void MeshcoreMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "MeshcoreMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("MeshcoreMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double MeshcoreMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void MeshcoreMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t MeshcoreMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

bool MeshcoreMod::getModulatorActive() const
{
    return m_basebandSource->getActive();
}

void MeshcoreMod::openUDP(const MeshcoreModSettings& settings)
{
    closeUDP();
    m_udpSocket = new QUdpSocket();
    if (!m_udpSocket->bind(QHostAddress(settings.m_udpAddress), settings.m_udpPort))
        qCritical() << "MeshcoreMod::openUDP: Failed to bind to port " << settings.m_udpAddress << ":" << settings.m_udpPort << ". Error: " << m_udpSocket->error();
    else
        qDebug() << "MeshcoreMod::openUDP: Listening for packets on " << settings.m_udpAddress << ":" << settings.m_udpPort;
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &MeshcoreMod::udpRx);
}

void MeshcoreMod::closeUDP()
{
    if (m_udpSocket != nullptr)
    {
        disconnect(m_udpSocket, &QUdpSocket::readyRead, this, &MeshcoreMod::udpRx);
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

void MeshcoreMod::udpRx()
{
    while (m_udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        MeshcoreModBaseband::MsgConfigureMeshcoreModPayload *payloadMsg = nullptr;
        std::vector<unsigned short> symbols;

        m_encoder.encodeBytes(datagram.data(), symbols);
        payloadMsg = MeshcoreModBaseband::MsgConfigureMeshcoreModPayload::create(symbols);

        if (payloadMsg)
        {
            m_basebandSource->getInputMessageQueue()->push(payloadMsg);
            m_currentPayloadTime = (symbols.size()*(1<<m_settings.m_spreadFactor)*1000.0) / MeshcoreModSettings::bandwidths[m_settings.m_bandwidthIndex];

            if (getMessageQueueToGUI())
            {
                MsgReportPayloadTime *rpt = MsgReportPayloadTime::create(m_currentPayloadTime, symbols.size());
                getMessageQueueToGUI()->push(rpt);
            }
        }
    }
}
