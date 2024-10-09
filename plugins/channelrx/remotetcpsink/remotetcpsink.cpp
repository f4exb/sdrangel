///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include "remotetcpsink.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>
#include <QJsonDocument>
#include <QEventLoop>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"

#include "remotetcpsinkbaseband.h"

MESSAGE_CLASS_DEFINITION(RemoteTCPSink::MsgConfigureRemoteTCPSink, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPSink::MsgReportConnection, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPSink::MsgReportDisconnect, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPSink::MsgReportBW, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPSink::MsgSendMessage, Message)
MESSAGE_CLASS_DEFINITION(RemoteTCPSink::MsgError, Message)

const char* const RemoteTCPSink::m_channelIdURI = "sdrangel.channel.remotetcpsink";
const char* const RemoteTCPSink::m_channelId = "RemoteTCPSink";

RemoteTCPSink::RemoteTCPSink(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0),
        m_clients(0),
        m_removeRequest(nullptr)
{
    setObjectName(m_channelId);

    m_basebandSink = new RemoteTCPSinkBaseband();
    m_basebandSink->setMessageQueueToChannel(&m_inputMessageQueue);
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, QStringList(), true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RemoteTCPSink::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &RemoteTCPSink::handleIndexInDeviceSetChanged
    );
}

RemoteTCPSink::~RemoteTCPSink()
{
    qDebug("RemoteTCPSink::~RemoteTCPSink");

    // Wait until remove listing request is finished
    if (m_removeRequest && !m_removeRequest->isFinished())
    {
        qDebug() << "RemoteTCPSink::~RemoteTCPSink: Waiting for remove listing request to finish";
        QEventLoop loop;
        connect(m_removeRequest, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    if (m_basebandSink->isRunning()) {
        stop();
    }

    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RemoteTCPSink::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    m_basebandSink->deleteLater();
}

void RemoteTCPSink::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t RemoteTCPSink::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void RemoteTCPSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void RemoteTCPSink::start()
{
    qDebug("RemoteTCPSink::start: m_basebandSampleRate: %d", m_basebandSampleRate);
    m_basebandSink->reset();
    m_basebandSink->setDeviceIndex(m_deviceAPI->getDeviceSetIndex());
    m_basebandSink->setChannelIndex(getIndexInDeviceSet());
    m_basebandSink->startWork();
    m_thread.start();

    if (m_basebandSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_basebandSampleRate);
    }

    updatePublicListing();
}

void RemoteTCPSink::stop()
{
    qDebug("RemoteTCPSink::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
    if (m_settings.m_public) {
        removePublicListing(m_settings.m_publicAddress, m_settings.m_publicPort);
    }
}

bool RemoteTCPSink::handleMessage(const Message& cmd)
{
    if (MsgConfigureRemoteTCPSink::match(cmd))
    {
        const MsgConfigureRemoteTCPSink& cfg = (const MsgConfigureRemoteTCPSink&) cmd;
        qDebug() << "RemoteTCPSink::handleMessage: MsgConfigureRemoteTCPSink";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce(), cfg.getRestartRequired());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        qDebug() << "RemoteTCPSink::handleMessage: DSPSignalNotification: m_basebandSampleRate:" << m_basebandSampleRate;

        // Forward to the sink
        m_basebandSink->getInputMessageQueue()->push(new DSPSignalNotification(notif));

        // Forward to the GUI
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MsgSendMessage::match(cmd))
    {
        const MsgSendMessage& msg = (const MsgSendMessage&) cmd;

        // Forward to the sink
        m_basebandSink->getInputMessageQueue()->push(MsgSendMessage::create(msg.getAddress(), msg.getPort(), msg.getCallsign(), msg.getText(), msg.getBroadcast()));
        return true;
    }
    else if (MsgReportConnection::match(cmd))
    {
        const MsgReportConnection& msg = (const MsgReportConnection&) cmd;
        m_clients = msg.getClients();
        updatePublicListing();
        return true;
    }
    else if (MsgReportDisconnect::match(cmd))
    {
        const MsgReportDisconnect& msg = (const MsgReportDisconnect&) cmd;
        m_clients = msg.getClients();
        updatePublicListing();
        return true;
    }
    else
    {
        return false;
    }
}

QByteArray RemoteTCPSink::serialize() const
{
    return m_settings.serialize();
}

bool RemoteTCPSink::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureRemoteTCPSink *msg = MsgConfigureRemoteTCPSink::create(m_settings, QStringList(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureRemoteTCPSink *msg = MsgConfigureRemoteTCPSink::create(m_settings, QStringList(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void RemoteTCPSink::setCenterFrequency(qint64 frequency)
{
    RemoteTCPSinkSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, {"inputFrequencyOffset"}, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRemoteTCPSink *msgToGUI = MsgConfigureRemoteTCPSink::create(settings, {"inputFrequencyOffset"}, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void RemoteTCPSink::applySettings(const RemoteTCPSinkSettings& settings, const QStringList& settingsKeys, bool force, bool restartRequired)
{
    qDebug() << "RemoteTCPSink::applySettings:"
            << " settingsKeys: " << settingsKeys
            << " m_channelSampleRate: " << settings.m_channelSampleRate
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_gain: " << settings.m_gain
            << " m_sampleBits: " << settings.m_sampleBits
            << " m_dataAddress: " << settings.m_dataAddress
            << " m_dataPort: " << settings.m_dataPort
            << " m_protocol: " << settings.m_protocol
            << " m_streamIndex: " << settings.m_streamIndex
            << " force: " << force
            << " restartRequired: " << restartRequired;

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

    MsgConfigureRemoteTCPSink *msg = MsgConfigureRemoteTCPSink::create(settings, settingsKeys, force, restartRequired);
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

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, settingsKeys, settings, force);
    }

    // Do we need to remove old listing
    bool removeListing = false;
    if (m_settings.m_public)
    {
        if ((settingsKeys.contains("public") || force) && !settings.m_public) {
            removeListing = true;
        }
        if ((settingsKeys.contains("publicAddress") || force) && (settings.m_publicAddress != m_settings.m_publicAddress)) {
            removeListing = true;
        }
        if ((settingsKeys.contains("publicPort") || force) && (settings.m_publicPort != m_settings.m_publicPort)) {
            removeListing = true;
        }
    }
    if (removeListing) {
        removePublicListing(m_settings.m_publicAddress, m_settings.m_publicPort);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    updatePublicListing();
}

int RemoteTCPSink::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRemoteTcpSinkSettings(new SWGSDRangel::SWGRemoteTCPSinkSettings());
    response.getRemoteTcpSinkSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int RemoteTCPSink::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int RemoteTCPSink::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    RemoteTCPSinkSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureRemoteTCPSink *msg = MsgConfigureRemoteTCPSink::create(settings, channelSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    qDebug("RemoteTCPSink::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRemoteTCPSink *msgToGUI = MsgConfigureRemoteTCPSink::create(settings, channelSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void RemoteTCPSink::webapiUpdateChannelSettings(
        RemoteTCPSinkSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("channelSampleRate")) {
        settings.m_channelSampleRate = response.getRemoteTcpSinkSettings()->getChannelSampleRate();
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getRemoteTcpSinkSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("gain")) {
        settings.m_gain = response.getRemoteTcpSinkSettings()->getGain();
    }
    if (channelSettingsKeys.contains("sampleBits")) {
        settings.m_sampleBits = response.getRemoteTcpSinkSettings()->getSampleBits();
    }
    if (channelSettingsKeys.contains("dataAddress")) {
        settings.m_dataAddress = *response.getRemoteTcpSinkSettings()->getDataAddress();
    }
    if (channelSettingsKeys.contains("dataPort"))
    {
        int dataPort = response.getRemoteTcpSinkSettings()->getDataPort();

        if ((dataPort < 1024) || (dataPort > 65535)) {
            settings.m_dataPort = 9090;
        } else {
            settings.m_dataPort = dataPort;
        }
    }
    if (channelSettingsKeys.contains("protocol")) {
        settings.m_protocol = (RemoteTCPSinkSettings::Protocol)response.getRemoteTcpSinkSettings()->getProtocol();
    }

    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getRemoteTcpSinkSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getRemoteTcpSinkSettings()->getTitle();
    }

    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getRemoteTcpSinkSettings()->getStreamIndex();
    }

    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRemoteTcpSinkSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRemoteTcpSinkSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRemoteTcpSinkSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getRemoteTcpSinkSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getRemoteTcpSinkSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getRemoteTcpSinkSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getRemoteTcpSinkSettings()->getRollupState());
    }
}

void RemoteTCPSink::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const RemoteTCPSinkSettings& settings)
{
    response.getRemoteTcpSinkSettings()->setChannelSampleRate(settings.m_channelSampleRate);
    response.getRemoteTcpSinkSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getRemoteTcpSinkSettings()->setGain(settings.m_gain);
    response.getRemoteTcpSinkSettings()->setSampleBits(settings.m_sampleBits);
    if (response.getRemoteTcpSinkSettings()->getDataAddress()) {
        *response.getRemoteTcpSinkSettings()->getDataAddress() = settings.m_dataAddress;
    } else {
        response.getRemoteTcpSinkSettings()->setDataAddress(new QString(settings.m_dataAddress));
    }
    response.getRemoteTcpSinkSettings()->setDataPort(settings.m_dataPort);
    response.getRemoteTcpSinkSettings()->setProtocol((int)settings.m_protocol);

    response.getRemoteTcpSinkSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getRemoteTcpSinkSettings()->getTitle()) {
        *response.getRemoteTcpSinkSettings()->getTitle() = settings.m_title;
    } else {
        response.getRemoteTcpSinkSettings()->setTitle(new QString(settings.m_title));
    }

    response.getRemoteTcpSinkSettings()->setStreamIndex(settings.m_streamIndex);
    response.getRemoteTcpSinkSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRemoteTcpSinkSettings()->getReverseApiAddress()) {
        *response.getRemoteTcpSinkSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRemoteTcpSinkSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRemoteTcpSinkSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRemoteTcpSinkSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getRemoteTcpSinkSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getRemoteTcpSinkSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getRemoteTcpSinkSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getRemoteTcpSinkSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getRemoteTcpSinkSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getRemoteTcpSinkSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getRemoteTcpSinkSettings()->setRollupState(swgRollupState);
        }
    }
}

void RemoteTCPSink::webapiReverseSendSettings(const QStringList& channelSettingsKeys, const RemoteTCPSinkSettings& settings, bool force)
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

void RemoteTCPSink::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    const QStringList& channelSettingsKeys,
    const RemoteTCPSinkSettings& settings,
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

void RemoteTCPSink::webapiFormatChannelSettings(
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const RemoteTCPSinkSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setRemoteTcpSinkSettings(new SWGSDRangel::SWGRemoteTCPSinkSettings());
    SWGSDRangel::SWGRemoteTCPSinkSettings *swgRemoteTCPSinkSettings = swgChannelSettings->getRemoteTcpSinkSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("channelSampleRate") || force) {
        swgRemoteTCPSinkSettings->setChannelSampleRate(settings.m_channelSampleRate);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgRemoteTCPSinkSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("gain") || force) {
        swgRemoteTCPSinkSettings->setGain(settings.m_gain);
    }
    if (channelSettingsKeys.contains("sampleBits") || force) {
        swgRemoteTCPSinkSettings->setSampleBits(settings.m_sampleBits);
    }
    if (channelSettingsKeys.contains("dataAddress") || force) {
        swgRemoteTCPSinkSettings->setDataAddress(new QString(settings.m_dataAddress));
    }
    if (channelSettingsKeys.contains("dataPort") || force) {
        swgRemoteTCPSinkSettings->setDataPort(settings.m_dataPort);
    }
    if (channelSettingsKeys.contains("protocol") || force) {
        swgRemoteTCPSinkSettings->setProtocol(settings.m_protocol);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgRemoteTCPSinkSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgRemoteTCPSinkSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgRemoteTCPSinkSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgRemoteTCPSinkSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgRemoteTCPSinkSettings->setRollupState(swgRollupState);
    }
}

void RemoteTCPSink::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "RemoteTCPSink::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("RemoteTCPSink::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    if (reply == m_removeRequest) {
        m_removeRequest = nullptr;
    }

    reply->deleteLater();
}

void RemoteTCPSink::handleIndexInDeviceSetChanged(int index)
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

void RemoteTCPSink::removePublicListing(const QString& address, quint16 port)
{
    QUrl url = QUrl("https://sdrangel.org/websdr/removedb.php");

    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    json.insert("address", address);
    json.insert("port", port);

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    m_removeRequest = m_networkManager->post(request, data);
}

void RemoteTCPSink::updatePublicListing()
{
    if (!m_settings.m_public || !m_thread.isRunning()) {
        return;
    }

    QUrl url = QUrl("https://sdrangel.org/websdr/updatedb.php");

    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Get device position
    float latitude, longitude, altitude;

    if (!ChannelWebAPIUtils::getDevicePosition(getDeviceSetIndex(), latitude, longitude, altitude))
    {
        latitude = MainCore::instance()->getSettings().getLatitude();
        longitude = MainCore::instance()->getSettings().getLongitude();
        altitude = MainCore::instance()->getSettings().getAltitude();
    }
    // FIXME: Optionally slightly obfuscate position

    // Get antenna direction
    double azimuth = m_settings.m_azimuth;
    double elevation = m_settings.m_elevation;
    if (!m_settings.m_isotropic && !m_settings.m_rotator.isEmpty() && (m_settings.m_rotator != "None"))
    {
        unsigned int rotatorFeatureSetIndex;
        unsigned int rotatorFeatureIndex;

        if (MainCore::getFeatureIndexFromId(m_settings.m_rotator, rotatorFeatureSetIndex, rotatorFeatureIndex))
        {
            ChannelWebAPIUtils::getFeatureReportValue(rotatorFeatureSetIndex, rotatorFeatureIndex, "currentAzimuth", azimuth);
            ChannelWebAPIUtils::getFeatureReportValue(rotatorFeatureSetIndex, rotatorFeatureIndex, "currentElevation", elevation);
        }
    }

    QString device = MainCore::instance()->getDevice(getDeviceSetIndex())->getHardwareId();

    QString protocol;
    if (m_settings.m_protocol == RemoteTCPSinkSettings::SDRA_WSS) {
        protocol = "SDRangel wss";
    } else {
        protocol = "SDRangel";
    }

    QJsonObject json;
    json.insert("address", m_settings.m_publicAddress);
    json.insert("port", m_settings.m_publicPort);
    json.insert("protocol", protocol);
    json.insert("minFrequency", m_settings.m_minFrequency);
    json.insert("maxFrequency", m_settings.m_maxFrequency);
    json.insert("maxSampleRate", m_settings.m_maxSampleRate);
    json.insert("device", device);
    json.insert("antenna", m_settings.m_antenna);
    json.insert("remoteControl", (int) m_settings.m_remoteControl);
    json.insert("stationName", MainCore::instance()->getSettings().getStationName());
    json.insert("location", m_settings.m_location);
    json.insert("latitude", latitude);
    json.insert("longitude", longitude);
    json.insert("altitude", altitude);
    json.insert("isotropic", (int) m_settings.m_isotropic);
    json.insert("azimuth", azimuth);
    json.insert("elevation", elevation);
    json.insert("clients", m_clients);
    json.insert("maxClients", m_settings.m_maxClients);
    json.insert("timeLimit", m_settings.m_timeLimit);

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    m_networkManager->post(request, data);
}
