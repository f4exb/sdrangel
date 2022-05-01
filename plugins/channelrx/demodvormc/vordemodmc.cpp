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

#include "vordemodmc.h"

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
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(VORDemodMC::MsgConfigureVORDemod, Message)

const char * const VORDemodMC::m_channelIdURI = "sdrangel.channel.vordemodmc";
const char * const VORDemodMC::m_channelId = "VORDemodMC";

VORDemodMC::VORDemodMC(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new VORDemodMCBaseband();
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &VORDemodMC::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &VORDemodMC::handleIndexInDeviceSetChanged
    );
}

VORDemodMC::~VORDemodMC()
{
    qDebug("VORDemodMC::~VORDemodMC");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &VORDemodMC::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void VORDemodMC::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t VORDemodMC::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void VORDemodMC::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void VORDemodMC::start()
{
    qDebug("VORDemodMC::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    VORDemodMCBaseband::MsgConfigureVORDemodBaseband *msg = VORDemodMCBaseband::MsgConfigureVORDemodBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void VORDemodMC::stop()
{
    qDebug("VORDemodMC::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool VORDemodMC::handleMessage(const Message& cmd)
{
    if (MsgConfigureVORDemod::match(cmd))
    {
        MsgConfigureVORDemod& cfg = (MsgConfigureVORDemod&) cmd;
        qDebug() << "VORDemodMC::handleMessage: MsgConfigureVORDemod";
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
        qDebug() << "VORDemodMC::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else
    {
        return false;
    }
}

void VORDemodMC::applySettings(const VORDemodMCSettings& settings, bool force)
{
    qDebug() << "VORDemodMC::applySettings:"
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

    VORDemodMCBaseband::MsgConfigureVORDemodBaseband *msg = VORDemodMCBaseband::MsgConfigureVORDemodBaseband::create(settings, force);
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

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, reverseAPIKeys, settings, force);
    }

    m_settings = settings;
}

QByteArray VORDemodMC::serialize() const
{
    return m_settings.serialize();
}

bool VORDemodMC::deserialize(const QByteArray& data)
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

int VORDemodMC::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setVorDemodMcSettings(new SWGSDRangel::SWGVORDemodMCSettings());
    response.getVorDemodMcSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int VORDemodMC::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    VORDemodMCSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureVORDemod *msg = MsgConfigureVORDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("VORDemodMC::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureVORDemod *msgToGUI = MsgConfigureVORDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void VORDemodMC::webapiUpdateChannelSettings(
        VORDemodMCSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getVorDemodMcSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getVorDemodMcSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getVorDemodMcSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getVorDemodMcSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getVorDemodMcSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getVorDemodMcSettings()->getAudioDeviceName();
    }

    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getVorDemodMcSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getVorDemodMcSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getVorDemodMcSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getVorDemodMcSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getVorDemodMcSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getVorDemodMcSettings()->getReverseApiChannelIndex();
    }
    if (channelSettingsKeys.contains("identThreshold")) {
        settings.m_identThreshold = response.getVorDemodMcSettings()->getIdentThreshold();
    }
    if (channelSettingsKeys.contains("magDecAdjust")) {
        settings.m_magDecAdjust = response.getVorDemodMcSettings()->getMagDecAdjust() != 0;
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getVorDemodMcSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getVorDemodMcSettings()->getRollupState());
    }
}

int VORDemodMC::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setVorDemodMcReport(new SWGSDRangel::SWGVORDemodMCReport());
    response.getVorDemodMcReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void VORDemodMC::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const VORDemodMCSettings& settings)
{
    response.getVorDemodMcSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getVorDemodMcSettings()->setRgbColor(settings.m_rgbColor);
    response.getVorDemodMcSettings()->setSquelch(settings.m_squelch);
    response.getVorDemodMcSettings()->setVolume(settings.m_volume);

    if (response.getVorDemodMcSettings()->getTitle()) {
        *response.getVorDemodMcSettings()->getTitle() = settings.m_title;
    } else {
        response.getVorDemodMcSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getVorDemodMcSettings()->getAudioDeviceName()) {
        *response.getVorDemodMcSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getVorDemodMcSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getVorDemodMcSettings()->setStreamIndex(settings.m_streamIndex);
    response.getVorDemodMcSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getVorDemodMcSettings()->getReverseApiAddress()) {
        *response.getVorDemodMcSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getVorDemodMcSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getVorDemodMcSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getVorDemodMcSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getVorDemodMcSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    response.getVorDemodMcSettings()->setIdentThreshold(settings.m_identThreshold);
    response.getVorDemodMcSettings()->setMagDecAdjust(settings.m_magDecAdjust ? 1 : 0);

    if (settings.m_channelMarker)
    {
        if (response.getVorDemodMcSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getVorDemodMcSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getVorDemodMcSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getVorDemodMcSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getVorDemodMcSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getVorDemodMcSettings()->setRollupState(swgRollupState);
        }
    }
}

void VORDemodMC::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getVorDemodMcReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getVorDemodMcReport()->setSquelch(m_basebandSink->getSquelchOpen() ? 1 : 0);
    response.getVorDemodMcReport()->setAudioSampleRate(m_basebandSink->getAudioSampleRate());
}

void VORDemodMC::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const VORDemodMCSettings& settings, bool force)
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

void VORDemodMC::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const VORDemodMCSettings& settings,
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

void VORDemodMC::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const VORDemodMCSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("VORDemodMC"));
    swgChannelSettings->setVorDemodMcSettings(new SWGSDRangel::SWGVORDemodMCSettings());
    SWGSDRangel::SWGVORDemodMCSettings *swgVORDemodSettings = swgChannelSettings->getVorDemodMcSettings();

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

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgVORDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgVORDemodSettings->setRollupState(swgRollupState);
    }
}

void VORDemodMC::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "VORDemodMC::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("VORDemodMC::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void VORDemodMC::handleIndexInDeviceSetChanged(int index)
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
