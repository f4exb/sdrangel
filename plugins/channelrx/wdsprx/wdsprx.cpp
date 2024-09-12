///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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
#include "SWGWDSPRxSettings.h"
#include "SWGChannelReport.h"
#include "SWGWDSPRxReport.h"

#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"
#include "util/db.h"
#include "maincore.h"

#include "wdsprx.h"

MESSAGE_CLASS_DEFINITION(WDSPRx::MsgConfigureWDSPRx, Message)

const char* const WDSPRx::m_channelIdURI = "sdrangel.channel.wdsprx";
const char* const WDSPRx::m_channelId = "WDSPRx";

WDSPRx::WDSPRx(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_thread(nullptr),
        m_basebandSink(nullptr),
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
        &WDSPRx::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &WDSPRx::handleIndexInDeviceSetChanged
    );
}

WDSPRx::~WDSPRx()
{
    qDebug("WDSPRx::~WDSPRx");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &WDSPRx::networkManagerFinished
    );
    delete m_networkManager;
	m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    WDSPRx::stop();
    qDebug("WDSPRx::~WDSPRx: emd");
}

void WDSPRx::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t WDSPRx::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void WDSPRx::setMessageQueueToGUI(MessageQueue* queue)
{
    ChannelAPI::setMessageQueueToGUI(queue);

    if (m_basebandSink) {
        m_basebandSink->setMessageQueueToGUI(queue);
    }
}

void WDSPRx::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) positiveOnly;

    if (m_running) {
        m_basebandSink->feed(begin, end);
    }
}

void WDSPRx::start()
{
    QMutexLocker m_lock(&m_mutex);

    if (m_running) {
        return;
    }

    qDebug() << "WDSPRx::start";
    m_thread = new QThread();
    m_basebandSink = new WDSPRxBaseband();
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

    WDSPRxBaseband::MsgConfigureWDSPRxBaseband *msg = WDSPRxBaseband::MsgConfigureWDSPRxBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);

    m_running = true;
}

void WDSPRx::stop()
{
    QMutexLocker m_lock(&m_mutex);

    if (!m_running) {
        return;
    }

    qDebug() << "WDSPRx::stop";
    m_running = false;
	m_thread->exit();
	m_thread->wait();
}

bool WDSPRx::handleMessage(const Message& cmd)
{
    if (MsgConfigureWDSPRx::match(cmd))
    {
        auto& cfg = (const MsgConfigureWDSPRx&) cmd;
        qDebug("WDSPRx::handleMessage: MsgConfigureWDSPRx");

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        qDebug() << "WDSPRx::handleMessage: DSPSignalNotification";
        auto& notif = (const DSPSignalNotification&) cmd;
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
        qDebug() << "WDSPRx::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
	else
	{
		return false;
	}
}

void WDSPRx::setCenterFrequency(qint64 frequency)
{
    WDSPRxSettings settings = m_settings;
    settings.m_inputFrequencyOffset = (qint32) frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureWDSPRx *msgToGUI = MsgConfigureWDSPRx::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void WDSPRx::applySettings(const WDSPRxSettings& settings, bool force)
{
    qDebug() << "WDSPRx::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_profileIndex: " << settings.m_profileIndex
            << " [m_spanLog2: " << settings.m_profiles[settings.m_profileIndex].m_spanLog2
            << " m_highCutoff: " << settings.m_profiles[settings.m_profileIndex].m_highCutoff
            << " m_lowCutoff: " << settings.m_profiles[settings.m_profileIndex].m_lowCutoff
            << " m_fftWindow: " << settings.m_profiles[settings.m_profileIndex].m_fftWindow << "]"
            << " m_volume: " << settings.m_volume
            << " m_audioBinaual: " << settings.m_audioBinaural
            << " m_audioFlipChannels: " << settings.m_audioFlipChannels
            << " m_dsb: " << settings.m_dsb
            << " m_audioMute: " << settings.m_audioMute
            << " m_agcActive: " << settings.m_agc
            << " m_agcMode: " << settings.m_agcMode
            << " m_agcGain: " << settings.m_agcGain
            << " m_agcSlope: " << settings.m_agcSlope
            << " m_agcHangThreshold: " << settings.m_agcHangThreshold
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
    if ((m_settings.m_profileIndex != settings.m_profileIndex) || force) {
        reverseAPIKeys.append("filterIndex");
    }
    if ((m_settings.m_profiles[m_settings.m_profileIndex].m_spanLog2 != settings.m_profiles[settings.m_profileIndex].m_spanLog2) || force) {
        reverseAPIKeys.append("spanLog2");
    }
    if ((m_settings.m_profiles[m_settings.m_profileIndex].m_highCutoff != settings.m_profiles[settings.m_profileIndex].m_highCutoff) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((m_settings.m_profiles[m_settings.m_profileIndex].m_lowCutoff != settings.m_profiles[settings.m_profileIndex].m_lowCutoff) || force) {
        reverseAPIKeys.append("lowCutoff");
    }
    if ((m_settings.m_profiles[m_settings.m_profileIndex].m_fftWindow != settings.m_profiles[settings.m_profileIndex].m_fftWindow) || force) {
        reverseAPIKeys.append("fftWindow");
    }
    if ((m_settings.m_volume != settings.m_volume) || force) {
        reverseAPIKeys.append("volume");
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
            m_settings.m_streamIndex = settings.m_streamIndex; // make sure ChannelAPI::getStreamIndex() is consistent
            emit streamIndexChanged(settings.m_streamIndex);
        }

        reverseAPIKeys.append("streamIndex");
    }

    if ((settings.m_dsb != m_settings.m_dsb)
    || (settings.m_profiles[settings.m_profileIndex].m_highCutoff != m_settings.m_profiles[m_settings.m_profileIndex].m_highCutoff)
    || (settings.m_profiles[settings.m_profileIndex].m_lowCutoff != m_settings.m_profiles[m_settings.m_profileIndex].m_lowCutoff) || force)
    {
        SpectrumSettings spectrumSettings = m_spectrumVis.getSettings();
        spectrumSettings.m_ssb = !settings.m_dsb;
        spectrumSettings.m_usb = (settings.m_profiles[settings.m_profileIndex].m_lowCutoff < settings.m_profiles[settings.m_profileIndex].m_highCutoff);
        SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
        m_spectrumVis.getInputMessageQueue()->push(msg);
    }

    if (m_running)
    {
        WDSPRxBaseband::MsgConfigureWDSPRxBaseband *msg = WDSPRxBaseband::MsgConfigureWDSPRxBaseband::create(settings, force);
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

    if (!pipes.empty()) {
        sendChannelSettings(pipes, reverseAPIKeys, settings, force);
    }

    m_settings = settings;
}

QByteArray WDSPRx::serialize() const
{
    return m_settings.serialize();
}

bool WDSPRx::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureWDSPRx *msg = MsgConfigureWDSPRx::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureWDSPRx *msg = MsgConfigureWDSPRx::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void WDSPRx::sendSampleRateToDemodAnalyzer() const
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    if (!pipes.empty())
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

int WDSPRx::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setWdspRxSettings(new SWGSDRangel::SWGWDSPRxSettings());
    response.getWdspRxSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int WDSPRx::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int WDSPRx::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    WDSPRxSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureWDSPRx *msg = MsgConfigureWDSPRx::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("WDSPRx::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureWDSPRx *msgToGUI = MsgConfigureWDSPRx::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void WDSPRx::webapiUpdateChannelSettings(
        WDSPRxSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = (qint32) response.getWdspRxSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("profileIndex")) {
        settings.m_profileIndex = response.getWdspRxSettings()->getProfileIndex();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getWdspRxSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("audioBinaural")) {
        settings.m_audioBinaural = response.getWdspRxSettings()->getAudioBinaural() != 0;
    }
    if (channelSettingsKeys.contains("audioFlipChannels")) {
        settings.m_audioFlipChannels = response.getWdspRxSettings()->getAudioFlipChannels() != 0;
    }
    if (channelSettingsKeys.contains("dsb")) {
        settings.m_dsb = response.getWdspRxSettings()->getDsb() != 0;
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getWdspRxSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("agc")) {
        settings.m_agc = response.getWdspRxSettings()->getAgc() != 0;
    }
    if (channelSettingsKeys.contains("demod")) {
        settings.m_demod = (WDSPRxProfile::WDSPRxDemod) response.getWdspRxSettings()->getDemod();
    }
    if (channelSettingsKeys.contains("agcMode")) {
        settings.m_agcMode = (WDSPRxProfile::WDSPRxAGCMode) response.getWdspRxSettings()->getAgcMode();
    }
    if (channelSettingsKeys.contains("agcGain")) {
        settings.m_agcGain = response.getWdspRxSettings()->getAgcGain();
    }
    if (channelSettingsKeys.contains("agcSlope")) {
        settings.m_agcSlope = response.getWdspRxSettings()->getAgcSlope();
    }
    if (channelSettingsKeys.contains("agcHangThreshold")) {
        settings.m_agcHangThreshold = response.getWdspRxSettings()->getAgcHangThreshold();
    }
    if (channelSettingsKeys.contains("dnb")) {
        settings.m_dnb = response.getWdspRxSettings()->getDnb() != 0;
    }
    if (channelSettingsKeys.contains("nbScheme")) {
        settings.m_nbScheme = (WDSPRxProfile::WDSPRxNBScheme) response.getWdspRxSettings()->getNbScheme();
    }
    if (channelSettingsKeys.contains("nb2Mode")) {
        settings.m_nb2Mode = (WDSPRxProfile::WDSPRxNB2Mode) response.getWdspRxSettings()->getNb2Mode();
    }
    if (channelSettingsKeys.contains("nbSlewTime")) {
        settings.m_nbSlewTime = response.getWdspRxSettings()->getNbSlewTime();
    }
    if (channelSettingsKeys.contains("nbLeadTime")) {
        settings.m_nbLeadTime = response.getWdspRxSettings()->getNbLeadTime();
    }
    if (channelSettingsKeys.contains("nbLagTime")) {
        settings.m_nbLagTime = response.getWdspRxSettings()->getNbLagTime();
    }
    if (channelSettingsKeys.contains("nbThreshold")) {
        settings.m_nbThreshold = response.getWdspRxSettings()->getNbThreshold();
    }
    if (channelSettingsKeys.contains("nbAvgTime")) {
        settings.m_nbAvgTime = response.getWdspRxSettings()->getNbAvgTime();
    }
    if (channelSettingsKeys.contains("dnr")) {
        settings.m_dnr = response.getWdspRxSettings()->getDnr() != 0;
    }
    if (channelSettingsKeys.contains("anf")) {
        settings.m_anf = response.getWdspRxSettings()->getAnf() != 0;
    }
    if (channelSettingsKeys.contains("nrScheme")) {
        settings.m_nrScheme = (WDSPRxProfile::WDSPRxNRScheme)  response.getWdspRxSettings()->getNrScheme();
    }
    if (channelSettingsKeys.contains("nr2Gain")) {
        settings.m_nr2Gain = (WDSPRxProfile::WDSPRxNR2Gain)  response.getWdspRxSettings()->getNr2Gain();
    }
    if (channelSettingsKeys.contains("nr2NPE")) {
        settings.m_nr2NPE = (WDSPRxProfile::WDSPRxNR2NPE)  response.getWdspRxSettings()->getNr2Npe();
    }
    if (channelSettingsKeys.contains("nrPosition")) {
        settings.m_nrPosition = (WDSPRxProfile::WDSPRxNRPosition)  response.getWdspRxSettings()->getNrPosition();
    }
    if (channelSettingsKeys.contains("nr2ArtifactReduction")) {
        settings.m_nr2ArtifactReduction = response.getWdspRxSettings()->getNr2ArtifactReduction() != 0;
    }
    if (channelSettingsKeys.contains("amFadeLevel")) {
        settings.m_amFadeLevel = response.getWdspRxSettings()->getAmFadeLevel() != 0;
    }
    if (channelSettingsKeys.contains("cwPeaking")) {
        settings.m_cwPeaking = response.getWdspRxSettings()->getCwPeaking() != 0;
    }
    if (channelSettingsKeys.contains("cwPeakFrequency")) {
        settings.m_cwPeakFrequency = response.getWdspRxSettings()->getCwPeakFrequency();
    }
    if (channelSettingsKeys.contains("cwBandwidth")) {
        settings.m_cwBandwidth = response.getWdspRxSettings()->getCwBandwidth();
    }
    if (channelSettingsKeys.contains("cwGain")) {
        settings.m_cwGain = response.getWdspRxSettings()->getCwGain();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getWdspRxSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("fmAFLow")) {
        settings.m_fmAFLow = response.getWdspRxSettings()->getFmAfLow();
    }
    if (channelSettingsKeys.contains("fmAFHigh")) {
        settings.m_fmAFHigh = response.getWdspRxSettings()->getFmAfHigh();
    }
    if (channelSettingsKeys.contains("fmAFLimiter")) {
        settings.m_fmAFLimiter = response.getWdspRxSettings()->getFmAfLimiter() != 0;
    }
    if (channelSettingsKeys.contains("fmAFLimiterGain")) {
        settings.m_fmAFLimiterGain = response.getWdspRxSettings()->getFmAfLimiterGain();
    }
    if (channelSettingsKeys.contains("fmCTCSSNotch")) {
        settings.m_fmCTCSSNotch = response.getWdspRxSettings()->getFmCtcssNotch() != 0;
    }
    if (channelSettingsKeys.contains("fmCTCSSNotchFrequency")) {
        settings.m_fmCTCSSNotchFrequency = response.getWdspRxSettings()->getFmCtcssNotchFrequency();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getWdspRxSettings()->getSquelch() != 0;
    }
    if (channelSettingsKeys.contains("squelchThreshold")) {
        settings.m_squelchThreshold = response.getWdspRxSettings()->getSquelchThreshold();
    }
    if (channelSettingsKeys.contains("squelchMode")) {
        settings.m_squelchMode = (WDSPRxProfile::WDSPRxSquelchMode)  response.getWdspRxSettings()->getSquelchMode();
    }
    if (channelSettingsKeys.contains("ssqlTauMute")) {
        settings.m_ssqlTauMute = response.getWdspRxSettings()->getSsqlTauMute();
    }
    if (channelSettingsKeys.contains("ssqlTauUnmute")) {
        settings.m_ssqlTauUnmute = response.getWdspRxSettings()->getSsqlTauUnmute();
    }
    if (channelSettingsKeys.contains("amsqMaxTail")) {
        settings.m_amsqMaxTail = response.getWdspRxSettings()->getAmsqMaxTail();
    }
    if (channelSettingsKeys.contains("equalizer")) {
        settings.m_equalizer = response.getWdspRxSettings()->getEqualizer() != 0;
    }

    if (channelSettingsKeys.contains("eqF"))
    {
        const QList<float>* eqF = response.getWdspRxSettings()->getEqF();

        for (int i = 0; i < 11; i++)
        {
            if (i >= eqF->size()) {
                break;
            }

            settings.m_eqF[i] = (*eqF)[i];
        }
    }

    if (channelSettingsKeys.contains("eqG"))
    {
        const QList<float>* eqG = response.getWdspRxSettings()->getEqG();

        for (int i = 0; i < 11; i++)
        {
            if (i >= eqG->size()) {
                break;
            }

            settings.m_eqG[i] = (*eqG)[i];
        }
    }

    if (channelSettingsKeys.contains("rit")) {
        settings.m_rit = response.getWdspRxSettings()->getRit() != 0;
    }
    if (channelSettingsKeys.contains("ritFrequency")) {
        settings.m_ritFrequency = response.getWdspRxSettings()->getRitFrequency();
    }
    if (channelSettingsKeys.contains("spanLog2")) {
        settings.m_profiles[settings.m_profileIndex].m_spanLog2 = response.getWdspRxSettings()->getSpanLog2();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_profiles[settings.m_profileIndex].m_highCutoff = response.getWdspRxSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("lowCutoff")) {
        settings.m_profiles[settings.m_profileIndex].m_lowCutoff = response.getWdspRxSettings()->getLowCutoff();
    }
    if (channelSettingsKeys.contains("fftWimdow")) {
        settings.m_profiles[settings.m_profileIndex].m_fftWindow = response.getWdspRxSettings()->getFftWindow();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getWdspRxSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getWdspRxSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getWdspRxSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getWdspRxSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getWdspRxSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getWdspRxSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = (uint16_t) response.getWdspRxSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = (uint16_t) response.getWdspRxSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = (uint16_t) response.getWdspRxSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_spectrumGUI && channelSettingsKeys.contains("spectrumConfig")) {
        settings.m_spectrumGUI->updateFrom(channelSettingsKeys, response.getWdspRxSettings()->getSpectrumConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getWdspRxSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getWdspRxSettings()->getRollupState());
    }
}

int WDSPRx::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setWdspRxReport(new SWGSDRangel::SWGWDSPRxReport());
    response.getWdspRxReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void WDSPRx::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const WDSPRxSettings& settings)
{
    response.getWdspRxSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getWdspRxSettings()->setProfileIndex(settings.m_profileIndex);
    response.getWdspRxSettings()->setDemod((int) settings.m_demod);
    response.getWdspRxSettings()->setVolume(settings.m_volume);
    response.getWdspRxSettings()->setAudioBinaural(settings.m_audioBinaural ? 1 : 0);
    response.getWdspRxSettings()->setAudioFlipChannels(settings.m_audioFlipChannels ? 1 : 0);
    response.getWdspRxSettings()->setDsb(settings.m_dsb ? 1 : 0);
    response.getWdspRxSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getWdspRxSettings()->setAgc(settings.m_agc ? 1 : 0);
    response.getWdspRxSettings()->setAgcMode((int) settings.m_agcMode);
    response.getWdspRxSettings()->setAgcGain(settings.m_agcGain);
    response.getWdspRxSettings()->setAgcSlope(settings.m_agcSlope);
    response.getWdspRxSettings()->setAgcHangThreshold(settings.m_agcHangThreshold);
    response.getWdspRxSettings()->setDnb(settings.m_dnb ? 1 : 0);
    response.getWdspRxSettings()->setNbScheme((int) settings.m_nbScheme);
    response.getWdspRxSettings()->setNb2Mode((int) settings.m_nb2Mode);
    response.getWdspRxSettings()->setNbSlewTime((float) settings.m_nbSlewTime);
    response.getWdspRxSettings()->setNbLeadTime((float) settings.m_nbLeadTime);
    response.getWdspRxSettings()->setNbLagTime((float) settings.m_nbLagTime);
    response.getWdspRxSettings()->setNbThreshold(settings.m_nbThreshold);
    response.getWdspRxSettings()->setNbAvgTime((float) settings.m_nbAvgTime);
    response.getWdspRxSettings()->setDnr(settings.m_dnr ? 1 : 0);
    response.getWdspRxSettings()->setAnf(settings.m_anf ? 1 : 0);
    response.getWdspRxSettings()->setNrScheme((int) settings.m_nrScheme);
    response.getWdspRxSettings()->setNr2Gain((int) settings.m_nr2Gain);
    response.getWdspRxSettings()->setNr2Npe((int) settings.m_nr2NPE);
    response.getWdspRxSettings()->setNrPosition((int) settings.m_nrPosition);
    response.getWdspRxSettings()->setNr2ArtifactReduction(settings.m_nr2ArtifactReduction ? 1 : 0);
    response.getWdspRxSettings()->setAmFadeLevel(settings.m_amFadeLevel ? 1 : 0);
    response.getWdspRxSettings()->setCwPeaking(settings.m_cwPeaking ? 1 : 0);
    response.getWdspRxSettings()->setCwPeakFrequency((float) settings.m_cwPeakFrequency);
    response.getWdspRxSettings()->setCwBandwidth((float) settings.m_cwBandwidth);
    response.getWdspRxSettings()->setCwGain((float) settings.m_cwGain);
    response.getWdspRxSettings()->setFmDeviation((float) settings.m_fmDeviation);
    response.getWdspRxSettings()->setFmAfLow((float) settings.m_fmAFLow);
    response.getWdspRxSettings()->setFmAfHigh((float) settings.m_fmAFHigh);
    response.getWdspRxSettings()->setFmAfLimiter(settings.m_fmAFLimiter ? 1 : 0);
    response.getWdspRxSettings()->setFmAfLimiterGain((float) settings.m_fmAFLimiterGain);
    response.getWdspRxSettings()->setFmCtcssNotch(settings.m_fmCTCSSNotch ? 1 : 0);
    response.getWdspRxSettings()->setFmCtcssNotchFrequency((float) settings.m_fmCTCSSNotchFrequency);
    response.getWdspRxSettings()->setSquelch(settings.m_squelch ? 1 : 0);
    response.getWdspRxSettings()->setSquelchThreshold(settings.m_squelchThreshold);
    response.getWdspRxSettings()->setSquelchMode((int) settings.m_squelchMode);
    response.getWdspRxSettings()->setSsqlTauMute((float) settings.m_ssqlTauMute);
    response.getWdspRxSettings()->setSsqlTauUnmute((float) settings.m_ssqlTauUnmute);
    response.getWdspRxSettings()->setAmsqMaxTail((float) settings.m_amsqMaxTail);
    response.getWdspRxSettings()->setEqualizer(settings.m_equalizer ? 1 : 0);

    if (!response.getWdspRxSettings()->getEqF()) {
        response.getWdspRxSettings()->setEqF(new QList<float>());
    }

    response.getWdspRxSettings()->getEqF()->clear();

    for (int i = 0; i < 11; i++) {
        response.getWdspRxSettings()->getEqF()->push_back(settings.m_eqF[i]);
    }

    if (!response.getWdspRxSettings()->getEqG()) {
        response.getWdspRxSettings()->setEqG(new QList<float>());
    }

    response.getWdspRxSettings()->getEqG()->clear();

    for (int i = 0; i < 11; i++) {
        response.getWdspRxSettings()->getEqG()->push_back(settings.m_eqG[i]);
    }

    response.getWdspRxSettings()->setRit(settings.m_rit ? 1 : 0);
    response.getWdspRxSettings()->setRitFrequency((float) settings.m_ritFrequency);
    response.getWdspRxSettings()->setSpanLog2(settings.m_profiles[settings.m_profileIndex].m_spanLog2);
    response.getWdspRxSettings()->setRfBandwidth(settings.m_profiles[settings.m_profileIndex].m_highCutoff);
    response.getWdspRxSettings()->setLowCutoff(settings.m_profiles[settings.m_profileIndex].m_lowCutoff);
    response.getWdspRxSettings()->setFftWindow(settings.m_profiles[settings.m_profileIndex].m_fftWindow);
    response.getWdspRxSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getWdspRxSettings()->getTitle()) {
        *response.getWdspRxSettings()->getTitle() = settings.m_title;
    } else {
        response.getWdspRxSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getWdspRxSettings()->getAudioDeviceName()) {
        *response.getWdspRxSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getWdspRxSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getWdspRxSettings()->setStreamIndex(settings.m_streamIndex);
    response.getWdspRxSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getWdspRxSettings()->getReverseApiAddress()) {
        *response.getWdspRxSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getWdspRxSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getWdspRxSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getWdspRxSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getWdspRxSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_spectrumGUI)
    {
        if (response.getWdspRxSettings()->getSpectrumConfig())
        {
            settings.m_spectrumGUI->formatTo(response.getWdspRxSettings()->getSpectrumConfig());
        }
        else
        {
            auto *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
            settings.m_spectrumGUI->formatTo(swgGLSpectrum);
            response.getWdspRxSettings()->setSpectrumConfig(swgGLSpectrum);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getWdspRxSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getWdspRxSettings()->getChannelMarker());
        }
        else
        {
            auto *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getWdspRxSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getWdspRxSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getWdspRxSettings()->getRollupState());
        }
        else
        {
            auto *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getWdspRxSettings()->setRollupState(swgRollupState);
        }
    }
}

void WDSPRx::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg;
    double magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getSsbDemodReport()->setChannelPowerDb((float) CalcDb::dbPower(magsqAvg));

    if (m_running)
    {
        response.getSsbDemodReport()->setSquelch(m_basebandSink->getAudioActive() ? 1 : 0);
        response.getSsbDemodReport()->setAudioSampleRate(m_basebandSink->getAudioSampleRate());
        response.getSsbDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
    }
}

void WDSPRx::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const WDSPRxSettings& settings, bool force)
{
    auto *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto *buffer = new QBuffer();
    buffer->open(QBuffer::ReadWrite);
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgChannelSettings;
}

void WDSPRx::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    const QList<QString>& channelSettingsKeys,
    const WDSPRxSettings& settings,
    bool force) const
{
    qDebug("WDSPRx::sendChannelSettings: %d pipes", (int)pipes.size());

    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

        if (messageQueue)
        {
            auto *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
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

void WDSPRx::webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const WDSPRxSettings& settings,
        bool force
) const
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setWdspRxSettings(new SWGSDRangel::SWGWDSPRxSettings());
    SWGSDRangel::SWGWDSPRxSettings *swgWDSPRxSettings = swgChannelSettings->getWdspRxSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        swgWDSPRxSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("profileIndex")) {
        swgWDSPRxSettings->setProfileIndex(settings.m_profileIndex);
    }
    if (channelSettingsKeys.contains("volume")) {
        swgWDSPRxSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("audioBinaural")) {
        swgWDSPRxSettings->setAudioBinaural(settings.m_audioBinaural ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioFlipChannels")) {
        swgWDSPRxSettings->setAudioFlipChannels(settings.m_audioFlipChannels ? 1 : 0);
    }
    if (channelSettingsKeys.contains("dsb")) {
        swgWDSPRxSettings->setDsb(settings.m_dsb ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioMute")) {
        swgWDSPRxSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("agc")) {
        swgWDSPRxSettings->setAgc(settings.m_agc ? 1 : 0);
    }
    if (channelSettingsKeys.contains("demod")) {
        swgWDSPRxSettings->setDemod((int) settings.m_demod);
    }
    if (channelSettingsKeys.contains("agcMode")) {
        swgWDSPRxSettings->setAgcMode((int) settings.m_agcMode);
    }
    if (channelSettingsKeys.contains("agcGain")) {
        swgWDSPRxSettings->setAgcGain(settings.m_agcGain);
    }
    if (channelSettingsKeys.contains("agcSlope")) {
        swgWDSPRxSettings->setAgcSlope(settings.m_agcSlope);
    }
    if (channelSettingsKeys.contains("agcHangThreshold")) {
        swgWDSPRxSettings->setAgcHangThreshold(settings.m_agcHangThreshold);
    }
    if (channelSettingsKeys.contains("dnb")) {
        swgWDSPRxSettings->setDnb(settings.m_dnb ? 1 : 0);
    }
    if (channelSettingsKeys.contains("nbScheme")) {
        swgWDSPRxSettings->setNbScheme((int) settings.m_nbScheme);
    }
    if (channelSettingsKeys.contains("nb2Mode")) {
        swgWDSPRxSettings->setNb2Mode((int) settings.m_nb2Mode);
    }
    if (channelSettingsKeys.contains("nbSlewTime")) {
        swgWDSPRxSettings->setNbSlewTime((float) settings.m_nbSlewTime);
    }
    if (channelSettingsKeys.contains("nbLeadTime")) {
        swgWDSPRxSettings->setNbLeadTime((float) settings.m_nbSlewTime);
    }
    if (channelSettingsKeys.contains("nbLagTime")) {
        swgWDSPRxSettings->setNbLagTime((float) settings.m_nbLagTime);
    }
    if (channelSettingsKeys.contains("nbThreshold")) {
        swgWDSPRxSettings->setNbThreshold(settings.m_nbThreshold);
    }
    if (channelSettingsKeys.contains("nbAvgTime")) {
        swgWDSPRxSettings->setNbAvgTime((float) settings.m_nbAvgTime);
    }
    if (channelSettingsKeys.contains("dnr")) {
        swgWDSPRxSettings->setDnr(settings.m_dnr ? 1 : 0);
    }
    if (channelSettingsKeys.contains("anf")) {
        swgWDSPRxSettings->setAnf(settings.m_anf ? 1 : 0);
    }
    if (channelSettingsKeys.contains("nrScheme")) {
        swgWDSPRxSettings->setNrScheme((int) settings.m_nrScheme);
    }
    if (channelSettingsKeys.contains("nr2Gain")) {
        swgWDSPRxSettings->setNr2Gain((int) settings.m_nr2Gain);
    }
    if (channelSettingsKeys.contains("nr2NPE")) {
        swgWDSPRxSettings->setNr2Npe((int) settings.m_nr2NPE);
    }
    if (channelSettingsKeys.contains("nrPosition")) {
        swgWDSPRxSettings->setNrPosition((int) settings.m_nrPosition);
    }
    if (channelSettingsKeys.contains("nr2ArtifactReduction")) {
        swgWDSPRxSettings->setNr2ArtifactReduction(settings.m_nr2ArtifactReduction ? 1 : 0);
    }
    if (channelSettingsKeys.contains("amFadeLevel")) {
        swgWDSPRxSettings->setAmFadeLevel(settings.m_amFadeLevel ? 1 : 0);
    }
    if (channelSettingsKeys.contains("cwPeaking")) {
        swgWDSPRxSettings->setCwPeaking(settings.m_cwPeaking ? 1 : 0);
    }
    if (channelSettingsKeys.contains("cwPeakFrequency")) {
        swgWDSPRxSettings->setCwPeakFrequency((float) settings.m_cwPeakFrequency);
    }
    if (channelSettingsKeys.contains("cwBandwidth")) {
        swgWDSPRxSettings->setCwBandwidth((float) settings.m_cwBandwidth);
    }
    if (channelSettingsKeys.contains("cwGain")) {
        swgWDSPRxSettings->setCwGain((float) settings.m_cwGain);
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        swgWDSPRxSettings->setFmDeviation((float) settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("fmAFLow")) {
        swgWDSPRxSettings->setFmAfLow((float) settings.m_fmAFLow);
    }
    if (channelSettingsKeys.contains("fmAFHigh")) {
        swgWDSPRxSettings->setFmAfHigh((float) settings.m_fmAFHigh);
    }
    if (channelSettingsKeys.contains("fmAFLimiter")) {
        swgWDSPRxSettings->setFmAfLimiter(settings.m_fmAFLimiter ? 1 : 0);
    }
    if (channelSettingsKeys.contains("fmAFLimiterGain")) {
        swgWDSPRxSettings->setFmAfLimiterGain((float) settings.m_fmAFLimiterGain);
    }
    if (channelSettingsKeys.contains("fmCTCSSNotch")) {
        swgWDSPRxSettings->setFmCtcssNotch(settings.m_fmCTCSSNotch ? 1 : 0);
    }
    if (channelSettingsKeys.contains("fmCTCSSNotchFrequency")) {
        swgWDSPRxSettings->setFmCtcssNotchFrequency((float) settings.m_fmCTCSSNotchFrequency);
    }
    if (channelSettingsKeys.contains("squelch")) {
        swgWDSPRxSettings->setSquelch(settings.m_squelch ? 1 : 0);
    }
    if (channelSettingsKeys.contains("squelchThreshold")) {
        swgWDSPRxSettings->setSquelchThreshold(settings.m_squelchThreshold);
    }
    if (channelSettingsKeys.contains("squelchMode")) {
        swgWDSPRxSettings->setSquelchMode((int) settings.m_squelchMode);
    }
    if (channelSettingsKeys.contains("ssqlTauMute")) {
        swgWDSPRxSettings->setSsqlTauMute((float) settings.m_ssqlTauMute);
    }
    if (channelSettingsKeys.contains("ssqlTauUnmute")) {
        swgWDSPRxSettings->setSsqlTauUnmute((float) settings.m_ssqlTauUnmute);
    }
    if (channelSettingsKeys.contains("amsqMaxTail")) {
        swgWDSPRxSettings->setAmsqMaxTail((float) settings.m_amsqMaxTail);
    }
    if (channelSettingsKeys.contains("equalizer")) {
        swgWDSPRxSettings->setEqualizer(settings.m_equalizer ? 1 : 0);
    }

    if (channelSettingsKeys.contains("eqF"))
    {
        if (!swgWDSPRxSettings->getEqF()) {
            swgWDSPRxSettings->setEqF(new QList<float>());
        }

        swgWDSPRxSettings->getEqF()->clear();

        for (int i = 0; i < 11; i++) {
            swgWDSPRxSettings->getEqF()->push_back(settings.m_eqF[i]);
        }
    }

    if (channelSettingsKeys.contains("eqG"))
    {
        if (!swgWDSPRxSettings->getEqG()) {
            swgWDSPRxSettings->setEqG(new QList<float>());
        }

        swgWDSPRxSettings->getEqG()->clear();

        for (int i = 0; i < 11; i++) {
            swgWDSPRxSettings->getEqG()->push_back(settings.m_eqG[i]);
        }
    }

    if (channelSettingsKeys.contains("rit")) {
        swgWDSPRxSettings->setRit(settings.m_rit ? 1 : 0);
    }
    if (channelSettingsKeys.contains("ritFrequency")) {
        swgWDSPRxSettings->setRit((qint32) settings.m_ritFrequency);
    }
    if (channelSettingsKeys.contains("spanLog2") || force) {
        swgWDSPRxSettings->setSpanLog2(settings.m_profiles[settings.m_profileIndex].m_spanLog2);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgWDSPRxSettings->setRfBandwidth(settings.m_profiles[settings.m_profileIndex].m_highCutoff);
    }
    if (channelSettingsKeys.contains("lowCutoff") || force) {
        swgWDSPRxSettings->setLowCutoff(settings.m_profiles[settings.m_profileIndex].m_lowCutoff);
    }
    if (channelSettingsKeys.contains("fftWindow") || force) {
        swgWDSPRxSettings->setLowCutoff((float) settings.m_profiles[settings.m_profileIndex].m_fftWindow);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgWDSPRxSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgWDSPRxSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgWDSPRxSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgWDSPRxSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_spectrumGUI && (channelSettingsKeys.contains("spectrunConfig") || force))
    {
        auto *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
        settings.m_spectrumGUI->formatTo(swgGLSpectrum);
        swgWDSPRxSettings->setSpectrumConfig(swgGLSpectrum);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        auto *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgWDSPRxSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        auto *swgRolllupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRolllupState);
        swgWDSPRxSettings->setRollupState(swgRolllupState);
    }
}

void WDSPRx::networkManagerFinished(QNetworkReply *reply) const
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "WDSPRx::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("WDSPRx::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void WDSPRx::handleIndexInDeviceSetChanged(int index)
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
