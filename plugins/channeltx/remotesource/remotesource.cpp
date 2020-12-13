///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2019 Edouard Griffiths, F4EXB                              //
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

#include "remotesource.h"

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGRemoteSourceReport.h"

#include "dsp/devicesamplesink.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/timeutil.h"
#include "util/db.h"
#include "maincore.h"

#include "remotesourcebaseband.h"

MESSAGE_CLASS_DEFINITION(RemoteSource::MsgConfigureRemoteSource, Message)
MESSAGE_CLASS_DEFINITION(RemoteSource::MsgQueryStreamData, Message)
MESSAGE_CLASS_DEFINITION(RemoteSource::MsgReportStreamData, Message)

const char* const RemoteSource::m_channelIdURI = "sdrangel.channeltx.remotesource";
const char* const RemoteSource::m_channelId ="RemoteSource";

RemoteSource::RemoteSource(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new RemoteSourceBaseband();
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

RemoteSource::~RemoteSource()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void RemoteSource::start()
{
    qDebug("RemoteSource::start");
    m_basebandSource->reset();
    m_thread->start();
    RemoteSourceBaseband::MsgConfigureRemoteSourceWork *msg = RemoteSourceBaseband::MsgConfigureRemoteSourceWork::create(true);
    m_basebandSource->getInputMessageQueue()->push(msg);
}

void RemoteSource::stop()
{
    qDebug("RemoteSource::stop");
	m_thread->exit();
	m_thread->wait();
}

void RemoteSource::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool RemoteSource::handleMessage(const Message& cmd)
{
    if (MsgConfigureRemoteSource::match(cmd))
    {
        MsgConfigureRemoteSource& cfg = (MsgConfigureRemoteSource&) cmd;
        qDebug() << "MsgConfigureRemoteSource::handleMessage: MsgConfigureRemoteSource";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgQueryStreamData::match(cmd))
    {
        if (m_guiMessageQueue)
        {
            uint64_t nowus = TimeUtil::nowus();
            RemoteDataReadQueue& dataReadQueue = m_basebandSource->getDataQueue();
            const RemoteMetaDataFEC& currentMeta = m_basebandSource->getRemoteMetaDataFEC();

            MsgReportStreamData *msg = MsgReportStreamData::create(
                    nowus / 1000000U,
                    nowus % 1000000U,
                    dataReadQueue.size(),
                    dataReadQueue.length(),
                    dataReadQueue.readSampleCount(),
                    m_basebandSource->getNbCorrectableErrors(),
                    m_basebandSource->getNbUncorrectableErrors(),
                    currentMeta.m_nbOriginalBlocks,
                    currentMeta.m_nbFECBlocks,
                    currentMeta.m_centerFrequency,
                    currentMeta.m_sampleRate);
            m_guiMessageQueue->push(msg);
        }

        return true;
    }
    return false;
}

QByteArray RemoteSource::serialize() const
{
    return m_settings.serialize();
}

bool RemoteSource::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureRemoteSource *msg = MsgConfigureRemoteSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureRemoteSource *msg = MsgConfigureRemoteSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void RemoteSource::applySettings(const RemoteSourceSettings& settings, bool force)
{
    qDebug() << "RemoteSource::applySettings:"
            << " m_dataAddress: " << settings.m_dataAddress
            << " m_dataPort: " << settings.m_dataPort
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_title: " << settings.m_title
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_dataAddress != settings.m_dataAddress) || force) {
        reverseAPIKeys.append("dataAddress");
    }

    if ((m_settings.m_dataPort != settings.m_dataPort) || force) {
        reverseAPIKeys.append("dataPort");
    }

    if (m_settings.m_streamIndex != settings.m_streamIndex)
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSourceAPI(this);
            m_deviceAPI->removeChannelSource(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSource(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSourceAPI(this);
        }

        reverseAPIKeys.append("streamIndex");
    }

    RemoteSourceBaseband::MsgConfigureRemoteSourceBaseband *msg = RemoteSourceBaseband::MsgConfigureRemoteSourceBaseband::create(settings, force);
    m_basebandSource->getInputMessageQueue()->push(msg);

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

int RemoteSource::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRemoteSourceSettings(new SWGSDRangel::SWGRemoteSourceSettings());
    response.getRemoteSourceSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int RemoteSource::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    RemoteSourceSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureRemoteSource *msg = MsgConfigureRemoteSource::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("RemoteSource::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRemoteSource *msgToGUI = MsgConfigureRemoteSource::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void RemoteSource::webapiUpdateChannelSettings(
        RemoteSourceSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("dataAddress")) {
        settings.m_dataAddress = *response.getRemoteSourceSettings()->getDataAddress();
    }
    if (channelSettingsKeys.contains("dataPort"))
    {
        int dataPort = response.getRemoteSourceSettings()->getDataPort();

        if ((dataPort < 1024) || (dataPort > 65535)) {
            settings.m_dataPort = 9090;
        } else {
            settings.m_dataPort = dataPort;
        }
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getRemoteSourceSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getRemoteSourceSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getRemoteSourceSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRemoteSourceSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRemoteSourceSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRemoteSourceSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getRemoteSourceSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getRemoteSourceSettings()->getReverseApiChannelIndex();
    }
}

int RemoteSource::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRemoteSourceReport(new SWGSDRangel::SWGRemoteSourceReport());
    response.getRemoteSourceReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void RemoteSource::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const RemoteSourceSettings& settings)
{
    if (response.getRemoteSourceSettings()->getDataAddress()) {
        *response.getRemoteSourceSettings()->getDataAddress() = settings.m_dataAddress;
    } else {
        response.getRemoteSourceSettings()->setDataAddress(new QString(settings.m_dataAddress));
    }

    response.getRemoteSourceSettings()->setDataPort(settings.m_dataPort);
    response.getRemoteSourceSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getRemoteSourceSettings()->getTitle()) {
        *response.getRemoteSourceSettings()->getTitle() = settings.m_title;
    } else {
        response.getRemoteSourceSettings()->setTitle(new QString(settings.m_title));
    }

    response.getRemoteSourceSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRemoteSourceSettings()->getReverseApiAddress()) {
        *response.getRemoteSourceSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRemoteSourceSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRemoteSourceSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRemoteSourceSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getRemoteSourceSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void RemoteSource::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    uint64_t nowus = TimeUtil::nowus();
    RemoteDataReadQueue& dataReadQueue = m_basebandSource->getDataQueue();
    const RemoteMetaDataFEC& currentMeta = m_basebandSource->getRemoteMetaDataFEC();

    response.getRemoteSourceReport()->setTvSec(nowus / 1000000U);
    response.getRemoteSourceReport()->setTvUSec(nowus % 1000000U);
    response.getRemoteSourceReport()->setQueueSize(dataReadQueue.size());
    response.getRemoteSourceReport()->setQueueLength(dataReadQueue.length());
    response.getRemoteSourceReport()->setSamplesCount(dataReadQueue.readSampleCount());
    response.getRemoteSourceReport()->setCorrectableErrorsCount(m_basebandSource->getNbCorrectableErrors());
    response.getRemoteSourceReport()->setUncorrectableErrorsCount(m_basebandSource->getNbUncorrectableErrors());
    response.getRemoteSourceReport()->setNbOriginalBlocks(currentMeta.m_nbOriginalBlocks);
    response.getRemoteSourceReport()->setNbFecBlocks(currentMeta.m_nbFECBlocks);
    response.getRemoteSourceReport()->setCenterFreq(currentMeta.m_centerFrequency);
    response.getRemoteSourceReport()->setSampleRate(currentMeta.m_sampleRate);
    response.getRemoteSourceReport()->setDeviceCenterFreq(m_deviceAPI->getSampleSink()->getCenterFrequency()/1000);
    response.getRemoteSourceReport()->setDeviceSampleRate(m_deviceAPI->getSampleSink()->getSampleRate());
}

void RemoteSource::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const RemoteSourceSettings& settings, bool force)
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

void RemoteSource::sendChannelSettings(
    QList<MessageQueue*> *messageQueues,
    QList<QString>& channelSettingsKeys,
    const RemoteSourceSettings& settings,
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

void RemoteSource::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const RemoteSourceSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setRemoteSourceSettings(new SWGSDRangel::SWGRemoteSourceSettings());
    SWGSDRangel::SWGRemoteSourceSettings *swgRemoteSourceSettings = swgChannelSettings->getRemoteSourceSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("dataAddress") || force) {
        swgRemoteSourceSettings->setDataAddress(new QString(settings.m_dataAddress));
    }
    if (channelSettingsKeys.contains("dataPort") || force) {
        swgRemoteSourceSettings->setDataPort(settings.m_dataPort);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgRemoteSourceSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgRemoteSourceSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgRemoteSourceSettings->setStreamIndex(settings.m_streamIndex);
    }
}

void RemoteSource::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "RemoteSource::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("RemoteSource::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

uint32_t RemoteSource::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}
