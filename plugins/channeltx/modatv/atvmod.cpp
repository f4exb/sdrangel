///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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
#include <complex.h>

#include <QTime>
#include <QDebug>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGATVModReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

#include "atvmodbaseband.h"
#include "atvmod.h"

MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureATVMod, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureSourceCenterFrequency, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureImageFileName, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureVideoFileName, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureVideoFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureVideoFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureCameraIndex, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureCameraData, Message)

const char* const ATVMod::m_channelIdURI = "sdrangel.channeltx.modatv";
const char* const ATVMod::m_channelId = "ATVMod";

ATVMod::ATVMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI)
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new ATVModBaseband();
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &ATVMod::networkManagerFinished
    );
}

ATVMod::~ATVMod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &ATVMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void ATVMod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t ATVMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

void ATVMod::start()
{
	qDebug("ATVMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void ATVMod::stop()
{
    qDebug("ATVMod::stop");
	m_thread->exit();
	m_thread->wait();
}

void ATVMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool ATVMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        qDebug() << "ATVMod::handleMessage: MsgConfigureChannelizer:"
                << " getSourceSampleRate: " << cfg.getSourceSampleRate()
                << " getSourceCenterFrequency: " << cfg.getSourceCenterFrequency();

        ATVModBaseband::MsgConfigureChannelizer *msg
            = ATVModBaseband::MsgConfigureChannelizer::create(cfg.getSourceSampleRate(), cfg.getSourceCenterFrequency());
        m_basebandSource->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (MsgConfigureSourceCenterFrequency::match(cmd))
    {
        MsgConfigureSourceCenterFrequency& cfg = (MsgConfigureSourceCenterFrequency&) cmd;
        qDebug() << "ATVMod::handleMessage: MsgConfigureSourceCenterFrequency:"
                << " getSourceCenterFrequency: " << cfg.getSourceCenterFrequency();

        ATVModBaseband::MsgConfigureChannelizer *msg
            = ATVModBaseband::MsgConfigureChannelizer::create(m_basebandSource->getChannelSampleRate(), cfg.getSourceCenterFrequency());
        m_basebandSource->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (MsgConfigureATVMod::match(cmd))
    {
        MsgConfigureATVMod& cfg = (MsgConfigureATVMod&) cmd;
        qDebug() << "ATVMod::handleMessage: MsgConfigureATVMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "ATVMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MsgConfigureImageFileName::match(cmd))
    {
        MsgConfigureImageFileName& cfg = (MsgConfigureImageFileName&) cmd;
        ATVModBaseband::MsgConfigureImageFileName *msg = ATVModBaseband::MsgConfigureImageFileName::create(
                cfg.getFileName());
        m_basebandSource->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (MsgConfigureVideoFileName::match(cmd))
    {
        MsgConfigureVideoFileName& cfg = (MsgConfigureVideoFileName&) cmd;
        ATVModBaseband::MsgConfigureVideoFileName *msg = ATVModBaseband::MsgConfigureVideoFileName::create(
                cfg.getFileName());
        m_basebandSource->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (MsgConfigureVideoFileSourceSeek::match(cmd))
    {
        MsgConfigureVideoFileSourceSeek& cfg = (MsgConfigureVideoFileSourceSeek&) cmd;
        ATVModBaseband::MsgConfigureVideoFileSourceSeek *rep = ATVModBaseband::MsgConfigureVideoFileSourceSeek::create(cfg.getPercentage());
        m_basebandSource->getInputMessageQueue()->push(rep);

        return true;
    }
    else if (MsgConfigureVideoFileSourceStreamTiming::match(cmd))
    {
        ATVModBaseband::MsgConfigureVideoFileSourceStreamTiming *rep = ATVModBaseband::MsgConfigureVideoFileSourceStreamTiming::create();
        m_basebandSource->getInputMessageQueue()->push(rep);

        return true;
    }
    else if (MsgConfigureCameraIndex::match(cmd))
    {
        MsgConfigureCameraIndex& cfg = (MsgConfigureCameraIndex&) cmd;
        ATVModBaseband::MsgConfigureCameraIndex *rep = ATVModBaseband::MsgConfigureCameraIndex::create(cfg.getIndex());
        m_basebandSource->getInputMessageQueue()->push(rep);

        return true;
    }
    else if (MsgConfigureCameraData::match(cmd))
    {
        MsgConfigureCameraData& cfg = (MsgConfigureCameraData&) cmd;
        ATVModBaseband::MsgConfigureCameraData *rep = ATVModBaseband::MsgConfigureCameraData::create(
            cfg.getIndex(), cfg.getManualFPS(), cfg.getManualFPSEnable()
        );
        m_basebandSource->getInputMessageQueue()->push(rep);

        return true;
    }
	else
	{
		return false;
	}
}

void ATVMod::setCenterFrequency(qint64 frequency)
{
    ATVModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureATVMod *msgToGUI = MsgConfigureATVMod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void ATVMod::applySettings(const ATVModSettings& settings, bool force)
{
    qDebug() << "ATVMod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_rfOppBandwidth: " << settings.m_rfOppBandwidth
            << " m_atvStd: " << (int) settings.m_atvStd
            << " m_nbLines: " << settings.m_nbLines
            << " m_fps: " << settings.m_fps
            << " m_atvModInput: " << (int) settings.m_atvModInput
            << " m_uniformLevel: " << settings.m_uniformLevel
            << " m_atvModulation: " << (int) settings.m_atvModulation
            << " m_videoPlayLoop: " << settings.m_videoPlayLoop
            << " m_videoPlay: " << settings.m_videoPlay
            << " m_cameraPlay: " << settings.m_cameraPlay
            << " m_channelMute: " << settings.m_channelMute
            << " m_invertedVideo: " << settings.m_invertedVideo
            << " m_rfScalingFactor: " << settings.m_rfScalingFactor
            << " m_fmExcursion: " << settings.m_fmExcursion
            << " m_forceDecimator: " << settings.m_forceDecimator
            << " m_showOverlayText: " << settings.m_showOverlayText
            << " m_overlayText: " << settings.m_overlayText
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_rfOppBandwidth != m_settings.m_rfOppBandwidth) || force) {
        reverseAPIKeys.append("rfOppBandwidth");
    }
    if ((settings.m_atvStd != m_settings.m_atvStd) || force) {
        reverseAPIKeys.append("atvStd");
    }
    if ((settings.m_nbLines != m_settings.m_nbLines) || force) {
        reverseAPIKeys.append("nbLines");
    }
    if ((settings.m_fps != m_settings.m_fps) || force) {
        reverseAPIKeys.append("fps");
    }
    if ((settings.m_atvModInput != m_settings.m_atvModInput) || force) {
        reverseAPIKeys.append("atvModInput");
    }
    if ((settings.m_uniformLevel != m_settings.m_uniformLevel) || force) {
        reverseAPIKeys.append("uniformLevel");
    }
    if ((settings.m_uniformLevel != m_settings.m_uniformLevel) || force) {
        reverseAPIKeys.append("uniformLevel");
    }
    if ((settings.m_atvModulation != m_settings.m_atvModulation) || force) {
        reverseAPIKeys.append("atvModulation");
    }
    if ((settings.m_videoPlayLoop != m_settings.m_videoPlayLoop) || force) {
        reverseAPIKeys.append("videoPlayLoop");
    }
    if ((settings.m_videoPlay != m_settings.m_videoPlay) || force) {
        reverseAPIKeys.append("videoPlay");
    }
    if ((settings.m_cameraPlay != m_settings.m_cameraPlay) || force) {
        reverseAPIKeys.append("cameraPlay");
    }
    if ((settings.m_channelMute != m_settings.m_channelMute) || force) {
        reverseAPIKeys.append("channelMute");
    }
    if ((settings.m_invertedVideo != m_settings.m_invertedVideo) || force) {
        reverseAPIKeys.append("invertedVideo");
    }
    if ((settings.m_rfScalingFactor != m_settings.m_rfScalingFactor) || force) {
        reverseAPIKeys.append("rfScalingFactor");
    }
    if ((settings.m_fmExcursion != m_settings.m_fmExcursion) || force) {
        reverseAPIKeys.append("fmExcursion");
    }
    if ((settings.m_forceDecimator != m_settings.m_forceDecimator) || force) {
        reverseAPIKeys.append("forceDecimator");
    }
    if ((settings.m_showOverlayText != m_settings.m_showOverlayText) || force) {
        reverseAPIKeys.append("showOverlayText");
    }
    if ((settings.m_overlayText != m_settings.m_overlayText) || force) {
        reverseAPIKeys.append("overlayText");
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

    ATVModBaseband::MsgConfigureATVModBaseband *msg = ATVModBaseband::MsgConfigureATVModBaseband::create(settings, force);
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

QByteArray ATVMod::serialize() const
{
    return m_settings.serialize();
}

bool ATVMod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureATVMod *msg = MsgConfigureATVMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureATVMod *msg = MsgConfigureATVMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int ATVMod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAtvModSettings(new SWGSDRangel::SWGATVModSettings());
    response.getAtvModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int ATVMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int ATVMod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    ATVModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    if (m_settings.m_inputFrequencyOffset != settings.m_inputFrequencyOffset)
    {
        ATVMod::MsgConfigureChannelizer *msgChan = ATVMod::MsgConfigureChannelizer::create(
            m_basebandSource->getChannelSampleRate(), settings.m_inputFrequencyOffset);
        m_inputMessageQueue.push(msgChan);
    }

    MsgConfigureATVMod *msg = MsgConfigureATVMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureATVMod *msgToGUI = MsgConfigureATVMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    if (channelSettingsKeys.contains("imageFileName"))
    {
        ATVModBaseband::MsgConfigureImageFileName *msg = ATVModBaseband::MsgConfigureImageFileName::create(
                *response.getAtvModSettings()->getImageFileName());
        m_basebandSource->getInputMessageQueue()->push(msg);

        if (m_guiMessageQueue) // forward to GUI if any
        {
            MsgConfigureImageFileName *msgToGUI = MsgConfigureImageFileName::create(
                    *response.getAtvModSettings()->getImageFileName());
            m_guiMessageQueue->push(msgToGUI);
        }
    }

    if (channelSettingsKeys.contains("videoFileName"))
    {
        ATVModBaseband::MsgConfigureVideoFileName *msg = ATVModBaseband::MsgConfigureVideoFileName::create(
                *response.getAtvModSettings()->getVideoFileName());
        m_inputMessageQueue.push(msg);

        if (m_guiMessageQueue) // forward to GUI if any
        {
            MsgConfigureVideoFileName *msgToGUI = MsgConfigureVideoFileName::create(
                    *response.getAtvModSettings()->getVideoFileName());
            m_guiMessageQueue->push(msgToGUI);
        }
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void ATVMod::webapiUpdateChannelSettings(
        ATVModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getAtvModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getAtvModSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("rfOppBandwidth")) {
        settings.m_rfOppBandwidth = response.getAtvModSettings()->getRfOppBandwidth();
    }
    if (channelSettingsKeys.contains("atvStd")) {
        settings.m_atvStd = (ATVModSettings::ATVStd) response.getAtvModSettings()->getAtvStd();
    }
    if (channelSettingsKeys.contains("nbLines")) {
        settings.m_nbLines = response.getAtvModSettings()->getNbLines();
    }
    if (channelSettingsKeys.contains("fps")) {
        settings.m_fps = response.getAtvModSettings()->getFps();
    }
    if (channelSettingsKeys.contains("atvModInput")) {
        settings.m_atvModInput = (ATVModSettings::ATVModInput) response.getAtvModSettings()->getAtvModInput();
    }
    if (channelSettingsKeys.contains("uniformLevel")) {
        settings.m_uniformLevel = response.getAtvModSettings()->getUniformLevel();
    }
    if (channelSettingsKeys.contains("atvModulation")) {
        settings.m_atvModulation = (ATVModSettings::ATVModulation) response.getAtvModSettings()->getAtvModulation();
    }
    if (channelSettingsKeys.contains("videoPlayLoop")) {
        settings.m_videoPlayLoop = response.getAtvModSettings()->getVideoPlayLoop() != 0;
    }
    if (channelSettingsKeys.contains("videoPlay")) {
        settings.m_videoPlay = response.getAtvModSettings()->getVideoPlay() != 0;
    }
    if (channelSettingsKeys.contains("cameraPlay")) {
        settings.m_cameraPlay = response.getAtvModSettings()->getCameraPlay() != 0;
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getAtvModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("invertedVideo")) {
        settings.m_invertedVideo = response.getAtvModSettings()->getInvertedVideo() != 0;
    }
    if (channelSettingsKeys.contains("rfScalingFactor")) {
        settings.m_rfScalingFactor = response.getAtvModSettings()->getRfScalingFactor();
    }
    if (channelSettingsKeys.contains("fmExcursion")) {
        settings.m_fmExcursion = response.getAtvModSettings()->getFmExcursion();
    }
    if (channelSettingsKeys.contains("forceDecimator")) {
        settings.m_forceDecimator = response.getAtvModSettings()->getForceDecimator() != 0;
    }
    if (channelSettingsKeys.contains("showOverlayText")) {
        settings.m_showOverlayText = response.getAtvModSettings()->getShowOverlayText() != 0;
    }
    if (channelSettingsKeys.contains("overlayText")) {
        settings.m_overlayText = *response.getAtvModSettings()->getOverlayText();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAtvModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getAtvModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getAtvModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAtvModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAtvModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAtvModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAtvModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getAtvModSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getAtvModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getAtvModSettings()->getRollupState());
    }
}

int ATVMod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAtvModReport(new SWGSDRangel::SWGATVModReport());
    response.getAtvModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void ATVMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const ATVModSettings& settings)
{
    response.getAtvModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getAtvModSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getAtvModSettings()->setRfOppBandwidth(settings.m_rfOppBandwidth);
    response.getAtvModSettings()->setAtvStd(settings.m_atvStd);
    response.getAtvModSettings()->setNbLines(settings.m_nbLines);
    response.getAtvModSettings()->setFps(settings.m_fps);
    response.getAtvModSettings()->setAtvModInput(settings.m_atvModInput);
    response.getAtvModSettings()->setUniformLevel(settings.m_uniformLevel);
    response.getAtvModSettings()->setAtvModulation(settings.m_atvModulation);
    response.getAtvModSettings()->setVideoPlayLoop(settings.m_videoPlayLoop ? 1 : 0);
    response.getAtvModSettings()->setVideoPlay(settings.m_videoPlay ? 1 : 0);
    response.getAtvModSettings()->setCameraPlay(settings.m_cameraPlay ? 1 : 0);
    response.getAtvModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getAtvModSettings()->setInvertedVideo(settings.m_invertedVideo ? 1 : 0);
    response.getAtvModSettings()->setRfScalingFactor(settings.m_rfScalingFactor);
    response.getAtvModSettings()->setFmExcursion(settings.m_fmExcursion);
    response.getAtvModSettings()->setForceDecimator(settings.m_forceDecimator ? 1 : 0);
    response.getAtvModSettings()->setShowOverlayText(settings.m_showOverlayText ? 1 : 0);

    if (response.getAtvModSettings()->getOverlayText()) {
        *response.getAtvModSettings()->getOverlayText() = settings.m_overlayText;
    } else {
        response.getAtvModSettings()->setOverlayText(new QString(settings.m_overlayText));
    }

    response.getAtvModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getAtvModSettings()->getTitle()) {
        *response.getAtvModSettings()->getTitle() = settings.m_title;
    } else {
        response.getAtvModSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getAtvModSettings()->getImageFileName()) {
        *response.getAtvModSettings()->getImageFileName() = settings.m_imageFileName;
    } else {
        response.getAtvModSettings()->setImageFileName(new QString(settings.m_imageFileName));
    }

    if (response.getAtvModSettings()->getVideoFileName()) {
        *response.getAtvModSettings()->getVideoFileName() = settings.m_videoFileName;
    } else {
        response.getAtvModSettings()->setVideoFileName(new QString(settings.m_videoFileName));
    }

    response.getAtvModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAtvModSettings()->getReverseApiAddress()) {
        *response.getAtvModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAtvModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAtvModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAtvModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getAtvModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getAtvModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getAtvModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getAtvModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getAtvModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getAtvModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getAtvModSettings()->setRollupState(swgRollupState);
        }
    }
}

void ATVMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getAtvModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getAtvModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void ATVMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const ATVModSettings& settings, bool force)
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

void ATVMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const ATVModSettings& settings,
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

void ATVMod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const ATVModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setAtvModSettings(new SWGSDRangel::SWGATVModSettings());
    SWGSDRangel::SWGATVModSettings *swgATVModSettings = swgChannelSettings->getAtvModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgATVModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgATVModSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("rfOppBandwidth") || force) {
        swgATVModSettings->setRfOppBandwidth(settings.m_rfOppBandwidth);
    }
    if (channelSettingsKeys.contains("atvStd") || force) {
        swgATVModSettings->setAtvStd((int) settings.m_atvStd);
    }
    if (channelSettingsKeys.contains("nbLines") || force) {
        swgATVModSettings->setNbLines(settings.m_nbLines);
    }
    if (channelSettingsKeys.contains("fps") || force) {
        swgATVModSettings->setFps(settings.m_fps);
    }
    if (channelSettingsKeys.contains("atvModInput") || force) {
        swgATVModSettings->setAtvModInput((int) settings.m_atvModInput);
    }
    if (channelSettingsKeys.contains("uniformLevel") || force) {
        swgATVModSettings->setUniformLevel(settings.m_uniformLevel);
    }
    if (channelSettingsKeys.contains("atvModulation") || force) {
        swgATVModSettings->setAtvModulation((int) settings.m_atvModulation);
    }
    if (channelSettingsKeys.contains("videoPlayLoop") || force) {
        swgATVModSettings->setVideoPlayLoop(settings.m_videoPlayLoop ? 1 : 0);
    }
    if (channelSettingsKeys.contains("videoPlay") || force) {
        swgATVModSettings->setVideoPlay(settings.m_videoPlay ? 1 : 0);
    }
    if (channelSettingsKeys.contains("cameraPlay") || force) {
        swgATVModSettings->setCameraPlay(settings.m_cameraPlay ? 1 : 0);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgATVModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("invertedVideo") || force) {
        swgATVModSettings->setInvertedVideo(settings.m_invertedVideo ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rfScalingFactor") || force) {
        swgATVModSettings->setRfScalingFactor(settings.m_rfScalingFactor);
    }
    if (channelSettingsKeys.contains("fmExcursion") || force) {
        swgATVModSettings->setFmExcursion(settings.m_fmExcursion);
    }
    if (channelSettingsKeys.contains("forceDecimator") || force) {
        swgATVModSettings->setForceDecimator(settings.m_forceDecimator ? 1 : 0);
    }
    if (channelSettingsKeys.contains("showOverlayText") || force) {
        swgATVModSettings->setShowOverlayText(settings.m_showOverlayText ? 1 : 0);
    }
    if (channelSettingsKeys.contains("overlayText") || force) {
        swgATVModSettings->setOverlayText(new QString(settings.m_overlayText));
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgATVModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgATVModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgATVModSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgATVModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgATVModSettings->setRollupState(swgRollupState);
    }
}

void ATVMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "ATVMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AMMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double ATVMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void ATVMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

int ATVMod::getEffectiveSampleRate() const
{
    return m_basebandSource->getEffectiveSampleRate();
}

void ATVMod::getCameraNumbers(std::vector<int>& numbers)
{
    m_basebandSource->getCameraNumbers(numbers);
}

void ATVMod::setMessageQueueToGUI(MessageQueue* queue) {
    ChannelAPI::setMessageQueueToGUI(queue);
    m_basebandSource->setMessageQueueToGUI(queue);
}
