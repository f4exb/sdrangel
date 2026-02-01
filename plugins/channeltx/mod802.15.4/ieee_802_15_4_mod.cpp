///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2021 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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
#include "SWGIEEE_802_15_4_ModReport.h"
#include "SWGIEEE_802_15_4_ModActions.h"

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "util/db.h"
#include "maincore.h"

#include "ieee_802_15_4_modbaseband.h"
#include "ieee_802_15_4_mod.h"

MESSAGE_CLASS_DEFINITION(IEEE_802_15_4_Mod::MsgConfigureIEEE_802_15_4_Mod, Message)
MESSAGE_CLASS_DEFINITION(IEEE_802_15_4_Mod::MsgTxHexString, Message)

const char* const IEEE_802_15_4_Mod::m_channelIdURI = "sdrangel.channeltx.mod802.15.4";
const char* const IEEE_802_15_4_Mod::m_channelId = "IEEE_802_15_4_Mod";

IEEE_802_15_4_Mod::IEEE_802_15_4_Mod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new IEEE_802_15_4_ModBaseband();
    m_basebandSource->setSpectrumSampleSink(&m_spectrumVis);
    m_basebandSource->moveToThread(m_thread);

    applySettings(QStringList(), m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &IEEE_802_15_4_Mod::networkManagerFinished
    );
}

IEEE_802_15_4_Mod::~IEEE_802_15_4_Mod()
{
    // closeUDP();
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &IEEE_802_15_4_Mod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this, true);
    stop();
    delete m_basebandSource;
    delete m_thread;
}

void IEEE_802_15_4_Mod::setDeviceAPI(DeviceAPI *deviceAPI)
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

void IEEE_802_15_4_Mod::start()
{
    qDebug("IEEE_802_15_4_Mod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void IEEE_802_15_4_Mod::stop()
{
    qDebug("IEEE_802_15_4_Mod::stop");
    m_thread->exit();
    m_thread->wait();
}

void IEEE_802_15_4_Mod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool IEEE_802_15_4_Mod::handleMessage(const Message& cmd)
{
    if (MsgConfigureIEEE_802_15_4_Mod::match(cmd))
    {
        MsgConfigureIEEE_802_15_4_Mod& cfg = (MsgConfigureIEEE_802_15_4_Mod&) cmd;
        qDebug() << "IEEE_802_15_4_Mod::handleMessage: MsgConfigureIEEE_802_15_4_Mod";

        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgTxHexString::match(cmd))
    {
        // Forward a copy to baseband
        MsgTxHexString* rep = new MsgTxHexString((MsgTxHexString&)cmd);
        qDebug() << "IEEE_802_15_4_Mod::handleMessage: MsgTxHexString";
        m_basebandSource->getInputMessageQueue()->push(rep);

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "IEEE_802_15_4_Mod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);

        // Forward to GUI
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

void IEEE_802_15_4_Mod::setCenterFrequency(qint64 frequency)
{
    IEEE_802_15_4_ModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList("inputFrequencyOffset"), settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureIEEE_802_15_4_Mod *msgToGUI = MsgConfigureIEEE_802_15_4_Mod::create(QStringList("inputFrequencyOffset"), settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void IEEE_802_15_4_Mod::applySettings(const QStringList& settingsKeys, const IEEE_802_15_4_ModSettings& settings, bool force)
{
    qDebug() << "IEEE_802_15_4_Mod::applySettings:" << settings.getDebugString(settingsKeys, force);

    if (settingsKeys.contains("streamIndex") && m_settings.m_streamIndex != settings.m_streamIndex)
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
    }

    IEEE_802_15_4_ModBaseband::MsgConfigureIEEE_802_15_4_ModBaseband *msg = IEEE_802_15_4_ModBaseband::MsgConfigureIEEE_802_15_4_ModBaseband::create(settingsKeys, settings, force);
    m_basebandSource->getInputMessageQueue()->push(msg);

    if (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI)
    {
        bool fullUpdate = ((settingsKeys.contains("useReverseAPI") && m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (settingsKeys.contains("reverseAPIAddress") && m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (settingsKeys.contains("reverseAPIPort") && m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (settingsKeys.contains("reverseAPIDeviceIndex") && m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (settingsKeys.contains("reverseAPIChannelIndex") && m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, settingsKeys, settings, force);
    }

    m_settings = settings;
}

QByteArray IEEE_802_15_4_Mod::serialize() const
{
    return m_settings.serialize();
}

bool IEEE_802_15_4_Mod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureIEEE_802_15_4_Mod *msg = MsgConfigureIEEE_802_15_4_Mod::create(QStringList(), m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

int IEEE_802_15_4_Mod::webapiSettingsGet(
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setIeee802154ModSettings(new SWGSDRangel::SWGIEEE_802_15_4_ModSettings());
    response.getIeee802154ModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int IEEE_802_15_4_Mod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int IEEE_802_15_4_Mod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    IEEE_802_15_4_ModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureIEEE_802_15_4_Mod *msg = MsgConfigureIEEE_802_15_4_Mod::create(channelSettingsKeys, settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureIEEE_802_15_4_Mod *msgToGUI = MsgConfigureIEEE_802_15_4_Mod::create(channelSettingsKeys, settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void IEEE_802_15_4_Mod::webapiUpdateChannelSettings(
        IEEE_802_15_4_ModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getIeee802154ModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("modulation")) {
        settings.m_modulation = (IEEE_802_15_4_ModSettings::Modulation) response.getIeee802154ModSettings()->getModulation();
    }
    if (channelSettingsKeys.contains("bitRate")) {
        settings.m_bitRate = response.getIeee802154ModSettings()->getBitRate();
    }
    if (channelSettingsKeys.contains("subGHzBand")) {
        settings.m_subGHzBand = response.getIeee802154ModSettings()->getSubGHzBand() != 0;
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getIeee802154ModSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("gain")) {
        settings.m_gain = response.getIeee802154ModSettings()->getGain();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getIeee802154ModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("repeat")) {
        settings.m_repeat = response.getIeee802154ModSettings()->getRepeat() != 0;
    }
    if (channelSettingsKeys.contains("repeatDelay")) {
        settings.m_repeatDelay = response.getIeee802154ModSettings()->getRepeatDelay();
    }
    if (channelSettingsKeys.contains("repeatCount")) {
        settings.m_repeatCount = response.getIeee802154ModSettings()->getRepeatCount();
    }
    if (channelSettingsKeys.contains("rampUpBits")) {
        settings.m_rampUpBits = response.getIeee802154ModSettings()->getRampUpBits();
    }
    if (channelSettingsKeys.contains("rampDownBits")) {
        settings.m_rampDownBits = response.getIeee802154ModSettings()->getRampDownBits();
    }
    if (channelSettingsKeys.contains("rampRange")) {
        settings.m_rampRange = response.getIeee802154ModSettings()->getRampRange();
    }
    if (channelSettingsKeys.contains("modulateWhileRamping")) {
        settings.m_modulateWhileRamping = response.getIeee802154ModSettings()->getModulateWhileRamping() != 0;
    }
    if (channelSettingsKeys.contains("lpfTaps")) {
        settings.m_lpfTaps = response.getIeee802154ModSettings()->getLpfTaps();
    }
    if (channelSettingsKeys.contains("bbNoise")) {
        settings.m_bbNoise = response.getIeee802154ModSettings()->getBbNoise() != 0;
    }
    if (channelSettingsKeys.contains("writeToFile")) {
        settings.m_writeToFile = response.getIeee802154ModSettings()->getWriteToFile() != 0;
    }
    if (channelSettingsKeys.contains("spectrumRate")) {
        settings.m_spectrumRate = response.getIeee802154ModSettings()->getSpectrumRate();
    }
    if (channelSettingsKeys.contains("data")) {
        settings.m_data = *response.getIeee802154ModSettings()->getData();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getIeee802154ModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getIeee802154ModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getIeee802154ModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getIeee802154ModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getIeee802154ModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getIeee802154ModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getIeee802154ModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getIeee802154ModSettings()->getReverseApiChannelIndex();
    }
    if (channelSettingsKeys.contains("scramble")) {
        settings.m_scramble = response.getIeee802154ModSettings()->getScramble() != 0;
    }
    if (channelSettingsKeys.contains("polynomial")) {
        settings.m_polynomial = response.getIeee802154ModSettings()->getPolynomial();
    }
    if (channelSettingsKeys.contains("pulseShaping")) {
        settings.m_pulseShaping = (IEEE_802_15_4_ModSettings::PulseShaping) response.getIeee802154ModSettings()->getPulseShaping();
    }
    if (channelSettingsKeys.contains("beta")) {
        settings.m_beta = response.getIeee802154ModSettings()->getBeta();
    }
    if (channelSettingsKeys.contains("symbolSpan")) {
        settings.m_symbolSpan = response.getIeee802154ModSettings()->getSymbolSpan();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getIeee802154ModSettings()->getUdpEnabled() != 0;
    }
    if (channelSettingsKeys.contains("udpBytesFormat")) {
        settings.m_udpBytesFormat = response.getIeee802154ModSettings()->getUdpBytesFormat() != 0;
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getIeee802154ModSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getIeee802154ModSettings()->getUdpPort();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getIeee802154ModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getIeee802154ModSettings()->getRollupState());
    }
}

int IEEE_802_15_4_Mod::webapiReportGet(
                SWGSDRangel::SWGChannelReport& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setIeee802154ModReport(new SWGSDRangel::SWGIEEE_802_15_4_ModReport());
    response.getIeee802154ModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

int IEEE_802_15_4_Mod::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGIEEE_802_15_4_ModActions *swgIEEE_802_15_4_ModActions = query.getIeee802154ModActions();

    if (swgIEEE_802_15_4_ModActions)
    {
        if (channelActionsKeys.contains("tx") && (swgIEEE_802_15_4_ModActions->getTx() != 0))
        {
            QString *dataP;

            if (channelActionsKeys.contains("data")) {
                dataP = swgIEEE_802_15_4_ModActions->getData();
            } else {
                dataP = &m_settings.m_data;
            }

            if (dataP != nullptr)
            {
                QString data(*dataP);
                IEEE_802_15_4_Mod::MsgTxHexString *msg = IEEE_802_15_4_Mod::MsgTxHexString::create(data);
                m_basebandSource->getInputMessageQueue()->push(msg);
                return 202;
            }
            else
            {
                errorMessage = "Missing data to transmit";
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
        errorMessage = "Missing IEEE_802_15_4_ModActions in query";
        return 400;
    }
}

void IEEE_802_15_4_Mod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const IEEE_802_15_4_ModSettings& settings)
{
    response.getIeee802154ModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getIeee802154ModSettings()->setModulation((int) settings.m_modulation);
    response.getIeee802154ModSettings()->setBitRate(settings.m_bitRate);
    response.getIeee802154ModSettings()->setSubGHzBand(settings.m_subGHzBand ? 1 : 0);
    response.getIeee802154ModSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getIeee802154ModSettings()->setGain(settings.m_gain);
    response.getIeee802154ModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getIeee802154ModSettings()->setRepeat(settings.m_repeat ? 1 : 0);
    response.getIeee802154ModSettings()->setRepeatDelay(settings.m_repeatDelay);
    response.getIeee802154ModSettings()->setRepeatCount(settings.m_repeatCount);
    response.getIeee802154ModSettings()->setRampUpBits(settings.m_rampUpBits);
    response.getIeee802154ModSettings()->setRampDownBits(settings.m_rampDownBits);
    response.getIeee802154ModSettings()->setRampRange(settings.m_rampRange);
    response.getIeee802154ModSettings()->setModulateWhileRamping(settings.m_modulateWhileRamping ? 1 : 0);
    response.getIeee802154ModSettings()->setLpfTaps(settings.m_lpfTaps);
    response.getIeee802154ModSettings()->setBbNoise(settings.m_bbNoise ? 1 : 0);
    response.getIeee802154ModSettings()->setWriteToFile(settings.m_writeToFile ? 1 : 0);
    response.getIeee802154ModSettings()->setSpectrumRate(settings.m_spectrumRate);
    response.getIeee802154ModSettings()->setData(new QString(settings.m_data));
    response.getIeee802154ModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getIeee802154ModSettings()->getTitle()) {
        *response.getIeee802154ModSettings()->getTitle() = settings.m_title;
    } else {
        response.getIeee802154ModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getIeee802154ModSettings()->setStreamIndex(settings.m_streamIndex);
    response.getIeee802154ModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getIeee802154ModSettings()->getReverseApiAddress()) {
        *response.getIeee802154ModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getIeee802154ModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getIeee802154ModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getIeee802154ModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getIeee802154ModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
    response.getIeee802154ModSettings()->setScramble(settings.m_scramble);
    response.getIeee802154ModSettings()->setPolynomial(settings.m_polynomial);
    response.getIeee802154ModSettings()->setPulseShaping(settings.m_pulseShaping ? 1 : 0);
    response.getIeee802154ModSettings()->setBeta(settings.m_beta);
    response.getIeee802154ModSettings()->setSymbolSpan(settings.m_symbolSpan);
    response.getIeee802154ModSettings()->setUdpEnabled(settings.m_udpEnabled ? 1 : 0);
    response.getIeee802154ModSettings()->setUdpBytesFormat(settings.m_udpBytesFormat ? 1 : 0);

    if (response.getIeee802154ModSettings()->getUdpAddress()) {
        *response.getIeee802154ModSettings()->getUdpAddress() = settings.m_udpAddress;
    } else {
        response.getIeee802154ModSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    }

    response.getIeee802154ModSettings()->setUdpPort(settings.m_udpPort);

    if (settings.m_channelMarker)
    {
        if (response.getIeee802154ModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getIeee802154ModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getIeee802154ModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getIeee802154ModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getIeee802154ModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getIeee802154ModSettings()->setRollupState(swgRollupState);
        }
    }
}

void IEEE_802_15_4_Mod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getIeee802154ModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getIeee802154ModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void IEEE_802_15_4_Mod::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const IEEE_802_15_4_ModSettings& settings, bool force)
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

void IEEE_802_15_4_Mod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    const QList<QString>& channelSettingsKeys,
    const IEEE_802_15_4_ModSettings& settings,
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

void IEEE_802_15_4_Mod::webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const IEEE_802_15_4_ModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setIeee802154ModSettings(new SWGSDRangel::SWGIEEE_802_15_4_ModSettings());
    SWGSDRangel::SWGIEEE_802_15_4_ModSettings *swgIEEE_802_15_4_ModSettings = swgChannelSettings->getIeee802154ModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgIEEE_802_15_4_ModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("modulation") || force) {
        swgIEEE_802_15_4_ModSettings->setModulation((int) settings.m_modulation);
    }
    if (channelSettingsKeys.contains("bitRate") || force) {
        swgIEEE_802_15_4_ModSettings->setBitRate(settings.m_bitRate);
    }
    if (channelSettingsKeys.contains("subGHzBand") || force) {
        swgIEEE_802_15_4_ModSettings->setSubGHzBand(settings.m_subGHzBand ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgIEEE_802_15_4_ModSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("gain") || force) {
        swgIEEE_802_15_4_ModSettings->setGain(settings.m_gain);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgIEEE_802_15_4_ModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("repeat") || force) {
        swgIEEE_802_15_4_ModSettings->setRepeat(settings.m_repeat ? 1 : 0);
    }
    if (channelSettingsKeys.contains("repeatDelay") || force) {
        swgIEEE_802_15_4_ModSettings->setRepeatDelay(settings.m_repeatDelay);
    }
    if (channelSettingsKeys.contains("repeatCount") || force) {
        swgIEEE_802_15_4_ModSettings->setRepeatCount(settings.m_repeatCount);
    }
    if (channelSettingsKeys.contains("rampUpBits") || force) {
        swgIEEE_802_15_4_ModSettings->setRampUpBits(settings.m_rampUpBits);
    }
    if (channelSettingsKeys.contains("rampDownBits") || force) {
        swgIEEE_802_15_4_ModSettings->setRampDownBits(settings.m_rampDownBits);
    }
    if (channelSettingsKeys.contains("rampRange") || force) {
        swgIEEE_802_15_4_ModSettings->setRampRange(settings.m_rampRange);
    }
    if (channelSettingsKeys.contains("rampRange") || force) {
        swgIEEE_802_15_4_ModSettings->setModulateWhileRamping(settings.m_modulateWhileRamping ? 1 : 0);
    }
    if (channelSettingsKeys.contains("lpfTaps") || force) {
        swgIEEE_802_15_4_ModSettings->setLpfTaps(settings.m_lpfTaps);
    }
    if (channelSettingsKeys.contains("bbNoise") || force) {
        swgIEEE_802_15_4_ModSettings->setBbNoise(settings.m_bbNoise ? 1 : 0);
    }
    if (channelSettingsKeys.contains("writeToFile") || force) {
        swgIEEE_802_15_4_ModSettings->setWriteToFile(settings.m_writeToFile ? 1 : 0);
    }
    if (channelSettingsKeys.contains("spectrumRate") || force) {
        swgIEEE_802_15_4_ModSettings->setSpectrumRate(settings.m_spectrumRate);
    }
    if (channelSettingsKeys.contains("data") || force) {
        swgIEEE_802_15_4_ModSettings->setData(new QString(settings.m_data));
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgIEEE_802_15_4_ModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgIEEE_802_15_4_ModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgIEEE_802_15_4_ModSettings->setStreamIndex(settings.m_streamIndex);
    }
    if (channelSettingsKeys.contains("useReverseAPI") || force) {
        swgIEEE_802_15_4_ModSettings->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);
    }
    if (channelSettingsKeys.contains("reverseAPIAddress") || force) {
        swgIEEE_802_15_4_ModSettings->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }
    if (channelSettingsKeys.contains("reverseAPIPort") || force) {
        swgIEEE_802_15_4_ModSettings->setReverseApiPort(settings.m_reverseAPIPort);
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex") || force) {
        swgIEEE_802_15_4_ModSettings->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex") || force) {
        swgIEEE_802_15_4_ModSettings->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
    }
    if (channelSettingsKeys.contains("scramble") || force) {
        swgIEEE_802_15_4_ModSettings->setScramble(settings.m_scramble ? 1 : 0);
    }
    if (channelSettingsKeys.contains("polynomial") || force) {
        swgIEEE_802_15_4_ModSettings->setPolynomial(settings.m_polynomial);
    }
    if (channelSettingsKeys.contains("pulseShaping") || force) {
        swgIEEE_802_15_4_ModSettings->setPolynomial(settings.m_pulseShaping ? 1 : 0);
    }
    if (channelSettingsKeys.contains("beta") || force) {
        swgIEEE_802_15_4_ModSettings->setBeta(settings.m_beta);
    }
    if (channelSettingsKeys.contains("symbolSpan") || force) {
        swgIEEE_802_15_4_ModSettings->setBeta(settings.m_symbolSpan);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgIEEE_802_15_4_ModSettings->setUdpEnabled(settings.m_udpEnabled ? 1 : 0);
    }
    if (channelSettingsKeys.contains("udpBytesFormat") || force) {
        swgIEEE_802_15_4_ModSettings->setUdpBytesFormat(settings.m_udpBytesFormat ? 1 : 0);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgIEEE_802_15_4_ModSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgIEEE_802_15_4_ModSettings->setUdpPort(settings.m_udpPort);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgIEEE_802_15_4_ModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgIEEE_802_15_4_ModSettings->setRollupState(swgRollupState);
    }
}

void IEEE_802_15_4_Mod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "IEEE_802_15_4_Mod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("IEEE_802_15_4_Mod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double IEEE_802_15_4_Mod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void IEEE_802_15_4_Mod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t IEEE_802_15_4_Mod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

ScopeVis *IEEE_802_15_4_Mod::getScopeSink()
{
    return m_basebandSource->getScopeSink();
}
