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
#include "SWGPSK31ModReport.h"
#include "SWGPSK31ModActions.h"

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

#include "psk31modbaseband.h"
#include "psk31mod.h"

MESSAGE_CLASS_DEFINITION(PSK31::MsgConfigurePSK31, Message)
MESSAGE_CLASS_DEFINITION(PSK31::MsgTx, Message)
MESSAGE_CLASS_DEFINITION(PSK31::MsgReportTx, Message)
MESSAGE_CLASS_DEFINITION(PSK31::MsgTXText, Message)

const char* const PSK31::m_channelIdURI = "sdrangel.channeltx.modpsk31";
const char* const PSK31::m_channelId = "PSK31Mod";

PSK31::PSK31(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF),
    m_sampleRate(48000),
    m_udpSocket(nullptr)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new PSK31Baseband();
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
        &PSK31::networkManagerFinished
    );
}

PSK31::~PSK31()
{
    closeUDP();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &PSK31::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void PSK31::setDeviceAPI(DeviceAPI *deviceAPI)
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

void PSK31::start()
{
    qDebug("PSK31::start");
    m_basebandSource->reset();
    m_thread->start();
}

void PSK31::stop()
{
    qDebug("PSK31::stop");
    m_thread->exit();
    m_thread->wait();
}

void PSK31::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool PSK31::handleMessage(const Message& cmd)
{
    if (MsgConfigurePSK31::match(cmd))
    {
        MsgConfigurePSK31& cfg = (MsgConfigurePSK31&) cmd;
        qDebug() << "PSK31::handleMessage: MsgConfigurePSK31";

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
        qDebug() << "PSK31::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "PSK31::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

void PSK31::setCenterFrequency(qint64 frequency)
{
    PSK31Settings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigurePSK31 *msgToGUI = MsgConfigurePSK31::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void PSK31::applySettings(const PSK31Settings& settings, bool force)
{
    qDebug() << "PSK31::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_baud: " << settings.m_baud
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_gain: " << settings.m_gain
            << " m_channelMute: " << settings.m_channelMute
            << " m_repeat: " << settings.m_repeat
            << " m_repeatCount: " << settings.m_repeatCount
            << " m_text: " << settings.m_text
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

    PSK31Baseband::MsgConfigurePSK31Baseband *msg = PSK31Baseband::MsgConfigurePSK31Baseband::create(settings, force);
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

QByteArray PSK31::serialize() const
{
    return m_settings.serialize();
}

bool PSK31::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigurePSK31 *msg = MsgConfigurePSK31::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

void PSK31::sendSampleRateToDemodAnalyzer()
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

int PSK31::webapiSettingsGet(
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setPsk31ModSettings(new SWGSDRangel::SWGPSK31ModSettings());
    response.getPsk31ModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int PSK31::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int PSK31::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    PSK31Settings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigurePSK31 *msg = MsgConfigurePSK31::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigurePSK31 *msgToGUI = MsgConfigurePSK31::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void PSK31::webapiUpdateChannelSettings(
        PSK31Settings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getPsk31ModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getPsk31ModSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("gain")) {
        settings.m_gain = response.getPsk31ModSettings()->getGain();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getPsk31ModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("repeat")) {
        settings.m_repeat = response.getPsk31ModSettings()->getRepeat() != 0;
    }
    if (channelSettingsKeys.contains("repeatCount")) {
        settings.m_repeatCount = response.getPsk31ModSettings()->getRepeatCount();
    }
    if (channelSettingsKeys.contains("lpfTaps")) {
        settings.m_lpfTaps = response.getPsk31ModSettings()->getLpfTaps();
    }
    if (channelSettingsKeys.contains("rfNoise")) {
        settings.m_rfNoise = response.getPsk31ModSettings()->getRfNoise() != 0;
    }
    if (channelSettingsKeys.contains("text")) {
        settings.m_text = *response.getPsk31ModSettings()->getText();
    }
    if (channelSettingsKeys.contains("beta")) {
        settings.m_beta = response.getPsk31ModSettings()->getBeta();
    }
    if (channelSettingsKeys.contains("symbolSpan")) {
        settings.m_symbolSpan = response.getPsk31ModSettings()->getSymbolSpan();
    }
    if (channelSettingsKeys.contains("prefixCRLF")) {
        settings.m_prefixCRLF = response.getPsk31ModSettings()->getPrefixCrlf();
    }
    if (channelSettingsKeys.contains("postfixCRLF")) {
        settings.m_postfixCRLF = response.getPsk31ModSettings()->getPostfixCrlf();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getPsk31ModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getPsk31ModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getPsk31ModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getPsk31ModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getPsk31ModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getPsk31ModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getPsk31ModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getPsk31ModSettings()->getReverseApiChannelIndex();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getPsk31ModSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getPsk31ModSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getPsk31ModSettings()->getUdpPort();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getPsk31ModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getPsk31ModSettings()->getRollupState());
    }
}

int PSK31::webapiReportGet(
                SWGSDRangel::SWGChannelReport& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setPsk31ModReport(new SWGSDRangel::SWGPSK31ModReport());
    response.getPsk31ModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

int PSK31::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGPSK31ModActions *swgPSK31Actions = query.getPsk31ModActions();

    if (swgPSK31Actions)
    {
        if (channelActionsKeys.contains("tx"))
        {
            if (swgPSK31Actions->getTx() != 0)
            {
                if (channelActionsKeys.contains("payload")
                   && (swgPSK31Actions->getPayload()->getText()))
                {
                    MsgTXText *msg = MsgTXText::create(
                        *swgPSK31Actions->getPayload()->getText()
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
            errorMessage = "Unknown PSK31Mod action";
            return 400;
        }
    }
    else
    {
        errorMessage = "Missing PSK31ModActions in query";
        return 400;
    }
    return 0;
}

void PSK31::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const PSK31Settings& settings)
{
    response.getPsk31ModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getPsk31ModSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getPsk31ModSettings()->setGain(settings.m_gain);
    response.getPsk31ModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getPsk31ModSettings()->setRepeat(settings.m_repeat ? 1 : 0);
    response.getPsk31ModSettings()->setRepeatCount(settings.m_repeatCount);
    response.getPsk31ModSettings()->setLpfTaps(settings.m_lpfTaps);
    response.getPsk31ModSettings()->setRfNoise(settings.m_rfNoise ? 1 : 0);

    if (response.getPsk31ModSettings()->getText()) {
        *response.getPsk31ModSettings()->getText() = settings.m_text;
    } else {
        response.getPsk31ModSettings()->setText(new QString(settings.m_text));
    }

    response.getPsk31ModSettings()->setPulseShaping(settings.m_pulseShaping ? 1 : 0);
    response.getPsk31ModSettings()->setBeta(settings.m_beta);
    response.getPsk31ModSettings()->setSymbolSpan(settings.m_symbolSpan);

    response.getPsk31ModSettings()->setPrefixCrlf(settings.m_prefixCRLF);
    response.getPsk31ModSettings()->setPostfixCrlf(settings.m_postfixCRLF);

    response.getPsk31ModSettings()->setUdpEnabled(settings.m_udpEnabled);
    response.getPsk31ModSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getPsk31ModSettings()->setUdpPort(settings.m_udpPort);

    response.getPsk31ModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getPsk31ModSettings()->getTitle()) {
        *response.getPsk31ModSettings()->getTitle() = settings.m_title;
    } else {
        response.getPsk31ModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getPsk31ModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getPsk31ModSettings()->getReverseApiAddress()) {
        *response.getPsk31ModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getPsk31ModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getPsk31ModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getPsk31ModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getPsk31ModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getPsk31ModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getPsk31ModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getPsk31ModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getPsk31ModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getPsk31ModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getPsk31ModSettings()->setRollupState(swgRollupState);
        }
    }
}

void PSK31::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getPsk31ModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getPsk31ModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void PSK31::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const PSK31Settings& settings, bool force)
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

void PSK31::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const PSK31Settings& settings,
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

void PSK31::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const PSK31Settings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setPsk31ModSettings(new SWGSDRangel::SWGPSK31ModSettings());
    SWGSDRangel::SWGPSK31ModSettings *swgPSK31ModSettings = swgChannelSettings->getPsk31ModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgPSK31ModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgPSK31ModSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("gain") || force) {
        swgPSK31ModSettings->setGain(settings.m_gain);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgPSK31ModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("repeat") || force) {
        swgPSK31ModSettings->setRepeat(settings.m_repeat ? 1 : 0);
    }
    if (channelSettingsKeys.contains("repeatCount") || force) {
        swgPSK31ModSettings->setRepeatCount(settings.m_repeatCount);
    }
    if (channelSettingsKeys.contains("lpfTaps")) {
        swgPSK31ModSettings->setLpfTaps(settings.m_lpfTaps);
    }
    if (channelSettingsKeys.contains("rfNoise")) {
        swgPSK31ModSettings->setRfNoise(settings.m_rfNoise ? 1 : 0);
    }
    if (channelSettingsKeys.contains("text")) {
        swgPSK31ModSettings->setText(new QString(settings.m_text));
    }
    if (channelSettingsKeys.contains("beta")) {
        swgPSK31ModSettings->setBeta(settings.m_beta);
    }
    if (channelSettingsKeys.contains("symbolSpan")) {
        swgPSK31ModSettings->setSymbolSpan(settings.m_symbolSpan);
    }
    if (channelSettingsKeys.contains("prefixCRLF")) {
        swgPSK31ModSettings->setPrefixCrlf(settings.m_prefixCRLF);
    }
    if (channelSettingsKeys.contains("postfixCRLF")) {
        swgPSK31ModSettings->setPostfixCrlf(settings.m_postfixCRLF);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgPSK31ModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgPSK31ModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgPSK31ModSettings->setStreamIndex(settings.m_streamIndex);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgPSK31ModSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgPSK31ModSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgPSK31ModSettings->setUdpPort(settings.m_udpPort);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgPSK31ModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgPSK31ModSettings->setRollupState(swgRollupState);
    }
}

void PSK31::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "PSK31::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("PSK31::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double PSK31::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void PSK31::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t PSK31::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

int PSK31::getSourceChannelSampleRate() const
{
    return m_basebandSource->getSourceChannelSampleRate();
}

void PSK31::openUDP(const PSK31Settings& settings)
{
    closeUDP();
    m_udpSocket = new QUdpSocket();
    if (!m_udpSocket->bind(QHostAddress(settings.m_udpAddress), settings.m_udpPort))
        qCritical() << "PSK31::openUDP: Failed to bind to port " << settings.m_udpAddress << ":" << settings.m_udpPort << ". Error: " << m_udpSocket->error();
    else
        qDebug() << "PSK31::openUDP: Listening for text on " << settings.m_udpAddress << ":" << settings.m_udpPort;
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &PSK31::udpRx);
}

void PSK31::closeUDP()
{
    if (m_udpSocket != nullptr)
    {
        disconnect(m_udpSocket, &QUdpSocket::readyRead, this, &PSK31::udpRx);
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

void PSK31::udpRx()
{
    while (m_udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        MsgTXText *msg = MsgTXText::create(QString(datagram.data()));
        m_basebandSource->getInputMessageQueue()->push(msg);
    }
}

void PSK31::setMessageQueueToGUI(MessageQueue* queue) {
    ChannelAPI::setMessageQueueToGUI(queue);
    m_basebandSource->setMessageQueueToGUI(queue);
}
