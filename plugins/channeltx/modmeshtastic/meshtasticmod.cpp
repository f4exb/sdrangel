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
#include "SWGChirpChatModReport.h"

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"
#include "channel/channelwebapiutils.h"

#include "meshtasticpacket.h"
#include "meshtasticmodbaseband.h"
#include "meshtasticmod.h"

MESSAGE_CLASS_DEFINITION(MeshtasticMod::MsgConfigureMeshtasticMod, Message)
MESSAGE_CLASS_DEFINITION(MeshtasticMod::MsgReportPayloadTime, Message)
MESSAGE_CLASS_DEFINITION(MeshtasticMod::MsgSendMessage, Message)

const char* const MeshtasticMod::m_channelIdURI = "sdrangel.channeltx.modmeshtastic";
const char* const MeshtasticMod::m_channelId = "MeshtasticMod";

MeshtasticMod::MeshtasticMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
	m_deviceAPI(deviceAPI),
    m_currentPayloadTime(0.0),
	m_sampleRate(48000),
        m_udpSocket(nullptr)
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new MeshtasticModBaseband();
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &MeshtasticMod::networkManagerFinished
    );
}

MeshtasticMod::~MeshtasticMod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &MeshtasticMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this, true);
    stop();
    delete m_basebandSource;
    delete m_thread;
}

void MeshtasticMod::setDeviceAPI(DeviceAPI *deviceAPI)
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

void MeshtasticMod::start()
{
	qDebug("MeshtasticMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void MeshtasticMod::stop()
{
    qDebug("MeshtasticMod::stop");
	m_thread->exit();
	m_thread->wait();
}

void MeshtasticMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool MeshtasticMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureMeshtasticMod::match(cmd))
    {
        MsgConfigureMeshtasticMod& cfg = (MsgConfigureMeshtasticMod&) cmd;
        qDebug() << "MeshtasticMod::handleMessage: MsgConfigureMeshtasticMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgSendMessage::match(cmd))
    {
        qDebug() << "MeshtasticMod::handleMessage: MsgSendMessage";
        sendCurrentSettingsMessage();
        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "MeshtasticMod::handleMessage: DSPSignalNotification";
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

void MeshtasticMod::sendMessage()
{
    m_inputMessageQueue.push(MsgSendMessage::create());
}

void MeshtasticMod::sendCurrentSettingsMessage()
{
    MeshtasticModBaseband::MsgConfigureMeshtasticModPayload *payloadMsg = nullptr;

    m_symbols.clear();
    m_encoder.encode(m_settings, m_symbols);
    payloadMsg = MeshtasticModBaseband::MsgConfigureMeshtasticModPayload::create(m_symbols);

    if (payloadMsg)
    {
        m_basebandSource->getInputMessageQueue()->push(payloadMsg);
        m_currentPayloadTime = (m_symbols.size()*(1<<m_settings.m_spreadFactor)*1000.0) / MeshtasticModSettings::bandwidths[m_settings.m_bandwidthIndex];

        if (getMessageQueueToGUI())
        {
            MsgReportPayloadTime *rpt = MsgReportPayloadTime::create(m_currentPayloadTime, m_symbols.size());
            getMessageQueueToGUI()->push(rpt);
        }
    }
}

void MeshtasticMod::setCenterFrequency(qint64 frequency)
{
    MeshtasticModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureMeshtasticMod *msgToGUI = MsgConfigureMeshtasticMod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

bool MeshtasticMod::applyMeshtasticRadioSettingsIfPresent(MeshtasticModSettings& settings) const
{
    if (settings.m_codingScheme != MeshtasticModSettings::CodingLoRa) {
        return false;
    }

    if (!modemmeshtastic::Packet::isCommand(settings.m_textMessage)) {
        return false;
    }

    modemmeshtastic::TxRadioSettings meshRadio;
    QString error;
    if (!modemmeshtastic::Packet::deriveTxRadioSettings(settings.m_textMessage, meshRadio, error))
    {
        qWarning() << "MeshtasticMod::applyMeshtasticRadioSettingsIfPresent:" << error;
        return false;
    }

    bool changed = false;

    if ((meshRadio.spreadFactor > 0) && (meshRadio.spreadFactor != settings.m_spreadFactor)) {
        settings.m_spreadFactor = meshRadio.spreadFactor;
        changed = true;
    }

    if ((meshRadio.parityBits > 0) && (meshRadio.parityBits != settings.m_nbParityBits)) {
        settings.m_nbParityBits = meshRadio.parityBits;
        changed = true;
    }

    if (meshRadio.deBits != settings.m_deBits) {
        settings.m_deBits = meshRadio.deBits;
        changed = true;
    }

    if (meshRadio.syncWord != settings.m_syncWord) {
        settings.m_syncWord = meshRadio.syncWord;
        changed = true;
    }

    if (meshRadio.hasCenterFrequency && (m_deviceAPI != nullptr))
    {
        const QList<quint64> centerFrequencies = m_deviceAPI->getCenterFrequency();
        const int streamIndex = std::max(0, settings.m_streamIndex);
        const int selectedIndex = (streamIndex < centerFrequencies.size()) ? streamIndex : 0;

        if (!centerFrequencies.isEmpty())
        {
            const qint64 deviceCenterFrequency = static_cast<qint64>(centerFrequencies.at(selectedIndex));
            const qint64 wantedOffset = meshRadio.centerFrequencyHz - deviceCenterFrequency;

            if (wantedOffset != settings.m_inputFrequencyOffset)
            {
                settings.m_inputFrequencyOffset = static_cast<int>(wantedOffset);
                changed = true;
            }
        }
    }

    if (changed) {
        qInfo() << "MeshtasticMod::applyMeshtasticRadioSettingsIfPresent:" << meshRadio.summary;
    }

    return changed;
}

void MeshtasticMod::applySettings(const MeshtasticModSettings& incomingSettings, bool force)
{
    MeshtasticModSettings settings(incomingSettings);
    applyMeshtasticRadioSettingsIfPresent(settings);

    qDebug() << "MeshtasticMod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_bandwidthIndex
            << " bandwidth: " << MeshtasticModSettings::bandwidths[settings.m_bandwidthIndex]
            << " m_channelMute: " << settings.m_channelMute
            << " m_textMessage: " << settings.m_textMessage
            << " m_bytesMessage: " << settings.m_bytesMessage.toHex()
            << " m_spreadFactor: " << settings.m_spreadFactor
            << " m_deBits: " << settings.m_deBits
            << " m_codingScheme: " << settings.m_codingScheme
            << " m_nbParityBits: " << settings.m_nbParityBits
            << " m_hasCRC: " << settings.m_hasCRC
            << " m_hasHeader: " << settings.m_hasHeader
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
            m_currentPayloadTime = (m_symbols.size()*(1<<settings.m_spreadFactor)*1000.0) / MeshtasticModSettings::bandwidths[settings.m_bandwidthIndex];
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

    MeshtasticModBaseband::MsgConfigureMeshtasticModPayload *payloadMsg = nullptr;

    if ((settings.m_textMessage != m_settings.m_textMessage) || force)
    {
        m_symbols.clear();
        m_encoder.encode(settings, m_symbols);
        payloadMsg = MeshtasticModBaseband::MsgConfigureMeshtasticModPayload::create(m_symbols);
    }

    if (payloadMsg)
    {
        m_basebandSource->getInputMessageQueue()->push(payloadMsg);
        m_currentPayloadTime = (m_symbols.size()*(1<<settings.m_spreadFactor)*1000.0) / MeshtasticModSettings::bandwidths[settings.m_bandwidthIndex];

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

    MeshtasticModBaseband::MsgConfigureMeshtasticModBaseband *msg =
        MeshtasticModBaseband::MsgConfigureMeshtasticModBaseband::create(settings, force);
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

QByteArray MeshtasticMod::serialize() const
{
    return m_settings.serialize();
}

bool MeshtasticMod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureMeshtasticMod *msg = MsgConfigureMeshtasticMod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

int MeshtasticMod::webapiSettingsGet(
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setMeshtasticModSettings(new SWGSDRangel::SWGMeshtasticModSettings());
    response.getMeshtasticModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int MeshtasticMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int MeshtasticMod::webapiSettingsPutPatch(
    bool force,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    MeshtasticModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureMeshtasticMod *msg = MsgConfigureMeshtasticMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureMeshtasticMod *msgToGUI = MsgConfigureMeshtasticMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void MeshtasticMod::webapiUpdateChannelSettings(
        MeshtasticModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getMeshtasticModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("bandwidthIndex")) {
        settings.m_bandwidthIndex = response.getMeshtasticModSettings()->getBandwidthIndex();
    }
    if (channelSettingsKeys.contains("spreadFactor")) {
        settings.m_spreadFactor = response.getMeshtasticModSettings()->getSpreadFactor();
    }
    if (channelSettingsKeys.contains("deBits")) {
        settings.m_deBits = response.getMeshtasticModSettings()->getDeBits();
    }
    if (channelSettingsKeys.contains("preambleChirps")) {
        settings.m_preambleChirps = response.getMeshtasticModSettings()->getPreambleChirps();
    }
    if (channelSettingsKeys.contains("quietMillis")) {
        settings.m_quietMillis = response.getMeshtasticModSettings()->getQuietMillis();
    }
    if (channelSettingsKeys.contains("syncWord")) {
        settings.m_syncWord = response.getMeshtasticModSettings()->getSyncWord();
    }
    if (channelSettingsKeys.contains("syncWord")) {
        settings.m_syncWord = response.getMeshtasticModSettings()->getSyncWord();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getMeshtasticModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("nbParityBits")) {
        settings.m_nbParityBits = response.getMeshtasticModSettings()->getNbParityBits();
    }
    if (channelSettingsKeys.contains("textMessage")) {
        settings.m_textMessage = *response.getMeshtasticModSettings()->getTextMessage();
    }
    if (channelSettingsKeys.contains("messageRepeat")) {
        settings.m_messageRepeat = response.getMeshtasticModSettings()->getMessageRepeat();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getMeshtasticModSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getMeshtasticModSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getMeshtasticModSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("invertRamps")) {
        settings.m_invertRamps = response.getMeshtasticModSettings()->getInvertRamps();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getMeshtasticModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getMeshtasticModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getMeshtasticModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getMeshtasticModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getMeshtasticModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getMeshtasticModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getMeshtasticModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getMeshtasticModSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getMeshtasticModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getMeshtasticModSettings()->getRollupState());
    }
}

int MeshtasticMod::webapiReportGet(
    SWGSDRangel::SWGChannelReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setMeshtasticModReport(new SWGSDRangel::SWGMeshtasticModReport());
    response.getMeshtasticModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void MeshtasticMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const MeshtasticModSettings& settings)
{
    response.getMeshtasticModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getMeshtasticModSettings()->setBandwidthIndex(settings.m_bandwidthIndex);
    response.getMeshtasticModSettings()->setSpreadFactor(settings.m_spreadFactor);
    response.getMeshtasticModSettings()->setDeBits(settings.m_deBits);
    response.getMeshtasticModSettings()->setPreambleChirps(settings.m_preambleChirps);
    response.getMeshtasticModSettings()->setQuietMillis(settings.m_quietMillis);
    response.getMeshtasticModSettings()->setSyncWord(settings.m_syncWord);
    response.getMeshtasticModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getMeshtasticModSettings()->setNbParityBits(settings.m_nbParityBits);

    if (response.getMeshtasticModSettings()->getTextMessage()) {
        *response.getMeshtasticModSettings()->getTextMessage() = settings.m_textMessage;
    } else {
        response.getMeshtasticModSettings()->setTextMessage(new QString(settings.m_textMessage));
    }

    response.getMeshtasticModSettings()->setMessageRepeat(settings.m_messageRepeat);
    response.getMeshtasticModSettings()->setUdpEnabled(settings.m_udpEnabled);
    response.getMeshtasticModSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getMeshtasticModSettings()->setUdpPort(settings.m_udpPort);
    response.getMeshtasticModSettings()->setInvertRamps(settings.m_invertRamps ? 1 : 0);

    response.getMeshtasticModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getMeshtasticModSettings()->getTitle()) {
        *response.getMeshtasticModSettings()->getTitle() = settings.m_title;
    } else {
        response.getMeshtasticModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getMeshtasticModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getMeshtasticModSettings()->getReverseApiAddress()) {
        *response.getMeshtasticModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getMeshtasticModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getMeshtasticModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getMeshtasticModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getMeshtasticModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getMeshtasticModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getMeshtasticModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getMeshtasticModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getMeshtasticModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getMeshtasticModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getMeshtasticModSettings()->setRollupState(swgRollupState);
        }
    }
}

void MeshtasticMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getMeshtasticModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getMeshtasticModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
    float fourthsMs = ((1<<m_settings.m_spreadFactor) * 250.0) / MeshtasticModSettings::bandwidths[m_settings.m_bandwidthIndex];
    float controlMs = (4*m_settings.m_preambleChirps + 8 + 9) * fourthsMs; // preamble + sync word + SFD
    response.getMeshtasticModReport()->setPayloadTimeMs(m_currentPayloadTime);
    response.getMeshtasticModReport()->setTotalTimeMs(m_currentPayloadTime + controlMs);
    response.getMeshtasticModReport()->setSymbolTimeMs(4.0 * fourthsMs);
    response.getMeshtasticModReport()->setPlaying(getModulatorActive() ? 1 : 0);
}

void MeshtasticMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const MeshtasticModSettings& settings, bool force)
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

void MeshtasticMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const MeshtasticModSettings& settings,
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

void MeshtasticMod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const MeshtasticModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setMeshtasticModSettings(new SWGSDRangel::SWGMeshtasticModSettings());
    SWGSDRangel::SWGMeshtasticModSettings *swgMeshtasticModSettings = swgChannelSettings->getMeshtasticModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgMeshtasticModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("bandwidthIndex") || force) {
        swgMeshtasticModSettings->setBandwidthIndex(settings.m_bandwidthIndex);
    }
    if (channelSettingsKeys.contains("spreadFactor") || force) {
        swgMeshtasticModSettings->setSpreadFactor(settings.m_spreadFactor);
    }
    if (channelSettingsKeys.contains("deBits") || force) {
        swgMeshtasticModSettings->setDeBits(settings.m_deBits);
    }
    if (channelSettingsKeys.contains("preambleChirps") || force) {
        swgMeshtasticModSettings->setPreambleChirps(settings.m_preambleChirps);
    }
    if (channelSettingsKeys.contains("quietMillis") || force) {
        swgMeshtasticModSettings->setQuietMillis(settings.m_quietMillis);
    }
    if (channelSettingsKeys.contains("syncWord") || force) {
        swgMeshtasticModSettings->setSyncWord(settings.m_syncWord);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgMeshtasticModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("nbParityBits") || force) {
        swgMeshtasticModSettings->setNbParityBits(settings.m_nbParityBits);
    }
    if (channelSettingsKeys.contains("textMessage") || force) {
        swgMeshtasticModSettings->setTextMessage(new QString(settings.m_textMessage));
    }

    if (channelSettingsKeys.contains("messageRepeat") || force) {
        swgMeshtasticModSettings->setMessageRepeat(settings.m_messageRepeat);
    }

    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgMeshtasticModSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgMeshtasticModSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgMeshtasticModSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("invertRamps") || force) {
        swgMeshtasticModSettings->setInvertRamps(settings.m_invertRamps ? 1 : 0);
    }

    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgMeshtasticModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgMeshtasticModSettings->setTitle(new QString(settings.m_title));
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgMeshtasticModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgMeshtasticModSettings->setRollupState(swgRollupState);
    }
}

void MeshtasticMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "MeshtasticMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("MeshtasticMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double MeshtasticMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void MeshtasticMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t MeshtasticMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

bool MeshtasticMod::getModulatorActive() const
{
    return m_basebandSource->getActive();
}

void MeshtasticMod::openUDP(const MeshtasticModSettings& settings)
{
    closeUDP();
    m_udpSocket = new QUdpSocket();
    if (!m_udpSocket->bind(QHostAddress(settings.m_udpAddress), settings.m_udpPort))
        qCritical() << "MeshtasticMod::openUDP: Failed to bind to port " << settings.m_udpAddress << ":" << settings.m_udpPort << ". Error: " << m_udpSocket->error();
    else
        qDebug() << "MeshtasticMod::openUDP: Listening for packets on " << settings.m_udpAddress << ":" << settings.m_udpPort;
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &MeshtasticMod::udpRx);
}

void MeshtasticMod::closeUDP()
{
    if (m_udpSocket != nullptr)
    {
        disconnect(m_udpSocket, &QUdpSocket::readyRead, this, &MeshtasticMod::udpRx);
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

void MeshtasticMod::udpRx()
{
    while (m_udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        MeshtasticModBaseband::MsgConfigureMeshtasticModPayload *payloadMsg = nullptr;
        std::vector<unsigned short> symbols;

        m_encoder.encodeBytes(datagram.data(), symbols);
        payloadMsg = MeshtasticModBaseband::MsgConfigureMeshtasticModPayload::create(symbols);

        if (payloadMsg)
        {
            m_basebandSource->getInputMessageQueue()->push(payloadMsg);
            m_currentPayloadTime = (symbols.size()*(1<<m_settings.m_spreadFactor)*1000.0) / MeshtasticModSettings::bandwidths[m_settings.m_bandwidthIndex];

            if (getMessageQueueToGUI())
            {
                MsgReportPayloadTime *rpt = MsgReportPayloadTime::create(m_currentPayloadTime, symbols.size());
                getMessageQueueToGUI()->push(rpt);
            }
        }
    }
}
