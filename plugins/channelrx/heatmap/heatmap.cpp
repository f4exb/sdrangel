///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
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

#include "heatmap.h"

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
#include "SWGChannelReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(HeatMap::MsgConfigureHeatMap, Message)

const char * const HeatMap::m_channelIdURI = "sdrangel.channel.heatmap";
const char * const HeatMap::m_channelId = "HeatMap";

HeatMap::HeatMap(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new HeatMapBaseband(this);
    m_basebandSink->setMessageQueueToChannel(getInputMessageQueue());
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &HeatMap::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &HeatMap::handleIndexInDeviceSetChanged
    );
}

HeatMap::~HeatMap()
{
    qDebug("HeatMap::~HeatMap");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &HeatMap::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void HeatMap::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t HeatMap::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void HeatMap::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void HeatMap::start()
{
    qDebug("HeatMap::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    HeatMapBaseband::MsgConfigureHeatMapBaseband *msg = HeatMapBaseband::MsgConfigureHeatMapBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void HeatMap::stop()
{
    qDebug("HeatMap::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool HeatMap::handleMessage(const Message& cmd)
{
    if (MsgConfigureHeatMap::match(cmd))
    {
        MsgConfigureHeatMap& cfg = (MsgConfigureHeatMap&) cmd;
        qDebug() << "HeatMap::handleMessage: MsgConfigureHeatMap";
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
        qDebug() << "HeatMap::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);

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

ScopeVis *HeatMap::getScopeSink()
{
    return m_basebandSink->getScopeSink();
}

void HeatMap::setCenterFrequency(qint64 frequency)
{
    HeatMapSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureHeatMap *msgToGUI = MsgConfigureHeatMap::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void HeatMap::applySettings(const HeatMapSettings& settings, bool force)
{
    qDebug() << "HeatMap::applySettings:"
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

    HeatMapBaseband::MsgConfigureHeatMapBaseband *msg = HeatMapBaseband::MsgConfigureHeatMapBaseband::create(settings, force);
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

    m_settings = settings;
}

QByteArray HeatMap::serialize() const
{
    return m_settings.serialize();
}

bool HeatMap::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureHeatMap *msg = MsgConfigureHeatMap::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureHeatMap *msg = MsgConfigureHeatMap::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int HeatMap::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setHeatMapSettings(new SWGSDRangel::SWGHeatMapSettings());
    response.getHeatMapSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int HeatMap::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int HeatMap::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    HeatMapSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureHeatMap *msg = MsgConfigureHeatMap::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("HeatMap::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureHeatMap *msgToGUI = MsgConfigureHeatMap::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void HeatMap::webapiUpdateChannelSettings(
        HeatMapSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getHeatMapSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getHeatMapSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("minPower")) {
        settings.m_minPower = response.getHeatMapSettings()->getMinPower();
    }
    if (channelSettingsKeys.contains("maxPower")) {
        settings.m_maxPower = response.getHeatMapSettings()->getMaxPower();
    }
    if (channelSettingsKeys.contains("colorMapName")) {
        settings.m_colorMapName = *response.getHeatMapSettings()->getColorMapName();
    }
    if (channelSettingsKeys.contains("mode")) {
        settings.m_mode = (HeatMapSettings::Mode)response.getHeatMapSettings()->getMode();
    }
    if (channelSettingsKeys.contains("pulseThreshold")) {
        settings.m_pulseThreshold = response.getHeatMapSettings()->getPulseThreshold();
    }
    if (channelSettingsKeys.contains("averagePeriodUS")) {
        settings.m_averagePeriodUS = response.getHeatMapSettings()->getAveragePeriodUs();
    }
    if (channelSettingsKeys.contains("sampleRate")) {
        settings.m_sampleRate = response.getHeatMapSettings()->getSampleRate();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getHeatMapSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getHeatMapSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getHeatMapSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getHeatMapSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getHeatMapSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getHeatMapSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getHeatMapSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getHeatMapSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_scopeGUI && channelSettingsKeys.contains("scopeConfig")) {
        settings.m_scopeGUI->updateFrom(channelSettingsKeys, response.getHeatMapSettings()->getScopeConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getHeatMapSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getHeatMapSettings()->getRollupState());
    }
}

void HeatMap::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const HeatMapSettings& settings)
{
    response.getHeatMapSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getHeatMapSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getHeatMapSettings()->setMinPower(settings.m_minPower);
    response.getHeatMapSettings()->setMaxPower(settings.m_maxPower);
    response.getHeatMapSettings()->setColorMapName(new QString(settings.m_colorMapName));
    response.getHeatMapSettings()->setMode((int) settings.m_mode);
    response.getHeatMapSettings()->setPulseThreshold(settings.m_pulseThreshold);
    response.getHeatMapSettings()->setAveragePeriodUs(settings.m_averagePeriodUS);
    response.getHeatMapSettings()->setSampleRate(settings.m_sampleRate);

    response.getHeatMapSettings()->setRgbColor(settings.m_rgbColor);
    if (response.getHeatMapSettings()->getTitle()) {
        *response.getHeatMapSettings()->getTitle() = settings.m_title;
    } else {
        response.getHeatMapSettings()->setTitle(new QString(settings.m_title));
    }

    response.getHeatMapSettings()->setStreamIndex(settings.m_streamIndex);
    response.getHeatMapSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getHeatMapSettings()->getReverseApiAddress()) {
        *response.getHeatMapSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getHeatMapSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getHeatMapSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getHeatMapSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getHeatMapSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_scopeGUI)
    {
        if (response.getHeatMapSettings()->getScopeConfig())
        {
            settings.m_scopeGUI->formatTo(response.getHeatMapSettings()->getScopeConfig());
        }
        else
        {
            SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
            settings.m_scopeGUI->formatTo(swgGLScope);
            response.getHeatMapSettings()->setScopeConfig(swgGLScope);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getHeatMapSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getHeatMapSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getHeatMapSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getHeatMapSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getHeatMapSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getHeatMapSettings()->setRollupState(swgRollupState);
        }
    }
}

void HeatMap::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const HeatMapSettings& settings, bool force)
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

void HeatMap::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const HeatMapSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("HeatMap"));
    swgChannelSettings->setHeatMapSettings(new SWGSDRangel::SWGHeatMapSettings());
    SWGSDRangel::SWGHeatMapSettings *swgHeatMapSettings = swgChannelSettings->getHeatMapSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgHeatMapSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgHeatMapSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("minPower") || force) {
        swgHeatMapSettings->setMinPower(settings.m_minPower);
    }
    if (channelSettingsKeys.contains("maxPower") || force) {
        swgHeatMapSettings->setMaxPower(settings.m_maxPower);
    }
    if (channelSettingsKeys.contains("colorMapName") || force) {
        swgHeatMapSettings->setColorMapName(new QString(settings.m_colorMapName));
    }
    if (channelSettingsKeys.contains("mode") || force) {
        swgHeatMapSettings->setMode((int) settings.m_mode);
    }
    if (channelSettingsKeys.contains("pulseThreshold") || force) {
        swgHeatMapSettings->setPulseThreshold(settings.m_pulseThreshold);
    }
    if (channelSettingsKeys.contains("averagePeriodUS") || force) {
        swgHeatMapSettings->setAveragePeriodUs(settings.m_averagePeriodUS);
    }
    if (channelSettingsKeys.contains("sampleRate") || force) {
        swgHeatMapSettings->setSampleRate(settings.m_sampleRate);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgHeatMapSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgHeatMapSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgHeatMapSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_scopeGUI && (channelSettingsKeys.contains("scopeConfig") || force))
    {
        SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
        settings.m_scopeGUI->formatTo(swgGLScope);
        swgHeatMapSettings->setScopeConfig(swgGLScope);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgHeatMapSettings->setChannelMarker(swgChannelMarker);
    }
}

void HeatMap::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "HeatMap::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("HeatMap::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void HeatMap::handleIndexInDeviceSetChanged(int index)
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

