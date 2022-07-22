///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#include <stdio.h>
#include <complex.h>

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGNFMDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGNFMDemodReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

#include "nfmdemod.h"

MESSAGE_CLASS_DEFINITION(NFMDemod::MsgConfigureNFMDemod, Message)

const char* const NFMDemod::m_channelIdURI = "sdrangel.channel.nfmdemod";
const char* const NFMDemod::m_channelId = "NFMDemod";

const int NFMDemod::m_udpBlockSize = 512;

NFMDemod::NFMDemod(DeviceAPI *devieAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(devieAPI),
        m_running(false),
        m_basebandSampleRate(0)
{
    qDebug("NFMDemod::NFMDemod");
	setObjectName(m_channelId);
	applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &NFMDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &NFMDemod::handleIndexInDeviceSetChanged
    );

    start();
}

NFMDemod::~NFMDemod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &NFMDemod::networkManagerFinished
    );
    delete m_networkManager;
	m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    stop();
}

void NFMDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t NFMDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void NFMDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void NFMDemod::start()
{
    if (m_running) {
        return;
    }

    qDebug() << "NFMDemod::start";
    m_thread = new QThread();
    m_basebandSink = new NFMDemodBaseband();
    m_basebandSink->setFifoLabel(QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(getIndexInDeviceSet())
    );
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(m_thread);

    QObject::connect(
        m_thread,
        &QThread::finished,
        m_basebandSink,
        &QObject::deleteLater
    );
    QObject::connect(
        m_thread,
        &QThread::finished,
        m_thread,
        &QThread::deleteLater
    );

    if (m_basebandSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_basebandSampleRate);
    }

    m_thread->start();

    NFMDemodBaseband::MsgConfigureNFMDemodBaseband *msg = NFMDemodBaseband::MsgConfigureNFMDemodBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);

    m_running = true;
}

void NFMDemod::stop()
{
    if (!m_running) {
        return;
    }

    qDebug() << "NFMDemod::stop";
    m_running = false;
	m_thread->quit();
	m_thread->wait();
}

bool NFMDemod::handleMessage(const Message& cmd)
{
	if (MsgConfigureNFMDemod::match(cmd))
	{
	    MsgConfigureNFMDemod& cfg = (MsgConfigureNFMDemod&) cmd;
		qDebug() << "NFMDemod::handleMessage: MsgConfigureNFMDemod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
	}
	else if (DSPSignalNotification::match(cmd))
	{
        qDebug() << "NFMDemod::handleMessage: DSPSignalNotification";
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        // Forward to the sink if any
        if (m_running) {
            m_basebandSink->getInputMessageQueue()->push(new DSPSignalNotification(notif));
        }

        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

	    return true;
	}
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "NFMDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
	else
	{
		return false;
	}
}

void NFMDemod::setCenterFrequency(qint64 frequency)
{
    NFMDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureNFMDemod *msgToGUI = MsgConfigureNFMDemod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void NFMDemod::applySettings(const NFMDemodSettings& settings, bool force)
{
    qDebug() << "NFMDemod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_afBandwidth: " << settings.m_afBandwidth
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_volume: " << settings.m_volume
            << " m_squelchGate: " << settings.m_squelchGate
            << " m_deltaSquelch: " << settings.m_deltaSquelch
            << " m_squelch: " << settings.m_squelch
            << " m_ctcssIndex: " << settings.m_ctcssIndex
            << " m_ctcssOn: " << settings.m_ctcssOn
            << " m_dcsOn: " << settings.m_dcsOn
            << " m_dcsCode: " << oct << settings.m_dcsCode << dec
            << " m_dcsPositive: " << settings.m_dcsPositive
            << " m_highPass: " << settings.m_highPass
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

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_volume != m_settings.m_volume) || force) {
        reverseAPIKeys.append("volume");
    }
    if ((settings.m_ctcssOn != m_settings.m_ctcssOn) || force) {
        reverseAPIKeys.append("ctcssOn");
    }
    if ((settings.m_audioMute != m_settings.m_audioMute) || force) {
        reverseAPIKeys.append("audioMute");
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
    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        reverseAPIKeys.append("fmDeviation");
    }
    if ((settings.m_afBandwidth != m_settings.m_afBandwidth) || force) {
        reverseAPIKeys.append("afBandwidth");
    }
    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force) {
        reverseAPIKeys.append("squelchGate");
    }
    if ((settings.m_squelch != m_settings.m_squelch) || force) {
        reverseAPIKeys.append("squelch");
    }
    if ((settings.m_deltaSquelch != m_settings.m_deltaSquelch) || force) {
        reverseAPIKeys.append("deltaSquelch");
    }
    if ((settings.m_ctcssIndex != m_settings.m_ctcssIndex) || force) {
        reverseAPIKeys.append("ctcssIndex");
    }
    if ((settings.m_highPass != m_settings.m_highPass) || force) {
        reverseAPIKeys.append("highPass");
    }
    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force) {
        reverseAPIKeys.append("audioDeviceName");
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
        NFMDemodBaseband::MsgConfigureNFMDemodBaseband *msg = NFMDemodBaseband::MsgConfigureNFMDemodBaseband::create(settings, force);
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

QByteArray NFMDemod::serialize() const
{
    return m_settings.serialize();
}

bool NFMDemod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureNFMDemod *msg = MsgConfigureNFMDemod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

void NFMDemod::sendSampleRateToDemodAnalyzer()
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

int NFMDemod::webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setNfmDemodSettings(new SWGSDRangel::SWGNFMDemodSettings());
    response.getNfmDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int NFMDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int NFMDemod::webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage)
{
    (void) errorMessage;
    NFMDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureNFMDemod *msg = MsgConfigureNFMDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureNFMDemod *msgToGUI = MsgConfigureNFMDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void NFMDemod::webapiUpdateChannelSettings(
        NFMDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("afBandwidth")) {
        settings.m_afBandwidth = response.getNfmDemodSettings()->getAfBandwidth();
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getNfmDemodSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("highPass")) {
        settings.m_highPass = response.getNfmDemodSettings()->getHighPass() != 0;
    }
    if (channelSettingsKeys.contains("ctcssIndex")) {
        settings.m_ctcssIndex = response.getNfmDemodSettings()->getCtcssIndex();
    }
    if (channelSettingsKeys.contains("ctcssOn")) {
        settings.m_ctcssOn = response.getNfmDemodSettings()->getCtcssOn() != 0;
    }
    if (channelSettingsKeys.contains("deltaSquelch")) {
        settings.m_deltaSquelch = response.getNfmDemodSettings()->getDeltaSquelch() != 0;
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getNfmDemodSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getNfmDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getNfmDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getNfmDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getNfmDemodSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("squelchGate")) {
        settings.m_squelchGate = response.getNfmDemodSettings()->getSquelchGate();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getNfmDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getNfmDemodSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getNfmDemodSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getNfmDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getNfmDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getNfmDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getNfmDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getNfmDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getNfmDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getNfmDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getNfmDemodSettings()->getRollupState());
    }
}

int NFMDemod::webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setNfmDemodReport(new SWGSDRangel::SWGNFMDemodReport());
    response.getNfmDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void NFMDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const NFMDemodSettings& settings)
{
    response.getNfmDemodSettings()->setAfBandwidth(settings.m_afBandwidth);
    response.getNfmDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getNfmDemodSettings()->setHighPass(settings.m_highPass ? 1 : 0);
    response.getNfmDemodSettings()->setCtcssIndex(settings.m_ctcssIndex);
    response.getNfmDemodSettings()->setCtcssOn(settings.m_ctcssOn ? 1 : 0);
    response.getNfmDemodSettings()->setDeltaSquelch(settings.m_deltaSquelch ? 1 : 0);
    response.getNfmDemodSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getNfmDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getNfmDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getNfmDemodSettings()->setRgbColor(settings.m_rgbColor);
    response.getNfmDemodSettings()->setSquelch(settings.m_squelch);
    response.getNfmDemodSettings()->setSquelchGate(settings.m_squelchGate);
    response.getNfmDemodSettings()->setVolume(settings.m_volume);

    if (response.getNfmDemodSettings()->getTitle()) {
        *response.getNfmDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getNfmDemodSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getNfmDemodSettings()->getAudioDeviceName()) {
        *response.getNfmDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getNfmDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getNfmDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getNfmDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getNfmDemodSettings()->getReverseApiAddress()) {
        *response.getNfmDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getNfmDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getNfmDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getNfmDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getNfmDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getNfmDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getNfmDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getNfmDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getNfmDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getNfmDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getNfmDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void NFMDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getNfmDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    int nbCtcssToneFrequencies;

    if (m_running)
    {
        const Real *ctcssToneFrequencies = m_basebandSink->getCtcssToneSet(nbCtcssToneFrequencies);
        response.getNfmDemodReport()->setCtcssTone(
            m_settings.m_ctcssOn ?
                m_settings.m_ctcssIndex < 0 ?
                    0
                    : m_settings.m_ctcssIndex < nbCtcssToneFrequencies ?
                        ctcssToneFrequencies[m_settings.m_ctcssIndex-1]
                        : 0
                : 0
        );
        response.getNfmDemodReport()->setSquelch(m_basebandSink->getSquelchOpen() ? 1 : 0);
        response.getNfmDemodReport()->setAudioSampleRate(m_basebandSink->getAudioSampleRate());
        response.getNfmDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
    }
}

void NFMDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const NFMDemodSettings& settings, bool force)
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

void NFMDemod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const NFMDemodSettings& settings,
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

void NFMDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const NFMDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setNfmDemodSettings(new SWGSDRangel::SWGNFMDemodSettings());
    SWGSDRangel::SWGNFMDemodSettings *swgNFMDemodSettings = swgChannelSettings->getNfmDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("afBandwidth") || force) {
        swgNFMDemodSettings->setAfBandwidth(settings.m_afBandwidth);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgNFMDemodSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("highPass") || force) {
        swgNFMDemodSettings->setAudioMute(settings.m_highPass ? 1 : 0);
    }
    if (channelSettingsKeys.contains("ctcssIndex") || force) {
        swgNFMDemodSettings->setCtcssIndex(settings.m_ctcssIndex);
    }
    if (channelSettingsKeys.contains("ctcssOn") || force) {
        swgNFMDemodSettings->setCtcssOn(settings.m_ctcssOn ? 1 : 0);
    }
    if (channelSettingsKeys.contains("deltaSquelch") || force) {
        swgNFMDemodSettings->setDeltaSquelch(settings.m_deltaSquelch ? 1 : 0);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgNFMDemodSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgNFMDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgNFMDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgNFMDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("squelch") || force) {
        swgNFMDemodSettings->setSquelch(settings.m_squelch);
    }
    if (channelSettingsKeys.contains("squelchGate") || force) {
        swgNFMDemodSettings->setSquelchGate(settings.m_squelchGate);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgNFMDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("volume") || force) {
        swgNFMDemodSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgNFMDemodSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgNFMDemodSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgNFMDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgNFMDemodSettings->setRollupState(swgRollupState);
    }
}

void NFMDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "NFMDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("NFMDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void NFMDemod::handleIndexInDeviceSetChanged(int index)
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
