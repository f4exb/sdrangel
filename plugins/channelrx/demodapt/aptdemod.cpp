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
#include "SWGAPTDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"
#include "SWGMapItem.h"
#include "SWGAPTDemodActions.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(APTDemod::MsgConfigureAPTDemod, Message)
MESSAGE_CLASS_DEFINITION(APTDemod::MsgPixels, Message)
MESSAGE_CLASS_DEFINITION(APTDemod::MsgImage, Message)
MESSAGE_CLASS_DEFINITION(APTDemod::MsgLine, Message)
MESSAGE_CLASS_DEFINITION(APTDemod::MsgResetDecoder, Message)

const char * const APTDemod::m_channelIdURI = "sdrangel.channel.aptdemod";
const char * const APTDemod::m_channelId = "APTDemod";

APTDemod::APTDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new APTDemodBaseband(this);
    m_basebandSink->moveToThread(&m_thread);

    m_imageWorker = new APTDemodImageWorker();
    m_basebandSink->setImagWorkerMessageQueue(m_imageWorker->getInputMessageQueue());
    m_imageWorker->moveToThread(&m_imageThread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

APTDemod::~APTDemod()
{
    qDebug("APTDemod::~APTDemod");
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_imageWorker->isRunning()) {
        stopImageWorker();
    }

    delete m_imageWorker;

    if (m_basebandSink->isRunning()) {
        stopBasebandSink();
    }

    delete m_basebandSink;
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
    startImageWorker();
}

void APTDemod::startBasebandSink()
{
    qDebug("APTDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    APTDemodBaseband::MsgConfigureAPTDemodBaseband *msg = APTDemodBaseband::MsgConfigureAPTDemodBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void APTDemod::startImageWorker()
{
    qDebug("APTDemod::startImageWorker");

    m_imageWorker->reset();
    m_imageWorker->startWork();
    m_imageThread.start();

    APTDemodImageWorker::MsgConfigureAPTDemodImageWorker *msg = APTDemodImageWorker::MsgConfigureAPTDemodImageWorker::create(m_settings, true);
    m_imageWorker->getInputMessageQueue()->push(msg);
}

void APTDemod::stop()
{
    stopImageWorker();
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
        applySettings(cfg.getSettings(), cfg.getForce());

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
        if (m_guiMessageQueue)
            m_guiMessageQueue->push(new DSPSignalNotification(notif));

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

void APTDemod::applySettings(const APTDemodSettings& settings, bool force)
{
    qDebug() << "APTDemod::applySettings:"
             << " m_cropNoise: " << settings.m_cropNoise
             << " m_denoise: " << settings.m_denoise
             << " m_linearEqualise: " << settings.m_linearEqualise
             << " m_histogramEqualise: " << settings.m_histogramEqualise
             << " m_precipitationOverlay: " << settings.m_precipitationOverlay
             << " m_flip: " << settings.m_flip
             << " m_channels: " << settings.m_channels
             << " m_decodeEnabled: " << settings.m_decodeEnabled
             << " m_autoSave: " << settings.m_autoSave
             << " m_autoSavePath: " << settings.m_autoSavePath
             << " m_autoSaveMinScanLines: " << settings.m_autoSaveMinScanLines
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
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        reverseAPIKeys.append("fmDeviation");
    }
    if ((settings.m_denoise != m_settings.m_denoise) || force) {
        reverseAPIKeys.append("denoise");
    }
    if ((settings.m_linearEqualise != m_settings.m_linearEqualise) || force) {
        reverseAPIKeys.append("linearEqualise");
    }
    if ((settings.m_histogramEqualise != m_settings.m_histogramEqualise) || force) {
        reverseAPIKeys.append("histogramEqualise");
    }
    if ((settings.m_precipitationOverlay != m_settings.m_precipitationOverlay) || force) {
        reverseAPIKeys.append("precipitationOverlay");
    }
    if ((settings.m_flip != m_settings.m_flip) || force) {
        reverseAPIKeys.append("flip");
    }
    if ((settings.m_channels != m_settings.m_channels) || force) {
        reverseAPIKeys.append("channels");
    }
    if ((settings.m_decodeEnabled != m_settings.m_decodeEnabled) || force) {
        reverseAPIKeys.append("decodeEnabled");
    }
    if ((settings.m_autoSave != m_settings.m_autoSave) || force) {
        reverseAPIKeys.append("autoSave");
    }
    if ((settings.m_autoSavePath != m_settings.m_autoSavePath) || force) {
        reverseAPIKeys.append("autoSavePath");
    }
    if ((settings.m_autoSaveMinScanLines != m_settings.m_autoSaveMinScanLines) || force) {
        reverseAPIKeys.append("autoSaveMinScanLines");
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

    APTDemodBaseband::MsgConfigureAPTDemodBaseband *msg
        = APTDemodBaseband::MsgConfigureAPTDemodBaseband::create(settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    APTDemodImageWorker::MsgConfigureAPTDemodImageWorker *msgToImg
        = APTDemodImageWorker::MsgConfigureAPTDemodImageWorker::create(settings, force);
    m_imageWorker->getInputMessageQueue()->push(msgToImg);

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
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
        MsgConfigureAPTDemod *msg = MsgConfigureAPTDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureAPTDemod *msg = MsgConfigureAPTDemod::create(m_settings, true);
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

int APTDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    APTDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureAPTDemod *msg = MsgConfigureAPTDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("APTDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAPTDemod *msgToGUI = MsgConfigureAPTDemod::create(settings, force);
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
}

void APTDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const APTDemodSettings& settings, bool force)
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
        QList<QString>& channelSettingsKeys,
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
    if (channelSettingsKeys.contains("m_autoSave") || force) {
        swgAPTDemodSettings->setAutoSave(settings.m_autoSave);
    }
    if (channelSettingsKeys.contains("m_autoSavePath") || force) {
        swgAPTDemodSettings->setAutoSavePath(new QString(settings.m_autoSavePath));
    }
    if (channelSettingsKeys.contains("m_autoSaveMinScanLines") || force) {
        swgAPTDemodSettings->setAutoSaveMinScanLines(settings.m_autoSaveMinScanLines);
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

                    // Save satellite name
                    m_satelliteName = *satelliteName;

                    // Enable decoder and set direction of pass
                    APTDemodSettings settings = m_settings;
                    settings.m_decodeEnabled = true;
                    settings.m_flip = !aos->getNorthToSouthPass();

                    m_inputMessageQueue.push(MsgConfigureAPTDemod::create(settings, false));
                    if (m_guiMessageQueue)
                        m_guiMessageQueue->push(MsgConfigureAPTDemod::create(settings, false));
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
                    m_inputMessageQueue.push(MsgConfigureAPTDemod::create(settings, false));

                    if (m_guiMessageQueue) {
                        m_guiMessageQueue->push(MsgConfigureAPTDemod::create(settings, false));
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
