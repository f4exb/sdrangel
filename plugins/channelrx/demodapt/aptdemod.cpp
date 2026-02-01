///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include "aptdemod.h"

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include <stdio.h>
#include <complex.h>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGAPTDemodSettings.h"
#include "SWGChannelActions.h"
#include "SWGAPTDemodActions.h"

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"

MESSAGE_CLASS_DEFINITION(APTDemod::MsgConfigureAPTDemod, Message)
MESSAGE_CLASS_DEFINITION(APTDemod::MsgPixels, Message)
MESSAGE_CLASS_DEFINITION(APTDemod::MsgImage, Message)
MESSAGE_CLASS_DEFINITION(APTDemod::MsgLine, Message)
MESSAGE_CLASS_DEFINITION(APTDemod::MsgMapImageName, Message)
MESSAGE_CLASS_DEFINITION(APTDemod::MsgResetDecoder, Message)

const char * const APTDemod::m_channelIdURI = "sdrangel.channel.aptdemod";
const char * const APTDemod::m_channelId = "APTDemod";

APTDemod::APTDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new APTDemodBaseband();
    m_basebandSink->moveToThread(&m_thread);

    m_imageWorker = new APTDemodImageWorker(this);
    m_basebandSink->setImagWorkerMessageQueue(m_imageWorker->getInputMessageQueue());
    m_imageWorker->moveToThread(&m_imageThread);

    applySettings(QStringList(), m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &APTDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &APTDemod::handleIndexInDeviceSetChanged
    );

    startImageWorker();
}

APTDemod::~APTDemod()
{
    qDebug("APTDemod::~APTDemod");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &APTDemod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true);

    if (m_imageWorker->isRunning()) {
        stopImageWorker();
    }

    delete m_imageWorker;

    if (m_basebandSink->isRunning()) {
        stopBasebandSink();
    }

    delete m_basebandSink;
}

void APTDemod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this, false);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

uint32_t APTDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void APTDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void APTDemod::start()
{
    startBasebandSink();
}

void APTDemod::startBasebandSink()
{
    qDebug("APTDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    APTDemodBaseband::MsgConfigureAPTDemodBaseband *msg = APTDemodBaseband::MsgConfigureAPTDemodBaseband::create(QStringList(), m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void APTDemod::startImageWorker()
{
    qDebug("APTDemod::startImageWorker");

    m_imageWorker->reset();
    m_imageWorker->startWork();
    m_imageThread.start();

    APTDemodImageWorker::MsgConfigureAPTDemodImageWorker *msg = APTDemodImageWorker::MsgConfigureAPTDemodImageWorker::create(QStringList(), m_settings, true);
    m_imageWorker->getInputMessageQueue()->push(msg);
}

void APTDemod::stop()
{
    stopBasebandSink();
}

void APTDemod::stopBasebandSink()
{
    qDebug("APTDemod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

void APTDemod::stopImageWorker()
{
    qDebug("APTDemod::stopImageWorker");
    m_imageWorker->stopWork();
    m_imageThread.quit();
    m_imageThread.wait();
}

bool APTDemod::matchSatellite(const QString satelliteName)
{
   return    m_settings.m_satelliteTrackerControl
          && (   (satelliteName == m_settings.m_satelliteName)
                || (   (m_settings.m_satelliteName == "All")
                    && (  (satelliteName == "NOAA 15")
                        || (satelliteName == "NOAA 18")
                        || (satelliteName == "NOAA 19"))));
}

bool APTDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureAPTDemod::match(cmd))
    {
        MsgConfigureAPTDemod& cfg = (MsgConfigureAPTDemod&) cmd;
        qDebug() << "APTDemod::handleMessage: MsgConfigureAPTDemod";
        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "APTDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (APTDemod::MsgResetDecoder::match(cmd))
    {
        m_imageWorker->getInputMessageQueue()->push(APTDemod::MsgResetDecoder::create());
        // Forward to sink
        m_basebandSink->getInputMessageQueue()->push(APTDemod::MsgResetDecoder::create());
        return true;
    }
    else
    {
        return false;
    }
}

void APTDemod::setCenterFrequency(qint64 frequency)
{
    APTDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList({"inputFrequencyOffset"}), settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAPTDemod *msgToGUI = MsgConfigureAPTDemod::create(QStringList({"inputFrequencyOffset"}), settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void APTDemod::applySettings(const QStringList& settingsKeys, const APTDemodSettings& settings, bool force)
{
    qDebug() << "APTDemod::applySettings:" << settings.getDebugString(settingsKeys, force);

    if (settingsKeys.contains("streamIndex"))
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
    }

    APTDemodBaseband::MsgConfigureAPTDemodBaseband *msg
        = APTDemodBaseband::MsgConfigureAPTDemodBaseband::create(settingsKeys, settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    APTDemodImageWorker::MsgConfigureAPTDemodImageWorker *msgToImg
        = APTDemodImageWorker::MsgConfigureAPTDemodImageWorker::create(settingsKeys, settings, force);
    m_imageWorker->getInputMessageQueue()->push(msgToImg);

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI")) ||
                (settingsKeys.contains("reverseAPIAddress")) ||
                (settingsKeys.contains("reverseAPIPort")) ||
                (settingsKeys.contains("reverseAPIDeviceIndex")) ||
                (settingsKeys.contains("reverseAPIChannelIndex"));
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

QByteArray APTDemod::serialize() const
{
    return m_settings.serialize();
}

bool APTDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureAPTDemod *msg = MsgConfigureAPTDemod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureAPTDemod *msg = MsgConfigureAPTDemod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int APTDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAptDemodSettings(new SWGSDRangel::SWGAPTDemodSettings());
    response.getAptDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int APTDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int APTDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    APTDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureAPTDemod *msg = MsgConfigureAPTDemod::create(channelSettingsKeys, settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("APTDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAPTDemod *msgToGUI = MsgConfigureAPTDemod::create(channelSettingsKeys, settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void APTDemod::webapiUpdateChannelSettings(
        APTDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getAptDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getAptDemodSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getAptDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("cropNoise")) {
        settings.m_cropNoise = response.getAptDemodSettings()->getCropNoise();
    }
    if (channelSettingsKeys.contains("denoise")) {
        settings.m_denoise = response.getAptDemodSettings()->getDenoise();
    }
    if (channelSettingsKeys.contains("linearEqualise")) {
        settings.m_linearEqualise = response.getAptDemodSettings()->getLinearEqualise();
    }
    if (channelSettingsKeys.contains("histogramEqualise")) {
        settings.m_histogramEqualise = response.getAptDemodSettings()->getHistogramEqualise();
    }
    if (channelSettingsKeys.contains("precipitationOverlay")) {
        settings.m_precipitationOverlay = response.getAptDemodSettings()->getPrecipitationOverlay();
    }
    if (channelSettingsKeys.contains("flip")) {
        settings.m_flip = response.getAptDemodSettings()->getFlip();
    }
    if (channelSettingsKeys.contains("channels")) {
        settings.m_channels = (APTDemodSettings::ChannelSelection)response.getAptDemodSettings()->getChannels();
    }
    if (channelSettingsKeys.contains("decodeEnabled")) {
        settings.m_decodeEnabled = response.getAptDemodSettings()->getDecodeEnabled();
    }
    if (channelSettingsKeys.contains("autoSave")) {
        settings.m_autoSave = response.getAptDemodSettings()->getAutoSave();
    }
    if (channelSettingsKeys.contains("autoSavePath")) {
        settings.m_autoSavePath = *response.getAptDemodSettings()->getAutoSavePath();
    }
    if (channelSettingsKeys.contains("autoSaveMinScanLines")) {
        settings.m_autoSaveMinScanLines = response.getAptDemodSettings()->getAutoSaveMinScanLines();
    }
    if (channelSettingsKeys.contains("saveCombined")) {
        settings.m_saveCombined = response.getAptDemodSettings()->getSaveCombined();
    }
    if (channelSettingsKeys.contains("saveSeparate")) {
        settings.m_saveSeparate = response.getAptDemodSettings()->getSaveSeparate();
    }
    if (channelSettingsKeys.contains("saveProjection")) {
        settings.m_saveProjection = response.getAptDemodSettings()->getSaveProjection();
    }
    if (channelSettingsKeys.contains("scanlinesPerImageUpdate")) {
        settings.m_scanlinesPerImageUpdate = response.getAptDemodSettings()->getScanlinesPerImageUpdate();
    }
    if (channelSettingsKeys.contains("transparencyThreshold")) {
        settings.m_transparencyThreshold = response.getAptDemodSettings()->getTransparencyThreshold();
    }
    if (channelSettingsKeys.contains("m_opacityThreshold")) {
        settings.m_opacityThreshold = response.getAptDemodSettings()->getOpacityThreshold();
    }
    if (channelSettingsKeys.contains("palettes")) {
        settings.m_palettes = (*response.getAptDemodSettings()->getPalettes()).split(";");
    }
    if (channelSettingsKeys.contains("palette")) {
        settings.m_palette = response.getAptDemodSettings()->getPalette();
    }
    if (channelSettingsKeys.contains("horizontalPixelsPerDegree")) {
        settings.m_horizontalPixelsPerDegree = response.getAptDemodSettings()->getHorizontalPixelsPerDegree();
    }
    if (channelSettingsKeys.contains("verticalPixelsPerDegree")) {
        settings.m_verticalPixelsPerDegree = response.getAptDemodSettings()->getVerticalPixelsPerDegree();
    }
    if (channelSettingsKeys.contains("satTimeOffset")) {
        settings.m_satTimeOffset = response.getAptDemodSettings()->getSatTimeOffset();
    }
    if (channelSettingsKeys.contains("satYaw")) {
        settings.m_satYaw = response.getAptDemodSettings()->getSatYaw();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAptDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getAptDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getAptDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAptDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAptDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAptDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAptDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getAptDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getAptDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getAptDemodSettings()->getRollupState());
    }
}

void APTDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const APTDemodSettings& settings)
{
    response.getAptDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getAptDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getAptDemodSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getAptDemodSettings()->setCropNoise(settings.m_cropNoise);
    response.getAptDemodSettings()->setCropNoise(settings.m_denoise);
    response.getAptDemodSettings()->setLinearEqualise(settings.m_linearEqualise);
    response.getAptDemodSettings()->setHistogramEqualise(settings.m_histogramEqualise);
    response.getAptDemodSettings()->setPrecipitationOverlay(settings.m_precipitationOverlay);
    response.getAptDemodSettings()->setFlip(settings.m_flip);
    response.getAptDemodSettings()->setChannels((int)settings.m_channels);
    response.getAptDemodSettings()->setDecodeEnabled(settings.m_decodeEnabled);
    response.getAptDemodSettings()->setAutoSave(settings.m_autoSave);
    response.getAptDemodSettings()->setAutoSavePath(new QString(settings.m_autoSavePath));
    response.getAptDemodSettings()->setAutoSaveMinScanLines(settings.m_autoSaveMinScanLines);
    response.getAptDemodSettings()->setSaveCombined(settings.m_saveCombined);
    response.getAptDemodSettings()->setSaveSeparate(settings.m_saveSeparate);
    response.getAptDemodSettings()->setSaveProjection(settings.m_saveProjection);
    response.getAptDemodSettings()->setScanlinesPerImageUpdate(settings.m_scanlinesPerImageUpdate);
    response.getAptDemodSettings()->setTransparencyThreshold(settings.m_transparencyThreshold);
    response.getAptDemodSettings()->setOpacityThreshold(settings.m_opacityThreshold);
    response.getAptDemodSettings()->setPalettes(new QString(settings.m_palettes.join(";")));
    response.getAptDemodSettings()->setPalette(settings.m_palette);
    response.getAptDemodSettings()->setHorizontalPixelsPerDegree(settings.m_horizontalPixelsPerDegree);
    response.getAptDemodSettings()->setVerticalPixelsPerDegree(settings.m_verticalPixelsPerDegree);
    response.getAptDemodSettings()->setSatTimeOffset(settings.m_satTimeOffset);
    response.getAptDemodSettings()->setSatYaw(settings.m_satYaw);

    response.getAptDemodSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getAptDemodSettings()->getTitle()) {
        *response.getAptDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getAptDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getAptDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getAptDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAptDemodSettings()->getReverseApiAddress()) {
        *response.getAptDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAptDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAptDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAptDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getAptDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getAptDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getAptDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getAptDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getAptDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getAptDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getAptDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void APTDemod::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const APTDemodSettings& settings, bool force)
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

void APTDemod::webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const APTDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("APTDemod"));
    swgChannelSettings->setAptDemodSettings(new SWGSDRangel::SWGAPTDemodSettings());
    SWGSDRangel::SWGAPTDemodSettings *swgAPTDemodSettings = swgChannelSettings->getAptDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgAPTDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgAPTDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgAPTDemodSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("cropNoise") || force) {
        swgAPTDemodSettings->setCropNoise(settings.m_cropNoise);
    }
    if (channelSettingsKeys.contains("denoise") || force) {
        swgAPTDemodSettings->setDenoise(settings.m_denoise);
    }
    if (channelSettingsKeys.contains("linearEqualise") || force) {
        swgAPTDemodSettings->setLinearEqualise(settings.m_linearEqualise);
    }
    if (channelSettingsKeys.contains("histogramEqualise") || force) {
        swgAPTDemodSettings->setHistogramEqualise(settings.m_histogramEqualise);
    }
    if (channelSettingsKeys.contains("precipitationOverlay") || force) {
        swgAPTDemodSettings->setPrecipitationOverlay(settings.m_precipitationOverlay);
    }
    if (channelSettingsKeys.contains("flip") || force) {
        swgAPTDemodSettings->setFlip(settings.m_flip);
    }
    if (channelSettingsKeys.contains("channels") || force) {
        swgAPTDemodSettings->setChannels((int)settings.m_channels);
    }
    if (channelSettingsKeys.contains("decodeEnabled") || force) {
        swgAPTDemodSettings->setDecodeEnabled(settings.m_decodeEnabled);
    }
    if (channelSettingsKeys.contains("autoSave") || force) {
        swgAPTDemodSettings->setAutoSave(settings.m_autoSave);
    }
    if (channelSettingsKeys.contains("autoSavePath") || force) {
        swgAPTDemodSettings->setAutoSavePath(new QString(settings.m_autoSavePath));
    }
    if (channelSettingsKeys.contains("autoSaveMinScanLines") || force) {
        swgAPTDemodSettings->setAutoSaveMinScanLines(settings.m_autoSaveMinScanLines);
    }
    if (channelSettingsKeys.contains("saveCombined") || force) {
        swgAPTDemodSettings->setSaveCombined(settings.m_saveCombined);
    }
    if (channelSettingsKeys.contains("saveSeparate") || force) {
        swgAPTDemodSettings->setSaveSeparate(settings.m_saveSeparate);
    }
    if (channelSettingsKeys.contains("saveProjection") || force) {
        swgAPTDemodSettings->setSaveProjection(settings.m_saveProjection);
    }
    if (channelSettingsKeys.contains("scanlinesPerImageUpdate") || force) {
        swgAPTDemodSettings->setScanlinesPerImageUpdate(settings.m_scanlinesPerImageUpdate);
    }
    if (channelSettingsKeys.contains("transparencyThreshold") || force) {
        swgAPTDemodSettings->setTransparencyThreshold(settings.m_transparencyThreshold);
    }
    if (channelSettingsKeys.contains("opacityThreshold") || force) {
        swgAPTDemodSettings->setOpacityThreshold(settings.m_opacityThreshold);
    }
    if (channelSettingsKeys.contains("palettes") || force) {
        swgAPTDemodSettings->setPalettes(new QString(settings.m_palettes.join(";")));
    }
    if (channelSettingsKeys.contains("palette") || force) {
        swgAPTDemodSettings->setPalette(settings.m_palette);
    }
    if (channelSettingsKeys.contains("horizontalPixelsPerDegree") || force) {
        swgAPTDemodSettings->setHorizontalPixelsPerDegree(settings.m_horizontalPixelsPerDegree);
    }
    if (channelSettingsKeys.contains("verticalPixelsPerDegree") || force) {
        swgAPTDemodSettings->setVerticalPixelsPerDegree(settings.m_verticalPixelsPerDegree);
    }
    if (channelSettingsKeys.contains("satTimeOffset") || force) {
        swgAPTDemodSettings->setSatTimeOffset(settings.m_satTimeOffset);
    }
    if (channelSettingsKeys.contains("satYaw") || force) {
        swgAPTDemodSettings->setSatYaw(settings.m_satYaw);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgAPTDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgAPTDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgAPTDemodSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgAPTDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgAPTDemodSettings->setRollupState(swgRollupState);
    }
}

int APTDemod::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGAPTDemodActions *swgAPTDemodActions = query.getAptDemodActions();

    if (swgAPTDemodActions)
    {
        if (channelActionsKeys.contains("aos"))
        {
            SWGSDRangel::SWGAPTDemodActions_aos* aos = swgAPTDemodActions->getAos();
            QString *satelliteName = aos->getSatelliteName();
            qDebug() << "APTDemod::webapiActionsPost - AOS " << *satelliteName;
            if (satelliteName != nullptr)
            {
                if (matchSatellite(*satelliteName))
                {
                    // Reset for new pass
                    m_imageWorker->getInputMessageQueue()->push(APTDemod::MsgResetDecoder::create());
                    m_basebandSink->getInputMessageQueue()->push(APTDemod::MsgResetDecoder::create());
                    if (m_guiMessageQueue) {
                        m_guiMessageQueue->push(APTDemod::MsgResetDecoder::create());
                    }

                    // Save satellite name
                    m_imageWorker->getInputMessageQueue()->push(APTDemodImageWorker::MsgSetSatelliteName::create(*satelliteName));

                    // Enable decoder and set direction of pass
                    APTDemodSettings settings = m_settings;
                    settings.m_decodeEnabled = true;
                    settings.m_flip = !aos->getNorthToSouthPass();
                    settings.m_tle = *aos->getTle();
                    settings.m_aosDateTime = QDateTime::fromString(*aos->getDateTime(), Qt::ISODateWithMs);
                    settings.m_northToSouth = aos->getNorthToSouthPass();

                    m_inputMessageQueue.push(MsgConfigureAPTDemod::create(channelActionsKeys, settings, false));
                    if (m_guiMessageQueue) {
                        m_guiMessageQueue->push(MsgConfigureAPTDemod::create(channelActionsKeys, settings, false));
                    }
                }

                return 202;
            }
            else
            {
                errorMessage = "Missing satellite name";
                return 400;
            }
        }
        else if (channelActionsKeys.contains("los"))
        {
            SWGSDRangel::SWGAPTDemodActions_los* los = swgAPTDemodActions->getLos();
            QString *satelliteName = los->getSatelliteName();
            qDebug() << "APTDemod::webapiActionsPost - LOS " << *satelliteName;

            if (satelliteName != nullptr)
            {
                if (matchSatellite(*satelliteName))
                {
                    // Save image
                    if (m_settings.m_autoSave)
                    {
                        APTDemodImageWorker::MsgSaveImageToDisk *msg = APTDemodImageWorker::MsgSaveImageToDisk::create();
                        m_imageWorker->getInputMessageQueue()->push(msg);
                    }
                    // Disable decoder
                    APTDemodSettings settings = m_settings;
                    settings.m_decodeEnabled = false;
                    m_inputMessageQueue.push(MsgConfigureAPTDemod::create(channelActionsKeys, settings, false));

                    if (m_guiMessageQueue) {
                        m_guiMessageQueue->push(MsgConfigureAPTDemod::create(channelActionsKeys, settings, false));
                    }
                }

                return 202;
            }
            else
            {
                errorMessage = "Missing satellite name";
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
        errorMessage = "Missing APTDemodActions in query";
        return 400;
    }
}

void APTDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "APTDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("APTDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void APTDemod::handleIndexInDeviceSetChanged(int index)
{
    if (index < 0) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
}
