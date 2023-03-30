///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
// using LeanSDR Framework (C) 2016 F4DAV                                        //
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

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGDATVDemodSettings.h"
#include "SWGChannelReport.h"

#include "device/deviceapi.h"
#include "settings/serializable.h"

#include "datvdemod.h"
#include "maincore.h"
#include "util/db.h"

const char* const DATVDemod::m_channelIdURI = "sdrangel.channel.demoddatv";
const char* const DATVDemod::m_channelId = "DATVDemod";

MESSAGE_CLASS_DEFINITION(DATVDemod::MsgConfigureDATVDemod, Message)

DATVDemod::DATVDemod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI),
    m_basebandSampleRate(0)
{
    qDebug("DATVDemod::DATVDemod");
    setObjectName(m_channelId);
    m_thread.setObjectName("bbDATVDemod");
    m_basebandSink = new DATVDemodBaseband();
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DATVDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &DATVDemod::handleIndexInDeviceSetChanged
    );
}

DATVDemod::~DATVDemod()
{
    qDebug("DATVDemod::~DATVDemod");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DATVDemod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    m_basebandSink->deleteLater();
}

void DATVDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

void DATVDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void DATVDemod::start()
{
	qDebug("DATVDemod::start");

    if (m_basebandSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_basebandSampleRate);
    }

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DATVDemodBaseband::MsgConfigureDATVDemodBaseband *msg = DATVDemodBaseband::MsgConfigureDATVDemodBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void DATVDemod::stop()
{
    qDebug("DATVDemod::stop");
	m_basebandSink->stopWork();
	m_thread.quit();
	m_thread.wait();
}

bool DATVDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureDATVDemod::match(cmd))
	{
        MsgConfigureDATVDemod& objCfg = (MsgConfigureDATVDemod&) cmd;
        qDebug() << "DATVDemod::handleMessage: MsgConfigureDATVDemod";
        applySettings(objCfg.getSettings(), objCfg.getForce());

        return true;
	}
    else if(DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate(); // store for init at start
        qDebug() << "DATVDemod::handleMessage: DSPSignalNotification" << m_basebandSampleRate;

        // Forward to the sink
        DSPSignalNotification* notifToSink = new DSPSignalNotification(notif); // make a copy
        m_basebandSink->getInputMessageQueue()->push(notifToSink);

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

void DATVDemod::setCenterFrequency(qint64 frequency)
{
    DATVDemodSettings settings = m_settings;
    settings.m_centerFrequency = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDATVDemod *msgToGUI = MsgConfigureDATVDemod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void DATVDemod::applySettings(const DATVDemodSettings& settings, bool force)
{
    QString debugMsg = tr("DATVDemod::applySettings: force: %1").arg(force);
    settings.debug(debugMsg);

    QList<QString> reverseAPIKeys;

    if (settings.m_rgbColor != m_settings.m_rgbColor) {
        reverseAPIKeys.append("rgbColor");
    }
    if (settings.m_title != m_settings.m_title) {
        reverseAPIKeys.append("title");
    }
    if (settings.m_rfBandwidth != m_settings.m_rfBandwidth) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if (settings.m_centerFrequency != m_settings.m_centerFrequency) {
        reverseAPIKeys.append("centerFrequency");
    }
    if (settings.m_standard != m_settings.m_standard) {
        reverseAPIKeys.append("standard");
    }
    if (settings.m_modulation != m_settings.m_modulation) {
        reverseAPIKeys.append("modulation");
    }
    if (settings.m_fec != m_settings.m_fec) {
        reverseAPIKeys.append("fec");
    }
    if (settings.m_softLDPC != m_settings.m_softLDPC) {
        reverseAPIKeys.append("softLDPC");
    }
    if (settings.m_softLDPCToolPath != m_settings.m_softLDPCToolPath) {
        reverseAPIKeys.append("softLDPCToolPath");
    }
    if (settings.m_softLDPCMaxTrials != m_settings.m_softLDPCMaxTrials) {
        reverseAPIKeys.append("softLDPCMaxTrials");
    }
    if (settings.m_maxBitflips != m_settings.m_maxBitflips) {
        reverseAPIKeys.append("maxBitflips");
    }
    if (settings.m_audioMute != m_settings.m_audioMute) {
        reverseAPIKeys.append("audioMute");
    }
    if (settings.m_audioDeviceName != m_settings.m_audioDeviceName) {
        reverseAPIKeys.append("audioDeviceName");
    }
    if (settings.m_symbolRate != m_settings.m_symbolRate) {
        reverseAPIKeys.append("symbolRate");
    }
    if (settings.m_notchFilters != m_settings.m_notchFilters) {
        reverseAPIKeys.append("notchFilters");
    }
    if (settings.m_allowDrift != m_settings.m_allowDrift) {
        reverseAPIKeys.append("allowDrift");
    }
    if (settings.m_fastLock != m_settings.m_fastLock) {
        reverseAPIKeys.append("fastLock");
    }
    if (settings.m_filter != m_settings.m_filter) {
        reverseAPIKeys.append("filter");
    }
    if (settings.m_hardMetric != m_settings.m_hardMetric) {
        reverseAPIKeys.append("hardMetric");
    }
    if (settings.m_rollOff != m_settings.m_rollOff) {
        reverseAPIKeys.append("rollOff");
    }
    if (settings.m_viterbi != m_settings.m_viterbi) {
        reverseAPIKeys.append("viterbi");
    }
    if (settings.m_excursion != m_settings.m_excursion) {
        reverseAPIKeys.append("excursion");
    }
    if (settings.m_audioVolume != m_settings.m_audioVolume) {
        reverseAPIKeys.append("audioVolume");
    }
    if (settings.m_videoMute != m_settings.m_videoMute) {
        reverseAPIKeys.append("videoMute");
    }
    if (settings.m_udpTSAddress != m_settings.m_udpTSAddress) {
        reverseAPIKeys.append("udpTSAddress");
    }
    if (settings.m_udpTSPort != m_settings.m_udpTSPort) {
        reverseAPIKeys.append("udpTSPort");
    }
    if (settings.m_udpTS != m_settings.m_udpTS) {
        reverseAPIKeys.append("udpTS");
    }
    if (settings.m_playerEnable != m_settings.m_playerEnable) {
        reverseAPIKeys.append("playerEnable");
    }

    DATVDemodBaseband::MsgConfigureDATVDemodBaseband *msg = DATVDemodBaseband::MsgConfigureDATVDemodBaseband::create(settings, force);
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

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, reverseAPIKeys, settings, force);
    }

    m_settings = settings;
}

uint32_t DATVDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

int DATVDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDatvDemodSettings(new SWGSDRangel::SWGDATVDemodSettings());
    response.getDatvDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int DATVDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int DATVDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDatvDemodReport(new SWGSDRangel::SWGDATVDemodReport());
    response.getDatvDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

int DATVDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    DATVDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureDATVDemod *msg = MsgConfigureDATVDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("DATVDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDATVDemod *msgToGUI = MsgConfigureDATVDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void DATVDemod::webapiUpdateChannelSettings(
        DATVDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getDatvDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getDatvDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getDatvDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getDatvDemodSettings()->getCenterFrequency();
    }
    if (channelSettingsKeys.contains("standard")) {
        settings.m_standard = (DATVDemodSettings::dvb_version) response.getDatvDemodSettings()->getStandard();
    }
    if (channelSettingsKeys.contains("modulation")) {
        settings.m_modulation = (DATVDemodSettings::DATVModulation) response.getDatvDemodSettings()->getModulation();
    }
    if (channelSettingsKeys.contains("fec")) {
        settings.m_fec = (DATVDemodSettings::DATVCodeRate) response.getDatvDemodSettings()->getFec();
    }
    if (channelSettingsKeys.contains("softLDPC")) {
        settings.m_softLDPC = response.getDatvDemodSettings()->getSoftLdpc() == 1;
    }
    if (channelSettingsKeys.contains("softLDPCToolPath")) {
        settings.m_softLDPCToolPath = *response.getDatvDemodSettings()->getSoftLdpcToolPath();
    }
    if (channelSettingsKeys.contains("softLDPCMaxTrials")) {
        settings.m_softLDPCMaxTrials = response.getDatvDemodSettings()->getSoftLdpcMaxTrials();
    }
    if (channelSettingsKeys.contains("maxBitflips")) {
        settings.m_maxBitflips = response.getDatvDemodSettings()->getMaxBitflips();
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getDatvDemodSettings()->getAudioMute() == 1;
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getDatvDemodSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("symbolRate")) {
        settings.m_symbolRate = response.getDatvDemodSettings()->getSymbolRate();
    }
    if (channelSettingsKeys.contains("notchFilters")) {
        settings.m_notchFilters = response.getDatvDemodSettings()->getNotchFilters();
    }
    if (channelSettingsKeys.contains("allowDrift")) {
        settings.m_allowDrift = response.getDatvDemodSettings()->getAllowDrift() == 1;
    }
    if (channelSettingsKeys.contains("fastLock")) {
        settings.m_fastLock = response.getDatvDemodSettings()->getFastLock() == 1;
    }
    if (channelSettingsKeys.contains("filter")) {
        settings.m_filter = (DATVDemodSettings::dvb_sampler) response.getDatvDemodSettings()->getFilter();
    }
    if (channelSettingsKeys.contains("hardMetric")) {
        settings.m_hardMetric = response.getDatvDemodSettings()->getHardMetric() == 1;
    }
    if (channelSettingsKeys.contains("rollOff")) {
        settings.m_rollOff = response.getDatvDemodSettings()->getRollOff();
    }
    if (channelSettingsKeys.contains("viterbi")) {
        settings.m_viterbi = response.getDatvDemodSettings()->getViterbi() == 1;
    }
    if (channelSettingsKeys.contains("excursion")) {
        settings.m_excursion = response.getDatvDemodSettings()->getExcursion();
    }
    if (channelSettingsKeys.contains("audioVolume")) {
        settings.m_audioVolume = response.getDatvDemodSettings()->getAudioVolume();
    }
    if (channelSettingsKeys.contains("videoMute")) {
        settings.m_videoMute = response.getDatvDemodSettings()->getVideoMute() == 1;
    }
    if (channelSettingsKeys.contains("udpTSAddress")) {
        settings.m_udpTSAddress = *response.getDatvDemodSettings()->getUdpTsAddress();
    }
    if (channelSettingsKeys.contains("udpTSPort")) {
        settings.m_udpTSPort = response.getDatvDemodSettings()->getUdpTsPort();
    }
    if (channelSettingsKeys.contains("udpTS")) {
        settings.m_udpTS = response.getDatvDemodSettings()->getUdpTs() == 1;
    }
    if (channelSettingsKeys.contains("playerEnable")) {
        settings.m_playerEnable = response.getDatvDemodSettings()->getPlayerEnable() == 1;
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getDatvDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getDatvDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getDatvDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getDatvDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getDatvDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getDatvDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getDatvDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getDatvDemodSettings()->getRollupState());
    }
}

void DATVDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DATVDemodSettings& settings)
{
    response.getDatvDemodSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getDatvDemodSettings()->getTitle()) {
        *response.getDatvDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getDatvDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getDatvDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getDatvDemodSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getDatvDemodSettings()->setStandard((int) settings.m_standard);
    response.getDatvDemodSettings()->setModulation((int) settings.m_modulation);
    response.getDatvDemodSettings()->setFec((int) settings.m_fec);
    response.getDatvDemodSettings()->setSoftLdpc((int) settings.m_softLDPC ? 1 : 0);

    if (response.getDatvDemodSettings()->getSoftLdpcToolPath()) {
        *response.getDatvDemodSettings()->getSoftLdpcToolPath() = settings.m_softLDPCToolPath;
    } else {
        response.getDatvDemodSettings()->setSoftLdpcToolPath(new QString(settings.m_softLDPCToolPath));
    }

    response.getDatvDemodSettings()->setSoftLdpcMaxTrials(settings.m_softLDPCMaxTrials);
    response.getDatvDemodSettings()->setMaxBitflips(settings.m_maxBitflips);
    response.getDatvDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);

    if (response.getDatvDemodSettings()->getAudioDeviceName()) {
        *response.getDatvDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getDatvDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getDatvDemodSettings()->setSymbolRate(settings.m_symbolRate);
    response.getDatvDemodSettings()->setNotchFilters(settings.m_notchFilters);
    response.getDatvDemodSettings()->setAllowDrift(settings.m_allowDrift ? 1 : 0);
    response.getDatvDemodSettings()->setFastLock(settings.m_fastLock ? 1 : 0);
    response.getDatvDemodSettings()->setFilter((int) settings.m_filter);
    response.getDatvDemodSettings()->setHardMetric(settings.m_hardMetric ? 1 : 0);
    response.getDatvDemodSettings()->setRollOff(settings.m_rollOff);
    response.getDatvDemodSettings()->setViterbi(settings.m_viterbi ? 1 : 0);
    response.getDatvDemodSettings()->setExcursion(settings.m_excursion);
    response.getDatvDemodSettings()->setAudioVolume(settings.m_audioVolume);
    response.getDatvDemodSettings()->setVideoMute(settings.m_videoMute ? 1 : 0);

    if (response.getDatvDemodSettings()->getUdpTsAddress()) {
        *response.getDatvDemodSettings()->getUdpTsAddress() = settings.m_udpTSAddress;
    } else {
        response.getDatvDemodSettings()->setUdpTsAddress(new QString(settings.m_udpTSAddress));
    }

    response.getDatvDemodSettings()->setUdpTsPort(settings.m_udpTSPort);
    response.getDatvDemodSettings()->setUdpTs(settings.m_udpTS ? 1 : 0);
    response.getDatvDemodSettings()->setPlayerEnable(settings.m_playerEnable ? 1 : 0);
    response.getDatvDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getDatvDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getDatvDemodSettings()->getReverseApiAddress()) {
        *response.getDatvDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getDatvDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getDatvDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getDatvDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getDatvDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getDatvDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getDatvDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getDatvDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getDatvDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getDatvDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getDatvDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void DATVDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsq = getMagSq() / (SDR_RX_SCALED*SDR_RX_SCALED);
    response.getDatvDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsq));
    response.getDatvDemodReport()->setAudioActive(audioActive() ? 1 : 0);
    response.getDatvDemodReport()->setAudioDecodeOk(audioDecodeOK() ? 1 : 0);
    response.getDatvDemodReport()->setModcodCodeRate(getModcodCodeRate());
    response.getDatvDemodReport()->setModcodModulation(getModcodModulation());
    response.getDatvDemodReport()->setSetByModcod(isCstlnSetByModcod() ? 1 : 0);
    response.getDatvDemodReport()->setUdpRunning(udpRunning() ? 1 : 0);
    response.getDatvDemodReport()->setVideoActive(videoActive() ? 1 : 0);
    response.getDatvDemodReport()->setVideoDecodeOk(videoDecodeOK() ? 1 : 0);
    response.getDatvDemodReport()->setMer(getMERAvg());
    response.getDatvDemodReport()->setCnr(getCNRAvg());
}

void DATVDemod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const DATVDemodSettings& settings,
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

void DATVDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DATVDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("DATVDemod"));
    swgChannelSettings->setDatvDemodSettings(new SWGSDRangel::SWGDATVDemodSettings());
    SWGSDRangel::SWGDATVDemodSettings *swgDATVDemodSettings = swgChannelSettings->getDatvDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgDATVDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgDATVDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgDATVDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgDATVDemodSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (channelSettingsKeys.contains("standard") || force) {
        swgDATVDemodSettings->setStandard((int) settings.m_standard);
    }
    if (channelSettingsKeys.contains("modulation") || force) {
        swgDATVDemodSettings->setModulation((int) settings.m_modulation);
    }
    if (channelSettingsKeys.contains("fec") || force) {
        swgDATVDemodSettings->setFec((int) settings.m_fec);
    }
    if (channelSettingsKeys.contains("softLDPC") || force) {
        swgDATVDemodSettings->setSoftLdpc(settings.m_softLDPC ? 1 : 0);
    }
    if (channelSettingsKeys.contains("softLDPCToolPath") || force) {
        swgDATVDemodSettings->setSoftLdpcToolPath(new QString(settings.m_softLDPCToolPath));
    }
    if (channelSettingsKeys.contains("softLDPCMaxTrials") || force) {
        swgDATVDemodSettings->setSoftLdpcMaxTrials(settings.m_softLDPCMaxTrials);
    }
    if (channelSettingsKeys.contains("maxBitflips") || force) {
        swgDATVDemodSettings->setMaxBitflips(settings.m_maxBitflips);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgDATVDemodSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgDATVDemodSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("symbolRate") || force) {
        swgDATVDemodSettings->setSymbolRate(settings.m_symbolRate);
    }
    if (channelSettingsKeys.contains("notchFilters") || force) {
        swgDATVDemodSettings->setNotchFilters(settings.m_notchFilters);
    }
    if (channelSettingsKeys.contains("allowDrift") || force) {
        swgDATVDemodSettings->setAllowDrift(settings.m_allowDrift ? 1 : 0);
    }
    if (channelSettingsKeys.contains("fastLock") || force) {
        swgDATVDemodSettings->setFastLock(settings.m_fastLock ? 1 : 0);
    }
    if (channelSettingsKeys.contains("filter") || force) {
        swgDATVDemodSettings->setFilter((int) settings.m_filter);
    }
    if (channelSettingsKeys.contains("hardMetric") || force) {
        swgDATVDemodSettings->setHardMetric(settings.m_hardMetric ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rollOff") || force) {
        swgDATVDemodSettings->setRollOff(settings.m_rollOff);
    }
    if (channelSettingsKeys.contains("viterbi") || force) {
        swgDATVDemodSettings->setHardMetric(settings.m_viterbi ? 1 : 0);
    }
    if (channelSettingsKeys.contains("excursion") || force) {
        swgDATVDemodSettings->setExcursion(settings.m_excursion);
    }
    if (channelSettingsKeys.contains("audioVolume") || force) {
        swgDATVDemodSettings->setAudioVolume(settings.m_audioVolume);
    }
    if (channelSettingsKeys.contains("videoMute") || force) {
        swgDATVDemodSettings->setVideoMute(settings.m_videoMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("udpTSAddress") || force) {
        swgDATVDemodSettings->setUdpTsAddress(new QString(settings.m_udpTSAddress));
    }
    if (channelSettingsKeys.contains("udpTSPort") || force) {
        swgDATVDemodSettings->setUdpTsPort(settings.m_udpTSPort);
    }
    if (channelSettingsKeys.contains("udpTS") || force) {
        swgDATVDemodSettings->setUdpTs(settings.m_udpTS ? 1 : 0);
    }
    if (channelSettingsKeys.contains("playerEnable") || force) {
        swgDATVDemodSettings->setPlayerEnable(settings.m_playerEnable ? 1 : 0);
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgDATVDemodSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgDATVDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgDATVDemodSettings->setRollupState(swgRollupState);
    }
}

void DATVDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const DATVDemodSettings& settings, bool force)
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

void DATVDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "DATVDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("DATVDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void DATVDemod::handleIndexInDeviceSetChanged(int index)
{
    if (index < 0) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
    m_basebandSink->setAudioFifoLabel(fifoLabel);
}
