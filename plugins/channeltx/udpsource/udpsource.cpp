///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGUDPSourceReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

#include "udpsourcebaseband.h"
#include "udpsource.h"

MESSAGE_CLASS_DEFINITION(UDPSource::MsgConfigureUDPSource, Message)
MESSAGE_CLASS_DEFINITION(UDPSource::MsgConfigureChannelizer, Message)

const char* const UDPSource::m_channelIdURI = "sdrangel.channeltx.udpsource";
const char* const UDPSource::m_channelId = "UDPSource";

UDPSource::UDPSource(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF),
    m_settingsMutex(QMutex::Recursive)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new UDPSourceBaseband();
    m_basebandSource->setSpectrumSink(&m_spectrumVis);
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &UDPSource::networkManagerFinished
    );
}

UDPSource::~UDPSource()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &UDPSource::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void UDPSource::setDeviceAPI(DeviceAPI *deviceAPI)
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

void UDPSource::start()
{
	qDebug("UDPSource::start");
    m_basebandSource->reset();
    m_thread->start();
}

void UDPSource::stop()
{
    qDebug("UDPSource::stop");
	m_thread->exit();
	m_thread->wait();
}

void UDPSource::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

void UDPSource::setCenterFrequency(qint64 frequency)
{
    UDPSourceSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureUDPSource *msgToGUI = MsgConfigureUDPSource::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

bool UDPSource::handleMessage(const Message& cmd)
{
    if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        qDebug() << "AMMod::handleMessage: MsgConfigureChannelizer:"
                << " getSourceSampleRate: " << cfg.getSourceSampleRate()
                << " getSourceCenterFrequency: " << cfg.getSourceCenterFrequency();

        UDPSourceBaseband::MsgConfigureChannelizer *msg
            = UDPSourceBaseband::MsgConfigureChannelizer::create(cfg.getSourceSampleRate(), cfg.getSourceCenterFrequency());
        m_basebandSource->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (MsgConfigureUDPSource::match(cmd))
    {
        MsgConfigureUDPSource& cfg = (MsgConfigureUDPSource&) cmd;
        qDebug() << "UDPSource::handleMessage: MsgConfigureUDPSource";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "UDPSource::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
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

void UDPSource::setSpectrum(bool enabled)
{
    Message* cmd = UDPSourceBaseband::MsgUDPSourceSpectrum::create(enabled);
    m_basebandSource->getInputMessageQueue()->push(cmd);
}

void UDPSource::resetReadIndex()
{
    Message* cmd = UDPSourceBaseband::MsgResetReadIndex::create();
    m_basebandSource->getInputMessageQueue()->push(cmd);
}

void UDPSource::applySettings(const UDPSourceSettings& settings, bool force)
{
    qDebug() << "UDPSource::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_sampleFormat: " << settings.m_sampleFormat
            << " m_inputSampleRate: " << settings.m_inputSampleRate
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_lowCutoff: " << settings.m_lowCutoff
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_amModFactor: " << settings.m_amModFactor
            << " m_udpAddressStr: " << settings.m_udpAddress
            << " m_udpPort: " << settings.m_udpPort
            << " m_multicastAddress: " << settings.m_multicastAddress
            << " m_multicastJoin: " << settings.m_multicastJoin
            << " m_channelMute: " << settings.m_channelMute
            << " m_gainIn: " << settings.m_gainIn
            << " m_gainOut: " << settings.m_gainOut
            << " m_squelchGate: " << settings.m_squelchGate
            << " m_squelch: " << settings.m_squelch << "dB"
            << " m_squelchEnabled: " << settings.m_squelchEnabled
            << " m_autoRWBalance: " << settings.m_autoRWBalance
            << " m_stereoInput: " << settings.m_stereoInput
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_sampleFormat != m_settings.m_sampleFormat) || force) {
        reverseAPIKeys.append("sampleFormat");
    }

    if ((settings.m_inputSampleRate != m_settings.m_inputSampleRate) || force)
    {
        reverseAPIKeys.append("inputSampleRate");
        DSPSignalNotification *msg = new DSPSignalNotification(settings.m_inputSampleRate, 0);
        m_spectrumVis.getInputMessageQueue()->push(msg);
    }

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_lowCutoff != m_settings.m_lowCutoff) || force) {
        reverseAPIKeys.append("lowCutoff");
    }
    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        reverseAPIKeys.append("fmDeviation");
    }
    if ((settings.m_amModFactor != m_settings.m_amModFactor) || force) {
        reverseAPIKeys.append("amModFactor");
    }
    if ((settings.m_udpAddress != m_settings.m_udpAddress) || force) {
        reverseAPIKeys.append("udpAddress");
    }
    if ((settings.m_udpPort != m_settings.m_udpPort) || force) {
        reverseAPIKeys.append("udpPort");
    }
    if ((settings.m_multicastAddress != m_settings.m_multicastAddress) || force) {
        reverseAPIKeys.append("multicastAddress");
    }
    if ((settings.m_multicastJoin != m_settings.m_multicastJoin) || force) {
        reverseAPIKeys.append("multicastJoin");
    }
    if ((settings.m_channelMute != m_settings.m_channelMute) || force) {
        reverseAPIKeys.append("channelMute");
    }
    if ((settings.m_gainIn != m_settings.m_gainIn) || force) {
        reverseAPIKeys.append("gainIn");
    }
    if ((settings.m_gainOut != m_settings.m_gainOut) || force) {
        reverseAPIKeys.append("gainOut");
    }
    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force) {
        reverseAPIKeys.append("squelchGate");
    }
    if ((settings.m_squelch != m_settings.m_squelch) || force) {
        reverseAPIKeys.append("squelch");
    }
    if ((settings.m_squelchEnabled != m_settings.m_squelchEnabled) || force) {
        reverseAPIKeys.append("squelchEnabled");
    }
    if ((settings.m_autoRWBalance != m_settings.m_autoRWBalance) || force) {
        reverseAPIKeys.append("autoRWBalance");
    }
    if ((settings.m_stereoInput != m_settings.m_stereoInput) || force) {
        reverseAPIKeys.append("stereoInput");
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

    UDPSourceBaseband::MsgConfigureUDPSourceBaseband *msg = UDPSourceBaseband::MsgConfigureUDPSourceBaseband::create(settings, force);
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

QByteArray UDPSource::serialize() const
{
    return m_settings.serialize();
}

bool UDPSource::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureUDPSource *msg = MsgConfigureUDPSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureUDPSource *msg = MsgConfigureUDPSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int UDPSource::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setUdpSourceSettings(new SWGSDRangel::SWGUDPSourceSettings());
    response.getUdpSourceSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int UDPSource::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int UDPSource::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    UDPSourceSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    if (m_settings.m_inputFrequencyOffset != settings.m_inputFrequencyOffset)
    {
        UDPSource::MsgConfigureChannelizer *msgChan = UDPSource::MsgConfigureChannelizer::create(
                settings.m_inputSampleRate,
                settings.m_inputFrequencyOffset);
        m_inputMessageQueue.push(msgChan);
    }

    MsgConfigureUDPSource *msg = MsgConfigureUDPSource::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureUDPSource *msgToGUI = MsgConfigureUDPSource::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void UDPSource::webapiUpdateChannelSettings(
        UDPSourceSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("sampleFormat")) {
        settings.m_sampleFormat = (UDPSourceSettings::SampleFormat) response.getUdpSourceSettings()->getSampleFormat();
    }
    if (channelSettingsKeys.contains("inputSampleRate")) {
        settings.m_inputSampleRate = response.getUdpSourceSettings()->getInputSampleRate();
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getUdpSourceSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getUdpSourceSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("lowCutoff")) {
        settings.m_lowCutoff = response.getUdpSourceSettings()->getLowCutoff();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getUdpSourceSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("amModFactor")) {
        settings.m_amModFactor = response.getUdpSourceSettings()->getAmModFactor();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getUdpSourceSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("gainIn")) {
        settings.m_gainIn = response.getUdpSourceSettings()->getGainIn();
    }
    if (channelSettingsKeys.contains("gainOut")) {
        settings.m_gainOut = response.getUdpSourceSettings()->getGainOut();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getUdpSourceSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("squelchGate")) {
        settings.m_squelchGate = response.getUdpSourceSettings()->getSquelchGate();
    }
    if (channelSettingsKeys.contains("squelchEnabled")) {
        settings.m_squelchEnabled = response.getUdpSourceSettings()->getSquelchEnabled() != 0;
    }
    if (channelSettingsKeys.contains("autoRWBalance")) {
        settings.m_autoRWBalance = response.getUdpSourceSettings()->getAutoRwBalance() != 0;
    }
    if (channelSettingsKeys.contains("stereoInput")) {
        settings.m_stereoInput = response.getUdpSourceSettings()->getStereoInput() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getUdpSourceSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getUdpSourceSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getUdpSourceSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("multicastAddress")) {
        settings.m_multicastAddress = *response.getUdpSourceSettings()->getMulticastAddress();
    }
    if (channelSettingsKeys.contains("multicastJoin")) {
        settings.m_multicastJoin = response.getUdpSourceSettings()->getMulticastJoin() != 0;
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getUdpSourceSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getUdpSourceSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getUdpSourceSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getUdpSourceSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getUdpSourceSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getUdpSourceSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getUdpSourceSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_spectrumGUI && channelSettingsKeys.contains("spectrumConfig")) {
        settings.m_spectrumGUI->updateFrom(channelSettingsKeys, response.getUdpSourceSettings()->getSpectrumConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getUdpSourceSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getUdpSourceSettings()->getRollupState());
    }
}

int UDPSource::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setUdpSourceReport(new SWGSDRangel::SWGUDPSourceReport());
    response.getUdpSourceReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void UDPSource::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const UDPSourceSettings& settings)
{
    response.getUdpSourceSettings()->setSampleFormat((int) settings.m_sampleFormat);
    response.getUdpSourceSettings()->setInputSampleRate(settings.m_inputSampleRate);
    response.getUdpSourceSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getUdpSourceSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getUdpSourceSettings()->setLowCutoff(settings.m_lowCutoff);
    response.getUdpSourceSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getUdpSourceSettings()->setAmModFactor(settings.m_amModFactor);
    response.getUdpSourceSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getUdpSourceSettings()->setGainIn(settings.m_gainIn);
    response.getUdpSourceSettings()->setGainOut(settings.m_gainOut);
    response.getUdpSourceSettings()->setSquelch(settings.m_squelch);
    response.getUdpSourceSettings()->setSquelchGate(settings.m_squelchGate);
    response.getUdpSourceSettings()->setSquelchEnabled(settings.m_squelchEnabled ? 1 : 0);
    response.getUdpSourceSettings()->setAutoRwBalance(settings.m_autoRWBalance ? 1 : 0);
    response.getUdpSourceSettings()->setStereoInput(settings.m_stereoInput ? 1 : 0);
    response.getUdpSourceSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getUdpSourceSettings()->getUdpAddress()) {
        *response.getUdpSourceSettings()->getUdpAddress() = settings.m_udpAddress;
    } else {
        response.getUdpSourceSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    }

    response.getUdpSourceSettings()->setUdpPort(settings.m_udpPort);

    if (response.getUdpSourceSettings()->getMulticastAddress()) {
        *response.getUdpSourceSettings()->getMulticastAddress() = settings.m_multicastAddress;
    } else {
        response.getUdpSourceSettings()->setMulticastAddress(new QString(settings.m_multicastAddress));
    }

    response.getUdpSourceSettings()->setMulticastJoin(settings.m_multicastJoin ? 1 : 0);

    if (response.getUdpSourceSettings()->getTitle()) {
        *response.getUdpSourceSettings()->getTitle() = settings.m_title;
    } else {
        response.getUdpSourceSettings()->setTitle(new QString(settings.m_title));
    }

    response.getUdpSourceSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getUdpSourceSettings()->getReverseApiAddress()) {
        *response.getUdpSourceSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getUdpSourceSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getUdpSourceSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getUdpSourceSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getUdpSourceSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_spectrumGUI)
    {
        if (response.getUdpSourceSettings()->getSpectrumConfig())
        {
            settings.m_spectrumGUI->formatTo(response.getUdpSourceSettings()->getSpectrumConfig());
        }
        else
        {
            SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
            settings.m_spectrumGUI->formatTo(swgGLSpectrum);
            response.getUdpSourceSettings()->setSpectrumConfig(swgGLSpectrum);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getUdpSourceSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getUdpSourceSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getUdpSourceSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getUdpSourceSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getUdpSourceSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getUdpSourceSettings()->setRollupState(swgRollupState);
        }
    }
}

void UDPSource::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getUdpSourceReport()->setInputPowerDb(CalcDb::dbPower(getInMagSq()));
    response.getUdpSourceReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getUdpSourceReport()->setSquelch(m_basebandSource->isSquelchOpen() ? 1 : 0);
    response.getUdpSourceReport()->setBufferGauge(getBufferGauge());
    response.getUdpSourceReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void UDPSource::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const UDPSourceSettings& settings, bool force)
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

void UDPSource::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const UDPSourceSettings& settings,
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

void UDPSource::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const UDPSourceSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setUdpSourceSettings(new SWGSDRangel::SWGUDPSourceSettings());
    SWGSDRangel::SWGUDPSourceSettings *swgUDPSourceSettings = swgChannelSettings->getUdpSourceSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("sampleFormat") || force) {
        swgUDPSourceSettings->setSampleFormat((int) settings.m_sampleFormat);
    }
    if (channelSettingsKeys.contains("inputSampleRate") || force) {
        swgUDPSourceSettings->setInputSampleRate(settings.m_inputSampleRate);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgUDPSourceSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgUDPSourceSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("lowCutoff") || force) {
        swgUDPSourceSettings->setLowCutoff(settings.m_lowCutoff);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgUDPSourceSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("amModFactor") || force) {
        swgUDPSourceSettings->setAmModFactor(settings.m_amModFactor);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgUDPSourceSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("gainIn") || force) {
        swgUDPSourceSettings->setGainIn(settings.m_gainIn);
    }
    if (channelSettingsKeys.contains("gainOut") || force) {
        swgUDPSourceSettings->setGainOut(settings.m_gainOut);
    }
    if (channelSettingsKeys.contains("squelch") || force) {
        swgUDPSourceSettings->setSquelch(settings.m_squelch);
    }
    if (channelSettingsKeys.contains("squelchGate") || force) {
        swgUDPSourceSettings->setSquelchGate(settings.m_squelchGate);
    }
    if (channelSettingsKeys.contains("squelchEnabled") || force) {
        swgUDPSourceSettings->setSquelchEnabled(settings.m_squelchEnabled ? 1 : 0);
    }
    if (channelSettingsKeys.contains("autoRWBalance") || force) {
        swgUDPSourceSettings->setAutoRwBalance(settings.m_autoRWBalance ? 1 : 0);
    }
    if (channelSettingsKeys.contains("stereoInput") || force) {
        swgUDPSourceSettings->setStereoInput(settings.m_stereoInput ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgUDPSourceSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgUDPSourceSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgUDPSourceSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("multicastAddress") || force) {
        swgUDPSourceSettings->setMulticastAddress(new QString(settings.m_multicastAddress));
    }
    if (channelSettingsKeys.contains("multicastJoin") || force) {
        swgUDPSourceSettings->setMulticastJoin(settings.m_multicastJoin ? 1 : 0);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgUDPSourceSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgUDPSourceSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_spectrumGUI && (channelSettingsKeys.contains("spectrunConfig") || force))
    {
        SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
        settings.m_spectrumGUI->formatTo(swgGLSpectrum);
        swgUDPSourceSettings->setSpectrumConfig(swgGLSpectrum);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgUDPSourceSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgUDPSourceSettings->setRollupState(swgRollupState);
    }
}

void UDPSource::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "UDPSource::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("UDPSource::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void UDPSource::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

double UDPSource::getMagSq() const
{
    return m_basebandSource->getMagSq();
}
double UDPSource::getInMagSq() const
{
    return m_basebandSource->getInMagSq();
}

int32_t UDPSource::getBufferGauge() const
{
    return m_basebandSource->getBufferGauge();
}

bool UDPSource::getSquelchOpen() const
{
    return m_basebandSource->getSquelchOpen();
}

uint32_t UDPSource::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}
