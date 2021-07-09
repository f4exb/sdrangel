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

#include "pagerdemod.h"

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(PagerDemod::MsgConfigurePagerDemod, Message)
MESSAGE_CLASS_DEFINITION(PagerDemod::MsgPagerMessage, Message)

const char * const PagerDemod::m_channelIdURI = "sdrangel.channel.pagerdemod";
const char * const PagerDemod::m_channelId = "PagerDemod";

PagerDemod::PagerDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new PagerDemodBaseband(this);
    m_basebandSink->setMessageQueueToChannel(getInputMessageQueue());
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    connect(&m_channelMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleChannelMessages()));
}

PagerDemod::~PagerDemod()
{
    qDebug("PagerDemod::~PagerDemod");
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

uint32_t PagerDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void PagerDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void PagerDemod::start()
{
    qDebug("PagerDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    PagerDemodBaseband::MsgConfigurePagerDemodBaseband *msg = PagerDemodBaseband::MsgConfigurePagerDemodBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void PagerDemod::stop()
{
    qDebug("PagerDemod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool PagerDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigurePagerDemod::match(cmd))
    {
        MsgConfigurePagerDemod& cfg = (MsgConfigurePagerDemod&) cmd;
        qDebug() << "PagerDemod::handleMessage: MsgConfigurePagerDemod";
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
        qDebug() << "PagerDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);

        return true;
    }
    else if (MsgPagerMessage::match(cmd))
    {
        // Forward to GUI
        MsgPagerMessage& report = (MsgPagerMessage&)cmd;
        if (getMessageQueueToGUI())
        {
            MsgPagerMessage *msg = new MsgPagerMessage(report);
            getMessageQueueToGUI()->push(msg);
        }

        // Forward via UDP
        if (m_settings.m_udpEnabled)
        {
            QByteArray message;
            message.append(report.getDateTime().toString().toLatin1());
            message.append(QString::number(report.getAddress()).toLatin1());
            message.append(QString::number(report.getFunctionBits()).toLatin1());
            message.append(report.getAlphaMessage().toLatin1());
            message.append(report.getNumericMessage().toLatin1());
            m_udpSocket.writeDatagram(message.data(), message.size(),
                                QHostAddress(m_settings.m_udpAddress), m_settings.m_udpPort);
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "PagerDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

ScopeVis *PagerDemod::getScopeSink()
{
    return m_basebandSink->getScopeSink();
}

void PagerDemod::applySettings(const PagerDemodSettings& settings, bool force)
{
    qDebug() << "PagerDemod::applySettings:"
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_baud != m_settings.m_baud) || force) {
        reverseAPIKeys.append("baud");
    }
    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        reverseAPIKeys.append("fmDeviation");
    }
    if ((settings.m_udpEnabled != m_settings.m_udpEnabled) || force) {
        reverseAPIKeys.append("udpEnabled");
    }
    if ((settings.m_udpAddress != m_settings.m_udpAddress) || force) {
        reverseAPIKeys.append("udpAddress");
    }
    if ((settings.m_udpPort != m_settings.m_udpPort) || force) {
        reverseAPIKeys.append("udpPort");
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

    PagerDemodBaseband::MsgConfigurePagerDemodBaseband *msg = PagerDemodBaseband::MsgConfigurePagerDemodBaseband::create(settings, force);
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

QByteArray PagerDemod::serialize() const
{
    return m_settings.serialize();
}

bool PagerDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigurePagerDemod *msg = MsgConfigurePagerDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigurePagerDemod *msg = MsgConfigurePagerDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void PagerDemod::sendSampleRateToDemodAnalyzer()
{
    QList<MessageQueue*> *messageQueues = MainCore::instance()->getMessagePipes().getMessageQueues(this, "reportdemod");

    if (messageQueues)
    {
        QList<MessageQueue*>::iterator it = messageQueues->begin();

        for (; it != messageQueues->end(); ++it)
        {
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(
                this,
                PagerDemodSettings::m_channelSampleRate
            );
            (*it)->push(msg);
        }
    }
}

int PagerDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setPagerDemodSettings(new SWGSDRangel::SWGPagerDemodSettings());
    response.getPagerDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int PagerDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    PagerDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigurePagerDemod *msg = MsgConfigurePagerDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("PagerDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigurePagerDemod *msgToGUI = MsgConfigurePagerDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void PagerDemod::webapiUpdateChannelSettings(
        PagerDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("baud")) {
        settings.m_baud = response.getPagerDemodSettings()->getBaud();
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getPagerDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getPagerDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getPagerDemodSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getPagerDemodSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getPagerDemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getPagerDemodSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getPagerDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getPagerDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getPagerDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getPagerDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getPagerDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getPagerDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getPagerDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getPagerDemodSettings()->getReverseApiChannelIndex();
    }
}

void PagerDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const PagerDemodSettings& settings)
{
    response.getPagerDemodSettings()->setBaud(settings.m_baud);
    response.getPagerDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getPagerDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getPagerDemodSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getPagerDemodSettings()->setUdpEnabled(settings.m_udpEnabled);
    response.getPagerDemodSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getPagerDemodSettings()->setUdpPort(settings.m_udpPort);

    response.getPagerDemodSettings()->setRgbColor(settings.m_rgbColor);
    if (response.getPagerDemodSettings()->getTitle()) {
        *response.getPagerDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getPagerDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getPagerDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getPagerDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getPagerDemodSettings()->getReverseApiAddress()) {
        *response.getPagerDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getPagerDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getPagerDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getPagerDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getPagerDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void PagerDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const PagerDemodSettings& settings, bool force)
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

void PagerDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const PagerDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("PagerDemod"));
    swgChannelSettings->setPagerDemodSettings(new SWGSDRangel::SWGPagerDemodSettings());
    SWGSDRangel::SWGPagerDemodSettings *swgPagerDemodSettings = swgChannelSettings->getPagerDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("baud") || force) {
        swgPagerDemodSettings->setBaud(settings.m_baud);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgPagerDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgPagerDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgPagerDemodSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgPagerDemodSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgPagerDemodSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgPagerDemodSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgPagerDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgPagerDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgPagerDemodSettings->setStreamIndex(settings.m_streamIndex);
    }
}

void PagerDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "PagerDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("PagerDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void PagerDemod::handleChannelMessages()
{
	Message* message;

	while ((message = m_channelMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}
