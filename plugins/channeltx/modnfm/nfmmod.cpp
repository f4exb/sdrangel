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

#include <QTime>
#include <QDebug>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGCWKeyerSettings.h"
#include "SWGChannelReport.h"
#include "SWGNFMModReport.h"

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/cwkeyer.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

#include "nfmmodbaseband.h"
#include "nfmmod.h"

MESSAGE_CLASS_DEFINITION(NFMMod::MsgConfigureNFMMod, Message)
MESSAGE_CLASS_DEFINITION(NFMMod::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(NFMMod::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(NFMMod::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(NFMMod::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(NFMMod::MsgReportFileSourceStreamTiming, Message)

const char* const NFMMod::m_channelIdURI = "sdrangel.channeltx.modnfm";
const char* const NFMMod::m_channelId = "NFMMod";

NFMMod::NFMMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
	m_deviceAPI(deviceAPI),
	m_settingsMutex(QMutex::Recursive),
	m_fileSize(0),
	m_recordLength(0),
	m_sampleRate(48000)
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new NFMModBaseband();
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
        &NFMMod::networkManagerFinished
    );
}

NFMMod::~NFMMod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &NFMMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void NFMMod::setDeviceAPI(DeviceAPI *deviceAPI)
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

void NFMMod::start()
{
	qDebug("NFMMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void NFMMod::stop()
{
    qDebug("NFMMod::stop");
	m_thread->exit();
	m_thread->wait();
}

void NFMMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

void NFMMod::setCenterFrequency(qint64 frequency)
{
    NFMModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureNFMMod *msgToGUI = MsgConfigureNFMMod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

bool NFMMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureNFMMod::match(cmd))
    {
        MsgConfigureNFMMod& cfg = (MsgConfigureNFMMod&) cmd;
        qDebug() << "NFMMod::handleMessage: MsgConfigureNFMMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
	else if (MsgConfigureFileSourceName::match(cmd))
    {
        MsgConfigureFileSourceName& conf = (MsgConfigureFileSourceName&) cmd;
        m_fileName = conf.getFileName();
        openFileStream();
	    qDebug() << "NFMMod::handleMessage: MsgConfigureFileSourceName:"
	             << " m_fileName: " << m_fileName;
        return true;
    }
    else if (MsgConfigureFileSourceSeek::match(cmd))
    {
        MsgConfigureFileSourceSeek& conf = (MsgConfigureFileSourceSeek&) cmd;
        int seekPercentage = conf.getPercentage();
        seekFileStream(seekPercentage);
        qDebug() << "NFMMod::handleMessage: MsgConfigureFileSourceSeek:"
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
	    qDebug() << "NFMMod::handleMessage: MsgConfigureFileSourceName:"
	             << " m_fileName: " << m_fileName;
        return true;
    }
    else if (MsgConfigureFileSourceSeek::match(cmd))
    {
        MsgConfigureFileSourceSeek& conf = (MsgConfigureFileSourceSeek&) cmd;
        int seekPercentage = conf.getPercentage();
        seekFileStream(seekPercentage);
        qDebug() << "NFMMod::handleMessage: MsgConfigureFileSourceSeek:"
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
    else if (CWKeyer::MsgConfigureCWKeyer::match(cmd))
    {
        const CWKeyer::MsgConfigureCWKeyer& cfg = (CWKeyer::MsgConfigureCWKeyer&) cmd;

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
        qDebug() << "NFMMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "NFMMod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
	else
	{
		return false;
	}
}

void NFMMod::openFileStream()
{
    if (m_ifstream.is_open()) {
        m_ifstream.close();
    }

    m_ifstream.open(m_fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
    m_fileSize = m_ifstream.tellg();
    m_ifstream.seekg(0,std::ios_base::beg);

    m_sampleRate = 48000; // fixed rate
    m_recordLength = m_fileSize / (sizeof(Real) * m_sampleRate);

    qDebug() << "NFMMod::openFileStream: " << m_fileName.toStdString().c_str()
            << " fileSize: " << m_fileSize << "bytes"
            << " length: " << m_recordLength << " seconds";

    MsgReportFileSourceStreamData *report;
    report = MsgReportFileSourceStreamData::create(m_sampleRate, m_recordLength);
    getMessageQueueToGUI()->push(report);
}

void NFMMod::seekFileStream(int seekPercentage)
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

void NFMMod::applySettings(const NFMModSettings& settings, bool force)
{
    qDebug() << "NFMMod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_afBandwidth: " << settings.m_afBandwidth
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_volumeFactor: " << settings.m_volumeFactor
            << " m_toneFrequency: " << settings.m_toneFrequency
            << " m_ctcssIndex: " << settings.m_ctcssIndex
            << " m_ctcssOn: " << settings.m_ctcssOn
            << " m_dcsOn: " << settings.m_dcsOn
            << " m_dcsCode: " << settings.m_dcsCode
            << " m_dcsPositive: " << settings.m_dcsPositive
            << " m_channelMute: " << settings.m_channelMute
            << " m_playLoop: " << settings.m_playLoop
            << " m_modAFInput " << settings.m_modAFInput
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
    if ((settings.m_ctcssOn != m_settings.m_ctcssOn) || force) {
        reverseAPIKeys.append("ctcssOn");
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
    if((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_afBandwidth != m_settings.m_afBandwidth) || force) {
        reverseAPIKeys.append("afBandwidth");
    }
    if ((settings.m_toneFrequency != m_settings.m_toneFrequency) || force) {
        reverseAPIKeys.append("toneFrequency");
    }
    if ((settings.m_ctcssIndex != m_settings.m_ctcssIndex) || force) {
        reverseAPIKeys.append("ctcssIndex");
    }
    if ((settings.m_dcsOn != m_settings.m_dcsOn) || force) {
        reverseAPIKeys.append("dcsOn");
    }
    if ((settings.m_dcsCode != m_settings.m_dcsCode) || force) {
        reverseAPIKeys.append("dcsCode");
    }
    if ((settings.m_dcsPositive != m_settings.m_dcsPositive) || force) {
        reverseAPIKeys.append("dcsPositive");
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

    NFMModBaseband::MsgConfigureNFMModBaseband *msg = NFMModBaseband::MsgConfigureNFMModBaseband::create(settings, force);
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

QByteArray NFMMod::serialize() const
{
    return m_settings.serialize();
}

bool NFMMod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureNFMMod *msg = MsgConfigureNFMMod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

void NFMMod::sendSampleRateToDemodAnalyzer()
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

int NFMMod::webapiSettingsGet(
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setNfmModSettings(new SWGSDRangel::SWGNFMModSettings());
    response.getNfmModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getNfmModSettings()->getCwKeyer();
    const CWKeyerSettings& cwKeyerSettings = m_basebandSource->getCWKeyer().getSettings();
    CWKeyer::webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);

    return 200;
}

int NFMMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int NFMMod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    NFMModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    if (channelSettingsKeys.contains("cwKeyer"))
    {
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getNfmModSettings()->getCwKeyer();
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

    MsgConfigureNFMMod *msg = MsgConfigureNFMMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureNFMMod *msgToGUI = MsgConfigureNFMMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void NFMMod::webapiUpdateChannelSettings(
        NFMModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("afBandwidth")) {
        settings.m_afBandwidth = response.getNfmModSettings()->getAfBandwidth();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getNfmModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("ctcssIndex")) {
        settings.m_ctcssIndex = response.getNfmModSettings()->getCtcssIndex();
    }
    if (channelSettingsKeys.contains("ctcssOn")) {
        settings.m_ctcssOn = response.getNfmModSettings()->getCtcssOn() != 0;
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getNfmModSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getNfmModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("modAFInput")) {
        settings.m_modAFInput = (NFMModSettings::NFMModInputAF) response.getNfmModSettings()->getModAfInput();
    }
    if (channelSettingsKeys.contains("playLoop")) {
        settings.m_playLoop = response.getNfmModSettings()->getPlayLoop() != 0;
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getNfmModSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getNfmModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getNfmModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("toneFrequency")) {
        settings.m_toneFrequency = response.getNfmModSettings()->getToneFrequency();
    }
    if (channelSettingsKeys.contains("volumeFactor")) {
        settings.m_volumeFactor = response.getNfmModSettings()->getVolumeFactor();
    }
    if (channelSettingsKeys.contains("dcsCode")) {
        settings.m_dcsCode = response.getNfmModSettings()->getDcsCode() % 512;
    }
    if (channelSettingsKeys.contains("dcsOn")) {
        settings.m_dcsOn = response.getNfmModSettings()->getDcsOn() != 0;
    }
    if (channelSettingsKeys.contains("dcsPositive")) {
        settings.m_dcsPositive = response.getNfmModSettings()->getDcsPositive() != 0;
    }
    if (channelSettingsKeys.contains("preEmphasisOn")) {
        settings.m_preEmphasisOn = response.getNfmModSettings()->getPreEmphasisOn() != 0;
    }
    if (channelSettingsKeys.contains("bpfOn")) {
        settings.m_bpfOn = response.getNfmModSettings()->getBpfOn() != 0;
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getNfmModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getNfmModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getNfmModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getNfmModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getNfmModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getNfmModSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getNfmModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getNfmModSettings()->getRollupState());
    }
}

int NFMMod::webapiReportGet(
                SWGSDRangel::SWGChannelReport& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setNfmModReport(new SWGSDRangel::SWGNFMModReport());
    response.getNfmModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void NFMMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const NFMModSettings& settings)
{
    response.getNfmModSettings()->setAfBandwidth(settings.m_afBandwidth);
    response.getNfmModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getNfmModSettings()->setCtcssIndex(settings.m_ctcssIndex);
    response.getNfmModSettings()->setCtcssOn(settings.m_ctcssOn ? 1 : 0);
    response.getNfmModSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getNfmModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getNfmModSettings()->setModAfInput((int) settings.m_modAFInput);
    response.getNfmModSettings()->setPlayLoop(settings.m_playLoop ? 1 : 0);
    response.getNfmModSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getNfmModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getNfmModSettings()->getTitle()) {
        *response.getNfmModSettings()->getTitle() = settings.m_title;
    } else {
        response.getNfmModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getNfmModSettings()->setToneFrequency(settings.m_toneFrequency);
    response.getNfmModSettings()->setVolumeFactor(settings.m_volumeFactor);

    if (!response.getNfmModSettings()->getCwKeyer()) {
        response.getNfmModSettings()->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings);
    }

    if (response.getNfmModSettings()->getAudioDeviceName()) {
        *response.getNfmModSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getNfmModSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getNfmModSettings()->setDcsCode(settings.m_dcsCode);
    response.getNfmModSettings()->setDcsOn(settings.m_dcsOn ? 1 : 0);
    response.getNfmModSettings()->setDcsPositive(settings.m_dcsPositive ? 1 : 0);
    response.getNfmModSettings()->setPreEmphasisOn(settings.m_preEmphasisOn ? 1 : 0);
    response.getNfmModSettings()->setBpfOn(settings.m_bpfOn ? 1 : 0);
    response.getNfmModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getNfmModSettings()->getReverseApiAddress()) {
        *response.getNfmModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getNfmModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getNfmModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getNfmModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getNfmModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getNfmModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getNfmModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getNfmModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getNfmModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getNfmModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getNfmModSettings()->setRollupState(swgRollupState);
        }
    }
}

void NFMMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getNfmModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getNfmModReport()->setAudioSampleRate(m_basebandSource->getAudioSampleRate());
    response.getNfmModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void NFMMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const NFMModSettings& settings, bool force)
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

void NFMMod::webapiReverseSendCWSettings(const CWKeyerSettings& cwKeyerSettings)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setChannelType(new QString("NFMMod"));
    swgChannelSettings->setNfmModSettings(new SWGSDRangel::SWGNFMModSettings());
    SWGSDRangel::SWGNFMModSettings *swgNFModSettings = swgChannelSettings->getNfmModSettings();

    swgNFModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgNFModSettings->getCwKeyer();
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

void NFMMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const NFMModSettings& settings,
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

void NFMMod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const NFMModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setNfmModSettings(new SWGSDRangel::SWGNFMModSettings());
    SWGSDRangel::SWGNFMModSettings *swgNFMModSettings = swgChannelSettings->getNfmModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("channelMute") || force) {
        swgNFMModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgNFMModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("modAFInput") || force) {
        swgNFMModSettings->setModAfInput((int) settings.m_modAFInput);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgNFMModSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("playLoop") || force) {
        swgNFMModSettings->setPlayLoop(settings.m_playLoop ? 1 : 0);
    }
    if (channelSettingsKeys.contains("afBandwidth") || force) {
        swgNFMModSettings->setAfBandwidth(settings.m_afBandwidth);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgNFMModSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgNFMModSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgNFMModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgNFMModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("toneFrequency") || force) {
        swgNFMModSettings->setToneFrequency(settings.m_toneFrequency);
    }
    if (channelSettingsKeys.contains("volumeFactor") || force) {
        swgNFMModSettings->setVolumeFactor(settings.m_volumeFactor);
    }
    if (channelSettingsKeys.contains("ctcssOn") || force) {
        swgNFMModSettings->setCtcssOn(settings.m_ctcssOn ? 1 : 0);
    }
    if (channelSettingsKeys.contains("ctcssIndex") || force) {
        swgNFMModSettings->setCtcssIndex(settings.m_ctcssIndex);
    }
    if (channelSettingsKeys.contains("dcsCode") || force) {
        swgNFMModSettings->setDcsCode(settings.m_dcsCode);
    }
    if (channelSettingsKeys.contains("dcsOn") || force) {
        swgNFMModSettings->setDcsOn(settings.m_dcsOn ? 1 : 0);
    }
    if (channelSettingsKeys.contains("dcsPositive") || force) {
        swgNFMModSettings->setDcsPositive(settings.m_dcsPositive ? 1 : 0);
    }
    if (channelSettingsKeys.contains("preEmphasisOn") || force) {
        swgNFMModSettings->setPreEmphasisOn(settings.m_preEmphasisOn ? 1 : 0);
    }
    if (channelSettingsKeys.contains("bpfOn") || force) {
        swgNFMModSettings->setBpfOn(settings.m_bpfOn ? 1 : 0);
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgNFMModSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgNFMModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgNFMModSettings->setRollupState(swgRollupState);
    }

    if (force)
    {
        const CWKeyerSettings& cwKeyerSettings = m_basebandSource->getCWKeyer().getSettings();
        swgNFMModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgNFMModSettings->getCwKeyer();
        m_basebandSource->getCWKeyer().webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);
    }
}

void NFMMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "NFMMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("NFMMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double NFMMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

CWKeyer *NFMMod::getCWKeyer()
{
    return &m_basebandSource->getCWKeyer();
}

void NFMMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t NFMMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

int NFMMod::getAudioSampleRate() const
{
    return m_basebandSource->getAudioSampleRate();
}

int NFMMod::getFeedbackAudioSampleRate() const
{
    return m_basebandSource->getFeedbackAudioSampleRate();
}
