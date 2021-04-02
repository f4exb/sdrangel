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

#include "SWGChannelSettings.h"
#include "SWGDATVDemodSettings.h"
#include "SWGChannelReport.h"

#include "device/deviceapi.h"

#include "datvdemod.h"
#include "maincore.h"

const char* const DATVDemod::m_channelIdURI = "sdrangel.channel.demoddatv";
const char* const DATVDemod::m_channelId = "DATVDemod";

MESSAGE_CLASS_DEFINITION(DATVDemod::MsgConfigureDATVDemod, Message)

DATVDemod::DATVDemod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI),
    m_basebandSampleRate(0)
{
    qDebug("DATVDemod::DATVDemod");
    setObjectName("DATVDemod");
    m_basebandSink = new DATVDemodBaseband();
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

}

DATVDemod::~DATVDemod()
{
    qDebug("DATVDemod::~DATVDemod");
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    m_basebandSink->deleteLater();
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

        return true;
    }
	else
	{
		return false;
	}
}

void DATVDemod::applySettings(const DATVDemodSettings& settings, bool force)
{
    QString debugMsg = tr("DATVDemod::applySettings: force: %1").arg(force);
    settings.debug(debugMsg);

    DATVDemodBaseband::MsgConfigureDATVDemodBaseband *msg = DATVDemodBaseband::MsgConfigureDATVDemodBaseband::create(settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

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
}

void DATVDemod::sendChannelSettings(
    QList<MessageQueue*> *messageQueues,
    QList<QString>& channelSettingsKeys,
    const DATVDemodSettings& settings,
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
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgDATVDemodSettings->setStreamIndex(settings.m_streamIndex);
    }
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
