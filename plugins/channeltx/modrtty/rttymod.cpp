///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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
#include "SWGRTTYModReport.h"
#include "SWGRTTYModActions.h"

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

#include "rttymodbaseband.h"
#include "rttymod.h"

MESSAGE_CLASS_DEFINITION(RttyMod::MsgConfigureRttyMod, Message)
MESSAGE_CLASS_DEFINITION(RttyMod::MsgTx, Message)
MESSAGE_CLASS_DEFINITION(RttyMod::MsgReportTx, Message)
MESSAGE_CLASS_DEFINITION(RttyMod::MsgTXText, Message)

const char* const RttyMod::m_channelIdURI = "sdrangel.channeltx.modrtty";
const char* const RttyMod::m_channelId = "RTTYMod";

RttyMod::RttyMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF),
    m_sampleRate(48000),
    m_udpSocket(nullptr)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new RttyModBaseband();
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
        &RttyMod::networkManagerFinished
    );
}

RttyMod::~RttyMod()
{
    closeUDP();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RttyMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void RttyMod::setDeviceAPI(DeviceAPI *deviceAPI)
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

void RttyMod::start()
{
    qDebug("RttyMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void RttyMod::stop()
{
    qDebug("RttyMod::stop");
    m_thread->exit();
    m_thread->wait();
}

void RttyMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool RttyMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureRttyMod::match(cmd))
    {
        MsgConfigureRttyMod& cfg = (MsgConfigureRttyMod&) cmd;
        qDebug() << "RttyMod::handleMessage: MsgConfigureRttyMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgTx::match(cmd))
    {
        MsgTx* msg = new MsgTx((const MsgTx&)cmd);
        m_basebandSource->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (MsgTXText::match(cmd))
    {
        MsgTXText* msg = new MsgTXText((const MsgTXText&)cmd);
        m_basebandSource->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "RttyMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "RttyMod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

void RttyMod::setCenterFrequency(qint64 frequency)
{
    RttyModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRttyMod *msgToGUI = MsgConfigureRttyMod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void RttyMod::applySettings(const RttyModSettings& settings, bool force)
{
    qDebug() << "RttyMod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_baud: " << settings.m_baud
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_frequencyShift: " << settings.m_frequencyShift
            << " m_gain: " << settings.m_gain
            << " m_channelMute: " << settings.m_channelMute
            << " m_repeat: " << settings.m_repeat
            << " m_repeatCount: " << settings.m_repeatCount
            << " m_text: " << settings.m_text
            << " m_characterSet: " << settings.m_characterSet
            << " m_unshiftOnSpace: " << settings.m_unshiftOnSpace
            << " m_msbFirst: " << settings.m_msbFirst
            << " m_spaceHigh: " << settings.m_spaceHigh
            << " m_prefixCRLF: " << settings.m_prefixCRLF
            << " m_postfixCRLF: " << settings.m_postfixCRLF
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

    if ((settings.m_baud != m_settings.m_baud) || force) {
        reverseAPIKeys.append("baud");
    }

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }

    if ((settings.m_frequencyShift != m_settings.m_frequencyShift) || force) {
        reverseAPIKeys.append("frequencyShift");
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

    if ((settings.m_repeatCount != m_settings.m_repeatCount) || force) {
        reverseAPIKeys.append("repeatCount");
    }

    if ((settings.m_lpfTaps != m_settings.m_lpfTaps) || force) {
        reverseAPIKeys.append("lpfTaps");
    }

    if ((settings.m_rfNoise != m_settings.m_rfNoise) || force) {
        reverseAPIKeys.append("rfNoise");
    }

    if ((settings.m_text != m_settings.m_text) || force) {
        reverseAPIKeys.append("text");
    }

    if ((settings.m_beta != m_settings.m_beta) || force) {
        reverseAPIKeys.append("beta");
    }

    if ((settings.m_symbolSpan != m_settings.m_symbolSpan) || force) {
        reverseAPIKeys.append("symbolSpan");
    }

    if ((settings.m_characterSet != m_settings.m_characterSet) || force) {
        reverseAPIKeys.append("characterSet");
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

    if ((settings.m_prefixCRLF != m_settings.m_prefixCRLF) || force) {
        reverseAPIKeys.append("prefixCRLF");
    }

    if ((settings.m_postfixCRLF != m_settings.m_postfixCRLF) || force) {
        reverseAPIKeys.append("postfixCRLF");
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

    RttyModBaseband::MsgConfigureRttyModBaseband *msg = RttyModBaseband::MsgConfigureRttyModBaseband::create(settings, force);
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

QByteArray RttyMod::serialize() const
{
    return m_settings.serialize();
}

bool RttyMod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureRttyMod *msg = MsgConfigureRttyMod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

void RttyMod::sendSampleRateToDemodAnalyzer()
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

int RttyMod::webapiSettingsGet(
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setRttyModSettings(new SWGSDRangel::SWGRTTYModSettings());
    response.getRttyModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int RttyMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int RttyMod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    RttyModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureRttyMod *msg = MsgConfigureRttyMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRttyMod *msgToGUI = MsgConfigureRttyMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void RttyMod::webapiUpdateChannelSettings(
        RttyModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getRttyModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("baud")) {
        settings.m_baud = response.getRttyModSettings()->getBaud();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getRttyModSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("frequencyShift")) {
        settings.m_frequencyShift = response.getRttyModSettings()->getFrequencyShift();
    }
    if (channelSettingsKeys.contains("gain")) {
        settings.m_gain = response.getRttyModSettings()->getGain();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getRttyModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("repeat")) {
        settings.m_repeat = response.getRttyModSettings()->getRepeat() != 0;
    }
    if (channelSettingsKeys.contains("repeatCount")) {
        settings.m_repeatCount = response.getRttyModSettings()->getRepeatCount();
    }
    if (channelSettingsKeys.contains("lpfTaps")) {
        settings.m_lpfTaps = response.getRttyModSettings()->getLpfTaps();
    }
    if (channelSettingsKeys.contains("rfNoise")) {
        settings.m_rfNoise = response.getRttyModSettings()->getRfNoise() != 0;
    }
    if (channelSettingsKeys.contains("text")) {
        settings.m_text = *response.getRttyModSettings()->getText();
    }
    if (channelSettingsKeys.contains("beta")) {
        settings.m_beta = response.getRttyModSettings()->getBeta();
    }
    if (channelSettingsKeys.contains("symbolSpan")) {
        settings.m_symbolSpan = response.getRttyModSettings()->getSymbolSpan();
    }
    if (channelSettingsKeys.contains("characterSet")) {
        settings.m_characterSet = (Baudot::CharacterSet) response.getRttyModSettings()->getCharacterSet();
    }
    if (channelSettingsKeys.contains("unshiftOnSpace")) {
        settings.m_unshiftOnSpace = response.getRttyModSettings()->getUnshiftOnSpace();
    }
    if (channelSettingsKeys.contains("msbFirst")) {
        settings.m_msbFirst = response.getRttyModSettings()->getMsbFirst();
    }
    if (channelSettingsKeys.contains("spaceHigh")) {
        settings.m_spaceHigh = response.getRttyModSettings()->getSpaceHigh();
    }
    if (channelSettingsKeys.contains("prefixCRLF")) {
        settings.m_prefixCRLF = response.getRttyModSettings()->getPrefixCrlf();
    }
    if (channelSettingsKeys.contains("postfixCRLF")) {
        settings.m_postfixCRLF = response.getRttyModSettings()->getPostfixCrlf();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getRttyModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getRttyModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getRttyModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRttyModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRttyModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRttyModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getRttyModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getRttyModSettings()->getReverseApiChannelIndex();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getRttyModSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getRttyModSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getRttyModSettings()->getUdpPort();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getRttyModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getRttyModSettings()->getRollupState());
    }
}

int RttyMod::webapiReportGet(
                SWGSDRangel::SWGChannelReport& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setRttyModReport(new SWGSDRangel::SWGRTTYModReport());
    response.getRttyModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

int RttyMod::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGRTTYModActions *swgRttyModActions = query.getRttyModActions();

    if (swgRttyModActions)
    {
        if (channelActionsKeys.contains("tx"))
        {
            if (swgRttyModActions->getTx() != 0)
            {
                if (channelActionsKeys.contains("payload")
                   && (swgRttyModActions->getPayload()->getText()))
                {
                    MsgTXText *msg = MsgTXText::create(
                        *swgRttyModActions->getPayload()->getText()
                    );
                    m_basebandSource->getInputMessageQueue()->push(msg);
                }
                else
                {
                    MsgTx *msg = MsgTx::create();
                    m_basebandSource->getInputMessageQueue()->push(msg);
                }

                return 202;
            }
            else
            {
                errorMessage = "Must contain tx action";
                return 400;
            }
        }
        else
        {
            errorMessage = "Unknown RTTYMod action";
            return 400;
        }
    }
    else
    {
        errorMessage = "Missing RTTYModActions in query";
        return 400;
    }
    return 0;
}

void RttyMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const RttyModSettings& settings)
{
    response.getRttyModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getRttyModSettings()->setBaud(settings.m_baud);
    response.getRttyModSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getRttyModSettings()->setFrequencyShift(settings.m_frequencyShift);
    response.getRttyModSettings()->setGain(settings.m_gain);
    response.getRttyModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getRttyModSettings()->setRepeat(settings.m_repeat ? 1 : 0);
    response.getRttyModSettings()->setRepeatCount(settings.m_repeatCount);
    response.getRttyModSettings()->setLpfTaps(settings.m_lpfTaps);
    response.getRttyModSettings()->setRfNoise(settings.m_rfNoise ? 1 : 0);

    if (response.getRttyModSettings()->getText()) {
        *response.getRttyModSettings()->getText() = settings.m_text;
    } else {
        response.getRttyModSettings()->setText(new QString(settings.m_text));
    }

    response.getRttyModSettings()->setPulseShaping(settings.m_pulseShaping ? 1 : 0);
    response.getRttyModSettings()->setBeta(settings.m_beta);
    response.getRttyModSettings()->setSymbolSpan(settings.m_symbolSpan);

    response.getRttyModSettings()->setCharacterSet((int) settings.m_characterSet);
    response.getRttyModSettings()->setSymbolSpan(settings.m_symbolSpan);
    response.getRttyModSettings()->setUnshiftOnSpace(settings.m_unshiftOnSpace);
    response.getRttyModSettings()->setMsbFirst(settings.m_msbFirst);
    response.getRttyModSettings()->setSpaceHigh(settings.m_spaceHigh);
    response.getRttyModSettings()->setPrefixCrlf(settings.m_prefixCRLF);
    response.getRttyModSettings()->setPostfixCrlf(settings.m_postfixCRLF);

    response.getRttyModSettings()->setUdpEnabled(settings.m_udpEnabled);
    response.getRttyModSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getRttyModSettings()->setUdpPort(settings.m_udpPort);

    response.getRttyModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getRttyModSettings()->getTitle()) {
        *response.getRttyModSettings()->getTitle() = settings.m_title;
    } else {
        response.getRttyModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getRttyModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRttyModSettings()->getReverseApiAddress()) {
        *response.getRttyModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRttyModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRttyModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRttyModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getRttyModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getRttyModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getRttyModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getRttyModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getRttyModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getRttyModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getRttyModSettings()->setRollupState(swgRollupState);
        }
    }
}

void RttyMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getRttyModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getRttyModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void RttyMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const RttyModSettings& settings, bool force)
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

void RttyMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const RttyModSettings& settings,
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

void RttyMod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const RttyModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setRttyModSettings(new SWGSDRangel::SWGRTTYModSettings());
    SWGSDRangel::SWGRTTYModSettings *swgRttyModSettings = swgChannelSettings->getRttyModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgRttyModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("baud") || force) {
        swgRttyModSettings->setBaud((int) settings.m_baud);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgRttyModSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("frequencyShift") || force) {
        swgRttyModSettings->setFrequencyShift(settings.m_frequencyShift);
    }
    if (channelSettingsKeys.contains("gain") || force) {
        swgRttyModSettings->setGain(settings.m_gain);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgRttyModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("repeat") || force) {
        swgRttyModSettings->setRepeat(settings.m_repeat ? 1 : 0);
    }
    if (channelSettingsKeys.contains("repeatCount") || force) {
        swgRttyModSettings->setRepeatCount(settings.m_repeatCount);
    }
    if (channelSettingsKeys.contains("lpfTaps")) {
        swgRttyModSettings->setLpfTaps(settings.m_lpfTaps);
    }
    if (channelSettingsKeys.contains("rfNoise")) {
        swgRttyModSettings->setRfNoise(settings.m_rfNoise ? 1 : 0);
    }
    if (channelSettingsKeys.contains("text")) {
        swgRttyModSettings->setText(new QString(settings.m_text));
    }
    if (channelSettingsKeys.contains("beta")) {
        swgRttyModSettings->setBeta(settings.m_beta);
    }
    if (channelSettingsKeys.contains("symbolSpan")) {
        swgRttyModSettings->setSymbolSpan(settings.m_symbolSpan);
    }
    if (channelSettingsKeys.contains("characterSet")) {
        swgRttyModSettings->setCharacterSet((int) settings.m_characterSet);
    }
    if (channelSettingsKeys.contains("unshiftOnSpace")) {
        swgRttyModSettings->setUnshiftOnSpace(settings.m_unshiftOnSpace);
    }
    if (channelSettingsKeys.contains("msbFirst")) {
        swgRttyModSettings->setMsbFirst(settings.m_msbFirst);
    }
    if (channelSettingsKeys.contains("spaceHigh")) {
        swgRttyModSettings->setSpaceHigh(settings.m_spaceHigh);
    }
    if (channelSettingsKeys.contains("prefixCRLF")) {
        swgRttyModSettings->setPrefixCrlf(settings.m_prefixCRLF);
    }
    if (channelSettingsKeys.contains("postfixCRLF")) {
        swgRttyModSettings->setPostfixCrlf(settings.m_postfixCRLF);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgRttyModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgRttyModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgRttyModSettings->setStreamIndex(settings.m_streamIndex);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgRttyModSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgRttyModSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgRttyModSettings->setUdpPort(settings.m_udpPort);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgRttyModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgRttyModSettings->setRollupState(swgRollupState);
    }
}

void RttyMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "RttyMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("RttyMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double RttyMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void RttyMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t RttyMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

int RttyMod::getSourceChannelSampleRate() const
{
    return m_basebandSource->getSourceChannelSampleRate();
}

void RttyMod::openUDP(const RttyModSettings& settings)
{
    closeUDP();
    m_udpSocket = new QUdpSocket();
    if (!m_udpSocket->bind(QHostAddress(settings.m_udpAddress), settings.m_udpPort))
        qCritical() << "RttyMod::openUDP: Failed to bind to port " << settings.m_udpAddress << ":" << settings.m_udpPort << ". Error: " << m_udpSocket->error();
    else
        qDebug() << "RttyMod::openUDP: Listening for text on " << settings.m_udpAddress << ":" << settings.m_udpPort;
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &RttyMod::udpRx);
}

void RttyMod::closeUDP()
{
    if (m_udpSocket != nullptr)
    {
        disconnect(m_udpSocket, &QUdpSocket::readyRead, this, &RttyMod::udpRx);
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

void RttyMod::udpRx()
{
    while (m_udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        MsgTXText *msg = MsgTXText::create(QString(datagram.data()));
        m_basebandSource->getInputMessageQueue()->push(msg);
    }
}

void RttyMod::setMessageQueueToGUI(MessageQueue* queue) {
    ChannelAPI::setMessageQueueToGUI(queue);
    m_basebandSource->setMessageQueueToGUI(queue);
}
