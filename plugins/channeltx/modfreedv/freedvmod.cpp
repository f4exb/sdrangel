///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGFreeDVModReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

#include "freedvmodbaseband.h"
#include "freedvmod.h"

MESSAGE_CLASS_DEFINITION(FreeDVMod::MsgConfigureFreeDVMod, Message)
MESSAGE_CLASS_DEFINITION(FreeDVMod::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(FreeDVMod::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(FreeDVMod::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FreeDVMod::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(FreeDVMod::MsgReportFileSourceStreamTiming, Message)

const char* const FreeDVMod::m_channelIdURI = "sdrangel.channeltx.freedvmod";
const char* const FreeDVMod::m_channelId = "FreeDVMod";

FreeDVMod::FreeDVMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF),
	m_settingsMutex(QMutex::Recursive),
	m_fileSize(0),
	m_recordLength(0),
	m_fileSampleRate(8000) // all modes take 8000 S/s input
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new FreeDVModBaseband();
    m_basebandSource->setSpectrumSampleSink(&m_spectrumVis);
    m_basebandSource->setInputFileStream(&m_ifstream);
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &FreeDVMod::networkManagerFinished
    );
}

FreeDVMod::~FreeDVMod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &FreeDVMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void FreeDVMod::setDeviceAPI(DeviceAPI *deviceAPI)
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

void FreeDVMod::start()
{
	qDebug("FreeDVMod::start");
    m_basebandSource->reset();
    m_thread->start();

    SpectrumSettings spectrumSettings = m_spectrumVis.getSettings();
    spectrumSettings.m_ssb = true;
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis.getInputMessageQueue()->push(msg);
}

void FreeDVMod::stop()
{
    qDebug("FreeDVMod::stop");
	m_thread->exit();
	m_thread->wait();
}

void FreeDVMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

void FreeDVMod::setCenterFrequency(qint64 frequency)
{
    FreeDVModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureFreeDVMod *msgToGUI = MsgConfigureFreeDVMod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

bool FreeDVMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureFreeDVMod::match(cmd))
    {
        MsgConfigureFreeDVMod& cfg = (MsgConfigureFreeDVMod&) cmd;
        qDebug() << "FreeDVMod::handleMessage: MsgConfigureFreeDVMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
	else if (MsgConfigureFileSourceName::match(cmd))
    {
        MsgConfigureFileSourceName& conf = (MsgConfigureFileSourceName&) cmd;
        m_fileName = conf.getFileName();
        openFileStream();
        return true;
    }
    else if (MsgConfigureFileSourceSeek::match(cmd))
    {
        MsgConfigureFileSourceSeek& conf = (MsgConfigureFileSourceSeek&) cmd;
        int seekPercentage = conf.getPercentage();
        seekFileStream(seekPercentage);

        return true;
    }
    else if (MsgConfigureFileSourceStreamTiming::match(cmd))
    {
    	std::size_t samplesCount;

    	if (m_ifstream.eof()) {
    		samplesCount = m_fileSize / sizeof(int16_t);
    	} else {
    		samplesCount = m_ifstream.tellg() / sizeof(int16_t);
    	}

        if (getMessageQueueToGUI())
        {
            MsgReportFileSourceStreamTiming *report;
            report = MsgReportFileSourceStreamTiming::create(samplesCount);
            getMessageQueueToGUI()->push(report);
        }

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
        qDebug() << "FreeDVMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
	else
	{
		return false;
	}
}

void FreeDVMod::openFileStream()
{
    if (m_ifstream.is_open()) {
        m_ifstream.close();
    }

    m_ifstream.open(m_fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
    m_fileSize = m_ifstream.tellg();
    m_ifstream.seekg(0,std::ios_base::beg);

    m_recordLength = m_fileSize / (sizeof(int16_t) * m_fileSampleRate);

    qDebug() << "FreeDVMod::openFileStream: " << m_fileName.toStdString().c_str()
            << " fileSize: " << m_fileSize << "bytes"
            << " length: " << m_recordLength << " seconds";

    if (getMessageQueueToGUI())
    {
        MsgReportFileSourceStreamData *report;
        report = MsgReportFileSourceStreamData::create(m_fileSampleRate, m_recordLength);
        getMessageQueueToGUI()->push(report);
    }
}

void FreeDVMod::seekFileStream(int seekPercentage)
{
    QMutexLocker mutexLocker(&m_settingsMutex);

    if (m_ifstream.is_open())
    {
        int seekPoint = ((m_recordLength * seekPercentage) / 100) * m_fileSampleRate;
        seekPoint *= sizeof(Real);
        m_ifstream.clear();
        m_ifstream.seekg(seekPoint, std::ios::beg);
    }
}

void FreeDVMod::applySettings(const FreeDVModSettings& settings, bool force)
{
    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_toneFrequency != m_settings.m_toneFrequency) || force) {
        reverseAPIKeys.append("toneFrequency");
    }
    if ((settings.m_volumeFactor != m_settings.m_volumeFactor) || force) {
        reverseAPIKeys.append("volumeFactor");
    }
    if ((settings.m_spanLog2 != m_settings.m_spanLog2) || force) {
        reverseAPIKeys.append("spanLog2");
    }
    if ((settings.m_audioMute != m_settings.m_audioMute) || force) {
        reverseAPIKeys.append("audioMute");
    }
    if ((settings.m_playLoop != m_settings.m_playLoop) || force) {
        reverseAPIKeys.append("playLoop");
    }
    if ((settings.m_playLoop != m_settings.m_gaugeInputElseModem) || force) {
        reverseAPIKeys.append("gaugeInputElseModem");
    }
    if ((settings.m_rgbColor != m_settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((settings.m_title != m_settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((settings.m_freeDVMode != m_settings.m_freeDVMode) || force) {
        reverseAPIKeys.append("freeDVMode");
    }
    if ((settings.m_modAFInput != m_settings.m_modAFInput) || force) {
        reverseAPIKeys.append("modAFInput");
    }
    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force) {
        reverseAPIKeys.append("audioDeviceName");
    }
    if ((settings.m_toneFrequency != m_settings.m_toneFrequency) || force) {
        reverseAPIKeys.append("toneFrequency");
    }
    if ((m_settings.m_freeDVMode != settings.m_freeDVMode) || force) {
        reverseAPIKeys.append("freeDVMode");
    }
    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force) {
        reverseAPIKeys.append("audioDeviceName");
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

    if ((settings.m_freeDVMode != m_settings.m_freeDVMode)
     || (settings.m_spanLog2 != m_settings.m_spanLog2) || force)
    {
        DSPSignalNotification *msg = new DSPSignalNotification(
            FreeDVModSettings::getModSampleRate(settings.m_freeDVMode)/(1<<settings.m_spanLog2),
            0);
        m_spectrumVis.getInputMessageQueue()->push(msg);
    }

    FreeDVModBaseband::MsgConfigureFreeDVModBaseband *msg = FreeDVModBaseband::MsgConfigureFreeDVModBaseband::create(settings, force);
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

QByteArray FreeDVMod::serialize() const
{
    return m_settings.serialize();
}

bool FreeDVMod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureFreeDVMod *msg = MsgConfigureFreeDVMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureFreeDVMod *msg = MsgConfigureFreeDVMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int FreeDVMod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFreeDvModSettings(new SWGSDRangel::SWGFreeDVModSettings());
    response.getFreeDvModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getFreeDvModSettings()->getCwKeyer();
    const CWKeyerSettings& cwKeyerSettings = m_basebandSource->getCWKeyer().getSettings();
    CWKeyer::webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);

    return 200;
}

int FreeDVMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int FreeDVMod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    FreeDVModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    if (channelSettingsKeys.contains("cwKeyer"))
    {
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getFreeDvModSettings()->getCwKeyer();
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

    MsgConfigureFreeDVMod *msg = MsgConfigureFreeDVMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureFreeDVMod *msgToGUI = MsgConfigureFreeDVMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void FreeDVMod::webapiUpdateChannelSettings(
        FreeDVModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getFreeDvModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("toneFrequency")) {
        settings.m_toneFrequency = response.getFreeDvModSettings()->getToneFrequency();
    }
    if (channelSettingsKeys.contains("volumeFactor")) {
        settings.m_volumeFactor = response.getFreeDvModSettings()->getVolumeFactor();
    }
    if (channelSettingsKeys.contains("spanLog2")) {
        settings.m_spanLog2 = response.getFreeDvModSettings()->getSpanLog2();
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getFreeDvModSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("playLoop")) {
        settings.m_playLoop = response.getFreeDvModSettings()->getPlayLoop() != 0;
    }
    if (channelSettingsKeys.contains("gaugeInputElseModem")) {
        settings.m_gaugeInputElseModem = response.getFreeDvModSettings()->getGaugeInputElseModem() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getFreeDvModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getFreeDvModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("freeDVMode")) {
        settings.m_freeDVMode = (FreeDVModSettings::FreeDVMode) response.getFreeDvModSettings()->getFreeDvMode();
    }
    if (channelSettingsKeys.contains("modAFInput")) {
        settings.m_modAFInput = (FreeDVModSettings::FreeDVModInputAF) response.getFreeDvModSettings()->getModAfInput();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getFreeDvModSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getFreeDvModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getFreeDvModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getFreeDvModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getFreeDvModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getFreeDvModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getFreeDvModSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_spectrumGUI && channelSettingsKeys.contains("spectrumConfig")) {
        settings.m_spectrumGUI->updateFrom(channelSettingsKeys, response.getFreeDvModSettings()->getSpectrumConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getFreeDvModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getFreeDvModSettings()->getRollupState());
    }
}

int FreeDVMod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFreeDvModReport(new SWGSDRangel::SWGFreeDVModReport());
    response.getFreeDvModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void FreeDVMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const FreeDVModSettings& settings)
{
    response.getFreeDvModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getFreeDvModSettings()->setToneFrequency(settings.m_toneFrequency);
    response.getFreeDvModSettings()->setVolumeFactor(settings.m_volumeFactor);
    response.getFreeDvModSettings()->setSpanLog2(settings.m_spanLog2);
    response.getFreeDvModSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getFreeDvModSettings()->setPlayLoop(settings.m_playLoop ? 1 : 0);
    response.getFreeDvModSettings()->setRgbColor(settings.m_rgbColor);
    response.getFreeDvModSettings()->setGaugeInputElseModem(settings.m_gaugeInputElseModem ? 1 : 0);

    if (response.getFreeDvModSettings()->getTitle()) {
        *response.getFreeDvModSettings()->getTitle() = settings.m_title;
    } else {
        response.getFreeDvModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getFreeDvModSettings()->setModAfInput((int) settings.m_modAFInput);
    response.getFreeDvModSettings()->setFreeDvMode((int) settings.m_freeDVMode);

    if (response.getFreeDvModSettings()->getAudioDeviceName()) {
        *response.getFreeDvModSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getFreeDvModSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    if (!response.getFreeDvModSettings()->getCwKeyer()) {
        response.getFreeDvModSettings()->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings);
    }

    response.getFreeDvModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getFreeDvModSettings()->getReverseApiAddress()) {
        *response.getFreeDvModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getFreeDvModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getFreeDvModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getFreeDvModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getFreeDvModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_spectrumGUI)
    {
        if (response.getFreeDvModSettings()->getSpectrumConfig())
        {
            settings.m_spectrumGUI->formatTo(response.getFreeDvModSettings()->getSpectrumConfig());
        }
        else
        {
            SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
            settings.m_spectrumGUI->formatTo(swgGLSpectrum);
            response.getFreeDvModSettings()->setSpectrumConfig(swgGLSpectrum);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getFreeDvModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getFreeDvModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getFreeDvModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getFreeDvModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getFreeDvModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getFreeDvModSettings()->setRollupState(swgRollupState);
        }
    }
}

void FreeDVMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getFreeDvModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getFreeDvModReport()->setAudioSampleRate(m_basebandSource->getAudioSampleRate());
    response.getFreeDvModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void FreeDVMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const FreeDVModSettings& settings, bool force)
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

void FreeDVMod::webapiReverseSendCWSettings(const CWKeyerSettings& cwKeyerSettings)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setChannelType(new QString("FreeDVMod"));
    swgChannelSettings->setFreeDvModSettings(new SWGSDRangel::SWGFreeDVModSettings());
    SWGSDRangel::SWGFreeDVModSettings *swgFreeDVModSettings = swgChannelSettings->getFreeDvModSettings();

    swgFreeDVModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgFreeDVModSettings->getCwKeyer();
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

void FreeDVMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const FreeDVModSettings& settings,
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

void FreeDVMod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const FreeDVModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setFreeDvModSettings(new SWGSDRangel::SWGFreeDVModSettings());
    SWGSDRangel::SWGFreeDVModSettings *swgFreeDVModSettings = swgChannelSettings->getFreeDvModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgFreeDVModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("toneFrequency") || force) {
        swgFreeDVModSettings->setToneFrequency(settings.m_toneFrequency);
    }
    if (channelSettingsKeys.contains("volumeFactor") || force) {
        swgFreeDVModSettings->setVolumeFactor(settings.m_volumeFactor);
    }
    if (channelSettingsKeys.contains("spanLog2") || force) {
        swgFreeDVModSettings->setSpanLog2(settings.m_spanLog2);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgFreeDVModSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("playLoop") || force) {
        swgFreeDVModSettings->setPlayLoop(settings.m_playLoop ? 1 : 0);
    }
    if (channelSettingsKeys.contains("gaugeInputElseModem") || force) {
        swgFreeDVModSettings->setPlayLoop(settings.m_gaugeInputElseModem ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgFreeDVModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgFreeDVModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("freeDVMode") || force) {
        swgFreeDVModSettings->setFreeDvMode((int) settings.m_freeDVMode);
    }
    if (channelSettingsKeys.contains("modAFInput") || force) {
        swgFreeDVModSettings->setModAfInput((int) settings.m_modAFInput);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgFreeDVModSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgFreeDVModSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_spectrumGUI && (channelSettingsKeys.contains("spectrunConfig") || force))
    {
        SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
        settings.m_spectrumGUI->formatTo(swgGLSpectrum);
        swgFreeDVModSettings->setSpectrumConfig(swgGLSpectrum);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgFreeDVModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgFreeDVModSettings->setRollupState(swgRollupState);
    }

    if (force)
    {
        const CWKeyerSettings& cwKeyerSettings = m_basebandSource->getCWKeyer().getSettings();
        swgFreeDVModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgFreeDVModSettings->getCwKeyer();
        m_basebandSource->getCWKeyer().webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);
    }
}

void FreeDVMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "FreeDVMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("FreeDVMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

int FreeDVMod::getAudioSampleRate() const
{
    return m_basebandSource->getAudioSampleRate();
}

uint32_t FreeDVMod::getModemSampleRate() const
{
    return m_basebandSource->getModemSampleRate();
}

Real FreeDVMod::getLowCutoff() const
{
    return m_basebandSource->getLowCutoff();
}

Real FreeDVMod::getHiCutoff() const
{
    return m_basebandSource->getHiCutoff();
}

double FreeDVMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

CWKeyer *FreeDVMod::getCWKeyer()
{
    return &m_basebandSource->getCWKeyer();
}

void FreeDVMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t FreeDVMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}
