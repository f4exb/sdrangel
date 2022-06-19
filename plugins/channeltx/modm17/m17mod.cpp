///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#include <QTime>
#include <QDebug>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGNFMModReport.h"

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

#include "m17modbaseband.h"
#include "m17mod.h"

MESSAGE_CLASS_DEFINITION(M17Mod::MsgConfigureM17Mod, Message)
MESSAGE_CLASS_DEFINITION(M17Mod::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(M17Mod::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(M17Mod::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(M17Mod::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(M17Mod::MsgReportFileSourceStreamTiming, Message)

const char* const M17Mod::m_channelIdURI = "sdrangel.channeltx.modm17";
const char* const M17Mod::m_channelId = "M17Mod";

M17Mod::M17Mod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
	m_deviceAPI(deviceAPI),
	m_settingsMutex(QMutex::Recursive),
	m_fileSize(0),
	m_recordLength(0),
	m_sampleRate(48000)
{
	setObjectName(m_channelId);
    m_loopPacketTimer.setInterval(m_settings.m_loopPacketInterval*1000);

    m_thread = new QThread(this);
    m_basebandSource = new M17ModBaseband();
    m_basebandSource->setInputFileStream(&m_ifstream);
    m_basebandSource->setChannel(this);
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &M17Mod::networkManagerFinished
    );
    QObject::connect(
        &m_loopPacketTimer,
        &QTimer::timeout,
        this,
        &M17Mod::packetLoopTimeout
    );
}

M17Mod::~M17Mod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &M17Mod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void M17Mod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSourceAPI(this);
        m_deviceAPI->removeChannelSource(this);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSource(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

void M17Mod::start()
{
	qDebug("M17Mod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void M17Mod::stop()
{
    qDebug("M17Mod::stop");
	m_thread->exit();
	m_thread->wait();
}

void M17Mod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

void M17Mod::setCenterFrequency(qint64 frequency)
{
    M17ModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureM17Mod *msgToGUI = MsgConfigureM17Mod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

bool M17Mod::handleMessage(const Message& cmd)
{
    if (MsgConfigureM17Mod::match(cmd))
    {
        MsgConfigureM17Mod& cfg = (MsgConfigureM17Mod&) cmd;
        qDebug() << "M17Mod::handleMessage: MsgConfigureM17Mod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
	else if (MsgConfigureFileSourceName::match(cmd))
    {
        MsgConfigureFileSourceName& conf = (MsgConfigureFileSourceName&) cmd;
        m_fileName = conf.getFileName();
        openFileStream();
	    qDebug() << "M17Mod::handleMessage: MsgConfigureFileSourceName:"
	             << " m_fileName: " << m_fileName;
        return true;
    }
    else if (MsgConfigureFileSourceSeek::match(cmd))
    {
        MsgConfigureFileSourceSeek& conf = (MsgConfigureFileSourceSeek&) cmd;
        int seekPercentage = conf.getPercentage();
        seekFileStream(seekPercentage);
        qDebug() << "M17Mod::handleMessage: MsgConfigureFileSourceSeek:"
                 << " seekPercentage: " << seekPercentage;

        return true;
    }
    else if (MsgConfigureFileSourceStreamTiming::match(cmd))
    {
    	std::size_t samplesCount;

    	if (m_ifstream.eof()) {
    		samplesCount = m_fileSize / sizeof(Real);
    	} else {
    		samplesCount = m_ifstream.tellg() / sizeof(Real);
    	}

    	MsgReportFileSourceStreamTiming *report;
        report = MsgReportFileSourceStreamTiming::create(samplesCount);
        getMessageQueueToGUI()->push(report);

        return true;
    }
	else if (MsgConfigureFileSourceName::match(cmd))
    {
        MsgConfigureFileSourceName& conf = (MsgConfigureFileSourceName&) cmd;
        m_fileName = conf.getFileName();
        openFileStream();
	    qDebug() << "M17Mod::handleMessage: MsgConfigureFileSourceName:"
	             << " m_fileName: " << m_fileName;
        return true;
    }
    else if (MsgConfigureFileSourceSeek::match(cmd))
    {
        MsgConfigureFileSourceSeek& conf = (MsgConfigureFileSourceSeek&) cmd;
        int seekPercentage = conf.getPercentage();
        seekFileStream(seekPercentage);
        qDebug() << "M17Mod::handleMessage: MsgConfigureFileSourceSeek:"
                 << " seekPercentage: " << seekPercentage;

        return true;
    }
    else if (MsgConfigureFileSourceStreamTiming::match(cmd))
    {
    	std::size_t samplesCount;

    	if (m_ifstream.eof()) {
    		samplesCount = m_fileSize / sizeof(Real);
    	} else {
    		samplesCount = m_ifstream.tellg() / sizeof(Real);
    	}

    	MsgReportFileSourceStreamTiming *report;
        report = MsgReportFileSourceStreamTiming::create(samplesCount);
        getMessageQueueToGUI()->push(report);

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "M17Mod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "M17Mod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
	else
	{
		return false;
	}
}

void M17Mod::openFileStream()
{
    if (m_ifstream.is_open()) {
        m_ifstream.close();
    }

    m_ifstream.open(m_fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
    m_fileSize = m_ifstream.tellg();
    m_ifstream.seekg(0,std::ios_base::beg);

    m_sampleRate = 48000; // fixed rate
    m_recordLength = m_fileSize / (sizeof(Real) * m_sampleRate);

    qDebug() << "M17Mod::openFileStream: " << m_fileName.toStdString().c_str()
            << " fileSize: " << m_fileSize << "bytes"
            << " length: " << m_recordLength << " seconds";

    MsgReportFileSourceStreamData *report;
    report = MsgReportFileSourceStreamData::create(m_sampleRate, m_recordLength);
    getMessageQueueToGUI()->push(report);
}

void M17Mod::seekFileStream(int seekPercentage)
{
    QMutexLocker mutexLocker(&m_settingsMutex);

    if (m_ifstream.is_open())
    {
        int seekPoint = ((m_recordLength * seekPercentage) / 100) * m_sampleRate;
        seekPoint *= sizeof(Real);
        m_ifstream.clear();
        m_ifstream.seekg(seekPoint, std::ios::beg);
    }
}

void M17Mod::applySettings(const M17ModSettings& settings, bool force)
{
    qDebug() << "M17Mod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_volumeFactor: " << settings.m_volumeFactor
            << " m_toneFrequency: " << settings.m_toneFrequency
            << " m_channelMute: " << settings.m_channelMute
            << " m_playLoop: " << settings.m_playLoop
            << " m_m17Mode " << settings.m_m17Mode
            << " m_audioType " << settings.m_audioType
            << " m_packetType " << settings.m_packetType
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIAddress: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        reverseAPIKeys.append("fmDeviation");
    }
    if ((settings.m_volumeFactor != m_settings.m_volumeFactor) || force) {
        reverseAPIKeys.append("volumeFactor");
    }
    if ((settings.m_channelMute != m_settings.m_channelMute) || force) {
        reverseAPIKeys.append("channelMute");
    }
    if ((settings.m_playLoop != m_settings.m_playLoop) || force) {
        reverseAPIKeys.append("playLoop");
    }
    if ((settings.m_audioType != m_settings.m_audioType) || force) {
        reverseAPIKeys.append("audioType");
    }
    if ((settings.m_packetType != m_settings.m_packetType) || force) {
        reverseAPIKeys.append("packetType");
    }
    if ((settings.m_m17Mode != m_settings.m_m17Mode) || force) {
        reverseAPIKeys.append("m17Mode");
    }
    if((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_toneFrequency != m_settings.m_toneFrequency) || force) {
        reverseAPIKeys.append("toneFrequency");
    }
    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force) {
        reverseAPIKeys.append("audioDeviceName");
    }
    if ((settings.m_feedbackAudioDeviceName != m_settings.m_feedbackAudioDeviceName) || force) {
        reverseAPIKeys.append("feedbackAudioDeviceName");
    }

    if ((settings.m_loopPacketInterval != m_settings.m_loopPacketInterval) || force)
    {
        reverseAPIKeys.append("loopPacketInterval");
        m_loopPacketTimer.setInterval(settings.m_loopPacketInterval*1000);
    }

    if ((settings.m_loopPacket != m_settings.m_loopPacket) || force)
    {
        reverseAPIKeys.append("loopPacket");

        if (settings.m_loopPacket) {
            m_loopPacketTimer.start(settings.m_loopPacketInterval*1000);
        } else {
            m_loopPacketTimer.stop();
        }
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

    M17ModBaseband::MsgConfigureM17ModBaseband *msg = M17ModBaseband::MsgConfigureM17ModBaseband::create(settings, force);
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

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, reverseAPIKeys, settings, force);
    }

    m_settings = settings;
}

QByteArray M17Mod::serialize() const
{
    return m_settings.serialize();
}

bool M17Mod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureM17Mod *msg = MsgConfigureM17Mod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

void M17Mod::sendSampleRateToDemodAnalyzer()
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue* messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(
                this,
                getAudioSampleRate()
            );
            messageQueue->push(msg);
        }
    }
}

int M17Mod::webapiSettingsGet(
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setM17ModSettings(new SWGSDRangel::SWGM17ModSettings());
    response.getM17ModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int M17Mod::webapiWorkspaceGet(
    SWGSDRangel::SWGWorkspaceInfo& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int M17Mod::webapiSettingsPutPatch(
    bool force,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    M17ModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureM17Mod *msg = MsgConfigureM17Mod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureM17Mod *msgToGUI = MsgConfigureM17Mod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void M17Mod::webapiUpdateChannelSettings(
    M17ModSettings& settings,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getM17ModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getM17ModSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getM17ModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("m17Mode")) {
        settings.m_m17Mode = (M17ModSettings::M17Mode) response.getM17ModSettings()->getM17Mode();
    }
    if (channelSettingsKeys.contains("audioType")) {
        settings.m_audioType = (M17ModSettings::AudioType) response.getM17ModSettings()->getAudioType();
    }
    if (channelSettingsKeys.contains("packetType")) {
        settings.m_packetType = (M17ModSettings::PacketType) response.getM17ModSettings()->getPacketType();
    }
    if (channelSettingsKeys.contains("playLoop")) {
        settings.m_playLoop = response.getM17ModSettings()->getPlayLoop() != 0;
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getM17ModSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getM17ModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getM17ModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("toneFrequency")) {
        settings.m_toneFrequency = response.getM17ModSettings()->getToneFrequency();
    }
    if (channelSettingsKeys.contains("volumeFactor")) {
        settings.m_volumeFactor = response.getM17ModSettings()->getVolumeFactor();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getM17ModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getM17ModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getM17ModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getM17ModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getM17ModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getNfmModSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getM17ModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getM17ModSettings()->getRollupState());
    }
}

int M17Mod::webapiReportGet(
    SWGSDRangel::SWGChannelReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setM17ModReport(new SWGSDRangel::SWGM17ModReport());
    response.getM17ModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void M17Mod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const M17ModSettings& settings)
{
    response.getM17ModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getM17ModSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getM17ModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getM17ModSettings()->setM17Mode((int) settings.m_m17Mode);
    response.getM17ModSettings()->setAudioType((int) settings.m_audioType);
    response.getM17ModSettings()->setPacketType((int) settings.m_packetType);
    response.getM17ModSettings()->setPlayLoop(settings.m_playLoop ? 1 : 0);
    response.getM17ModSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getM17ModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getM17ModSettings()->getTitle()) {
        *response.getM17ModSettings()->getTitle() = settings.m_title;
    } else {
        response.getM17ModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getM17ModSettings()->setToneFrequency(settings.m_toneFrequency);
    response.getM17ModSettings()->setVolumeFactor(settings.m_volumeFactor);

    if (response.getM17ModSettings()->getAudioDeviceName()) {
        *response.getM17ModSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getM17ModSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getM17ModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getM17ModSettings()->getReverseApiAddress()) {
        *response.getM17ModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getM17ModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getM17ModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getM17ModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getM17ModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getM17ModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getM17ModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getM17ModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getM17ModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getM17ModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getM17ModSettings()->setRollupState(swgRollupState);
        }
    }
}

void M17Mod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getM17ModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getM17ModReport()->setAudioSampleRate(m_basebandSource->getAudioSampleRate());
    response.getM17ModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void M17Mod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const M17ModSettings& settings, bool force)
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

void M17Mod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const M17ModSettings& settings,
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

void M17Mod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const M17ModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setM17ModSettings(new SWGSDRangel::SWGM17ModSettings());
    SWGSDRangel::SWGM17ModSettings *swgM17ModSettings = swgChannelSettings->getM17ModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("channelMute") || force) {
        swgM17ModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgM17ModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("m17Mode") || force) {
        swgM17ModSettings->setM17Mode((int) settings.m_m17Mode);
    }
    if (channelSettingsKeys.contains("audioType") || force) {
        swgM17ModSettings->setAudioType((int) settings.m_audioType);
    }
    if (channelSettingsKeys.contains("packetType") || force) {
        swgM17ModSettings->setPacketType((int) settings.m_packetType);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgM17ModSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("playLoop") || force) {
        swgM17ModSettings->setPlayLoop(settings.m_playLoop ? 1 : 0);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgM17ModSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgM17ModSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgM17ModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgM17ModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("toneFrequency") || force) {
        swgM17ModSettings->setToneFrequency(settings.m_toneFrequency);
    }
    if (channelSettingsKeys.contains("volumeFactor") || force) {
        swgM17ModSettings->setVolumeFactor(settings.m_volumeFactor);
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgM17ModSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgM17ModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgM17ModSettings->setRollupState(swgRollupState);
    }
}

void M17Mod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "M17Mod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("M17Mod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void M17Mod::packetLoopTimeout()
{
    if (m_settings.m_m17Mode == M17ModSettings::M17ModeM17Packet) {
        m_basebandSource->sendPacket();
    }
}

double M17Mod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void M17Mod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t M17Mod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

int M17Mod::getAudioSampleRate() const
{
    return m_basebandSource->getAudioSampleRate();
}

int M17Mod::getFeedbackAudioSampleRate() const
{
    return m_basebandSource->getFeedbackAudioSampleRate();
}

void M17Mod::sendPacket()
{
    if (m_settings.m_m17Mode == M17ModSettings::M17ModeM17Packet) {
        m_basebandSource->sendPacket();
    }
}
