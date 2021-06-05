///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#define BOOST_CHRONO_HEADER_ONLY
#include <boost/chrono/chrono.hpp>

#include <stdio.h>
#include <complex.h>

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGADSBDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGADSBDemodReport.h"
#include "SWGTargetAzimuthElevation.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"
#include "util/db.h"
#include "maincore.h"

#include "adsbdemod.h"
#include "adsbdemodworker.h"

MESSAGE_CLASS_DEFINITION(ADSBDemod::MsgConfigureADSBDemod, Message)

const char* const ADSBDemod::m_channelIdURI = "sdrangel.channel.adsbdemod";
const char* const ADSBDemod::m_channelId = "ADSBDemod";

ADSBDemod::ADSBDemod(DeviceAPI *devieAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(devieAPI),
        m_basebandSampleRate(0),
        m_targetAzElValid(false),
        m_targetAzimuth(0.0f),
        m_targetElevation(0.0f)
{
    qDebug("ADSBDemod::ADSBDemod");
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSink = new ADSBDemodBaseband();
    m_basebandSink->moveToThread(m_thread);

    m_worker = new ADSBDemodWorker();
    m_basebandSink->setMessageQueueToWorker(m_worker->getInputMessageQueue());

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

ADSBDemod::~ADSBDemod()
{
    if (m_worker->isRunning()) {
        stop();
    }
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);
    delete m_worker;
    delete m_basebandSink;
    delete m_thread;
}

uint32_t ADSBDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void ADSBDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void ADSBDemod::start()
{
    qDebug() << "ADSBDemod::start";

    if (m_basebandSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_basebandSampleRate);
    }

    m_worker->reset();
    m_worker->startWork();
    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread->start();

    ADSBDemodWorker::MsgConfigureADSBDemodWorker *msg = ADSBDemodWorker::MsgConfigureADSBDemodWorker::create(m_settings, true);
    m_worker->getInputMessageQueue()->push(msg);
}

void ADSBDemod::stop()
{
    qDebug() << "ADSBDemod::stop";
    m_basebandSink->stopWork();
    m_worker->stopWork();
    m_thread->exit();
    m_thread->wait();
}

bool ADSBDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureADSBDemod::match(cmd))
    {
        MsgConfigureADSBDemod& cfg = (MsgConfigureADSBDemod&) cmd;
        qDebug() << "ADSBDemod::handleMessage: MsgConfigureADSBDemod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "ADSBDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);

        return true;
    }
    else
    {
        return false;
    }
}

void ADSBDemod::applySettings(const ADSBDemodSettings& settings, bool force)
{
    qDebug() << "ADSBDemod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
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
    if ((settings.m_rgbColor != m_settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((settings.m_title != m_settings.m_title) || force) {
        reverseAPIKeys.append("title");
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

    ADSBDemodBaseband::MsgConfigureADSBDemodBaseband *msg = ADSBDemodBaseband::MsgConfigureADSBDemodBaseband::create(settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    ADSBDemodWorker::MsgConfigureADSBDemodWorker *workerMsg = ADSBDemodWorker::MsgConfigureADSBDemodWorker::create(settings, force);
    m_worker->getInputMessageQueue()->push(workerMsg);

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

QByteArray ADSBDemod::serialize() const
{
    return m_settings.serialize();
}

bool ADSBDemod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureADSBDemod *msg = MsgConfigureADSBDemod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

int ADSBDemod::webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setAdsbDemodSettings(new SWGSDRangel::SWGADSBDemodSettings());
    response.getAdsbDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int ADSBDemod::webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage)
{
    (void) errorMessage;
    ADSBDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureADSBDemod *msg = MsgConfigureADSBDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureADSBDemod *msgToGUI = MsgConfigureADSBDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void ADSBDemod::webapiUpdateChannelSettings(
        ADSBDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getAdsbDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getAdsbDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("correlationThreshold")) {
        settings.m_correlationThreshold = response.getAdsbDemodSettings()->getCorrelationThreshold();
    }
    if (channelSettingsKeys.contains("samplesPerBit")) {
        settings.m_samplesPerBit = response.getAdsbDemodSettings()->getSamplesPerBit();
    }
    if (channelSettingsKeys.contains("removeTimeout")) {
        settings.m_removeTimeout = response.getAdsbDemodSettings()->getRemoveTimeout();
    }
    if (channelSettingsKeys.contains("beastEnabled")) {
        settings.m_feedEnabled = response.getAdsbDemodSettings()->getBeastEnabled() != 0;
    }
    if (channelSettingsKeys.contains("beastHost")) {
        settings.m_feedHost = *response.getAdsbDemodSettings()->getBeastHost();
    }
    if (channelSettingsKeys.contains("beastPort")) {
        settings.m_feedPort = response.getAdsbDemodSettings()->getBeastPort();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAdsbDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getAdsbDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getAdsbDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAdsbDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAdsbDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAdsbDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAdsbDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getAdsbDemodSettings()->getReverseApiChannelIndex();
    }
}

int ADSBDemod::webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setAdsbDemodReport(new SWGSDRangel::SWGADSBDemodReport());
    response.getAdsbDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void ADSBDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const ADSBDemodSettings& settings)
{
    response.getAdsbDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getAdsbDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getAdsbDemodSettings()->setCorrelationThreshold(settings.m_correlationThreshold);
    response.getAdsbDemodSettings()->setSamplesPerBit(settings.m_samplesPerBit);
    response.getAdsbDemodSettings()->setRemoveTimeout(settings.m_removeTimeout);
    response.getAdsbDemodSettings()->setBeastEnabled(settings.m_feedEnabled ? 1 : 0);
    response.getAdsbDemodSettings()->setBeastHost(new QString(settings.m_feedHost));
    response.getAdsbDemodSettings()->setBeastPort(settings.m_feedPort);
    response.getAdsbDemodSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getAdsbDemodSettings()->getTitle()) {
        *response.getAdsbDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getAdsbDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getAdsbDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getAdsbDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAdsbDemodSettings()->getReverseApiAddress()) {
        *response.getAdsbDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAdsbDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAdsbDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAdsbDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getAdsbDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void ADSBDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getAdsbDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getAdsbDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
    if (m_targetAzElValid)
    {
        response.getAdsbDemodReport()->setTargetAzimuth(m_targetAzimuth);
        response.getAdsbDemodReport()->setTargetElevation(m_targetElevation);
    }
}

void ADSBDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const ADSBDemodSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(0); // single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("ADSBDemod"));
    swgChannelSettings->setAdsbDemodSettings(new SWGSDRangel::SWGADSBDemodSettings());
    SWGSDRangel::SWGADSBDemodSettings *swgADSBDemodSettings = swgChannelSettings->getAdsbDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgADSBDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgADSBDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("correlationThreshold") || force) {
        swgADSBDemodSettings->setCorrelationThreshold(settings.m_correlationThreshold);
    }
    if (channelSettingsKeys.contains("samplesPerBit") || force) {
        swgADSBDemodSettings->setSamplesPerBit(settings.m_samplesPerBit);
    }
    if (channelSettingsKeys.contains("removeTimeout") || force) {
        swgADSBDemodSettings->setRemoveTimeout(settings.m_removeTimeout);
    }
    if (channelSettingsKeys.contains("beastEnabled") || force) {
        swgADSBDemodSettings->setBeastEnabled(settings.m_feedEnabled ? 1 : 0);
    }
    if (channelSettingsKeys.contains("beastHost") || force) {
        swgADSBDemodSettings->setBeastHost(new QString(settings.m_feedHost));
    }
    if (channelSettingsKeys.contains("beastPort") || force) {
        swgADSBDemodSettings->setBeastPort(settings.m_feedPort);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgADSBDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgADSBDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgADSBDemodSettings->setStreamIndex(settings.m_streamIndex);
    }

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

void ADSBDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "ADSBDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("ADSBDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void ADSBDemod::setTarget(const QString& name, float targetAzimuth, float targetElevation)
{
    m_targetAzimuth = targetAzimuth;
    m_targetElevation = targetElevation;
    m_targetAzElValid = true;

    // Send to Rotator Controllers
    MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
    QList<MessageQueue*> *mapMessageQueues = messagePipes.getMessageQueues(this, "target");
    if (mapMessageQueues)
    {
        QList<MessageQueue*>::iterator it = mapMessageQueues->begin();

        for (; it != mapMessageQueues->end(); ++it)
        {
            SWGSDRangel::SWGTargetAzimuthElevation *swgTarget = new SWGSDRangel::SWGTargetAzimuthElevation();
            swgTarget->setName(new QString(name));
            swgTarget->setAzimuth(targetAzimuth);
            swgTarget->setElevation(targetElevation);
            (*it)->push(MainCore::MsgTargetAzimuthElevation::create(this, swgTarget));
        }
    }
}
