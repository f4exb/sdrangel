///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
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

#include "vordemod.h"

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include <stdio.h>
#include <complex.h>

#include "SWGChannelSettings.h"
#include "SWGVORDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGVORDemodReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(VORDemod::MsgConfigureVORDemod, Message)

const char * const VORDemod::m_channelIdURI = "sdrangel.channel.vordemod";
const char * const VORDemod::m_channelId = "VORDemod";

VORDemod::VORDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new VORDemodBaseband();
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

VORDemod::~VORDemod()
{
    qDebug("VORDemod::~VORDemod");
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

uint32_t VORDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void VORDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void VORDemod::start()
{
    qDebug("VORDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    VORDemodBaseband::MsgConfigureVORDemodBaseband *msg = VORDemodBaseband::MsgConfigureVORDemodBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void VORDemod::stop()
{
    qDebug("VORDemod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool VORDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureVORDemod::match(cmd))
    {
        MsgConfigureVORDemod& cfg = (MsgConfigureVORDemod&) cmd;
        qDebug() << "VORDemod::handleMessage: MsgConfigureVORDemod";
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
        qDebug() << "VORDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (m_guiMessageQueue)
        {
            rep = new DSPSignalNotification(notif);
            m_guiMessageQueue->push(rep);
        }

        return true;
    }
    else
    {
        return false;
    }
}

void VORDemod::applySettings(const VORDemodSettings& settings, bool force)
{
    qDebug() << "VORDemod::applySettings:"
            << " m_volume: " << settings.m_volume
            << " m_squelch: " << settings.m_squelch
            << " m_audioMute: " << settings.m_audioMute
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_squelch != settings.m_squelch) || force) {
        reverseAPIKeys.append("squelch");
    }
    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force) {
        reverseAPIKeys.append("audioDeviceName");
    }

    if ((m_settings.m_audioMute != settings.m_audioMute) || force) {
        reverseAPIKeys.append("audioMute");
    }

    if ((m_settings.m_volume != settings.m_volume) || force) {
        reverseAPIKeys.append("volume");
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

    if ((m_settings.m_identThreshold != settings.m_identThreshold) || force) {
        reverseAPIKeys.append("identThreshold");
    }

    if ((m_settings.m_magDecAdjust != settings.m_magDecAdjust) || force) {
        reverseAPIKeys.append("magDecAdjust");
    }

    VORDemodBaseband::MsgConfigureVORDemodBaseband *msg = VORDemodBaseband::MsgConfigureVORDemodBaseband::create(settings, force);
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

    QList<MessageQueue*> *messageQueues = MainCore::instance()->getMessagePipes().getMessageQueues(this, "settings");

    if (messageQueues) {
        sendChannelSettings(messageQueues, reverseAPIKeys, settings, force);
    }

    m_settings = settings;
}

QByteArray VORDemod::serialize() const
{
    return m_settings.serialize();
}

bool VORDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureVORDemod *msg = MsgConfigureVORDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureVORDemod *msg = MsgConfigureVORDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int VORDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setVorDemodSettings(new SWGSDRangel::SWGVORDemodSettings());
    response.getVorDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int VORDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    VORDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureVORDemod *msg = MsgConfigureVORDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("VORDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureVORDemod *msgToGUI = MsgConfigureVORDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void VORDemod::webapiUpdateChannelSettings(
        VORDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getVorDemodSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getVorDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getVorDemodSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getVorDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getVorDemodSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getVorDemodSettings()->getAudioDeviceName();
    }

    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getVorDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getVorDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getVorDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getVorDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getVorDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getVorDemodSettings()->getReverseApiChannelIndex();
    }
    if (channelSettingsKeys.contains("identThreshold")) {
        settings.m_identThreshold = response.getVorDemodSettings()->getIdentThreshold();
    }
    if (channelSettingsKeys.contains("magDecAdjust")) {
        settings.m_magDecAdjust = response.getVorDemodSettings()->getMagDecAdjust() != 0;
    }
}

int VORDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setVorDemodReport(new SWGSDRangel::SWGVORDemodReport());
    response.getVorDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void VORDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const VORDemodSettings& settings)
{
    response.getVorDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getVorDemodSettings()->setRgbColor(settings.m_rgbColor);
    response.getVorDemodSettings()->setSquelch(settings.m_squelch);
    response.getVorDemodSettings()->setVolume(settings.m_volume);

    if (response.getVorDemodSettings()->getTitle()) {
        *response.getVorDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getVorDemodSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getVorDemodSettings()->getAudioDeviceName()) {
        *response.getVorDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getVorDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getVorDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getVorDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getVorDemodSettings()->getReverseApiAddress()) {
        *response.getVorDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getVorDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getVorDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getVorDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getVorDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    response.getVorDemodSettings()->setIdentThreshold(settings.m_identThreshold);
    response.getVorDemodSettings()->setMagDecAdjust(settings.m_magDecAdjust ? 1 : 0);
}

void VORDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getVorDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getVorDemodReport()->setSquelch(m_basebandSink->getSquelchOpen() ? 1 : 0);
    response.getVorDemodReport()->setAudioSampleRate(m_basebandSink->getAudioSampleRate());
}

void VORDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const VORDemodSettings& settings, bool force)
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

void VORDemod::sendChannelSettings(
    QList<MessageQueue*> *messageQueues,
    QList<QString>& channelSettingsKeys,
    const VORDemodSettings& settings,
    bool force)
{
    QList<MessageQueue*>::iterator it = messageQueues->begin();

    for (; it != messageQueues->end(); ++it)
    {
        SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
        webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);
        MainCore::MsgChannelSettings *msg = MainCore::MsgChannelSettings::create(
            this,
            channelSettingsKeys,
            swgChannelSettings,
            force
        );
        (*it)->push(msg);
    }
}

void VORDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const VORDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("VORDemod"));
    swgChannelSettings->setVorDemodSettings(new SWGSDRangel::SWGVORDemodSettings());
    SWGSDRangel::SWGVORDemodSettings *swgVORDemodSettings = swgChannelSettings->getVorDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("audioMute") || force) {
        swgVORDemodSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgVORDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("squelch") || force) {
        swgVORDemodSettings->setSquelch(settings.m_squelch);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgVORDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("volume") || force) {
        swgVORDemodSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgVORDemodSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgVORDemodSettings->setStreamIndex(settings.m_streamIndex);
    }
    if (channelSettingsKeys.contains("identThreshold") || force) {
        swgVORDemodSettings->setIdentThreshold(settings.m_identThreshold);
    }
    if (channelSettingsKeys.contains("magDecAdjust") || force) {
        swgVORDemodSettings->setMagDecAdjust(settings.m_magDecAdjust ? 1 : 0);
    }
}

void VORDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "VORDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("VORDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
