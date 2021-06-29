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

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGSSBModReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/cwkeyer.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

#include "ssbmodbaseband.h"
#include "ssbmod.h"

MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureSSBMod, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgReportFileSourceStreamTiming, Message)

const char* const SSBMod::m_channelIdURI = "sdrangel.channeltx.modssb";
const char* const SSBMod::m_channelId = "SSBMod";

SSBMod::SSBMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF),
	m_settingsMutex(QMutex::Recursive),
	m_fileSize(0),
	m_recordLength(0),
	m_sampleRate(48000)
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new SSBModBaseband();
    m_basebandSource->setSpectrumSink(&m_spectrumVis);
    m_basebandSource->setInputFileStream(&m_ifstream);
    m_basebandSource->setChannel(this);
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    connect(&m_channelMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleChannelMessages()));
}

SSBMod::~SSBMod()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void SSBMod::start()
{
	qDebug("SSBMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void SSBMod::stop()
{
    qDebug("SSBMod::stop");
	m_thread->exit();
	m_thread->wait();
}

void SSBMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool SSBMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureSSBMod::match(cmd))
    {
        MsgConfigureSSBMod& cfg = (MsgConfigureSSBMod&) cmd;
        qDebug() << "SSBMod::handleMessage: MsgConfigureSSBMod";

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
    		samplesCount = m_fileSize / sizeof(Real);
    	} else {
    		samplesCount = m_ifstream.tellg() / sizeof(Real);
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
        qDebug() << "SSBMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "SSBMod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
	else
	{
		return false;
	}
}

void SSBMod::openFileStream()
{
    if (m_ifstream.is_open()) {
        m_ifstream.close();
    }

    m_ifstream.open(m_fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
    m_fileSize = m_ifstream.tellg();
    m_ifstream.seekg(0,std::ios_base::beg);

    m_sampleRate = 48000; // fixed rate
    m_recordLength = m_fileSize / (sizeof(Real) * m_sampleRate);

    qDebug() << "SSBMod::openFileStream: " << m_fileName.toStdString().c_str()
            << " fileSize: " << m_fileSize << "bytes"
            << " length: " << m_recordLength << " seconds";

    if (getMessageQueueToGUI())
    {
        MsgReportFileSourceStreamData *report;
        report = MsgReportFileSourceStreamData::create(m_sampleRate, m_recordLength);
        getMessageQueueToGUI()->push(report);
    }
}

void SSBMod::seekFileStream(int seekPercentage)
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

void SSBMod::applySettings(const SSBModSettings& settings, bool force)
{
    float band = settings.m_bandwidth;
    float lowCutoff = settings.m_lowCutoff;
    bool usb = settings.m_usb;
    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_bandwidth != m_settings.m_bandwidth) || force) {
        reverseAPIKeys.append("bandwidth");
    }
    if ((settings.m_lowCutoff != m_settings.m_lowCutoff) || force) {
        reverseAPIKeys.append("lowCutoff");
    }
    if ((settings.m_usb != m_settings.m_usb) || force) {
        reverseAPIKeys.append("usb");
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
    if ((settings.m_audioBinaural != m_settings.m_audioBinaural) || force) {
        reverseAPIKeys.append("audioBinaural");
    }
    if ((settings.m_audioFlipChannels != m_settings.m_audioFlipChannels) || force) {
        reverseAPIKeys.append("audioFlipChannels");
    }
    if ((settings.m_dsb != m_settings.m_dsb) || force) {
        reverseAPIKeys.append("dsb");
    }
    if ((settings.m_audioMute != m_settings.m_audioMute) || force) {
        reverseAPIKeys.append("audioMute");
    }
    if ((settings.m_playLoop != m_settings.m_playLoop) || force) {
        reverseAPIKeys.append("playLoop");
    }
    if ((settings.m_agc != m_settings.m_agc) || force) {
        reverseAPIKeys.append("agc");
    }
    if ((settings.m_cmpPreGainDB != m_settings.m_cmpPreGainDB) || force) {
        reverseAPIKeys.append("cmpPreGainDB");
    }
    if ((settings.m_cmpThresholdDB != m_settings.m_cmpThresholdDB) || force) {
        reverseAPIKeys.append("cmpThresholdDB");
    }
    if ((settings.m_rgbColor != m_settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((settings.m_title != m_settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((settings.m_modAFInput != m_settings.m_modAFInput) || force) {
        reverseAPIKeys.append("modAFInput");
    }
    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force) {
        reverseAPIKeys.append("audioDeviceName");
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

    if ((settings.m_dsb != m_settings.m_dsb)
     || (settings.m_usb != m_settings.m_usb) || force)
    {
        SpectrumSettings spectrumSettings = m_spectrumVis.getSettings();
        spectrumSettings.m_ssb = !settings.m_dsb;
        spectrumSettings.m_usb = settings.m_usb;
        SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
        m_spectrumVis.getInputMessageQueue()->push(msg);
    }

    SSBModBaseband::MsgConfigureSSBModBaseband *msg = SSBModBaseband::MsgConfigureSSBModBaseband::create(settings, force);
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
    m_settings.m_bandwidth = band;
    m_settings.m_lowCutoff = lowCutoff;
    m_settings.m_usb = usb;
}

QByteArray SSBMod::serialize() const
{
    return m_settings.serialize();
}

bool SSBMod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureSSBMod *msg = MsgConfigureSSBMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureSSBMod *msg = MsgConfigureSSBMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void SSBMod::sendSampleRateToDemodAnalyzer()
{
    QList<MessageQueue*> *messageQueues = MainCore::instance()->getMessagePipes().getMessageQueues(this, "reportdemod");

    if (messageQueues)
    {
        QList<MessageQueue*>::iterator it = messageQueues->begin();

        for (; it != messageQueues->end(); ++it)
        {
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(
                this,
                getAudioSampleRate()
            );
            (*it)->push(msg);
        }
    }
}

int SSBMod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setSsbModSettings(new SWGSDRangel::SWGSSBModSettings());
    response.getSsbModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getSsbModSettings()->getCwKeyer();
    const CWKeyerSettings& cwKeyerSettings = m_basebandSource->getCWKeyer().getSettings();
    CWKeyer::webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);

    return 200;
}

int SSBMod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    SSBModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    if (channelSettingsKeys.contains("cwKeyer"))
    {
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getSsbModSettings()->getCwKeyer();
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

    MsgConfigureSSBMod *msg = MsgConfigureSSBMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSSBMod *msgToGUI = MsgConfigureSSBMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void SSBMod::webapiUpdateChannelSettings(
        SSBModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getSsbModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = response.getSsbModSettings()->getBandwidth();
    }
    if (channelSettingsKeys.contains("lowCutoff")) {
        settings.m_lowCutoff = response.getSsbModSettings()->getLowCutoff();
    }
    if (channelSettingsKeys.contains("usb")) {
        settings.m_usb = response.getSsbModSettings()->getUsb() != 0;
    }
    if (channelSettingsKeys.contains("toneFrequency")) {
        settings.m_toneFrequency = response.getSsbModSettings()->getToneFrequency();
    }
    if (channelSettingsKeys.contains("volumeFactor")) {
        settings.m_volumeFactor = response.getSsbModSettings()->getVolumeFactor();
    }
    if (channelSettingsKeys.contains("spanLog2")) {
        settings.m_spanLog2 = response.getSsbModSettings()->getSpanLog2();
    }
    if (channelSettingsKeys.contains("audioBinaural")) {
        settings.m_audioBinaural = response.getSsbModSettings()->getAudioBinaural() != 0;
    }
    if (channelSettingsKeys.contains("audioFlipChannels")) {
        settings.m_audioFlipChannels = response.getSsbModSettings()->getAudioFlipChannels() != 0;
    }
    if (channelSettingsKeys.contains("dsb")) {
        settings.m_dsb = response.getSsbModSettings()->getDsb() != 0;
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getSsbModSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("playLoop")) {
        settings.m_playLoop = response.getSsbModSettings()->getPlayLoop() != 0;
    }
    if (channelSettingsKeys.contains("agc")) {
        settings.m_agc = response.getSsbModSettings()->getAgc() != 0;
    }
    if (channelSettingsKeys.contains("cmpPreGainDB")) {
        settings.m_cmpPreGainDB = response.getSsbModSettings()->getCmpPreGainDb();
    }
    if (channelSettingsKeys.contains("cmpThresholdDB")) {
        settings.m_cmpThresholdDB = response.getSsbModSettings()->getCmpThresholdDb();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getSsbModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getSsbModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("modAFInput")) {
        settings.m_modAFInput = (SSBModSettings::SSBModInputAF) response.getSsbModSettings()->getModAfInput();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getSsbModSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getSsbModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getSsbModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getSsbModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getSsbModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getSsbModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getSsbModSettings()->getReverseApiChannelIndex();
    }
}

int SSBMod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setSsbModReport(new SWGSDRangel::SWGSSBModReport());
    response.getSsbModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void SSBMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const SSBModSettings& settings)
{
    response.getSsbModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getSsbModSettings()->setBandwidth(settings.m_bandwidth);
    response.getSsbModSettings()->setLowCutoff(settings.m_lowCutoff);
    response.getSsbModSettings()->setUsb(settings.m_usb ? 1 : 0);
    response.getSsbModSettings()->setToneFrequency(settings.m_toneFrequency);
    response.getSsbModSettings()->setVolumeFactor(settings.m_volumeFactor);
    response.getSsbModSettings()->setSpanLog2(settings.m_spanLog2);
    response.getSsbModSettings()->setAudioBinaural(settings.m_audioBinaural ? 1 : 0);
    response.getSsbModSettings()->setAudioFlipChannels(settings.m_audioFlipChannels ? 1 : 0);
    response.getSsbModSettings()->setDsb(settings.m_dsb ? 1 : 0);
    response.getSsbModSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getSsbModSettings()->setPlayLoop(settings.m_playLoop ? 1 : 0);
    response.getSsbModSettings()->setAgc(settings.m_agc ? 1 : 0);
    response.getSsbModSettings()->setCmpPreGainDb(settings.m_cmpPreGainDB);
    response.getSsbModSettings()->setCmpThresholdDb(settings.m_cmpThresholdDB);
    response.getSsbModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getSsbModSettings()->getTitle()) {
        *response.getSsbModSettings()->getTitle() = settings.m_title;
    } else {
        response.getSsbModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getSsbModSettings()->setModAfInput((int) settings.m_modAFInput);

    if (response.getSsbModSettings()->getAudioDeviceName()) {
        *response.getSsbModSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getSsbModSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    if (!response.getSsbModSettings()->getCwKeyer()) {
        response.getSsbModSettings()->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings);
    }

    response.getSsbModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getSsbModSettings()->getReverseApiAddress()) {
        *response.getSsbModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getSsbModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getSsbModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getSsbModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getSsbModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void SSBMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getSsbModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getSsbModReport()->setAudioSampleRate(m_basebandSource->getAudioSampleRate());
    response.getSsbModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void SSBMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const SSBModSettings& settings, bool force)
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

void SSBMod::webapiReverseSendCWSettings(const CWKeyerSettings& cwKeyerSettings)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setChannelType(new QString("SSBMod"));
    swgChannelSettings->setSsbModSettings(new SWGSDRangel::SWGSSBModSettings());
    SWGSDRangel::SWGSSBModSettings *swgSSBModSettings = swgChannelSettings->getSsbModSettings();

    swgSSBModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgSSBModSettings->getCwKeyer();
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

void SSBMod::sendChannelSettings(
    QList<MessageQueue*> *messageQueues,
    QList<QString>& channelSettingsKeys,
    const SSBModSettings& settings,
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

void SSBMod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const SSBModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setSsbModSettings(new SWGSDRangel::SWGSSBModSettings());
    SWGSDRangel::SWGSSBModSettings *swgSSBModSettings = swgChannelSettings->getSsbModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgSSBModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("bandwidth") || force) {
        swgSSBModSettings->setBandwidth(settings.m_bandwidth);
    }
    if (channelSettingsKeys.contains("lowCutoff") || force) {
        swgSSBModSettings->setLowCutoff(settings.m_lowCutoff);
    }
    if (channelSettingsKeys.contains("usb") || force) {
        swgSSBModSettings->setUsb(settings.m_usb ? 1 : 0);
    }
    if (channelSettingsKeys.contains("toneFrequency") || force) {
        swgSSBModSettings->setToneFrequency(settings.m_toneFrequency);
    }
    if (channelSettingsKeys.contains("volumeFactor") || force) {
        swgSSBModSettings->setVolumeFactor(settings.m_volumeFactor);
    }
    if (channelSettingsKeys.contains("spanLog2") || force) {
        swgSSBModSettings->setSpanLog2(settings.m_spanLog2);
    }
    if (channelSettingsKeys.contains("audioBinaural") || force) {
        swgSSBModSettings->setAudioBinaural(settings.m_audioBinaural ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioFlipChannels") || force) {
        swgSSBModSettings->setAudioFlipChannels(settings.m_audioFlipChannels ? 1 : 0);
    }
    if (channelSettingsKeys.contains("dsb") || force) {
        swgSSBModSettings->setDsb(settings.m_dsb ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgSSBModSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("playLoop") || force) {
        swgSSBModSettings->setPlayLoop(settings.m_playLoop ? 1 : 0);
    }
    if (channelSettingsKeys.contains("agc") || force) {
        swgSSBModSettings->setAgc(settings.m_agc ? 1 : 0);
    }
    if (channelSettingsKeys.contains("cmpPreGainDB") || force) {
        swgSSBModSettings->setCmpPreGainDb(settings.m_cmpPreGainDB);
    }
    if (channelSettingsKeys.contains("cmpThresholdDB") || force) {
        swgSSBModSettings->setCmpThresholdDb(settings.m_cmpThresholdDB);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgSSBModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgSSBModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("modAFInput") || force) {
        swgSSBModSettings->setModAfInput((int) settings.m_modAFInput);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgSSBModSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgSSBModSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (force)
    {
        const CWKeyerSettings& cwKeyerSettings = m_basebandSource->getCWKeyer().getSettings();
        swgSSBModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgSSBModSettings->getCwKeyer();
        m_basebandSource->getCWKeyer().webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);
    }
}

void SSBMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "SSBMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("SSBMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double SSBMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

CWKeyer *SSBMod::getCWKeyer()
{
    return &m_basebandSource->getCWKeyer();
}

void SSBMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

int SSBMod::getAudioSampleRate() const
{
    return m_basebandSource->getAudioSampleRate();
}

int SSBMod::getFeedbackAudioSampleRate() const
{
    return m_basebandSource->getFeedbackAudioSampleRate();
}

uint32_t SSBMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

void SSBMod::handleChannelMessages()
{
	Message* message;

	while ((message = m_channelMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}
