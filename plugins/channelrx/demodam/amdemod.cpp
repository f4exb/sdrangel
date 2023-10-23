///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
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

#include "amdemod.h"

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
#include "SWGAMDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGAMDemodReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplesource.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(AMDemod::MsgConfigureAMDemod, Message)

const char* const AMDemod::m_channelIdURI = "sdrangel.channel.amdemod";
const char* const AMDemod::m_channelId = "AMDemod";
const int AMDemod::m_udpBlockSize = 512;

AMDemod::AMDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_thread(nullptr),
        m_basebandSink(nullptr),
        m_running(false),
        m_basebandSampleRate(0),
        m_lastTs(0)
{
    setObjectName(m_channelId);
	applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AMDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &AMDemod::handleIndexInDeviceSetChanged
    );
    start();
}

AMDemod::~AMDemod()
{
    qDebug("AMDemod::~AMDemod");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AMDemod::networkManagerFinished
    );
    delete m_networkManager;
	m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);
    stop();
}

void AMDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t AMDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void AMDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void AMDemod::start()
{
    if (m_running) {
        return;
    }

	qDebug("AMDemod::start");
    m_thread = new QThread();
    m_basebandSink = new AMDemodBaseband();
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(m_thread);

    QObject::connect(m_thread, &QThread::finished, m_basebandSink, &QObject::deleteLater);
    QObject::connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread->start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    AMDemodBaseband::MsgConfigureAMDemodBaseband *msg = AMDemodBaseband::MsgConfigureAMDemodBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);

    m_running = true;
}

void AMDemod::stop()
{
    if (!m_running) {
        return;
    }

    qDebug("AMDemod::stop");
    m_running = false;
	m_thread->quit();
	m_thread->wait();
}

bool AMDemod::handleMessage(const Message& cmd)
{
	if (MsgConfigureAMDemod::match(cmd))
	{
        MsgConfigureAMDemod& cfg = (MsgConfigureAMDemod&) cmd;
        qDebug() << "AMDemod::handleMessage: MsgConfigureAMDemod";
        applySettings(cfg.getSettings(), cfg.getForce());

		return true;
	}
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        qDebug() << "AMDemod::handleMessage: DSPSignalNotification";

        // Forward to the sink
        if (m_running)
        {
            DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
            m_basebandSink->getInputMessageQueue()->push(rep);
        }

        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

	    return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "AMDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
	else
	{
		return false;
	}
}

void AMDemod::setCenterFrequency(qint64 frequency)
{
    AMDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAMDemod *msgToGUI = MsgConfigureAMDemod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void AMDemod::applySettings(const AMDemodSettings& settings, bool force)
{
    qDebug() << "AMDemod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_afBandwidth: " << settings.m_afBandwidth
            << " m_volume: " << settings.m_volume
            << " m_squelch: " << settings.m_squelch
            << " m_audioMute: " << settings.m_audioMute
            << " m_bandpassEnable: " << settings.m_bandpassEnable
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_pll: " << settings.m_pll
            << " m_syncAMOperation: " << (int) settings.m_syncAMOperation
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_rfBandwidth != settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((m_settings.m_afBandwidth != settings.m_afBandwidth) || force) {
        reverseAPIKeys.append("afBandwidth");
    }
    if ((m_settings.m_bandpassEnable != settings.m_bandpassEnable) || force) {
        reverseAPIKeys.append("bandpassEnable");
    }
    if ((m_settings.m_squelch != settings.m_squelch) || force) {
        reverseAPIKeys.append("squelch");
    }
    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force) {
        reverseAPIKeys.append("audioDeviceName");
    }

    if ((m_settings.m_pll != settings.m_pll) || force)
    {
        reverseAPIKeys.append("pll");
        reverseAPIKeys.append("syncAMOperation");
    }

    if ((m_settings.m_syncAMOperation != settings.m_syncAMOperation) || force)
    {
        reverseAPIKeys.append("pll");
        reverseAPIKeys.append("syncAMOperation");
    }

    if ((m_settings.m_inputFrequencyOffset != settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
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

    if (m_running)
    {
        AMDemodBaseband::MsgConfigureAMDemodBaseband *msg = AMDemodBaseband::MsgConfigureAMDemodBaseband::create(settings, force);
        m_basebandSink->getInputMessageQueue()->push(msg);
    }

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

QByteArray AMDemod::serialize() const
{
    return m_settings.serialize();
}

bool AMDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureAMDemod *msg = MsgConfigureAMDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureAMDemod *msg = MsgConfigureAMDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void AMDemod::sendSampleRateToDemodAnalyzer()
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(
                this,
                getAudioSampleRate()
            );
            messageQueue->push(msg);
        }
    }
}

int AMDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAmDemodSettings(new SWGSDRangel::SWGAMDemodSettings());
    response.getAmDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int AMDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int AMDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    AMDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureAMDemod *msg = MsgConfigureAMDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("AMDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAMDemod *msgToGUI = MsgConfigureAMDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void AMDemod::webapiUpdateChannelSettings(
        AMDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getAmDemodSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getAmDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getAmDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("afBandwidth")) {
        settings.m_afBandwidth = response.getAmDemodSettings()->getAfBandwidth();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAmDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getAmDemodSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getAmDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getAmDemodSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("bandpassEnable")) {
        settings.m_bandpassEnable = response.getAmDemodSettings()->getBandpassEnable() != 0;
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getAmDemodSettings()->getAudioDeviceName();
    }

    if (channelSettingsKeys.contains("pll")) {
        settings.m_pll = response.getAmDemodSettings()->getPll();
    }

    if (channelSettingsKeys.contains("syncAMOperation")) {
        qint32 syncAMOperationCode = response.getAmDemodSettings()->getSyncAmOperation();
        settings.m_syncAMOperation = syncAMOperationCode < 0 ?
                AMDemodSettings::SyncAMDSB : syncAMOperationCode > 2 ?
                        AMDemodSettings::SyncAMLSB : (AMDemodSettings::SyncAMOperation) syncAMOperationCode;
    }

    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getAmDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAmDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAmDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAmDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAmDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getAmDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getAmDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getAmDemodSettings()->getRollupState());
    }
}

int AMDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAmDemodReport(new SWGSDRangel::SWGAMDemodReport());
    response.getAmDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void AMDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const AMDemodSettings& settings)
{
    response.getAmDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getAmDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getAmDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getAmDemodSettings()->setAfBandwidth(settings.m_afBandwidth);
    response.getAmDemodSettings()->setRgbColor(settings.m_rgbColor);
    response.getAmDemodSettings()->setSquelch(settings.m_squelch);
    response.getAmDemodSettings()->setVolume(settings.m_volume);
    response.getAmDemodSettings()->setBandpassEnable(settings.m_bandpassEnable ? 1 : 0);

    if (response.getAmDemodSettings()->getTitle()) {
        *response.getAmDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getAmDemodSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getAmDemodSettings()->getAudioDeviceName()) {
        *response.getAmDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getAmDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getAmDemodSettings()->setPll(settings.m_pll ? 1 : 0);
    response.getAmDemodSettings()->setSyncAmOperation((int) settings.m_syncAMOperation);
    response.getAmDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getAmDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAmDemodSettings()->getReverseApiAddress()) {
        *response.getAmDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAmDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAmDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAmDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getAmDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getAmDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getAmDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getAmDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getAmDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getAmDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getAmDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void AMDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    if (!m_running) {
        return;
    }

    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    response.getAmDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getAmDemodReport()->setSquelch(m_basebandSink->getSquelchOpen() ? 1 : 0);
    response.getAmDemodReport()->setAudioSampleRate(m_basebandSink->getAudioSampleRate());
    response.getAmDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
}

void AMDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const AMDemodSettings& settings, bool force)
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

void AMDemod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const AMDemodSettings& settings,
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

void AMDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const AMDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("AMDemod"));
    swgChannelSettings->setAmDemodSettings(new SWGSDRangel::SWGAMDemodSettings());
    SWGSDRangel::SWGAMDemodSettings *swgAMDemodSettings = swgChannelSettings->getAmDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("audioMute") || force) {
        swgAMDemodSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgAMDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgAMDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("afBandwidth") || force) {
        swgAMDemodSettings->setAfBandwidth(settings.m_afBandwidth);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgAMDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("squelch") || force) {
        swgAMDemodSettings->setSquelch(settings.m_squelch);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgAMDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("volume") || force) {
        swgAMDemodSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("bandpassEnable") || force) {
        swgAMDemodSettings->setBandpassEnable(settings.m_bandpassEnable ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgAMDemodSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("pll") || force) {
        swgAMDemodSettings->setPll(settings.m_pll);
    }
    if (channelSettingsKeys.contains("syncAMOperation") || force) {
        swgAMDemodSettings->setSyncAmOperation((int) settings.m_syncAMOperation);
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgAMDemodSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgAMDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgAMDemodSettings->setRollupState(swgRollupState);
    }
}

void AMDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AMDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AMDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void AMDemod::handleWrittenToFifo(int nsamples, qint64 timestamp)
{
    QDateTime dt;
    dt.setMSecsSinceEpoch(MainCore::instance()->getStartMsecsSinceEpoch() + timestamp/1000000);
    qDebug("AMDemod::handleWrittenToFifo: %s: nsamples: %d, dts: %lld sr: %f",
        qPrintable(dt.toString("yyyy.MM.dd hh:mm:ss.zzz")),
        nsamples,
        timestamp - m_lastTs, nsamples*1e9 / (timestamp - m_lastTs)
    );
    m_lastTs = timestamp;
}

void AMDemod::handleIndexInDeviceSetChanged(int index)
{
    if (!m_running || (index < 0)) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
    m_basebandSink->setAudioFifoLabel(fifoLabel);
}
