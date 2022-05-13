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

#include "radioclock.h"

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
#include "radioclocksink.h"
#include "radioclocksettings.h"

MESSAGE_CLASS_DEFINITION(RadioClock::MsgConfigureRadioClock, Message)
MESSAGE_CLASS_DEFINITION(RadioClock::MsgDateTime, Message)
MESSAGE_CLASS_DEFINITION(RadioClock::MsgStatus, Message)

const char * const RadioClock::m_channelIdURI = "sdrangel.channel.radioclock";
const char * const RadioClock::m_channelId = "RadioClock";

RadioClock::RadioClock(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new RadioClockBaseband(this);
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
        &RadioClock::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &RadioClock::handleIndexInDeviceSetChanged
    );
}

RadioClock::~RadioClock()
{
    qDebug("RadioClock::~RadioClock");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RadioClock::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void RadioClock::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t RadioClock::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void RadioClock::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void RadioClock::start()
{
    qDebug("RadioClock::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    RadioClockBaseband::MsgConfigureRadioClockBaseband *msg = RadioClockBaseband::MsgConfigureRadioClockBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void RadioClock::stop()
{
    qDebug("RadioClock::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

void RadioClock::setCenterFrequency(qint64 frequency)
{
    RadioClockSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRadioClock *msgToGUI = MsgConfigureRadioClock::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

bool RadioClock::handleMessage(const Message& cmd)
{
    if (MsgConfigureRadioClock::match(cmd))
    {
        MsgConfigureRadioClock& cfg = (MsgConfigureRadioClock&) cmd;
        qDebug() << "RadioClock::handleMessage: MsgConfigureRadioClock";
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
        qDebug() << "RadioClock::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MsgDateTime::match(cmd))
    {
        MsgDateTime& report = (MsgDateTime&)cmd;
        // Save time for web report
        m_dateTime = report.getDateTime();
        // Forward to GUI
        if (getMessageQueueToGUI())
        {
            getMessageQueueToGUI()->push(new MsgDateTime(report));
        }

        return true;
    }
    else if (MsgStatus::match(cmd))
    {
        // Forward to GUI
        MsgStatus& report = (MsgStatus&)cmd;
        if (getMessageQueueToGUI())
        {
            getMessageQueueToGUI()->push(new MsgStatus(report));
        }

        return true;
    }
    else
    {
        return false;
    }
}

ScopeVis *RadioClock::getScopeSink()
{
    return m_basebandSink->getScopeSink();
}

void RadioClock::applySettings(const RadioClockSettings& settings, bool force)
{
    qDebug() << "RadioClock::applySettings:"
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
    if ((settings.m_threshold != m_settings.m_threshold) || force) {
        reverseAPIKeys.append("threshold");
    }
    if ((settings.m_modulation != m_settings.m_modulation) || force) {
        reverseAPIKeys.append("modulation");
    }
    if ((settings.m_timezone != m_settings.m_timezone) || force) {
        reverseAPIKeys.append("timezone");
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

    RadioClockBaseband::MsgConfigureRadioClockBaseband *msg = RadioClockBaseband::MsgConfigureRadioClockBaseband::create(settings, force);
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

QByteArray RadioClock::serialize() const
{
    return m_settings.serialize();
}

bool RadioClock::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureRadioClock *msg = MsgConfigureRadioClock::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureRadioClock *msg = MsgConfigureRadioClock::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int RadioClock::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRadioClockSettings(new SWGSDRangel::SWGRadioClockSettings());
    response.getRadioClockSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int RadioClock::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int RadioClock::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    RadioClockSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureRadioClock *msg = MsgConfigureRadioClock::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("RadioClock::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRadioClock *msgToGUI = MsgConfigureRadioClock::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void RadioClock::webapiUpdateChannelSettings(
        RadioClockSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getRadioClockSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getRadioClockSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("threshold")) {
        settings.m_threshold = response.getRadioClockSettings()->getThreshold();
    }
    if (channelSettingsKeys.contains("modulation")) {
        settings.m_modulation = (RadioClockSettings::Modulation)response.getRadioClockSettings()->getModulation();
    }
    if (channelSettingsKeys.contains("timezone")) {
        settings.m_timezone = (RadioClockSettings::DisplayTZ)response.getRadioClockSettings()->getTimezone();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getRadioClockSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getRadioClockSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getRadioClockSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRadioClockSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRadioClockSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRadioClockSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getRadioClockSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getRadioClockSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_scopeGUI && channelSettingsKeys.contains("scopeConfig")) {
        settings.m_scopeGUI->updateFrom(channelSettingsKeys, response.getRadioClockSettings()->getScopeConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getRadioClockSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getRadioClockSettings()->getRollupState());
    }
}

void RadioClock::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const RadioClockSettings& settings)
{
    response.getRadioClockSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getRadioClockSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getRadioClockSettings()->setThreshold(settings.m_threshold);
    response.getRadioClockSettings()->setModulation((int)settings.m_modulation);
    response.getRadioClockSettings()->setTimezone((int)settings.m_timezone);

    response.getRadioClockSettings()->setRgbColor(settings.m_rgbColor);
    if (response.getRadioClockSettings()->getTitle()) {
        *response.getRadioClockSettings()->getTitle() = settings.m_title;
    } else {
        response.getRadioClockSettings()->setTitle(new QString(settings.m_title));
    }

    response.getRadioClockSettings()->setStreamIndex(settings.m_streamIndex);
    response.getRadioClockSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRadioClockSettings()->getReverseApiAddress()) {
        *response.getRadioClockSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRadioClockSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRadioClockSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRadioClockSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getRadioClockSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_scopeGUI)
    {
        if (response.getRadioClockSettings()->getScopeConfig())
        {
            settings.m_scopeGUI->formatTo(response.getRadioClockSettings()->getScopeConfig());
        }
        else
        {
            SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
            settings.m_scopeGUI->formatTo(swgGLScope);
            response.getRadioClockSettings()->setScopeConfig(swgGLScope);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getRadioClockSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getRadioClockSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getRadioClockSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getRadioClockSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getRadioClockSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getRadioClockSettings()->setRollupState(swgRollupState);
        }
    }
}

void RadioClock::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getRadioClockReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getRadioClockReport()->setChannelSampleRate(RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE);
    response.getRadioClockReport()->setDate(new QString(m_dateTime.date().toString()));
    response.getRadioClockReport()->setTime(new QString(m_dateTime.time().toString()));
}

void RadioClock::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const RadioClockSettings& settings, bool force)
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

int RadioClock::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRadioClockReport(new SWGSDRangel::SWGRadioClockReport());
    response.getRadioClockReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void RadioClock::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const RadioClockSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("RadioClock"));
    swgChannelSettings->setRadioClockSettings(new SWGSDRangel::SWGRadioClockSettings());
    SWGSDRangel::SWGRadioClockSettings *swgRadioClockSettings = swgChannelSettings->getRadioClockSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgRadioClockSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgRadioClockSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("threshold") || force) {
        swgRadioClockSettings->setThreshold(settings.m_threshold);
    }
    if (channelSettingsKeys.contains("modulation") || force) {
        swgRadioClockSettings->setModulation(settings.m_modulation);
    }
    if (channelSettingsKeys.contains("timezone") || force) {
        swgRadioClockSettings->setTimezone(settings.m_timezone);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgRadioClockSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgRadioClockSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgRadioClockSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_scopeGUI && (channelSettingsKeys.contains("scopeConfig") || force))
    {
        SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
        settings.m_scopeGUI->formatTo(swgGLScope);
        swgRadioClockSettings->setScopeConfig(swgGLScope);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgRadioClockSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgRadioClockSettings->setRollupState(swgRollupState);
    }
}

void RadioClock::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "RadioClock::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("RadioClock::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void RadioClock::handleIndexInDeviceSetChanged(int index)
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
