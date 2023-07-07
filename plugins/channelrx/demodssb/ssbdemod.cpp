///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// (c) 2014 Modified by John Greb
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


#include <stdio.h>

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>
#include <QMutexLocker>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGSSBDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGSSBDemodReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

#include "ssbdemod.h"

MESSAGE_CLASS_DEFINITION(SSBDemod::MsgConfigureSSBDemod, Message)

const char* const SSBDemod::m_channelIdURI = "sdrangel.channel.ssbdemod";
const char* const SSBDemod::m_channelId = "SSBDemod";

SSBDemod::SSBDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_running(false),
        m_spectrumVis(SDR_RX_SCALEF),
        m_basebandSampleRate(0)
{
	setObjectName(m_channelId);

	applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SSBDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &SSBDemod::handleIndexInDeviceSetChanged
    );

    start();
}

SSBDemod::~SSBDemod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SSBDemod::networkManagerFinished
    );
    delete m_networkManager;
	m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    stop();
}

void SSBDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t SSBDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void SSBDemod::setMessageQueueToGUI(MessageQueue* queue)
{
    ChannelAPI::setMessageQueueToGUI(queue);

    if (m_basebandSink) {
        m_basebandSink->setMessageQueueToGUI(queue);
    }
}

void SSBDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) positiveOnly;

    if (m_running) {
        m_basebandSink->feed(begin, end);
    }
}

void SSBDemod::start()
{
    QMutexLocker m_lock(&m_mutex);

    if (m_running) {
        return;
    }

    qDebug() << "SSBDemod::start";
    m_thread = new QThread();
    m_basebandSink = new SSBDemodBaseband();
    m_basebandSink->setFifoLabel(QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(getIndexInDeviceSet())
    );
    m_basebandSink->setSpectrumSink(&m_spectrumVis);
    m_basebandSink->setChannel(this);
    m_basebandSink->setMessageQueueToGUI(getMessageQueueToGUI());
    m_basebandSink->moveToThread(m_thread);

    QObject::connect(
        m_thread,
        &QThread::finished,
        m_basebandSink,
        &QObject::deleteLater
    );
    QObject::connect(
        m_thread,
        &QThread::finished,
        m_thread,
        &QThread::deleteLater
    );

    if (m_basebandSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_basebandSampleRate);
    }

    m_thread->start();

    SSBDemodBaseband::MsgConfigureSSBDemodBaseband *msg = SSBDemodBaseband::MsgConfigureSSBDemodBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);

    m_running = true;
}

void SSBDemod::stop()
{
    QMutexLocker m_lock(&m_mutex);

    if (!m_running) {
        return;
    }

    qDebug() << "SSBDemod::stop";
    m_running = false;
	m_thread->exit();
	m_thread->wait();
}

bool SSBDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureSSBDemod::match(cmd))
    {
        MsgConfigureSSBDemod& cfg = (MsgConfigureSSBDemod&) cmd;
        qDebug("SSBDemod::handleMessage: MsgConfigureSSBDemod");

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        qDebug() << "SSBDemod::handleMessage: DSPSignalNotification";
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        // Forward to the sink
        if (m_running) {
            m_basebandSink->getInputMessageQueue()->push(new DSPSignalNotification(notif));
        }
        // Forwatd to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "SSBDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
	else
	{
		return false;
	}
}

void SSBDemod::setCenterFrequency(qint64 frequency)
{
    SSBDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSSBDemod *msgToGUI = MsgConfigureSSBDemod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void SSBDemod::applySettings(const SSBDemodSettings& settings, bool force)
{
    qDebug() << "SSBDemod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_filterIndex: " << settings.m_filterIndex
            << " [m_spanLog2: " << settings.m_filterBank[settings.m_filterIndex].m_spanLog2
            << " m_rfBandwidth: " << settings.m_filterBank[settings.m_filterIndex].m_rfBandwidth
            << " m_lowCutoff: " << settings.m_filterBank[settings.m_filterIndex].m_lowCutoff
            << " m_fftWindow: " << settings.m_filterBank[settings.m_filterIndex].m_fftWindow << "]"
            << " m_volume: " << settings.m_volume
            << " m_audioBinaual: " << settings.m_audioBinaural
            << " m_audioFlipChannels: " << settings.m_audioFlipChannels
            << " m_dsb: " << settings.m_dsb
            << " m_audioMute: " << settings.m_audioMute
            << " m_agcActive: " << settings.m_agc
            << " m_agcClamping: " << settings.m_agcClamping
            << " m_agcTimeLog2: " << settings.m_agcTimeLog2
            << " agcPowerThreshold: " << settings.m_agcPowerThreshold
            << " agcThresholdGate: " << settings.m_agcThresholdGate
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_inputFrequencyOffset != settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((m_settings.m_filterIndex != settings.m_filterIndex) || force) {
        reverseAPIKeys.append("filterIndex");
    }
    if ((m_settings.m_filterBank[m_settings.m_filterIndex].m_spanLog2 != settings.m_filterBank[settings.m_filterIndex].m_spanLog2) || force) {
        reverseAPIKeys.append("spanLog2");
    }
    if ((m_settings.m_filterBank[m_settings.m_filterIndex].m_rfBandwidth != settings.m_filterBank[settings.m_filterIndex].m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((m_settings.m_filterBank[m_settings.m_filterIndex].m_lowCutoff != settings.m_filterBank[settings.m_filterIndex].m_lowCutoff) || force) {
        reverseAPIKeys.append("lowCutoff");
    }
    if ((m_settings.m_filterBank[m_settings.m_filterIndex].m_fftWindow != settings.m_filterBank[settings.m_filterIndex].m_fftWindow) || force) {
        reverseAPIKeys.append("fftWindow");
    }
    if ((m_settings.m_volume != settings.m_volume) || force) {
        reverseAPIKeys.append("volume");
    }
    if ((m_settings.m_agcTimeLog2 != settings.m_agcTimeLog2) || force) {
        reverseAPIKeys.append("agcTimeLog2");
    }
    if ((m_settings.m_agcPowerThreshold != settings.m_agcPowerThreshold) || force) {
        reverseAPIKeys.append("agcPowerThreshold");
    }
    if ((m_settings.m_agcThresholdGate != settings.m_agcThresholdGate) || force) {
        reverseAPIKeys.append("agcThresholdGate");
    }
    if ((m_settings.m_agcClamping != settings.m_agcClamping) || force) {
        reverseAPIKeys.append("agcClamping");
    }
    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force) {
        reverseAPIKeys.append("audioDeviceName");
    }
    if ((m_settings.m_audioBinaural != settings.m_audioBinaural) || force) {
        reverseAPIKeys.append("audioBinaural");
    }
    if ((m_settings.m_audioFlipChannels != settings.m_audioFlipChannels) || force) {
        reverseAPIKeys.append("audioFlipChannels");
    }
    if ((m_settings.m_dsb != settings.m_dsb) || force) {
        reverseAPIKeys.append("dsb");
    }
    if ((m_settings.m_audioMute != settings.m_audioMute) || force) {
        reverseAPIKeys.append("audioMute");
    }
    if ((m_settings.m_agc != settings.m_agc) || force) {
        reverseAPIKeys.append("agc");
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

    if ((settings.m_dsb != m_settings.m_dsb)
     || (settings.m_filterBank[settings.m_filterIndex].m_rfBandwidth != m_settings.m_filterBank[m_settings.m_filterIndex].m_rfBandwidth)
     || (settings.m_filterBank[settings.m_filterIndex].m_lowCutoff != m_settings.m_filterBank[m_settings.m_filterIndex].m_lowCutoff) || force)
    {
        SpectrumSettings spectrumSettings = m_spectrumVis.getSettings();
        spectrumSettings.m_ssb = !settings.m_dsb;
        spectrumSettings.m_usb = (settings.m_filterBank[settings.m_filterIndex].m_lowCutoff < settings.m_filterBank[settings.m_filterIndex].m_rfBandwidth);
        SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
        m_spectrumVis.getInputMessageQueue()->push(msg);
    }

    if (m_running)
    {
        SSBDemodBaseband::MsgConfigureSSBDemodBaseband *msg = SSBDemodBaseband::MsgConfigureSSBDemodBaseband::create(settings, force);
        m_basebandSink->getInputMessageQueue()->push(msg);
    }

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

QByteArray SSBDemod::serialize() const
{
    return m_settings.serialize();
}

bool SSBDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureSSBDemod *msg = MsgConfigureSSBDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureSSBDemod *msg = MsgConfigureSSBDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void SSBDemod::sendSampleRateToDemodAnalyzer()
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe: pipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

            if (messageQueue)
            {
                MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(
                    this,
                    getAudioSampleRate()
                );
                messageQueue->push(msg);
            }
        }
    }
}

int SSBDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setSsbDemodSettings(new SWGSDRangel::SWGSSBDemodSettings());
    response.getSsbDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int SSBDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int SSBDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    SSBDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureSSBDemod *msg = MsgConfigureSSBDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("SSBDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSSBDemod *msgToGUI = MsgConfigureSSBDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void SSBDemod::webapiUpdateChannelSettings(
        SSBDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getSsbDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("filterIndex")) {
        settings.m_filterIndex = response.getSsbDemodSettings()->getFilterIndex();
    }
    if (channelSettingsKeys.contains("spanLog2")) {
        settings.m_filterBank[settings.m_filterIndex].m_spanLog2 = response.getSsbDemodSettings()->getSpanLog2();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_filterBank[settings.m_filterIndex].m_rfBandwidth = response.getSsbDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("lowCutoff")) {
        settings.m_filterBank[settings.m_filterIndex].m_lowCutoff = response.getSsbDemodSettings()->getLowCutoff();
    }
    if (channelSettingsKeys.contains("fftWimdow")) {
        settings.m_filterBank[settings.m_filterIndex].m_fftWindow = (FFTWindow::Function) response.getSsbDemodSettings()->getFftWindow();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getSsbDemodSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("audioBinaural")) {
        settings.m_audioBinaural = response.getSsbDemodSettings()->getAudioBinaural() != 0;
    }
    if (channelSettingsKeys.contains("audioFlipChannels")) {
        settings.m_audioFlipChannels = response.getSsbDemodSettings()->getAudioFlipChannels() != 0;
    }
    if (channelSettingsKeys.contains("dsb")) {
        settings.m_dsb = response.getSsbDemodSettings()->getDsb() != 0;
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getSsbDemodSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("agc")) {
        settings.m_agc = response.getSsbDemodSettings()->getAgc() != 0;
    }
    if (channelSettingsKeys.contains("agcClamping")) {
        settings.m_agcClamping = response.getSsbDemodSettings()->getAgcClamping() != 0;
    }
    if (channelSettingsKeys.contains("agcTimeLog2")) {
        settings.m_agcTimeLog2 = response.getSsbDemodSettings()->getAgcTimeLog2();
    }
    if (channelSettingsKeys.contains("agcPowerThreshold")) {
        settings.m_agcPowerThreshold = response.getSsbDemodSettings()->getAgcPowerThreshold();
    }
    if (channelSettingsKeys.contains("agcThresholdGate")) {
        settings.m_agcThresholdGate = response.getSsbDemodSettings()->getAgcThresholdGate();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getSsbDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getSsbDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getSsbDemodSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getSsbDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getSsbDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getSsbDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getSsbDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getSsbDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getSsbDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_spectrumGUI && channelSettingsKeys.contains("spectrumConfig")) {
        settings.m_spectrumGUI->updateFrom(channelSettingsKeys, response.getSsbDemodSettings()->getSpectrumConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getSsbDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getSsbDemodSettings()->getRollupState());
    }
}

int SSBDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setSsbDemodReport(new SWGSDRangel::SWGSSBDemodReport());
    response.getSsbDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void SSBDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const SSBDemodSettings& settings)
{
    response.getSsbDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getSsbDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getSsbDemodSettings()->setFilterIndex(settings.m_filterIndex);
    response.getSsbDemodSettings()->setSpanLog2(settings.m_filterBank[settings.m_filterIndex].m_spanLog2);
    response.getSsbDemodSettings()->setRfBandwidth(settings.m_filterBank[settings.m_filterIndex].m_rfBandwidth);
    response.getSsbDemodSettings()->setLowCutoff(settings.m_filterBank[settings.m_filterIndex].m_lowCutoff);
    response.getSsbDemodSettings()->setFftWindow((int) settings.m_filterBank[settings.m_filterIndex].m_fftWindow);
    response.getSsbDemodSettings()->setVolume(settings.m_volume);
    response.getSsbDemodSettings()->setAudioBinaural(settings.m_audioBinaural ? 1 : 0);
    response.getSsbDemodSettings()->setAudioFlipChannels(settings.m_audioFlipChannels ? 1 : 0);
    response.getSsbDemodSettings()->setDsb(settings.m_dsb ? 1 : 0);
    response.getSsbDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getSsbDemodSettings()->setAgc(settings.m_agc ? 1 : 0);
    response.getSsbDemodSettings()->setAgcClamping(settings.m_agcClamping ? 1 : 0);
    response.getSsbDemodSettings()->setAgcTimeLog2(settings.m_agcTimeLog2);
    response.getSsbDemodSettings()->setAgcPowerThreshold(settings.m_agcPowerThreshold);
    response.getSsbDemodSettings()->setAgcThresholdGate(settings.m_agcThresholdGate);
    response.getSsbDemodSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getSsbDemodSettings()->getTitle()) {
        *response.getSsbDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getSsbDemodSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getSsbDemodSettings()->getAudioDeviceName()) {
        *response.getSsbDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getSsbDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getSsbDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getSsbDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getSsbDemodSettings()->getReverseApiAddress()) {
        *response.getSsbDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getSsbDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getSsbDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getSsbDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getSsbDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_spectrumGUI)
    {
        if (response.getSsbDemodSettings()->getSpectrumConfig())
        {
            settings.m_spectrumGUI->formatTo(response.getSsbDemodSettings()->getSpectrumConfig());
        }
        else
        {
            SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
            settings.m_spectrumGUI->formatTo(swgGLSpectrum);
            response.getSsbDemodSettings()->setSpectrumConfig(swgGLSpectrum);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getSsbDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getSsbDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getSsbDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getSsbDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getSsbDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getSsbDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void SSBDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getSsbDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));

    if (m_running)
    {
        response.getSsbDemodReport()->setSquelch(m_basebandSink->getAudioActive() ? 1 : 0);
        response.getSsbDemodReport()->setAudioSampleRate(m_basebandSink->getAudioSampleRate());
        response.getSsbDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
    }
}

void SSBDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const SSBDemodSettings& settings, bool force)
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

void SSBDemod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const SSBDemodSettings& settings,
    bool force)
{
    qDebug("SSBDemod::sendChannelSettings: %d pipes", pipes.size());

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

void SSBDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const SSBDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setSsbDemodSettings(new SWGSDRangel::SWGSSBDemodSettings());
    SWGSDRangel::SWGSSBDemodSettings *swgSSBDemodSettings = swgChannelSettings->getSsbDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgSSBDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("filteIndex") || force) {
        swgSSBDemodSettings->setFilterIndex(settings.m_filterIndex);
    }
    if (channelSettingsKeys.contains("spanLog2") || force) {
        swgSSBDemodSettings->setSpanLog2(settings.m_filterBank[settings.m_filterIndex].m_spanLog2);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgSSBDemodSettings->setRfBandwidth(settings.m_filterBank[settings.m_filterIndex].m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("lowCutoff") || force) {
        swgSSBDemodSettings->setLowCutoff(settings.m_filterBank[settings.m_filterIndex].m_lowCutoff);
    }
    if (channelSettingsKeys.contains("fftWindow") || force) {
        swgSSBDemodSettings->setLowCutoff(settings.m_filterBank[settings.m_filterIndex].m_fftWindow);
    }
    if (channelSettingsKeys.contains("volume") || force) {
        swgSSBDemodSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("audioBinaural") || force) {
        swgSSBDemodSettings->setAudioBinaural(settings.m_audioBinaural ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioFlipChannels") || force) {
        swgSSBDemodSettings->setAudioFlipChannels(settings.m_audioFlipChannels ? 1 : 0);
    }
    if (channelSettingsKeys.contains("dsb") || force) {
        swgSSBDemodSettings->setDsb(settings.m_dsb ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgSSBDemodSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("agc") || force) {
        swgSSBDemodSettings->setAgc(settings.m_agc ? 1 : 0);
    }
    if (channelSettingsKeys.contains("agcClamping") || force) {
        swgSSBDemodSettings->setAgcClamping(settings.m_agcClamping ? 1 : 0);
    }
    if (channelSettingsKeys.contains("agcTimeLog2") || force) {
        swgSSBDemodSettings->setAgcTimeLog2(settings.m_agcTimeLog2);
    }
    if (channelSettingsKeys.contains("agcPowerThreshold") || force) {
        swgSSBDemodSettings->setAgcPowerThreshold(settings.m_agcPowerThreshold);
    }
    if (channelSettingsKeys.contains("agcThresholdGate") || force) {
        swgSSBDemodSettings->setAgcThresholdGate(settings.m_agcThresholdGate);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgSSBDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgSSBDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgSSBDemodSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgSSBDemodSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_spectrumGUI && (channelSettingsKeys.contains("spectrunConfig") || force))
    {
        SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
        settings.m_spectrumGUI->formatTo(swgGLSpectrum);
        swgSSBDemodSettings->setSpectrumConfig(swgGLSpectrum);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgSSBDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRolllupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRolllupState);
        swgSSBDemodSettings->setRollupState(swgRolllupState);
    }
}

void SSBDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "SSBDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("SSBDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void SSBDemod::handleIndexInDeviceSetChanged(int index)
{
    if (!m_running || (index < 0)) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
    m_basebandSink->setAudioFifoLabel(fifoLabel);
}
