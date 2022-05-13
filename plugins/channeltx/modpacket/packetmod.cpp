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
#include "SWGPacketModReport.h"
#include "SWGPacketModActions.h"

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "util/crc.h"
#include "maincore.h"

#include "packetmodbaseband.h"
#include "packetmod.h"

MESSAGE_CLASS_DEFINITION(PacketMod::MsgConfigurePacketMod, Message)
MESSAGE_CLASS_DEFINITION(PacketMod::MsgTx, Message)
MESSAGE_CLASS_DEFINITION(PacketMod::MsgReportTx, Message)
MESSAGE_CLASS_DEFINITION(PacketMod::MsgTXPacketBytes, Message)
MESSAGE_CLASS_DEFINITION(PacketMod::MsgTXPacketData, Message)

const char* const PacketMod::m_channelIdURI = "sdrangel.channeltx.modpacket";
const char* const PacketMod::m_channelId = "PacketMod";

PacketMod::PacketMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF),
    m_settingsMutex(QMutex::Recursive),
    m_sampleRate(48000),
    m_udpSocket(nullptr)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new PacketModBaseband();
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
        &PacketMod::networkManagerFinished
    );
}

PacketMod::~PacketMod()
{
    closeUDP();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &PacketMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void PacketMod::setDeviceAPI(DeviceAPI *deviceAPI)
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

void PacketMod::start()
{
    qDebug("PacketMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void PacketMod::stop()
{
    qDebug("PacketMod::stop");
    m_thread->exit();
    m_thread->wait();
}

void PacketMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool PacketMod::handleMessage(const Message& cmd)
{
    if (MsgConfigurePacketMod::match(cmd))
    {
        MsgConfigurePacketMod& cfg = (MsgConfigurePacketMod&) cmd;
        qDebug() << "PacketMod::handleMessage: MsgConfigurePacketMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    if (MsgTx::match(cmd))
    {
        MsgTx *msg = new MsgTx((const MsgTx&) cmd);
        m_basebandSource->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "PacketMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "PacketMod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

void PacketMod::setCenterFrequency(qint64 frequency)
{
    PacketModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigurePacketMod *msgToGUI = MsgConfigurePacketMod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void PacketMod::applySettings(const PacketModSettings& settings, bool force)
{
    qDebug() << "PacketMod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_modulation: " << settings.m_modulation
            << " m_baud: " << settings.m_baud
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_gain: " << settings.m_gain
            << " m_channelMute: " << settings.m_channelMute
            << " m_repeat: " << settings.m_repeat
            << " m_repeatDelay: " << settings.m_repeatDelay
            << " m_repeatCount: " << settings.m_repeatCount
            << " m_ax25PreFlags: " << settings.m_ax25PreFlags
            << " m_ax25PostFlags: " << settings.m_ax25PostFlags
            << " m_preEmphasis: " << settings.m_preEmphasis
            << " m_preEmphasisTau: " << settings.m_preEmphasisTau
            << " m_preEmphasisHighFreq: " << settings.m_preEmphasisHighFreq
            << " m_bpf: " << settings.m_bpf
            << " m_bpfLowCutoff: " << settings.m_bpfLowCutoff
            << " m_bpfHighCutoff: " << settings.m_bpfHighCutoff
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

    if ((settings.m_modulation != m_settings.m_modulation) || force) {
        reverseAPIKeys.append("modulation");
    }

    if ((settings.m_baud != m_settings.m_baud) || force) {
        reverseAPIKeys.append("baud");
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

    if ((settings.m_modulateWhileRamping != m_settings.m_modulateWhileRamping) || force) {
        reverseAPIKeys.append("modulateWhileRamping");
    }

    if ((settings.m_markFrequency != m_settings.m_markFrequency) || force) {
        reverseAPIKeys.append("markFrequency");
    }

    if ((settings.m_spaceFrequency != m_settings.m_spaceFrequency) || force) {
        reverseAPIKeys.append("spaceFrequency");
    }

    if ((settings.m_ax25PreFlags != m_settings.m_ax25PreFlags) || force) {
        reverseAPIKeys.append("ax25PreFlags");
    }

    if ((settings.m_ax25PostFlags != m_settings.m_ax25PostFlags) || force) {
        reverseAPIKeys.append("ax25PostFlags");
    }

    if ((settings.m_ax25Control != m_settings.m_ax25Control) || force) {
        reverseAPIKeys.append("ax25Control");
    }

    if ((settings.m_ax25PID != m_settings.m_ax25PID) || force) {
        reverseAPIKeys.append("ax25PID");
    }

    if ((settings.m_preEmphasis != m_settings.m_preEmphasis) || force) {
        reverseAPIKeys.append("preEmphasis");
    }

    if ((settings.m_preEmphasisTau != m_settings.m_preEmphasisTau) || force) {
        reverseAPIKeys.append("preEmphasisTau");
    }

    if ((settings.m_preEmphasisHighFreq != m_settings.m_preEmphasisHighFreq) || force) {
        reverseAPIKeys.append("preEmphasisHighFreq");
    }

    if ((settings.m_lpfTaps != m_settings.m_lpfTaps) || force) {
        reverseAPIKeys.append("lpfTaps");
    }

    if ((settings.m_bbNoise != m_settings.m_bbNoise) || force) {
        reverseAPIKeys.append("bbNoise");
    }

    if ((settings.m_rfNoise != m_settings.m_rfNoise) || force) {
        reverseAPIKeys.append("rfNoise");
    }

    if ((settings.m_writeToFile != m_settings.m_writeToFile) || force) {
        reverseAPIKeys.append("writeToFile");
    }

    if ((settings.m_spectrumRate != m_settings.m_spectrumRate) || force) {
        reverseAPIKeys.append("spectrumRate");
    }

    if ((settings.m_callsign != m_settings.m_callsign) || force) {
        reverseAPIKeys.append("callsign");
    }

    if ((settings.m_to != m_settings.m_to) || force) {
        reverseAPIKeys.append("to");
    }

    if ((settings.m_via != m_settings.m_via) || force) {
        reverseAPIKeys.append("via");
    }

    if ((settings.m_data != m_settings.m_data) || force) {
        reverseAPIKeys.append("data");
    }

    if ((settings.m_bpf != m_settings.m_bpf) || force) {
        reverseAPIKeys.append("bpf");
    }

    if ((settings.m_bpfLowCutoff != m_settings.m_bpfLowCutoff) || force) {
        reverseAPIKeys.append("bpfLowCutoff");
    }

    if ((settings.m_bpfHighCutoff != m_settings.m_bpfHighCutoff) || force) {
        reverseAPIKeys.append("bpfHighCutoff");
    }

    if ((settings.m_bpfTaps != m_settings.m_bpfTaps) || force) {
        reverseAPIKeys.append("bpfTaps");
    }

    if ((settings.m_scramble != m_settings.m_scramble) || force) {
        reverseAPIKeys.append("scramble");
    }

    if ((settings.m_polynomial != m_settings.m_polynomial) || force) {
        reverseAPIKeys.append("polynomial");
    }

    if ((settings.m_beta != m_settings.m_beta) || force) {
        reverseAPIKeys.append("beta");
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

    PacketModBaseband::MsgConfigurePacketModBaseband *msg = PacketModBaseband::MsgConfigurePacketModBaseband::create(settings, force);
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

QByteArray PacketMod::serialize() const
{
    return m_settings.serialize();
}

bool PacketMod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigurePacketMod *msg = MsgConfigurePacketMod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

void PacketMod::sendSampleRateToDemodAnalyzer()
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
                getSourceChannelSampleRate()
            );
            messageQueue->push(msg);
        }
    }
}

int PacketMod::webapiSettingsGet(
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setPacketModSettings(new SWGSDRangel::SWGPacketModSettings());
    response.getPacketModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int PacketMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int PacketMod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    PacketModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigurePacketMod *msg = MsgConfigurePacketMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigurePacketMod *msgToGUI = MsgConfigurePacketMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void PacketMod::webapiUpdateChannelSettings(
        PacketModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getPacketModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("modulation")) {
        settings.m_modulation = (PacketModSettings::Modulation) response.getPacketModSettings()->getModulation();
    }
    if (channelSettingsKeys.contains("baud")) {
        settings.m_baud = response.getPacketModSettings()->getBaud();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getPacketModSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getPacketModSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("gain")) {
        settings.m_gain = response.getPacketModSettings()->getGain();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getPacketModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("repeat")) {
        settings.m_repeat = response.getPacketModSettings()->getRepeat() != 0;
    }
    if (channelSettingsKeys.contains("repeatDelay")) {
        settings.m_repeatDelay = response.getPacketModSettings()->getRepeatDelay();
    }
    if (channelSettingsKeys.contains("repeatCount")) {
        settings.m_repeatCount = response.getPacketModSettings()->getRepeatCount();
    }
    if (channelSettingsKeys.contains("rampUpBits")) {
        settings.m_rampUpBits = response.getPacketModSettings()->getRampUpBits();
    }
    if (channelSettingsKeys.contains("rampDownBits")) {
        settings.m_rampDownBits = response.getPacketModSettings()->getRampDownBits();
    }
    if (channelSettingsKeys.contains("rampRange")) {
        settings.m_rampRange = response.getPacketModSettings()->getRampRange();
    }
    if (channelSettingsKeys.contains("modulateWhileRamping")) {
        settings.m_modulateWhileRamping = response.getPacketModSettings()->getModulateWhileRamping() != 0;
    }
    if (channelSettingsKeys.contains("markFrequency")) {
        settings.m_markFrequency = response.getPacketModSettings()->getMarkFrequency();
    }
    if (channelSettingsKeys.contains("spaceFrequency")) {
        settings.m_spaceFrequency = response.getPacketModSettings()->getSpaceFrequency();
    }
    if (channelSettingsKeys.contains("ax25PreFlags")) {
        settings.m_ax25PreFlags = response.getPacketModSettings()->getAx25PreFlags();
    }
    if (channelSettingsKeys.contains("ax25PostFlags")) {
        settings.m_ax25PostFlags = response.getPacketModSettings()->getAx25PostFlags();
    }
    if (channelSettingsKeys.contains("ax25Control")) {
        settings.m_ax25Control = response.getPacketModSettings()->getAx25Control();
    }
    if (channelSettingsKeys.contains("ax25PID")) {
        settings.m_ax25PID = response.getPacketModSettings()->getAx25Pid();
    }
    if (channelSettingsKeys.contains("preEmphasis")) {
        settings.m_preEmphasis = response.getPacketModSettings()->getPreEmphasis() != 0;
    }
    if (channelSettingsKeys.contains("preEmphasisTau")) {
        settings.m_preEmphasisTau = response.getPacketModSettings()->getPreEmphasisTau();
    }
    if (channelSettingsKeys.contains("preEmphasisHighFreq")) {
        settings.m_preEmphasisHighFreq = response.getPacketModSettings()->getPreEmphasisHighFreq();
    }
    if (channelSettingsKeys.contains("lpfTaps")) {
        settings.m_lpfTaps = response.getPacketModSettings()->getLpfTaps();
    }
    if (channelSettingsKeys.contains("bbNoise")) {
        settings.m_bbNoise = response.getPacketModSettings()->getBbNoise() != 0;
    }
    if (channelSettingsKeys.contains("rfNoise")) {
        settings.m_rfNoise = response.getPacketModSettings()->getRfNoise() != 0;
    }
    if (channelSettingsKeys.contains("writeToFile")) {
        settings.m_writeToFile = response.getPacketModSettings()->getWriteToFile() != 0;
    }
    if (channelSettingsKeys.contains("spectrumRate")) {
        settings.m_spectrumRate = response.getPacketModSettings()->getSpectrumRate();
    }
    if (channelSettingsKeys.contains("callsign")) {
        settings.m_callsign = *response.getPacketModSettings()->getCallsign();
    }
    if (channelSettingsKeys.contains("to")) {
        settings.m_to = *response.getPacketModSettings()->getTo();
    }
    if (channelSettingsKeys.contains("via")) {
        settings.m_via = *response.getPacketModSettings()->getVia();
    }
    if (channelSettingsKeys.contains("data")) {
        settings.m_data = *response.getPacketModSettings()->getData();
    }
    if (channelSettingsKeys.contains("bpf")) {
        settings.m_bpf = response.getPacketModSettings()->getBpf() != 0;
    }
    if (channelSettingsKeys.contains("bpfLowCutoff")) {
        settings.m_bpfLowCutoff = response.getPacketModSettings()->getBpfLowCutoff();
    }
    if (channelSettingsKeys.contains("bpfHighCutoff")) {
        settings.m_bpfHighCutoff = response.getPacketModSettings()->getBpfHighCutoff();
    }
    if (channelSettingsKeys.contains("bpfTaps")) {
        settings.m_bpfTaps = response.getPacketModSettings()->getBpfTaps();
    }
    if (channelSettingsKeys.contains("scramble")) {
        settings.m_scramble = response.getPacketModSettings()->getScramble() != 0;
    }
    if (channelSettingsKeys.contains("polynomial")) {
        settings.m_polynomial = response.getPacketModSettings()->getPolynomial();
    }
    if (channelSettingsKeys.contains("beta")) {
        settings.m_beta = response.getPacketModSettings()->getBeta();
    }
    if (channelSettingsKeys.contains("symbolSpan")) {
        settings.m_symbolSpan = response.getPacketModSettings()->getSymbolSpan();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getPacketModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getPacketModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getPacketModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getPacketModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getPacketModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getPacketModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getPacketModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getPacketModSettings()->getReverseApiChannelIndex();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getPacketModSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getPacketModSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getPacketModSettings()->getUdpPort();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getPacketModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getPacketModSettings()->getRollupState());
    }
}

int PacketMod::webapiReportGet(
                SWGSDRangel::SWGChannelReport& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setPacketModReport(new SWGSDRangel::SWGPacketModReport());
    response.getPacketModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

int PacketMod::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGPacketModActions *swgPacketModActions = query.getPacketModActions();

    if (swgPacketModActions)
    {
        if (channelActionsKeys.contains("tx"))
        {
            if (swgPacketModActions->getTx() != 0)
            {
                if (channelActionsKeys.contains("payload")
                   && (swgPacketModActions->getPayload()->getCallsign())
                   && (swgPacketModActions->getPayload()->getTo())
                   && (swgPacketModActions->getPayload()->getVia())
                   && (swgPacketModActions->getPayload()->getData()))
                {
                    MsgTXPacketData *msg = MsgTXPacketData::create(
                        *swgPacketModActions->getPayload()->getCallsign(),
                        *swgPacketModActions->getPayload()->getTo(),
                        *swgPacketModActions->getPayload()->getVia(),
                        *swgPacketModActions->getPayload()->getData()
                    );
                    m_basebandSource->getInputMessageQueue()->push(msg);
                }
                else
                {
                    MsgTx *msg = MsgTx::create();
                    m_basebandSource->getInputMessageQueue()->push(msg);
                }

                if (getMessageQueueToGUI())
                {
                    MsgReportTx *msg = MsgReportTx::create();
                    getMessageQueueToGUI()->push(msg);
                }

                return 202;
            }
            else
            {
                errorMessage = "Packet must contain tx action";
                return 400;
            }
        }
        else
        {
            errorMessage = "Unknown action";
            return 400;
        }
    }
    else
    {
        errorMessage = "Missing PacketModActions in query";
        return 400;
    }
}

void PacketMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const PacketModSettings& settings)
{
    response.getPacketModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getPacketModSettings()->setModulation((int) settings.m_modulation);
    response.getPacketModSettings()->setBaud(settings.m_baud);
    response.getPacketModSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getPacketModSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getPacketModSettings()->setGain(settings.m_gain);
    response.getPacketModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getPacketModSettings()->setRepeat(settings.m_repeat ? 1 : 0);
    response.getPacketModSettings()->setRepeatDelay(settings.m_repeatDelay);
    response.getPacketModSettings()->setRepeatCount(settings.m_repeatCount);
    response.getPacketModSettings()->setRampUpBits(settings.m_rampUpBits);
    response.getPacketModSettings()->setRampDownBits(settings.m_rampDownBits);
    response.getPacketModSettings()->setRampRange(settings.m_rampRange);
    response.getPacketModSettings()->setModulateWhileRamping(settings.m_modulateWhileRamping ? 1 : 0);
    response.getPacketModSettings()->setMarkFrequency(settings.m_markFrequency);
    response.getPacketModSettings()->setSpaceFrequency(settings.m_spaceFrequency);
    response.getPacketModSettings()->setAx25PreFlags(settings.m_ax25PreFlags);
    response.getPacketModSettings()->setAx25PostFlags(settings.m_ax25PostFlags);
    response.getPacketModSettings()->setAx25Control(settings.m_ax25Control);
    response.getPacketModSettings()->setAx25Pid(settings.m_ax25PID);
    response.getPacketModSettings()->setPreEmphasis(settings.m_preEmphasis ? 1 : 0);
    response.getPacketModSettings()->setPreEmphasisTau(settings.m_preEmphasisTau);
    response.getPacketModSettings()->setPreEmphasisHighFreq(settings.m_preEmphasisHighFreq);
    response.getPacketModSettings()->setLpfTaps(settings.m_lpfTaps);
    response.getPacketModSettings()->setBbNoise(settings.m_bbNoise ? 1 : 0);
    response.getPacketModSettings()->setRfNoise(settings.m_rfNoise ? 1 : 0);
    response.getPacketModSettings()->setWriteToFile(settings.m_writeToFile ? 1 : 0);
    response.getPacketModSettings()->setSpectrumRate(settings.m_spectrumRate);

    if (response.getPacketModSettings()->getCallsign()) {
        *response.getPacketModSettings()->getCallsign() = settings.m_callsign;
    } else {
        response.getPacketModSettings()->setCallsign(new QString(settings.m_callsign));
    }

    if (response.getPacketModSettings()->getTo()) {
        *response.getPacketModSettings()->getTo() = settings.m_to;
    } else {
        response.getPacketModSettings()->setTo(new QString(settings.m_to));
    }

    if (response.getPacketModSettings()->getVia()) {
        *response.getPacketModSettings()->getVia() = settings.m_via;
    } else {
        response.getPacketModSettings()->setVia(new QString(settings.m_via));
    }

    if (response.getPacketModSettings()->getData()) {
        *response.getPacketModSettings()->getData() = settings.m_data;
    } else {
        response.getPacketModSettings()->setData(new QString(settings.m_data));
    }

    response.getPacketModSettings()->setBpf(settings.m_bpf ? 1 : 0);
    response.getPacketModSettings()->setBpfLowCutoff(settings.m_bpfLowCutoff);
    response.getPacketModSettings()->setBpfHighCutoff(settings.m_bpfHighCutoff);
    response.getPacketModSettings()->setBpfTaps(settings.m_bpfTaps);
    response.getPacketModSettings()->setScramble(settings.m_scramble ? 1 : 0);
    response.getPacketModSettings()->setPolynomial(settings.m_polynomial);
    response.getPacketModSettings()->setPulseShaping(settings.m_pulseShaping ? 1 : 0);
    response.getPacketModSettings()->setBeta(settings.m_beta);
    response.getPacketModSettings()->setSymbolSpan(settings.m_symbolSpan);
    response.getPacketModSettings()->setUdpEnabled(settings.m_udpEnabled);
    response.getPacketModSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getPacketModSettings()->setUdpPort(settings.m_udpPort);

    response.getPacketModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getPacketModSettings()->getTitle()) {
        *response.getPacketModSettings()->getTitle() = settings.m_title;
    } else {
        response.getPacketModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getPacketModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getPacketModSettings()->getReverseApiAddress()) {
        *response.getPacketModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getPacketModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getPacketModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getPacketModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getPacketModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getPacketModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getPacketModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getPacketModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getPacketModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getPacketModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getPacketModSettings()->setRollupState(swgRollupState);
        }
    }
}

void PacketMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getPacketModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getPacketModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void PacketMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const PacketModSettings& settings, bool force)
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

void PacketMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const PacketModSettings& settings,
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

void PacketMod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const PacketModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setPacketModSettings(new SWGSDRangel::SWGPacketModSettings());
    SWGSDRangel::SWGPacketModSettings *swgPacketModSettings = swgChannelSettings->getPacketModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgPacketModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("modulation") || force) {
        swgPacketModSettings->setModulation((int) settings.m_modulation);
    }
    if (channelSettingsKeys.contains("baud") || force) {
        swgPacketModSettings->setBaud((int) settings.m_baud);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgPacketModSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgPacketModSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("gain") || force) {
        swgPacketModSettings->setGain(settings.m_gain);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgPacketModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("repeat") || force) {
        swgPacketModSettings->setRepeat(settings.m_repeat ? 1 : 0);
    }
    if (channelSettingsKeys.contains("repeatDelay") || force) {
        swgPacketModSettings->setRepeatDelay(settings.m_repeatDelay);
    }
    if (channelSettingsKeys.contains("repeatCount") || force) {
        swgPacketModSettings->setRepeatCount(settings.m_repeatCount);
    }
    if (channelSettingsKeys.contains("rampUpBits")) {
        swgPacketModSettings->setRampUpBits(settings.m_rampUpBits);
    }
    if (channelSettingsKeys.contains("rampDownBits")) {
        swgPacketModSettings->setRampDownBits(settings.m_rampDownBits);
    }
    if (channelSettingsKeys.contains("rampRange")) {
        swgPacketModSettings->setRampRange(settings.m_rampRange);
    }
    if (channelSettingsKeys.contains("modulateWhileRamping")) {
        swgPacketModSettings->setRampRange(settings.m_modulateWhileRamping ? 1 : 0);
    }
    if (channelSettingsKeys.contains("markFrequency")) {
        swgPacketModSettings->setMarkFrequency(settings.m_markFrequency);
    }
    if (channelSettingsKeys.contains("spaceFrequency")) {
        swgPacketModSettings->setSpaceFrequency(settings.m_spaceFrequency);
    }
    if (channelSettingsKeys.contains("ax25PreFlags") || force) {
        swgPacketModSettings->setAx25PreFlags(settings.m_ax25PreFlags);
    }
    if (channelSettingsKeys.contains("ax25PostFlags") || force) {
        swgPacketModSettings->setAx25PostFlags(settings.m_ax25PostFlags);
    }
    if (channelSettingsKeys.contains("ax25Control")) {
        swgPacketModSettings->setAx25Control(settings.m_ax25Control);
    }
    if (channelSettingsKeys.contains("ax25PID")) {
        swgPacketModSettings->setAx25Pid(settings.m_ax25PID);
    }
    if (channelSettingsKeys.contains("preEmphasis") || force) {
        swgPacketModSettings->setPreEmphasis(settings.m_preEmphasis ? 1 : 0);
    }
    if (channelSettingsKeys.contains("preEmphasisTau") || force) {
        swgPacketModSettings->setPreEmphasisTau(settings.m_preEmphasisTau);
    }
    if (channelSettingsKeys.contains("preEmphasisHighFreq") || force) {
        swgPacketModSettings->setPreEmphasisHighFreq(settings.m_preEmphasisHighFreq);
    }
    if (channelSettingsKeys.contains("lpfTaps")) {
        swgPacketModSettings->setLpfTaps(settings.m_lpfTaps);
    }
    if (channelSettingsKeys.contains("bbNoise")) {
        swgPacketModSettings->setBbNoise(settings.m_bbNoise ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rfNoise")) {
        swgPacketModSettings->setRfNoise(settings.m_rfNoise ? 1 : 0);
    }
    if (channelSettingsKeys.contains("writeToFile")) {
        swgPacketModSettings->setWriteToFile(settings.m_writeToFile ? 1 : 0);
    }
    if (channelSettingsKeys.contains("spectrumRate")) {
        swgPacketModSettings->setSpectrumRate(settings.m_spectrumRate);
    }
    if (channelSettingsKeys.contains("callsign")) {
        swgPacketModSettings->setCallsign(new QString(settings.m_callsign));
    }
    if (channelSettingsKeys.contains("to")) {
        swgPacketModSettings->setTo(new QString(settings.m_to));
    }
    if (channelSettingsKeys.contains("via")) {
        swgPacketModSettings->setVia(new QString(settings.m_via));
    }
    if (channelSettingsKeys.contains("data")) {
        swgPacketModSettings->setData(new QString(settings.m_data));
    }
    if (channelSettingsKeys.contains("bpf") || force) {
        swgPacketModSettings->setBpf(settings.m_bpf ? 1 : 0);
    }
    if (channelSettingsKeys.contains("bpfLowCutoff") || force) {
        swgPacketModSettings->setBpfLowCutoff(settings.m_bpfLowCutoff);
    }
    if (channelSettingsKeys.contains("bpfHighCutoff") || force) {
        swgPacketModSettings->setBpfHighCutoff(settings.m_bpfHighCutoff);
    }
    if (channelSettingsKeys.contains("bpfTaps")) {
        swgPacketModSettings->setBpfTaps(settings.m_bpfTaps);
    }
    if (channelSettingsKeys.contains("scramble")) {
        swgPacketModSettings->setScramble(settings.m_scramble ? 1 : 0);
    }
    if (channelSettingsKeys.contains("polynomial")) {
        swgPacketModSettings->setPolynomial(settings.m_polynomial);
    }
    if (channelSettingsKeys.contains("beta")) {
        swgPacketModSettings->setBeta(settings.m_beta);
    }
    if (channelSettingsKeys.contains("symbolSpan")) {
        swgPacketModSettings->setSymbolSpan(settings.m_symbolSpan);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgPacketModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgPacketModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgPacketModSettings->setStreamIndex(settings.m_streamIndex);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgPacketModSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgPacketModSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgPacketModSettings->setUdpPort(settings.m_udpPort);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgPacketModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgPacketModSettings->setRollupState(swgRollupState);
    }
}

void PacketMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "PacketMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("PacketMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double PacketMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void PacketMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t PacketMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

int PacketMod::getSourceChannelSampleRate() const
{
    return m_basebandSource->getSourceChannelSampleRate();
}

void PacketMod::openUDP(const PacketModSettings& settings)
{
    closeUDP();
    m_udpSocket = new QUdpSocket();
    if (!m_udpSocket->bind(QHostAddress(settings.m_udpAddress), settings.m_udpPort))
        qCritical() << "PacketMod::openUDP: Failed to bind to port " << settings.m_udpAddress << ":" << settings.m_udpPort << ". Error: " << m_udpSocket->error();
    else
        qDebug() << "PacketMod::openUDP: Listening for packets on " << settings.m_udpAddress << ":" << settings.m_udpPort;
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &PacketMod::udpRx);
}

void PacketMod::closeUDP()
{
    if (m_udpSocket != nullptr)
    {
        disconnect(m_udpSocket, &QUdpSocket::readyRead, this, &PacketMod::udpRx);
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

void PacketMod::udpRx()
{
    while (m_udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        MsgTXPacketBytes *msg = MsgTXPacketBytes::create(datagram.data());
        m_basebandSource->getInputMessageQueue()->push(msg);
    }
}
