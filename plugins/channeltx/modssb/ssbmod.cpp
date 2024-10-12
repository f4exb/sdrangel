///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2019 Stefan Biereigel <stefan@biereigel.de>                     //
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
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGSSBModReport.h"

#include "dsp/dspcommands.h"
#include "dsp/cwkeyer.h"
#include "device/deviceapi.h"
#include "util/db.h"
#include "maincore.h"

#include "ssbmodbaseband.h"
#include "ssbmod.h"

MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureSSBMod, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgReportFileSourceStreamTiming, Message)

const char* const SSBMod::m_channelIdURI = "sdrangel.channeltx.modssb";
const char* const SSBMod::m_channelId = "SSBMod";

SSBMod::SSBMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF)
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new SSBModBaseband();
    m_basebandSource->setSpectrumSink(&m_spectrumVis);
    m_basebandSource->setInputFileStream(&m_ifstream);
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
        &SSBMod::networkManagerFinished
    );
}

SSBMod::~SSBMod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SSBMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void SSBMod::setDeviceAPI(DeviceAPI *deviceAPI)
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

void SSBMod::start()
{
    if (m_running) {
        return;
    }

	qDebug("SSBMod::start");
    m_basebandSource->reset();
    m_thread->start();
    m_running = true;
}

void SSBMod::stop()
{
    if (!m_running) {
        return;
    }

    qDebug("SSBMod::stop");
    m_running = false;
	m_thread->exit();
	m_thread->wait();
}

void SSBMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    if (m_running) {
        m_basebandSource->pull(begin, nbSamples);
    }
}

void SSBMod::setCenterFrequency(qint64 frequency)
{
    SSBModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSSBMod *msgToGUI = MsgConfigureSSBMod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

bool SSBMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureSSBMod::match(cmd))
    {
        auto& cfg = (const MsgConfigureSSBMod&) cmd;
        qDebug() << "SSBMod::handleMessage: MsgConfigureSSBMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
	else if (MsgConfigureFileSourceName::match(cmd))
    {
        auto& conf = (const MsgConfigureFileSourceName&) cmd;
        m_fileName = conf.getFileName();
        openFileStream();
        return true;
    }
    else if (MsgConfigureFileSourceSeek::match(cmd))
    {
        auto& conf = (const MsgConfigureFileSourceSeek&) cmd;
        int seekPercentage = conf.getPercentage();
        seekFileStream(seekPercentage);

        return true;
    }
    else if (MsgConfigureFileSourceStreamTiming::match(cmd))
    {
        std::size_t samplesCount;

        if (m_ifstream.eof()) {
            samplesCount = m_fileSize / sizeof(Real);
        } else {
            samplesCount = m_ifstream.tellg() / sizeof(Real);
        }

        if (getMessageQueueToGUI())
        {
            auto *report = MsgReportFileSourceStreamTiming::create(samplesCount);
            getMessageQueueToGUI()->push(report);
        }

        return true;
    }
    else if (CWKeyer::MsgConfigureCWKeyer::match(cmd))
    {
        auto& cfg = (const CWKeyer::MsgConfigureCWKeyer&) cmd;

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendCWSettings(cfg.getSettings());
        }

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        auto& notif = (const DSPSignalNotification&) cmd;
        // Forward to the source
        if (m_running)
        {
            auto* rep = new DSPSignalNotification(notif); // make a copy
            qDebug() << "SSBMod::handleMessage: DSPSignalNotification";
            m_basebandSource->getInputMessageQueue()->push(rep);
        }
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "SSBMod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
	else
	{
		return false;
	}
}

void SSBMod::openFileStream()
{
    if (m_ifstream.is_open()) {
        m_ifstream.close();
    }

    m_ifstream.open(m_fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
    m_fileSize = m_ifstream.tellg();
    m_ifstream.seekg(0,std::ios_base::beg);

    m_sampleRate = 48000; // fixed rate
    m_recordLength = (quint32) (m_fileSize / (sizeof(Real) * m_sampleRate));

    qDebug() << "SSBMod::openFileStream: " << m_fileName.toStdString().c_str()
            << " fileSize: " << m_fileSize << "bytes"
            << " length: " << m_recordLength << " seconds";

    if (getMessageQueueToGUI())
    {
        MsgReportFileSourceStreamData *report;
        report = MsgReportFileSourceStreamData::create(m_sampleRate, m_recordLength);
        getMessageQueueToGUI()->push(report);
    }
}

void SSBMod::seekFileStream(int seekPercentage)
{
    QMutexLocker mutexLocker(&m_settingsMutex);

    if (m_ifstream.is_open())
    {
        int seekPoint = ((m_recordLength * seekPercentage) / 100) * m_sampleRate;
        seekPoint *= sizeof(Real);
        m_ifstream.clear();
        m_ifstream.seekg(seekPoint, std::ios::beg);
    }
}

void SSBMod::applySettings(const SSBModSettings& settings, bool force)
{
    float band = settings.m_bandwidth;
    float lowCutoff = settings.m_lowCutoff;
    bool usb = settings.m_usb;
    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_bandwidth != m_settings.m_bandwidth) || force) {
        reverseAPIKeys.append("bandwidth");
    }
    if ((settings.m_lowCutoff != m_settings.m_lowCutoff) || force) {
        reverseAPIKeys.append("lowCutoff");
    }
    if ((settings.m_usb != m_settings.m_usb) || force) {
        reverseAPIKeys.append("usb");
    }
    if ((settings.m_toneFrequency != m_settings.m_toneFrequency) || force) {
        reverseAPIKeys.append("toneFrequency");
    }
    if ((settings.m_volumeFactor != m_settings.m_volumeFactor) || force) {
        reverseAPIKeys.append("volumeFactor");
    }
    if ((settings.m_spanLog2 != m_settings.m_spanLog2) || force) {
        reverseAPIKeys.append("spanLog2");
    }
    if ((settings.m_audioBinaural != m_settings.m_audioBinaural) || force) {
        reverseAPIKeys.append("audioBinaural");
    }
    if ((settings.m_audioFlipChannels != m_settings.m_audioFlipChannels) || force) {
        reverseAPIKeys.append("audioFlipChannels");
    }
    if ((settings.m_dsb != m_settings.m_dsb) || force) {
        reverseAPIKeys.append("dsb");
    }
    if ((settings.m_audioMute != m_settings.m_audioMute) || force) {
        reverseAPIKeys.append("audioMute");
    }
    if ((settings.m_playLoop != m_settings.m_playLoop) || force) {
        reverseAPIKeys.append("playLoop");
    }
    if ((settings.m_agc != m_settings.m_agc) || force) {
        reverseAPIKeys.append("agc");
    }
    if ((settings.m_cmpPreGainDB != m_settings.m_cmpPreGainDB) || force) {
        reverseAPIKeys.append("cmpPreGainDB");
    }
    if ((settings.m_cmpThresholdDB != m_settings.m_cmpThresholdDB) || force) {
        reverseAPIKeys.append("cmpThresholdDB");
    }
    if ((settings.m_rgbColor != m_settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((settings.m_title != m_settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((settings.m_modAFInput != m_settings.m_modAFInput) || force) {
        reverseAPIKeys.append("modAFInput");
    }
    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force) {
        reverseAPIKeys.append("audioDeviceName");
    }
    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force) {
        reverseAPIKeys.append("audioDeviceName");
    }
    if ((settings.m_feedbackAudioDeviceName != m_settings.m_feedbackAudioDeviceName) || force) {
        reverseAPIKeys.append("feedbackAudioDeviceName");
    }

    if (m_settings.m_streamIndex != settings.m_streamIndex)
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSourceAPI(this);
            m_deviceAPI->removeChannelSource(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSource(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSourceAPI(this);
            m_settings.m_streamIndex = settings.m_streamIndex; // make sure ChannelAPI::getStreamIndex() is consistent
            emit streamIndexChanged(settings.m_streamIndex);
        }

        reverseAPIKeys.append("streamIndex");
    }

    if ((settings.m_dsb != m_settings.m_dsb)
     || (settings.m_usb != m_settings.m_usb) || force)
    {
        SpectrumSettings spectrumSettings = m_spectrumVis.getSettings();
        spectrumSettings.m_ssb = !settings.m_dsb;
        spectrumSettings.m_usb = settings.m_usb;
        SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
        m_spectrumVis.getInputMessageQueue()->push(msg);
    }

    if (m_running)
    {
        SSBModBaseband::MsgConfigureSSBModBaseband *msg = SSBModBaseband::MsgConfigureSSBModBaseband::create(settings, force);
        m_basebandSource->getInputMessageQueue()->push(msg);
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
    m_settings.m_bandwidth = band;
    m_settings.m_lowCutoff = lowCutoff;
    m_settings.m_usb = usb;
}

QByteArray SSBMod::serialize() const
{
    return m_settings.serialize();
}

bool SSBMod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureSSBMod *msg = MsgConfigureSSBMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureSSBMod *msg = MsgConfigureSSBMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void SSBMod::sendSampleRateToDemodAnalyzer() const
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    if (!pipes.empty())
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue* messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(
                this,
                getAudioSampleRate()
            );
            messageQueue->push(msg);
        }
    }
}

int SSBMod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setSsbModSettings(new SWGSDRangel::SWGSSBModSettings());
    response.getSsbModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getSsbModSettings()->getCwKeyer();
    const CWKeyerSettings& cwKeyerSettings = m_cwKeyer.getSettings();
    CWKeyer::webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);

    return 200;
}

int SSBMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int SSBMod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    SSBModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    if (channelSettingsKeys.contains("cwKeyer"))
    {
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getSsbModSettings()->getCwKeyer();
        CWKeyerSettings cwKeyerSettings = m_cwKeyer.getSettings();
        CWKeyer::webapiSettingsPutPatch(channelSettingsKeys, cwKeyerSettings, apiCwKeyerSettings);

        CWKeyer::MsgConfigureCWKeyer *msgCwKeyer = CWKeyer::MsgConfigureCWKeyer::create(cwKeyerSettings, force);
        m_cwKeyer.getInputMessageQueue()->push(msgCwKeyer);

        if (m_guiMessageQueue) // forward to GUI if any
        {
            CWKeyer::MsgConfigureCWKeyer *msgCwKeyerToGUI = CWKeyer::MsgConfigureCWKeyer::create(cwKeyerSettings, force);
            m_guiMessageQueue->push(msgCwKeyerToGUI);
        }
    }

    MsgConfigureSSBMod *msg = MsgConfigureSSBMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSSBMod *msgToGUI = MsgConfigureSSBMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void SSBMod::webapiUpdateChannelSettings(
        SSBModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getSsbModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = response.getSsbModSettings()->getBandwidth();
    }
    if (channelSettingsKeys.contains("lowCutoff")) {
        settings.m_lowCutoff = response.getSsbModSettings()->getLowCutoff();
    }
    if (channelSettingsKeys.contains("usb")) {
        settings.m_usb = response.getSsbModSettings()->getUsb() != 0;
    }
    if (channelSettingsKeys.contains("toneFrequency")) {
        settings.m_toneFrequency = response.getSsbModSettings()->getToneFrequency();
    }
    if (channelSettingsKeys.contains("volumeFactor")) {
        settings.m_volumeFactor = response.getSsbModSettings()->getVolumeFactor();
    }
    if (channelSettingsKeys.contains("spanLog2")) {
        settings.m_spanLog2 = response.getSsbModSettings()->getSpanLog2();
    }
    if (channelSettingsKeys.contains("audioBinaural")) {
        settings.m_audioBinaural = response.getSsbModSettings()->getAudioBinaural() != 0;
    }
    if (channelSettingsKeys.contains("audioFlipChannels")) {
        settings.m_audioFlipChannels = response.getSsbModSettings()->getAudioFlipChannels() != 0;
    }
    if (channelSettingsKeys.contains("dsb")) {
        settings.m_dsb = response.getSsbModSettings()->getDsb() != 0;
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getSsbModSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("playLoop")) {
        settings.m_playLoop = response.getSsbModSettings()->getPlayLoop() != 0;
    }
    if (channelSettingsKeys.contains("agc")) {
        settings.m_agc = response.getSsbModSettings()->getAgc() != 0;
    }
    if (channelSettingsKeys.contains("cmpPreGainDB")) {
        settings.m_cmpPreGainDB = response.getSsbModSettings()->getCmpPreGainDb();
    }
    if (channelSettingsKeys.contains("cmpThresholdDB")) {
        settings.m_cmpThresholdDB = response.getSsbModSettings()->getCmpThresholdDb();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getSsbModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getSsbModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("modAFInput")) {
        settings.m_modAFInput = (SSBModSettings::SSBModInputAF) response.getSsbModSettings()->getModAfInput();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getSsbModSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getSsbModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getSsbModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getSsbModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = (uint16_t) response.getSsbModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = (uint16_t) response.getSsbModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = (uint16_t) response.getSsbModSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_spectrumGUI && channelSettingsKeys.contains("spectrumConfig")) {
        settings.m_spectrumGUI->updateFrom(channelSettingsKeys, response.getSsbModSettings()->getSpectrumConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getSsbModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getSsbModSettings()->getRollupState());
    }
}

int SSBMod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setSsbModReport(new SWGSDRangel::SWGSSBModReport());
    response.getSsbModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void SSBMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const SSBModSettings& settings)
{
    response.getSsbModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getSsbModSettings()->setBandwidth(settings.m_bandwidth);
    response.getSsbModSettings()->setLowCutoff(settings.m_lowCutoff);
    response.getSsbModSettings()->setUsb(settings.m_usb ? 1 : 0);
    response.getSsbModSettings()->setToneFrequency(settings.m_toneFrequency);
    response.getSsbModSettings()->setVolumeFactor(settings.m_volumeFactor);
    response.getSsbModSettings()->setSpanLog2(settings.m_spanLog2);
    response.getSsbModSettings()->setAudioBinaural(settings.m_audioBinaural ? 1 : 0);
    response.getSsbModSettings()->setAudioFlipChannels(settings.m_audioFlipChannels ? 1 : 0);
    response.getSsbModSettings()->setDsb(settings.m_dsb ? 1 : 0);
    response.getSsbModSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getSsbModSettings()->setPlayLoop(settings.m_playLoop ? 1 : 0);
    response.getSsbModSettings()->setAgc(settings.m_agc ? 1 : 0);
    response.getSsbModSettings()->setCmpPreGainDb(settings.m_cmpPreGainDB);
    response.getSsbModSettings()->setCmpThresholdDb(settings.m_cmpThresholdDB);
    response.getSsbModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getSsbModSettings()->getTitle()) {
        *response.getSsbModSettings()->getTitle() = settings.m_title;
    } else {
        response.getSsbModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getSsbModSettings()->setModAfInput((int) settings.m_modAFInput);

    if (response.getSsbModSettings()->getAudioDeviceName()) {
        *response.getSsbModSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getSsbModSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    if (!response.getSsbModSettings()->getCwKeyer()) {
        response.getSsbModSettings()->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings);
    }

    response.getSsbModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getSsbModSettings()->getReverseApiAddress()) {
        *response.getSsbModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getSsbModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getSsbModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getSsbModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getSsbModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_spectrumGUI)
    {
        if (response.getSsbModSettings()->getSpectrumConfig())
        {
            settings.m_spectrumGUI->formatTo(response.getSsbModSettings()->getSpectrumConfig());
        }
        else
        {
            auto *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
            settings.m_spectrumGUI->formatTo(swgGLSpectrum);
            response.getSsbModSettings()->setSpectrumConfig(swgGLSpectrum);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getSsbModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getSsbModSettings()->getChannelMarker());
        }
        else
        {
            auto *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getSsbModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getSsbModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getSsbModSettings()->getRollupState());
        }
        else
        {
            auto *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getSsbModSettings()->setRollupState(swgRollupState);
        }
    }
}

void SSBMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response) const
{
    response.getSsbModReport()->setChannelPowerDb((float) CalcDb::dbPower(getMagSq()));

    if (m_running)
    {
        response.getSsbModReport()->setAudioSampleRate(m_basebandSource->getAudioSampleRate());
        response.getSsbModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
    }
}

void SSBMod::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const SSBModSettings& settings, bool force)
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

void SSBMod::webapiReverseSendCWSettings(const CWKeyerSettings& cwKeyerSettings)
{
    auto *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setChannelType(new QString("SSBMod"));
    swgChannelSettings->setSsbModSettings(new SWGSDRangel::SWGSSBModSettings());
    SWGSDRangel::SWGSSBModSettings *swgSSBModSettings = swgChannelSettings->getSsbModSettings();

    swgSSBModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgSSBModSettings->getCwKeyer();
    CWKeyer::webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex)
            .arg(m_settings.m_reverseAPIChannelIndex);
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

void SSBMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    const QList<QString>& channelSettingsKeys,
    const SSBModSettings& settings,
    bool force) const
{
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

void SSBMod::webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const SSBModSettings& settings,
        bool force
) const
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setSsbModSettings(new SWGSDRangel::SWGSSBModSettings());
    SWGSDRangel::SWGSSBModSettings *swgSSBModSettings = swgChannelSettings->getSsbModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgSSBModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("bandwidth") || force) {
        swgSSBModSettings->setBandwidth(settings.m_bandwidth);
    }
    if (channelSettingsKeys.contains("lowCutoff") || force) {
        swgSSBModSettings->setLowCutoff(settings.m_lowCutoff);
    }
    if (channelSettingsKeys.contains("usb") || force) {
        swgSSBModSettings->setUsb(settings.m_usb ? 1 : 0);
    }
    if (channelSettingsKeys.contains("toneFrequency") || force) {
        swgSSBModSettings->setToneFrequency(settings.m_toneFrequency);
    }
    if (channelSettingsKeys.contains("volumeFactor") || force) {
        swgSSBModSettings->setVolumeFactor(settings.m_volumeFactor);
    }
    if (channelSettingsKeys.contains("spanLog2") || force) {
        swgSSBModSettings->setSpanLog2(settings.m_spanLog2);
    }
    if (channelSettingsKeys.contains("audioBinaural") || force) {
        swgSSBModSettings->setAudioBinaural(settings.m_audioBinaural ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioFlipChannels") || force) {
        swgSSBModSettings->setAudioFlipChannels(settings.m_audioFlipChannels ? 1 : 0);
    }
    if (channelSettingsKeys.contains("dsb") || force) {
        swgSSBModSettings->setDsb(settings.m_dsb ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgSSBModSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("playLoop") || force) {
        swgSSBModSettings->setPlayLoop(settings.m_playLoop ? 1 : 0);
    }
    if (channelSettingsKeys.contains("agc") || force) {
        swgSSBModSettings->setAgc(settings.m_agc ? 1 : 0);
    }
    if (channelSettingsKeys.contains("cmpPreGainDB") || force) {
        swgSSBModSettings->setCmpPreGainDb(settings.m_cmpPreGainDB);
    }
    if (channelSettingsKeys.contains("cmpThresholdDB") || force) {
        swgSSBModSettings->setCmpThresholdDb(settings.m_cmpThresholdDB);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgSSBModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgSSBModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("modAFInput") || force) {
        swgSSBModSettings->setModAfInput((int) settings.m_modAFInput);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgSSBModSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgSSBModSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_spectrumGUI && (channelSettingsKeys.contains("spectrunConfig") || force))
    {
        auto *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
        settings.m_spectrumGUI->formatTo(swgGLSpectrum);
        swgSSBModSettings->setSpectrumConfig(swgGLSpectrum);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        auto *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgSSBModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        auto *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgSSBModSettings->setRollupState(swgRollupState);
    }

    if (force)
    {
        const CWKeyerSettings& cwKeyerSettings = m_cwKeyer.getSettings();
        swgSSBModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgSSBModSettings->getCwKeyer();
        CWKeyer::webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);
    }
}

void SSBMod::networkManagerFinished(QNetworkReply *reply) const
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "SSBMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("SSBMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double SSBMod::getMagSq() const
{
    if (m_running) {
        return m_basebandSource->getMagSq();
    }

    return 0;
}

CWKeyer *SSBMod::getCWKeyer()
{
    return &m_cwKeyer;
}

int SSBMod::getAudioSampleRate() const
{
    if (m_running) {
        return m_basebandSource->getAudioSampleRate();
    }

    return 0;
}

int SSBMod::getFeedbackAudioSampleRate() const
{
    if (m_running) {
        return m_basebandSource->getFeedbackAudioSampleRate();
    }

    return 0;
}

uint32_t SSBMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}
