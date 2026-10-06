///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include <algorithm>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "util/db.h"
#include "maincore.h"

#include "bfmmodbaseband.h"
#include "bfmmod.h"

MESSAGE_CLASS_DEFINITION(BFMMod::MsgConfigureBFMMod, Message)
MESSAGE_CLASS_DEFINITION(BFMMod::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(BFMMod::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(BFMMod::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(BFMMod::MsgReportFileSourceStreamTiming, Message)

const char* const BFMMod::m_channelIdURI = "sdrangel.channeltx.modbfm";
const char* const BFMMod::m_channelId = "BFMMod";

BFMMod::BFMMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
	m_recordLength(0),
	m_sampleRate(48000)
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_thread->setObjectName(m_channelId);
    m_basebandSource = new BFMModBaseband();
    m_basebandSource->setInputFileReader(&m_audioFileReader);
    m_basebandSource->setChannel(this);
    m_basebandSource->moveToThread(m_thread);

    applySettings(QStringList(), m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &BFMMod::networkManagerFinished
    );
}

BFMMod::~BFMMod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &BFMMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this, true);
    stop();
    delete m_basebandSource;
    delete m_thread;
}

void BFMMod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSourceAPI(this);
        m_deviceAPI->removeChannelSource(this, false);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSource(this);
        m_deviceAPI->addChannelSourceAPI(this);
    }
}

void BFMMod::start()
{
	qDebug("BFMMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void BFMMod::stop()
{
    qDebug("BFMMod::stop");
	m_thread->exit();
	m_thread->wait();
}

void BFMMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

void BFMMod::setCenterFrequency(qint64 frequency)
{
    BFMModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList("inputFrequencyOffset"), settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureBFMMod *msgToGUI = MsgConfigureBFMMod::create(QStringList("inputFrequencyOffset"), settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

bool BFMMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureBFMMod::match(cmd))
    {
        MsgConfigureBFMMod& cfg = (MsgConfigureBFMMod&) cmd;
        qDebug() << "BFMMod::handleMessage: MsgConfigureBFMMod";

        BFMModSettings settings = cfg.getSettings();

        applySettings(cfg.getSettingsKeys(), settings, cfg.getForce());

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

        samplesCount = m_audioFileReader.getPosition();

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
        qDebug() << "BFMMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "BFMMod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
	else
	{
		return false;
	}
}

void BFMMod::openFileStream(const QString& fileName)
{
    if (fileName.isEmpty())
    {
        m_audioFileReader.close();
        m_sampleRate = 0;
        m_recordLength = 0;
    }
    else if (m_audioFileReader.open(fileName))
    {
        m_sampleRate = m_audioFileReader.getSampleRate();
        m_recordLength = m_sampleRate > 0 ?
            static_cast<quint32>(m_audioFileReader.getFrameCount() / m_sampleRate) : 0;

        qDebug() << "BFMMod::openFileStream:" << fileName
                << "sampleRate:" << m_sampleRate
                << "channels:" << m_audioFileReader.getChannelCount()
                << "frames:" << m_audioFileReader.getFrameCount()
                << "length:" << m_recordLength << "seconds";

        BFMModBaseband::MsgConfigureFileSampleRate *sampleRateMessage =
            BFMModBaseband::MsgConfigureFileSampleRate::create(m_sampleRate);
        m_basebandSource->getInputMessageQueue()->push(sampleRateMessage);
    }
    else
    {
        qWarning() << "BFMMod::openFileStream:" << fileName
                   << m_audioFileReader.getErrorString();
        m_sampleRate = 0;
        m_recordLength = 0;
    }

    if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(MsgReportFileSourceStreamData::create(m_sampleRate, m_recordLength));
    }
}

void BFMMod::seekFileStream(int seekPercentage)
{
    if (m_audioFileReader.isOpen())
    {
        const quint64 seekFrame = (m_audioFileReader.getFrameCount() * seekPercentage) / 100;
        m_audioFileReader.seek(seekFrame);
    }
}

void BFMMod::applySettings(const QStringList& settingsKeys, const BFMModSettings& settings, bool force)
{
    qDebug() << "BFMMod::applySettings:" << settings.getDebugString(settingsKeys, force);

    if (settingsKeys.contains("streamIndex") && m_settings.m_streamIndex != settings.m_streamIndex)
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSourceAPI(this);
            m_deviceAPI->removeChannelSource(this, false, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSource(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSourceAPI(this);
            m_settings.m_streamIndex = settings.m_streamIndex; // make sure ChannelAPI::getStreamIndex() is consistent
            emit streamIndexChanged(settings.m_streamIndex);
        }
    }

    BFMModBaseband::MsgConfigureBFMModBaseband *msg = BFMModBaseband::MsgConfigureBFMModBaseband::create(settingsKeys, settings, force);
    m_basebandSource->getInputMessageQueue()->push(msg);

    // Open the file when it changes, including when settings are loaded from a preset
    if ((settingsKeys.contains("fileName") && (settings.m_fileName != m_settings.m_fileName)) || force) {
        openFileStream(settings.m_fileName);
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((settingsKeys.contains("useReverseAPI") && m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (settingsKeys.contains("reverseAPIAddress") && m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (settingsKeys.contains("reverseAPIPort") && m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (settingsKeys.contains("reverseAPIDeviceIndex") && m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (settingsKeys.contains("reverseAPIChannelIndex") && m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, settingsKeys, settings, force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

QByteArray BFMMod::serialize() const
{
    return m_settings.serialize();
}

bool BFMMod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureBFMMod *msg = MsgConfigureBFMMod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureBFMMod *msg = MsgConfigureBFMMod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void BFMMod::sendSampleRateToDemodAnalyzer()
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
                m_basebandSource->getChannelSampleRate()
            );
            messageQueue->push(msg);
        }
    }
}

int BFMMod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setBfmModSettings(new SWGSDRangel::SWGBFMModSettings());
    response.getBfmModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getBfmModSettings()->getCwKeyer();
    const CWKeyerSettings& cwKeyerSettings = m_basebandSource->getCWKeyer().getSettings();
    CWKeyer::webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);

    return 200;
}

int BFMMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int BFMMod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    BFMModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    if (channelSettingsKeys.contains("cwKeyer"))
    {
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getBfmModSettings()->getCwKeyer();
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

    MsgConfigureBFMMod *msg = MsgConfigureBFMMod::create(channelSettingsKeys, settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureBFMMod *msgToGUI = MsgConfigureBFMMod::create(channelSettingsKeys, settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void BFMMod::webapiUpdateChannelSettings(
        BFMModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getBfmModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getBfmModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("modAFInput")) {
        settings.m_modAFInput = (BFMModSettings::BFMModInputAF) response.getBfmModSettings()->getModAfInput();
    }
    if (channelSettingsKeys.contains("playLoop")) {
        settings.m_playLoop = response.getBfmModSettings()->getPlayLoop() != 0;
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = BFMModSettings::boundRFBandwidth(response.getBfmModSettings()->getRfBandwidth());
    }
    if (channelSettingsKeys.contains("afBandwidth")) {
        settings.m_afBandwidth = BFMModSettings::boundAFBandwidth(response.getBfmModSettings()->getAfBandwidth());
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getBfmModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getBfmModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("toneFrequency")) {
        settings.m_toneFrequency = BFMModSettings::boundToneFrequency(response.getBfmModSettings()->getToneFrequency());
    }
    if (channelSettingsKeys.contains("volumeFactor")) {
        settings.m_volumeFactor = BFMModSettings::boundVolumeFactor(response.getBfmModSettings()->getVolumeFactor());
    }
    if (channelSettingsKeys.contains("audioStereo")) {
        settings.m_audioStereo = response.getBfmModSettings()->getAudioStereo() != 0;
    }
    if (channelSettingsKeys.contains("preEmphasis")) {
        settings.m_preEmphasis = qBound(0, response.getBfmModSettings()->getPreEmphasis(), 2);
    }
    if (channelSettingsKeys.contains("pilotLevel")) {
        settings.m_pilotLevel = qBound(0.0f, response.getBfmModSettings()->getPilotLevel(), 0.2f);
    }
    if (channelSettingsKeys.contains("rdsActive")) {
        settings.m_rdsActive = response.getBfmModSettings()->getRdsActive() != 0;
    }
    if (channelSettingsKeys.contains("rdsLevel")) {
        settings.m_rdsLevel = qBound(0.0f, response.getBfmModSettings()->getRdsLevel(), 0.1f);
    }
    if (channelSettingsKeys.contains("rdsPI")) {
        settings.m_rdsPI = quint16(qBound(0, response.getBfmModSettings()->getRdsPi(), 0xffff));
    }
    if (channelSettingsKeys.contains("rdsPTY")) {
        settings.m_rdsPTY = quint8(qBound(0, response.getBfmModSettings()->getRdsPty(), 31));
    }
    if (channelSettingsKeys.contains("rdsPS") && response.getBfmModSettings()->getRdsPs()) {
        settings.m_rdsPS = response.getBfmModSettings()->getRdsPs()->left(8);
    }
    if (channelSettingsKeys.contains("rdsRadioText") && response.getBfmModSettings()->getRdsRadioText()) {
        settings.m_rdsRadioText = response.getBfmModSettings()->getRdsRadioText()->left(64);
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = BFMModSettings::boundFMDeviation(response.getBfmModSettings()->getFmDeviation());
    }
    if (channelSettingsKeys.contains("audioDeviceName") && response.getBfmModSettings()->getAudioDeviceName()) {
        settings.m_audioDeviceName = *response.getBfmModSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("feedbackAudioDeviceName") && response.getBfmModSettings()->getFeedbackAudioDeviceName()) {
        settings.m_feedbackAudioDeviceName = *response.getBfmModSettings()->getFeedbackAudioDeviceName();
    }
    if (channelSettingsKeys.contains("feedbackVolumeFactor")) {
        settings.m_feedbackVolumeFactor = BFMModSettings::boundFeedbackVolumeFactor(response.getBfmModSettings()->getFeedbackVolumeFactor());
    }
    if (channelSettingsKeys.contains("fileName") && response.getBfmModSettings()->getFileName()) {
        settings.m_fileName = *response.getBfmModSettings()->getFileName();
    }
    if (channelSettingsKeys.contains("feedbackAudioEnable")) {
        settings.m_feedbackAudioEnable = response.getBfmModSettings()->getFeedbackAudioEnable() != 0;
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getBfmModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getBfmModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getBfmModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getBfmModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getBfmModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getBfmModSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getBfmModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getBfmModSettings()->getRollupState());
    }
}

int BFMMod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setBfmModReport(new SWGSDRangel::SWGBFMModReport());
    response.getBfmModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void BFMMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const BFMModSettings& settings)
{
    response.getBfmModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getBfmModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getBfmModSettings()->setModAfInput((int) settings.m_modAFInput);
    response.getBfmModSettings()->setPlayLoop(settings.m_playLoop ? 1 : 0);
    response.getBfmModSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getBfmModSettings()->setAfBandwidth(settings.m_afBandwidth);
    response.getBfmModSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getBfmModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getBfmModSettings()->getTitle()) {
        *response.getBfmModSettings()->getTitle() = settings.m_title;
    } else {
        response.getBfmModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getBfmModSettings()->setToneFrequency(settings.m_toneFrequency);
    response.getBfmModSettings()->setVolumeFactor(settings.m_volumeFactor);
    response.getBfmModSettings()->setAudioStereo(settings.m_audioStereo ? 1 : 0);
    response.getBfmModSettings()->setPreEmphasis(settings.m_preEmphasis);
    response.getBfmModSettings()->setPilotLevel(settings.m_pilotLevel);
    response.getBfmModSettings()->setRdsActive(settings.m_rdsActive ? 1 : 0);
    response.getBfmModSettings()->setRdsLevel(settings.m_rdsLevel);
    response.getBfmModSettings()->setRdsPi(settings.m_rdsPI);
    response.getBfmModSettings()->setRdsPty(settings.m_rdsPTY);
    if (response.getBfmModSettings()->getRdsPs()) {
        *response.getBfmModSettings()->getRdsPs() = settings.m_rdsPS;
    } else {
        response.getBfmModSettings()->setRdsPs(new QString(settings.m_rdsPS));
    }
    if (response.getBfmModSettings()->getRdsRadioText()) {
        *response.getBfmModSettings()->getRdsRadioText() = settings.m_rdsRadioText;
    } else {
        response.getBfmModSettings()->setRdsRadioText(new QString(settings.m_rdsRadioText));
    }

    if (!response.getBfmModSettings()->getCwKeyer()) {
        response.getBfmModSettings()->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings);
    }

    if (response.getBfmModSettings()->getAudioDeviceName()) {
        *response.getBfmModSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getBfmModSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    if (response.getBfmModSettings()->getFeedbackAudioDeviceName()) {
        *response.getBfmModSettings()->getFeedbackAudioDeviceName() = settings.m_feedbackAudioDeviceName;
    } else {
        response.getBfmModSettings()->setFeedbackAudioDeviceName(new QString(settings.m_feedbackAudioDeviceName));
    }

    response.getBfmModSettings()->setFeedbackVolumeFactor(settings.m_feedbackVolumeFactor);

    if (response.getBfmModSettings()->getFileName()) {
        *response.getBfmModSettings()->getFileName() = settings.m_fileName;
    } else {
        response.getBfmModSettings()->setFileName(new QString(settings.m_fileName));
    }

    response.getBfmModSettings()->setFeedbackAudioEnable(settings.m_feedbackAudioEnable ? 1 : 0);
    response.getBfmModSettings()->setStreamIndex(settings.m_streamIndex);
    response.getBfmModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getBfmModSettings()->getReverseApiAddress()) {
        *response.getBfmModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getBfmModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getBfmModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getBfmModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getBfmModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getBfmModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getBfmModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getBfmModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getBfmModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getBfmModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getBfmModSettings()->setRollupState(swgRollupState);
        }
    }
}

void BFMMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getBfmModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getBfmModReport()->setAudioSampleRate(m_basebandSource->getAudioSampleRate());
    response.getBfmModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void BFMMod::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const BFMModSettings& settings, bool force)
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

void BFMMod::webapiReverseSendCWSettings(const CWKeyerSettings& cwKeyerSettings)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setBfmModSettings(new SWGSDRangel::SWGBFMModSettings());
    SWGSDRangel::SWGBFMModSettings *swgBFMModSettings = swgChannelSettings->getBfmModSettings();

    swgBFMModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgBFMModSettings->getCwKeyer();
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

void BFMMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    const QList<QString>& channelSettingsKeys,
    const BFMModSettings& settings,
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

void BFMMod::webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const BFMModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    if (swgChannelSettings->getChannelType()) {
        *swgChannelSettings->getChannelType() = m_channelId;
    } else {
        swgChannelSettings->setChannelType(new QString(m_channelId));
    }
    swgChannelSettings->setBfmModSettings(new SWGSDRangel::SWGBFMModSettings());
    SWGSDRangel::SWGBFMModSettings *swgBFMModSettings = swgChannelSettings->getBfmModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("channelMute") || force) {
        swgBFMModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgBFMModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("modAFInput") || force) {
        swgBFMModSettings->setModAfInput((int) settings.m_modAFInput);
    }
    if (channelSettingsKeys.contains("playLoop") || force) {
        swgBFMModSettings->setPlayLoop(settings.m_playLoop ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgBFMModSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("afBandwidth") || force) {
        swgBFMModSettings->setAfBandwidth(settings.m_afBandwidth);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgBFMModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        if (swgBFMModSettings->getTitle()) {
            *swgBFMModSettings->getTitle() = settings.m_title;
        } else {
            swgBFMModSettings->setTitle(new QString(settings.m_title));
        }
    }
    if (channelSettingsKeys.contains("toneFrequency") || force) {
        swgBFMModSettings->setToneFrequency(settings.m_toneFrequency);
    }
    if (channelSettingsKeys.contains("volumeFactor") || force) {
        swgBFMModSettings->setVolumeFactor(settings.m_volumeFactor);
    }
    if (channelSettingsKeys.contains("audioStereo") || force) {
        swgBFMModSettings->setAudioStereo(settings.m_audioStereo ? 1 : 0);
    }
    if (channelSettingsKeys.contains("preEmphasis") || force) {
        swgBFMModSettings->setPreEmphasis(settings.m_preEmphasis);
    }
    if (channelSettingsKeys.contains("pilotLevel") || force) {
        swgBFMModSettings->setPilotLevel(settings.m_pilotLevel);
    }
    if (channelSettingsKeys.contains("rdsActive") || force) {
        swgBFMModSettings->setRdsActive(settings.m_rdsActive ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rdsLevel") || force) {
        swgBFMModSettings->setRdsLevel(settings.m_rdsLevel);
    }
    if (channelSettingsKeys.contains("rdsPI") || force) {
        swgBFMModSettings->setRdsPi(settings.m_rdsPI);
    }
    if (channelSettingsKeys.contains("rdsPTY") || force) {
        swgBFMModSettings->setRdsPty(settings.m_rdsPTY);
    }
    if (channelSettingsKeys.contains("rdsPS") || force) {
        swgBFMModSettings->setRdsPs(new QString(settings.m_rdsPS));
    }
    if (channelSettingsKeys.contains("rdsRadioText") || force) {
        swgBFMModSettings->setRdsRadioText(new QString(settings.m_rdsRadioText));
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgBFMModSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgBFMModSettings->setStreamIndex(settings.m_streamIndex);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        if (swgBFMModSettings->getAudioDeviceName()) {
            *swgBFMModSettings->getAudioDeviceName() = settings.m_audioDeviceName;
        } else {
            swgBFMModSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
        }
    }
    if (channelSettingsKeys.contains("feedbackAudioDeviceName") || force) {
        swgBFMModSettings->setFeedbackAudioDeviceName(new QString(settings.m_feedbackAudioDeviceName));
    }
    if (channelSettingsKeys.contains("feedbackVolumeFactor") || force) {
        swgBFMModSettings->setFeedbackVolumeFactor(settings.m_feedbackVolumeFactor);
    }
    if (channelSettingsKeys.contains("fileName") || force) {
        swgBFMModSettings->setFileName(new QString(settings.m_fileName));
    }
    if (channelSettingsKeys.contains("feedbackAudioEnable") || force) {
        swgBFMModSettings->setFeedbackAudioEnable(settings.m_feedbackAudioEnable ? 1 : 0);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgBFMModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgBFMModSettings->setRollupState(swgRollupState);
    }

    if (force)
    {
        const CWKeyerSettings& cwKeyerSettings = m_basebandSource->getCWKeyer().getSettings();
        swgBFMModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgBFMModSettings->getCwKeyer();
        m_basebandSource->getCWKeyer().webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);
    }
}

void BFMMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "BFMMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("BFMMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double BFMMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

CWKeyer *BFMMod::getCWKeyer()
{
    return &m_basebandSource->getCWKeyer();
}

void BFMMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t BFMMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

int BFMMod::getAudioSampleRate() const
{
    return m_basebandSource->getAudioSampleRate();
}

int BFMMod::getFeedbackAudioSampleRate() const
{
    return m_basebandSource->getFeedbackAudioSampleRate();
}
