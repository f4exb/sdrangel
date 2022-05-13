///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QUdpSocket>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGUDPSinkSettings.h"
#include "SWGChannelReport.h"
#include "SWGUDPSinkReport.h"

#include "dsp/dspengine.h"
#include "util/db.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "maincore.h"

#include "udpsink.h"

MESSAGE_CLASS_DEFINITION(UDPSink::MsgConfigureUDPSink, Message)

const char* const UDPSink::m_channelIdURI = "sdrangel.channel.udpsink";
const char* const UDPSink::m_channelId = "UDPSink";

UDPSink::UDPSink(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_spectrumVis(SDR_RX_SCALEF),
        m_channelSampleRate(48000),
        m_channelFrequencyOffset(0)
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSink = new UDPSinkBaseband();
    m_basebandSink->setSpectrum(&m_spectrumVis);
    m_basebandSink->moveToThread(m_thread);

	applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &UDPSink::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &UDPSink::handleIndexInDeviceSetChanged
    );
}

UDPSink::~UDPSink()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &UDPSink::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);
    delete m_basebandSink;
    delete m_thread;
}

void UDPSink::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

uint32_t UDPSink::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void UDPSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) positiveOnly;
    m_basebandSink->feed(begin, end);
}

void UDPSink::start()
{
    qDebug() << "UDPSink::start";

    if (m_basebandSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_basebandSampleRate);
    }

    m_basebandSink->reset();
    m_thread->start();
}

void UDPSink::stop()
{
    qDebug() << "UDPSink::stop";
	m_thread->exit();
	m_thread->wait();
}

bool UDPSink::handleMessage(const Message& cmd)
{
    if (MsgConfigureUDPSink::match(cmd))
    {
        MsgConfigureUDPSink& cfg = (MsgConfigureUDPSink&) cmd;
        qDebug("UDPSink::handleMessage: MsgConfigureUDPSink");

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "UDPSink::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
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

void UDPSink::setCenterFrequency(qint64 frequency)
{
    UDPSinkSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureUDPSink *msgToGUI = MsgConfigureUDPSink::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void UDPSink::applySettings(const UDPSinkSettings& settings, bool force)
{
    qDebug() << "UDPSink::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_audioActive: " << settings.m_audioActive
            << " m_audioStereo: " << settings.m_audioStereo
            << " m_gain: " << settings.m_gain
            << " m_volume: " << settings.m_volume
            << " m_squelchEnabled: " << settings.m_squelchEnabled
            << " m_squelchdB: " << settings.m_squelchdB
            << " m_squelchGate" << settings.m_squelchGate
            << " m_agc" << settings.m_agc
            << " m_sampleFormat: " << settings.m_sampleFormat
            << " m_outputSampleRate: " << settings.m_outputSampleRate
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_udpAddressStr: " << settings.m_udpAddress
            << " m_udpPort: " << settings.m_udpPort
            << " m_audioPort: " << settings.m_audioPort
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_audioActive != m_settings.m_audioActive) || force) {
        reverseAPIKeys.append("audioActive");
    }
    if ((settings.m_audioStereo != m_settings.m_audioStereo) || force) {
        reverseAPIKeys.append("audioStereo");
    }
    if ((settings.m_gain != m_settings.m_gain) || force) {
        reverseAPIKeys.append("gain");
    }
    if ((settings.m_volume != m_settings.m_volume) || force) {
        reverseAPIKeys.append("volume");
    }
    if ((settings.m_squelchEnabled != m_settings.m_squelchEnabled) || force) {
        reverseAPIKeys.append("squelchEnabled");
    }
    if ((settings.m_squelchdB != m_settings.m_squelchdB) || force) {
        reverseAPIKeys.append("squelchDB");
    }
    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force) {
        reverseAPIKeys.append("squelchGate");
    }
    if ((settings.m_agc != m_settings.m_agc) || force) {
        reverseAPIKeys.append("agc");
    }
    if ((settings.m_sampleFormat != m_settings.m_sampleFormat) || force) {
        reverseAPIKeys.append("sampleFormat");
    }
    if ((settings.m_outputSampleRate != m_settings.m_outputSampleRate) || force) {
        reverseAPIKeys.append("outputSampleRate");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        reverseAPIKeys.append("fmDeviation");
    }
    if ((settings.m_udpAddress != m_settings.m_udpAddress) || force) {
        reverseAPIKeys.append("udpAddress");
    }
    if ((settings.m_udpPort != m_settings.m_udpPort) || force) {
        reverseAPIKeys.append("udpPort");
    }
    if ((settings.m_audioPort != m_settings.m_audioPort) || force) {
        reverseAPIKeys.append("audioPort");
    }

    if (m_settings.m_streamIndex != settings.m_streamIndex)
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSinkAPI(this);
            m_deviceAPI->removeChannelSink(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSink(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSinkAPI(this);
        }

        reverseAPIKeys.append("streamIndex");
    }

    UDPSinkBaseband::MsgConfigureUDPSinkBaseband *msg = UDPSinkBaseband::MsgConfigureUDPSinkBaseband::create(settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

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

QByteArray UDPSink::serialize() const
{
    return m_settings.serialize();
}

bool UDPSink::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureUDPSink *msg = MsgConfigureUDPSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureUDPSink *msg = MsgConfigureUDPSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int UDPSink::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setUdpSinkSettings(new SWGSDRangel::SWGUDPSinkSettings());
    response.getUdpSinkSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int UDPSink::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int UDPSink::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    UDPSinkSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureUDPSink *msg = MsgConfigureUDPSink::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("getUdpSinkSettings::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureUDPSink *msgToGUI = MsgConfigureUDPSink::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void UDPSink::webapiUpdateChannelSettings(
        UDPSinkSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("outputSampleRate")) {
        settings.m_outputSampleRate = response.getUdpSinkSettings()->getOutputSampleRate();
    }
    if (channelSettingsKeys.contains("sampleFormat")) {
        settings.m_sampleFormat = (UDPSinkSettings::SampleFormat) response.getUdpSinkSettings()->getSampleFormat();
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getUdpSinkSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getUdpSinkSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getUdpSinkSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getUdpSinkSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("gain")) {
        settings.m_gain = response.getUdpSinkSettings()->getGain();
    }
    if (channelSettingsKeys.contains("squelchDB")) {
        settings.m_squelchdB = response.getUdpSinkSettings()->getSquelchDb();
    }
    if (channelSettingsKeys.contains("squelchGate")) {
        settings.m_squelchGate = response.getUdpSinkSettings()->getSquelchGate();
    }
    if (channelSettingsKeys.contains("squelchEnabled")) {
        settings.m_squelchEnabled = response.getUdpSinkSettings()->getSquelchEnabled() != 0;
    }
    if (channelSettingsKeys.contains("agc")) {
        settings.m_agc = response.getUdpSinkSettings()->getAgc() != 0;
    }
    if (channelSettingsKeys.contains("audioActive")) {
        settings.m_audioActive = response.getUdpSinkSettings()->getAudioActive() != 0;
    }
    if (channelSettingsKeys.contains("audioStereo")) {
        settings.m_audioStereo = response.getUdpSinkSettings()->getAudioStereo() != 0;
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getUdpSinkSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getUdpSinkSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getUdpSinkSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("audioPort")) {
        settings.m_audioPort = response.getUdpSinkSettings()->getAudioPort();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getUdpSinkSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getUdpSinkSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getUdpSinkSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getUdpSinkSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getUdpSinkSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getUdpSinkSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getUdpSinkSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getUdpSinkSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_spectrumGUI && channelSettingsKeys.contains("spectrumConfig")) {
        settings.m_spectrumGUI->updateFrom(channelSettingsKeys, response.getUdpSinkSettings()->getSpectrumConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getUdpSinkSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getUdpSinkSettings()->getRollupState());
    }
}

int UDPSink::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setUdpSinkReport(new SWGSDRangel::SWGUDPSinkReport());
    response.getUdpSinkReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void UDPSink::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const UDPSinkSettings& settings)
{
    response.getUdpSinkSettings()->setOutputSampleRate(settings.m_outputSampleRate);
    response.getUdpSinkSettings()->setSampleFormat((int) settings.m_sampleFormat);
    response.getUdpSinkSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getUdpSinkSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getUdpSinkSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getUdpSinkSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getUdpSinkSettings()->setGain(settings.m_gain);
    response.getUdpSinkSettings()->setSquelchDb(settings.m_squelchdB);
    response.getUdpSinkSettings()->setSquelchGate(settings.m_squelchGate);
    response.getUdpSinkSettings()->setSquelchEnabled(settings.m_squelchEnabled ? 1 : 0);
    response.getUdpSinkSettings()->setAgc(settings.m_agc ? 1 : 0);
    response.getUdpSinkSettings()->setAudioActive(settings.m_audioActive ? 1 : 0);
    response.getUdpSinkSettings()->setAudioStereo(settings.m_audioStereo ? 1 : 0);
    response.getUdpSinkSettings()->setVolume(settings.m_volume);

    if (response.getUdpSinkSettings()->getUdpAddress()) {
        *response.getUdpSinkSettings()->getUdpAddress() = settings.m_udpAddress;
    } else {
        response.getUdpSinkSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    }

    response.getUdpSinkSettings()->setUdpPort(settings.m_udpPort);
    response.getUdpSinkSettings()->setAudioPort(settings.m_audioPort);
    response.getUdpSinkSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getUdpSinkSettings()->getTitle()) {
        *response.getUdpSinkSettings()->getTitle() = settings.m_title;
    } else {
        response.getUdpSinkSettings()->setTitle(new QString(settings.m_title));
    }

    response.getUdpSinkSettings()->setStreamIndex(settings.m_streamIndex);
    response.getUdpSinkSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getUdpSinkSettings()->getReverseApiAddress()) {
        *response.getUdpSinkSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getUdpSinkSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getUdpSinkSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getUdpSinkSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getUdpSinkSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_spectrumGUI)
    {
        if (response.getUdpSinkSettings()->getSpectrumConfig())
        {
            settings.m_spectrumGUI->formatTo(response.getUdpSinkSettings()->getSpectrumConfig());
        }
        else
        {
            SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
            settings.m_spectrumGUI->formatTo(swgGLSpectrum);
            response.getUdpSinkSettings()->setSpectrumConfig(swgGLSpectrum);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getUdpSinkSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getUdpSinkSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getUdpSinkSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getUdpSinkSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getUdpSinkSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getUdpSinkSettings()->setRollupState(swgRollupState);
        }
    }
}

void UDPSink::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getUdpSinkReport()->setChannelPowerDb(CalcDb::dbPower(getInMagSq()));
    response.getUdpSinkReport()->setOutputPowerDb(CalcDb::dbPower(getMagSq()));
    response.getUdpSinkReport()->setSquelch(getSquelchOpen() ? 1 : 0);
    response.getUdpSinkReport()->setInputSampleRate(m_channelSampleRate);
}

void UDPSink::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const UDPSinkSettings& settings, bool force)
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

void UDPSink::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const UDPSinkSettings& settings,
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

void UDPSink::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const UDPSinkSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setUdpSinkSettings(new SWGSDRangel::SWGUDPSinkSettings());
    SWGSDRangel::SWGUDPSinkSettings *swgUDPSinkSettings = swgChannelSettings->getUdpSinkSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("outputSampleRate") || force) {
        swgUDPSinkSettings->setOutputSampleRate(settings.m_outputSampleRate);
    }
    if (channelSettingsKeys.contains("sampleFormat") || force) {
        swgUDPSinkSettings->setSampleFormat((int) settings.m_sampleFormat);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgUDPSinkSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgUDPSinkSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgUDPSinkSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgUDPSinkSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("gain") || force) {
        swgUDPSinkSettings->setGain(settings.m_gain);
    }
    if (channelSettingsKeys.contains("squelchDB") || force) {
        swgUDPSinkSettings->setSquelchDb(settings.m_squelchdB);
    }
    if (channelSettingsKeys.contains("squelchGate") || force) {
        swgUDPSinkSettings->setSquelchGate(settings.m_squelchGate);
    }
    if (channelSettingsKeys.contains("squelchEnabled") || force) {
        swgUDPSinkSettings->setSquelchEnabled(settings.m_squelchEnabled ? 1 : 0);
    }
    if (channelSettingsKeys.contains("agc") || force) {
        swgUDPSinkSettings->setAgc(settings.m_agc ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioActive") || force) {
        swgUDPSinkSettings->setAudioActive(settings.m_audioActive ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioStereo") || force) {
        swgUDPSinkSettings->setAudioStereo(settings.m_audioStereo ? 1 : 0);
    }
    if (channelSettingsKeys.contains("volume") || force) {
        swgUDPSinkSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgUDPSinkSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgUDPSinkSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("audioPort") || force) {
        swgUDPSinkSettings->setAudioPort(settings.m_audioPort);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgUDPSinkSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgUDPSinkSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgUDPSinkSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_spectrumGUI && (channelSettingsKeys.contains("spectrunConfig") || force))
    {
        SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
        settings.m_spectrumGUI->formatTo(swgGLSpectrum);
        swgUDPSinkSettings->setSpectrumConfig(swgGLSpectrum);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgUDPSinkSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgUDPSinkSettings->setRollupState(swgRollupState);
    }
}

void UDPSink::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "UDPSink::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("UDPSink::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void UDPSink::enableSpectrum(bool enable)
{
    UDPSinkBaseband::MsgEnableSpectrum *msg = UDPSinkBaseband::MsgEnableSpectrum::create(enable);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void UDPSink::handleIndexInDeviceSetChanged(int index)
{
    if (index < 0) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
    m_basebandSink->setAudioFifoLabel(fifoLabel);
}
