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

#include "channelpower.h"

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

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"
#include "util/db.h"

MESSAGE_CLASS_DEFINITION(ChannelPower::MsgConfigureChannelPower, Message)

const char * const ChannelPower::m_channelIdURI = "sdrangel.channel.channelpower";
const char * const ChannelPower::m_channelId = "ChannelPower";

ChannelPower::ChannelPower(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new ChannelPowerBaseband();
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, QStringList(), true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &ChannelPower::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &ChannelPower::handleIndexInDeviceSetChanged
    );
}

ChannelPower::~ChannelPower()
{
    qDebug("ChannelPower::~ChannelPower");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &ChannelPower::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void ChannelPower::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t ChannelPower::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void ChannelPower::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void ChannelPower::start()
{
    qDebug("ChannelPower::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    ChannelPowerBaseband::MsgConfigureChannelPowerBaseband *msg = ChannelPowerBaseband::MsgConfigureChannelPowerBaseband::create(m_settings, QStringList(), true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void ChannelPower::stop()
{
    qDebug("ChannelPower::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool ChannelPower::handleMessage(const Message& cmd)
{
    if (MsgConfigureChannelPower::match(cmd))
    {
        MsgConfigureChannelPower& cfg = (MsgConfigureChannelPower&) cmd;
        qDebug() << "ChannelPower::handleMessage: MsgConfigureChannelPower";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "ChannelPower::handleMessage: DSPSignalNotification";
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

void ChannelPower::setCenterFrequency(qint64 frequency)
{
    ChannelPowerSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, {"inputFrequencyOffset"}, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureChannelPower *msgToGUI = MsgConfigureChannelPower::create(settings, {"inputFrequencyOffset"}, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void ChannelPower::applySettings(const ChannelPowerSettings& settings, const QStringList& settingsKeys, bool force)
{
    qDebug() << "ChannelPower::applySettings:"
             << settings.getDebugString(settingsKeys, force)
             << " force: " << force;

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

    ChannelPowerBaseband::MsgConfigureChannelPowerBaseband *msg = ChannelPowerBaseband::MsgConfigureChannelPowerBaseband::create(settings, settingsKeys, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
            settingsKeys.contains("reverseAPIAddress") ||
            settingsKeys.contains("reverseAPIPort") ||
            settingsKeys.contains("reverseAPIDeviceIndex") ||
            settingsKeys.contains("reverseAPIChannelIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

QByteArray ChannelPower::serialize() const
{
    return m_settings.serialize();
}

bool ChannelPower::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureChannelPower *msg = MsgConfigureChannelPower::create(m_settings, QStringList(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureChannelPower *msg = MsgConfigureChannelPower::create(m_settings, QStringList(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int ChannelPower::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setChannelPowerReport(new SWGSDRangel::SWGChannelPowerReport());
    response.getChannelPowerReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

int ChannelPower::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setChannelPowerSettings(new SWGSDRangel::SWGChannelPowerSettings());
    response.getChannelPowerSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int ChannelPower::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int ChannelPower::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    ChannelPowerSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    // Ensure inputFrequencyOffset and frequency are consistent
    QStringList settingsKeys = channelSettingsKeys;
    if (settingsKeys.contains("frequency") && !settingsKeys.contains("inputFrequencyOffset"))
    {
        settings.m_inputFrequencyOffset = settings.m_frequency - m_centerFrequency;
        settingsKeys.append("inputFrequencyOffset");
    }
    else if (settingsKeys.contains("inputFrequencyOffset") && !settingsKeys.contains("frequency"))
    {
        settings.m_frequency = m_centerFrequency + settings.m_inputFrequencyOffset;
        settingsKeys.append("frequency");
    }

    MsgConfigureChannelPower *msg = MsgConfigureChannelPower::create(settings, settingsKeys, force);
    m_inputMessageQueue.push(msg);

    qDebug("ChannelPower::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureChannelPower *msgToGUI = MsgConfigureChannelPower::create(settings, settingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void ChannelPower::webapiUpdateChannelSettings(
        ChannelPowerSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getChannelPowerSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("frequencyMode")) {
        settings.m_frequencyMode = (ChannelPowerSettings::FrequencyMode) response.getChannelPowerSettings()->getFrequencyMode();
    }
    if (channelSettingsKeys.contains("frequency")) {
        settings.m_frequency = response.getChannelPowerSettings()->getFrequency();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getChannelPowerSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("pulseThreshold")) {
        settings.m_pulseThreshold = response.getChannelPowerSettings()->getPulseThreshold();
    }
    if (channelSettingsKeys.contains("averagePeriodUS")) {
        settings.m_averagePeriodUS = response.getChannelPowerSettings()->getAveragePeriodUs();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getChannelPowerSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getChannelPowerSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getChannelPowerSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getChannelPowerSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getChannelPowerSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getChannelPowerSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getChannelPowerSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getChannelPowerSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getChannelPowerSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getChannelPowerSettings()->getRollupState());
    }
}

void ChannelPower::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const ChannelPowerSettings& settings)
{
    response.getChannelPowerSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getChannelPowerSettings()->setFrequencyMode(settings.m_frequencyMode);
    response.getChannelPowerSettings()->setFrequency(settings.m_frequency);
    response.getChannelPowerSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getChannelPowerSettings()->setPulseThreshold(settings.m_pulseThreshold);
    response.getChannelPowerSettings()->setAveragePeriodUs(settings.m_averagePeriodUS);

    response.getChannelPowerSettings()->setRgbColor(settings.m_rgbColor);
    if (response.getChannelPowerSettings()->getTitle()) {
        *response.getChannelPowerSettings()->getTitle() = settings.m_title;
    } else {
        response.getChannelPowerSettings()->setTitle(new QString(settings.m_title));
    }

    response.getChannelPowerSettings()->setStreamIndex(settings.m_streamIndex);
    response.getChannelPowerSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getChannelPowerSettings()->getReverseApiAddress()) {
        *response.getChannelPowerSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getChannelPowerSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getChannelPowerSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getChannelPowerSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getChannelPowerSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getChannelPowerSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getChannelPowerSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getChannelPowerSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getChannelPowerSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getChannelPowerSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getChannelPowerSettings()->setRollupState(swgRollupState);
        }
    }
}

void ChannelPower::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    //if (!m_running) {
    //    return;
    //}

    double magAvg, magPulseAvg, magMaxPeak, magMinPeak;
    getMagLevels(magAvg, magPulseAvg, magMaxPeak, magMinPeak);

    response.getChannelPowerReport()->setChannelPowerDb(CalcDb::dbPower(magAvg * magAvg));
    response.getChannelPowerReport()->setChannelPowerMaxDb(CalcDb::dbPower(magMaxPeak * magMaxPeak));
    response.getChannelPowerReport()->setChannelPowerMinDb(CalcDb::dbPower(magMinPeak * magMinPeak));
    response.getChannelPowerReport()->setChannelPowerPulseDb(CalcDb::dbPower(magPulseAvg * magPulseAvg));
    response.getChannelPowerReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
}

void ChannelPower::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const ChannelPowerSettings& settings, bool force)
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

void ChannelPower::webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const ChannelPowerSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("ChannelPower"));
    swgChannelSettings->setChannelPowerSettings(new SWGSDRangel::SWGChannelPowerSettings());
    SWGSDRangel::SWGChannelPowerSettings *swgChannelPowerSettings = swgChannelSettings->getChannelPowerSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgChannelPowerSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("frequencyMode") || force) {
        swgChannelPowerSettings->setFrequencyMode(settings.m_frequencyMode);
    }
    if (channelSettingsKeys.contains("inputFrequency") || force) {
        swgChannelPowerSettings->setFrequency(settings.m_frequency);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgChannelPowerSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("pulseThreshold") || force) {
        swgChannelPowerSettings->setPulseThreshold(settings.m_pulseThreshold);
    }
    if (channelSettingsKeys.contains("averagePeriodUS") || force) {
        swgChannelPowerSettings->setAveragePeriodUs(settings.m_averagePeriodUS);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgChannelPowerSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgChannelPowerSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgChannelPowerSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgChannelPowerSettings->setChannelMarker(swgChannelMarker);
    }
}

void ChannelPower::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "ChannelPower::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("ChannelPower::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void ChannelPower::handleIndexInDeviceSetChanged(int index)
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

