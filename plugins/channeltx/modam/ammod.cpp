///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGAMModReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "dsp/cwkeyer.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

#include "ammodbaseband.h"
#include "ammod.h"

MESSAGE_CLASS_DEFINITION(AMMod::MsgConfigureAMMod, Message)
MESSAGE_CLASS_DEFINITION(AMMod::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(AMMod::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(AMMod::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(AMMod::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(AMMod::MsgReportFileSourceStreamTiming, Message)

const char* const AMMod::m_channelIdURI = "sdrangel.channeltx.modam";
const char* const AMMod::m_channelId ="AMMod";

AMMod::AMMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
	m_settingsMutex(QMutex::Recursive),
	m_fileSize(0),
	m_recordLength(0),
	m_sampleRate(48000)
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new AMModBaseband();
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
        &AMMod::networkManagerFinished
    );
}

AMMod::~AMMod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AMMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void AMMod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t AMMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

void AMMod::start()
{
	qDebug("AMMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void AMMod::stop()
{
    qDebug("AMMod::stop");
	m_thread->exit();
	m_thread->wait();
}

void AMMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

void AMMod::setCenterFrequency(qint64 frequency)
{
    AMModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAMMod *msgToGUI = MsgConfigureAMMod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

bool AMMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureAMMod::match(cmd))
    {
        MsgConfigureAMMod& cfg = (MsgConfigureAMMod&) cmd;
        qDebug() << "AMMod::handleMessage: MsgConfigureAMMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
	else if (MsgConfigureFileSourceName::match(cmd))
    {
        MsgConfigureFileSourceName& conf = (MsgConfigureFileSourceName&) cmd;
        m_fileName = conf.getFileName();
        qDebug() << "AMMod::handleMessage: MsgConfigureFileSourceName";
        openFileStream();
        return true;
    }
    else if (MsgConfigureFileSourceSeek::match(cmd))
    {
        MsgConfigureFileSourceSeek& conf = (MsgConfigureFileSourceSeek&) cmd;
        int seekPercentage = conf.getPercentage();
        qDebug() << "AMMod::handleMessage: MsgConfigureFileSourceSeek";
        seekFileStream(seekPercentage);

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
    else if (CWKeyer::MsgConfigureCWKeyer::match(cmd))
    {
        const CWKeyer::MsgConfigureCWKeyer& cfg = (CWKeyer::MsgConfigureCWKeyer&) cmd;
        qDebug() << "AMMod::handleMessage: MsgConfigureCWKeyer";

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendCWSettings(cfg.getSettings());
        }

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "AMMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "AMMod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
	else
	{
		return false;
	}
}

void AMMod::openFileStream()
{
    if (m_ifstream.is_open()) {
        m_ifstream.close();
    }

    m_ifstream.open(m_fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
    m_fileSize = m_ifstream.tellg();
    m_ifstream.seekg(0,std::ios_base::beg);

    m_sampleRate = 48000; // fixed rate
    m_recordLength = m_fileSize / (sizeof(Real) * m_sampleRate);

    qDebug() << "AMMod::openFileStream: " << m_fileName.toStdString().c_str()
            << " fileSize: " << m_fileSize << "bytes"
            << " length: " << m_recordLength << " seconds";

    MsgReportFileSourceStreamData *report;
    report = MsgReportFileSourceStreamData::create(m_sampleRate, m_recordLength);
    getMessageQueueToGUI()->push(report);
}

void AMMod::seekFileStream(int seekPercentage)
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

void AMMod::applySettings(const AMModSettings& settings, bool force)
{
    qDebug() << "AMMod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_modFactor: " << settings.m_modFactor
            << " m_toneFrequency: " << settings.m_toneFrequency
            << " m_volumeFactor: " << settings.m_volumeFactor
            << " m_channelMute: " << settings.m_channelMute
            << " m_playLoop: " << settings.m_playLoop
            << " m_modAFInput " << settings.m_modAFInput
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_streamIndex: " << settings.m_streamIndex
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

    if ((settings.m_modFactor != m_settings.m_modFactor) || force) {
        reverseAPIKeys.append("modFactor");
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

    if ((settings.m_modAFInput != m_settings.m_modAFInput) || force) {
        reverseAPIKeys.append("modAFInput");
    }

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
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

    AMModBaseband::MsgConfigureAMModBaseband *msg = AMModBaseband::MsgConfigureAMModBaseband::create(settings, force);
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

QByteArray AMMod::serialize() const
{
    return m_settings.serialize();
}

bool AMMod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureAMMod *msg = MsgConfigureAMMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureAMMod *msg = MsgConfigureAMMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void AMMod::sendSampleRateToDemodAnalyzer()
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

int AMMod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAmModSettings(new SWGSDRangel::SWGAMModSettings());
    response.getAmModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getAmModSettings()->getCwKeyer();
    const CWKeyerSettings& cwKeyerSettings = m_basebandSource->getCWKeyer().getSettings();
    CWKeyer::webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);

    return 200;
}

int AMMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int AMMod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    AMModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    if (channelSettingsKeys.contains("cwKeyer"))
    {
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getAmModSettings()->getCwKeyer();
        CWKeyerSettings cwKeyerSettings = m_basebandSource->getCWKeyer().getSettings();
        CWKeyer::webapiSettingsPutPatch(channelSettingsKeys, cwKeyerSettings, apiCwKeyerSettings);

        CWKeyer::MsgConfigureCWKeyer *msgCwKeyer = CWKeyer::MsgConfigureCWKeyer::create(cwKeyerSettings, force);
        m_basebandSource->getCWKeyer().getInputMessageQueue()->push(msgCwKeyer);

        if (m_guiMessageQueue) // forward to GUI if any
        {
            CWKeyer::MsgConfigureCWKeyer *msgCwKeyerToGUI = CWKeyer::MsgConfigureCWKeyer::create(cwKeyerSettings, force);
            m_guiMessageQueue->push(msgCwKeyerToGUI);
        }
    }

    MsgConfigureAMMod *msg = MsgConfigureAMMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAMMod *msgToGUI = MsgConfigureAMMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void AMMod::webapiUpdateChannelSettings(
        AMModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getAmModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getAmModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("modAFInput")) {
        settings.m_modAFInput = (AMModSettings::AMModInputAF) response.getAmModSettings()->getModAfInput();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getAmModSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("playLoop")) {
        settings.m_playLoop = response.getAmModSettings()->getPlayLoop() != 0;
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getAmModSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAmModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getAmModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("toneFrequency")) {
        settings.m_toneFrequency = response.getAmModSettings()->getToneFrequency();
    }
    if (channelSettingsKeys.contains("volumeFactor")) {
        settings.m_volumeFactor = response.getAmModSettings()->getVolumeFactor();
    }
    if (channelSettingsKeys.contains("modFactor")) {
        settings.m_modFactor = response.getAmModSettings()->getModFactor();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getAmModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAmModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAmModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAmModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAmModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getAmModSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getAmModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getAmModSettings()->getRollupState());
    }
}

int AMMod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAmModReport(new SWGSDRangel::SWGAMModReport());
    response.getAmModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void AMMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const AMModSettings& settings)
{
    response.getAmModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getAmModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getAmModSettings()->setModAfInput((int) settings.m_modAFInput);
    response.getAmModSettings()->setPlayLoop(settings.m_playLoop ? 1 : 0);
    response.getAmModSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getAmModSettings()->setModFactor(settings.m_modFactor);
    response.getAmModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getAmModSettings()->getTitle()) {
        *response.getAmModSettings()->getTitle() = settings.m_title;
    } else {
        response.getAmModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getAmModSettings()->setToneFrequency(settings.m_toneFrequency);
    response.getAmModSettings()->setVolumeFactor(settings.m_volumeFactor);

    if (!response.getAmModSettings()->getCwKeyer()) {
        response.getAmModSettings()->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings);
    }

    if (response.getAmModSettings()->getAudioDeviceName()) {
        *response.getAmModSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getAmModSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getAmModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAmModSettings()->getReverseApiAddress()) {
        *response.getAmModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAmModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAmModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAmModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getAmModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getAmModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getAmModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getAmModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getAmModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getAmModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getAmModSettings()->setRollupState(swgRollupState);
        }
    }
}

void AMMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getAmModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getAmModReport()->setAudioSampleRate(m_basebandSource->getAudioSampleRate());
    response.getAmModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void AMMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const AMModSettings& settings, bool force)
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

void AMMod::webapiReverseSendCWSettings(const CWKeyerSettings& cwKeyerSettings)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setChannelType(new QString("AMMod"));
    swgChannelSettings->setAmModSettings(new SWGSDRangel::SWGAMModSettings());
    SWGSDRangel::SWGAMModSettings *swgAMModSettings = swgChannelSettings->getAmModSettings();

    swgAMModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgAMModSettings->getCwKeyer();
    m_basebandSource->getCWKeyer().webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex)
            .arg(m_settings.m_reverseAPIChannelIndex);
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

void AMMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const AMModSettings& settings,
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

void AMMod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const AMModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setAmModSettings(new SWGSDRangel::SWGAMModSettings());
    SWGSDRangel::SWGAMModSettings *swgAMModSettings = swgChannelSettings->getAmModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("channelMute") || force) {
        swgAMModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgAMModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("modAFInput") || force) {
        swgAMModSettings->setModAfInput((int) settings.m_modAFInput);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgAMModSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("playLoop") || force) {
        swgAMModSettings->setPlayLoop(settings.m_playLoop ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgAMModSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgAMModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgAMModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("toneFrequency") || force) {
        swgAMModSettings->setToneFrequency(settings.m_toneFrequency);
    }
    if (channelSettingsKeys.contains("volumeFactor") || force) {
        swgAMModSettings->setVolumeFactor(settings.m_volumeFactor);
    }
    if (channelSettingsKeys.contains("modFactor") || force) {
        swgAMModSettings->setModFactor(settings.m_modFactor);
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgAMModSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (force)
    {
        const CWKeyerSettings& cwKeyerSettings = m_basebandSource->getCWKeyer().getSettings();
        swgAMModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgAMModSettings->getCwKeyer();
        m_basebandSource->getCWKeyer().webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgAMModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgAMModSettings->setRollupState(swgRollupState);
    }
}

void AMMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AMMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AMMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double AMMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

CWKeyer *AMMod::getCWKeyer()
{
    return &m_basebandSource->getCWKeyer();
}

void AMMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

int AMMod::getAudioSampleRate() const
{
    return m_basebandSource->getAudioSampleRate();
}

int AMMod::getFeedbackAudioSampleRate() const
{
    return m_basebandSource->getFeedbackAudioSampleRate();
}
