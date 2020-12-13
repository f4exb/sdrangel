///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Remote sink channel (Rx) UDP sender thread                                    //
//                                                                               //
// SDRangel can work as a detached SDR front end. With this plugin it can        //
// sends the I/Q samples stream to another SDRangel instance via UDP.            //
// It is controlled via a Web REST API.                                          //
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

#include "remotesink.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/devicesamplemimo.h"
#include "dsp/dspdevicesourceengine.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "maincore.h"

#include "remotesinkbaseband.h"

MESSAGE_CLASS_DEFINITION(RemoteSink::MsgConfigureRemoteSink, Message)

const char* const RemoteSink::m_channelIdURI = "sdrangel.channel.remotesink";
const char* const RemoteSink::m_channelId = "RemoteSink";

RemoteSink::RemoteSink(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_frequencyOffset(0),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSink = new RemoteSinkBaseband();
    m_basebandSink->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

RemoteSink::~RemoteSink()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);
    delete m_basebandSink;
    delete m_thread;
}

uint32_t RemoteSink::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void RemoteSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void RemoteSink::start()
{
    qDebug("RemoteSink::start: m_basebandSampleRate: %d", m_basebandSampleRate);
    m_basebandSink->reset();

    if (m_basebandSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_basebandSampleRate);
    }

    m_thread->start();
    m_basebandSink->startSender();
}

void RemoteSink::stop()
{
    qDebug("RemoteSink::stop");
    m_basebandSink->stopSender();
	m_thread->exit();
	m_thread->wait();
}

bool RemoteSink::handleMessage(const Message& cmd)
{
    if (MsgConfigureRemoteSink::match(cmd))
    {
        MsgConfigureRemoteSink& cfg = (MsgConfigureRemoteSink&) cmd;
        qDebug() << "RemoteSink::handleMessage: MsgConfigureRemoteSink";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        qDebug() << "RemoteSink::handleMessage: DSPSignalNotification: m_basebandSampleRate:" << m_basebandSampleRate;
        calculateFrequencyOffset();

        // Forward to the sink
        DSPSignalNotification* msgToBaseband = new DSPSignalNotification(notif); // make a copy
        m_basebandSink->getInputMessageQueue()->push(msgToBaseband);

        // Forward to the GUI
        if (getMessageQueueToGUI())
        {
            DSPSignalNotification* msgToGUI = new DSPSignalNotification(notif); // make a copy
            getMessageQueueToGUI()->push(msgToGUI);
        }

	    return true;
    }
    else
    {
        return false;
    }
}

QByteArray RemoteSink::serialize() const
{
    return m_settings.serialize();
}

bool RemoteSink::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureRemoteSink *msg = MsgConfigureRemoteSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureRemoteSink *msg = MsgConfigureRemoteSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void RemoteSink::applySettings(const RemoteSinkSettings& settings, bool force)
{
    qDebug() << "RemoteSink::applySettings:"
            << " m_nbFECBlocks: " << settings.m_nbFECBlocks
            << " m_txDelay: " << settings.m_txDelay
            << " m_dataAddress: " << settings.m_dataAddress
            << " m_dataPort: " << settings.m_dataPort
            << " m_streamIndex: " << settings.m_streamIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;
    bool frequencyOffsetChange = false;

    if ((m_settings.m_nbFECBlocks != settings.m_nbFECBlocks) || force) {
        reverseAPIKeys.append("nbFECBlocks");
    }
    if ((m_settings.m_txDelay != settings.m_txDelay) || force) {
        reverseAPIKeys.append("txDelay");
    }
    if ((m_settings.m_dataAddress != settings.m_dataAddress) || force) {
        reverseAPIKeys.append("dataAddress");
    }
    if ((m_settings.m_dataPort != settings.m_dataPort) || force) {
        reverseAPIKeys.append("dataPort");
    }
    if ((m_settings.m_rgbColor != settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((m_settings.m_title != settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        reverseAPIKeys.append("log2Decim");
        frequencyOffsetChange = true;
    }

    if ((m_settings.m_filterChainHash != settings.m_filterChainHash) || force)
    {
        reverseAPIKeys.append("filterChainHash");
        frequencyOffsetChange = true;
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

    RemoteSinkBaseband::MsgConfigureRemoteSinkBaseband *msg = RemoteSinkBaseband::MsgConfigureRemoteSinkBaseband::create(settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if ((settings.m_useReverseAPI) && (reverseAPIKeys.size() != 0))
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

    if (frequencyOffsetChange) {
        calculateFrequencyOffset();
    }
}

void RemoteSink::validateFilterChainHash(RemoteSinkSettings& settings)
{
    unsigned int s = 1;

    for (unsigned int i = 0; i < settings.m_log2Decim; i++) {
        s *= 3;
    }

    settings.m_filterChainHash = settings.m_filterChainHash >= s ? s-1 : settings.m_filterChainHash;
}

void RemoteSink::calculateFrequencyOffset()
{
    double shiftFactor = HBFilterChainConverter::getShiftFactor(m_settings.m_log2Decim, m_settings.m_filterChainHash);
    m_frequencyOffset = m_basebandSampleRate * shiftFactor;
}

int RemoteSink::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRemoteSinkSettings(new SWGSDRangel::SWGRemoteSinkSettings());
    response.getRemoteSinkSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int RemoteSink::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    RemoteSinkSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureRemoteSink *msg = MsgConfigureRemoteSink::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("RemoteSink::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRemoteSink *msgToGUI = MsgConfigureRemoteSink::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void RemoteSink::webapiUpdateChannelSettings(
        RemoteSinkSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("nbFECBlocks"))
    {
        int nbFECBlocks = response.getRemoteSinkSettings()->getNbFecBlocks();

        if ((nbFECBlocks < 0) || (nbFECBlocks > 127)) {
            settings.m_nbFECBlocks = 8;
        } else {
            settings.m_nbFECBlocks = response.getRemoteSinkSettings()->getNbFecBlocks();
        }
    }

    if (channelSettingsKeys.contains("txDelay"))
    {
        int txDelay = response.getRemoteSinkSettings()->getTxDelay();

        if (txDelay < 0) {
            settings.m_txDelay = 35;
        } else {
            settings.m_txDelay = txDelay;
        }
    }

    if (channelSettingsKeys.contains("dataAddress")) {
        settings.m_dataAddress = *response.getRemoteSinkSettings()->getDataAddress();
    }

    if (channelSettingsKeys.contains("dataPort"))
    {
        int dataPort = response.getRemoteSinkSettings()->getDataPort();

        if ((dataPort < 1024) || (dataPort > 65535)) {
            settings.m_dataPort = 9090;
        } else {
            settings.m_dataPort = dataPort;
        }
    }

    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getRemoteSinkSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getRemoteSinkSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getRemoteSinkSettings()->getLog2Decim();
    }

    if (channelSettingsKeys.contains("filterChainHash"))
    {
        settings.m_filterChainHash = response.getRemoteSinkSettings()->getFilterChainHash();
        validateFilterChainHash(settings);
    }

    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getRemoteSinkSettings()->getStreamIndex();
    }

    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRemoteSinkSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRemoteSinkSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRemoteSinkSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getRemoteSinkSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getRemoteSinkSettings()->getReverseApiChannelIndex();
    }
}

void RemoteSink::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const RemoteSinkSettings& settings)
{
    response.getRemoteSinkSettings()->setNbFecBlocks(settings.m_nbFECBlocks);
    response.getRemoteSinkSettings()->setTxDelay(settings.m_txDelay);

    if (response.getRemoteSinkSettings()->getDataAddress()) {
        *response.getRemoteSinkSettings()->getDataAddress() = settings.m_dataAddress;
    } else {
        response.getRemoteSinkSettings()->setDataAddress(new QString(settings.m_dataAddress));
    }

    response.getRemoteSinkSettings()->setDataPort(settings.m_dataPort);
    response.getRemoteSinkSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getRemoteSinkSettings()->getTitle()) {
        *response.getRemoteSinkSettings()->getTitle() = settings.m_title;
    } else {
        response.getRemoteSinkSettings()->setTitle(new QString(settings.m_title));
    }

    response.getRemoteSinkSettings()->setLog2Decim(settings.m_log2Decim);
    response.getRemoteSinkSettings()->setFilterChainHash(settings.m_filterChainHash);
    response.getRemoteSinkSettings()->setStreamIndex(settings.m_streamIndex);
    response.getRemoteSinkSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRemoteSinkSettings()->getReverseApiAddress()) {
        *response.getRemoteSinkSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRemoteSinkSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRemoteSinkSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRemoteSinkSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getRemoteSinkSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void RemoteSink::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const RemoteSinkSettings& settings, bool force)
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

void RemoteSink::sendChannelSettings(
    QList<MessageQueue*> *messageQueues,
    QList<QString>& channelSettingsKeys,
    const RemoteSinkSettings& settings,
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

void RemoteSink::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const RemoteSinkSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setRemoteSinkSettings(new SWGSDRangel::SWGRemoteSinkSettings());
    SWGSDRangel::SWGRemoteSinkSettings *swgRemoteSinkSettings = swgChannelSettings->getRemoteSinkSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("nbFECBlocks") || force) {
        swgRemoteSinkSettings->setNbFecBlocks(settings.m_nbFECBlocks);
    }
    if (channelSettingsKeys.contains("txDelay") || force)
    {
        swgRemoteSinkSettings->setTxDelay(settings.m_txDelay);
    }
    if (channelSettingsKeys.contains("dataAddress") || force) {
        swgRemoteSinkSettings->setDataAddress(new QString(settings.m_dataAddress));
    }
    if (channelSettingsKeys.contains("dataPort") || force) {
        swgRemoteSinkSettings->setDataPort(settings.m_dataPort);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgRemoteSinkSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgRemoteSinkSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("log2Decim") || force) {
        swgRemoteSinkSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (channelSettingsKeys.contains("filterChainHash") || force) {
        swgRemoteSinkSettings->setFilterChainHash(settings.m_filterChainHash);
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgRemoteSinkSettings->setStreamIndex(settings.m_streamIndex);
    }
}

void RemoteSink::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "RemoteSink::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("RemoteSink::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
