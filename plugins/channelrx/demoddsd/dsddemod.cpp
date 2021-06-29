///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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


#include <string.h>
#include <stdio.h>
#include <complex.h>

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGDSDDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGDSDDemodReport.h"
#include "SWGRDSReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

#include "dsddemod.h"

MESSAGE_CLASS_DEFINITION(DSDDemod::MsgConfigureDSDDemod, Message)

const char* const DSDDemod::m_channelIdURI = "sdrangel.channel.dsddemod";
const char* const DSDDemod::m_channelId = "DSDDemod";
const int DSDDemod::m_udpBlockSize = 512;

DSDDemod::DSDDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    qDebug("DSDDemod::DSDDemod");
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSink = new DSDDemodBaseband();
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    connect(&m_channelMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleChannelMessages()));
}

DSDDemod::~DSDDemod()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
	m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);
    delete m_basebandSink;
    delete m_thread;
}

uint32_t DSDDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void DSDDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void DSDDemod::start()
{
    qDebug() << "DSDDemod::start";

    if (m_basebandSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_basebandSampleRate);
    }

    m_basebandSink->reset();
    m_thread->start();
}

void DSDDemod::stop()
{
    qDebug() << "DSDDemod::stop";
	m_thread->exit();
	m_thread->wait();
}

bool DSDDemod::handleMessage(const Message& cmd)
{
	qDebug() << "DSDDemod::handleMessage";

    if (MsgConfigureDSDDemod::match(cmd))
    {
        MsgConfigureDSDDemod& cfg = (MsgConfigureDSDDemod&) cmd;
        qDebug("DSDDemod::handleMessage: MsgConfigureDSDDemod: m_rfBandwidth");

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "DSDDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);

	    return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "DSDDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
	else
	{
		return false;
	}
}

void DSDDemod::applySettings(const DSDDemodSettings& settings, bool force)
{
    qDebug() << "DSDDemod::applySettings: "
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_demodGain: " << settings.m_demodGain
            << " m_volume: " << settings.m_volume
            << " m_baudRate: " << settings.m_baudRate
            << " m_squelchGate" << settings.m_squelchGate
            << " m_squelch: " << settings.m_squelch
            << " m_audioMute: " << settings.m_audioMute
            << " m_enableCosineFiltering: " << settings.m_enableCosineFiltering
            << " m_syncOrConstellation: " << settings.m_syncOrConstellation
            << " m_slot1On: " << settings.m_slot1On
            << " m_slot2On: " << settings.m_slot2On
            << " m_tdmaStereo: " << settings.m_tdmaStereo
            << " m_pllLock: " << settings.m_pllLock
            << " m_highPassFilter: "<< settings.m_highPassFilter
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_traceLengthMutliplier: " << settings.m_traceLengthMutliplier
            << " m_traceStroke: " << settings.m_traceStroke
            << " m_traceDecay: " << settings.m_traceDecay
            << " m_streamIndex: " << settings.m_streamIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_demodGain != m_settings.m_demodGain) || force) {
        reverseAPIKeys.append("demodGain");
    }
    if ((settings.m_audioMute != m_settings.m_audioMute) || force) {
        reverseAPIKeys.append("audioMute");
    }
    if ((settings.m_syncOrConstellation != m_settings.m_syncOrConstellation) || force) {
        reverseAPIKeys.append("syncOrConstellation");
    }
    if ((settings.m_slot1On != m_settings.m_slot1On) || force) {
        reverseAPIKeys.append("slot1On");
    }
    if ((settings.m_slot2On != m_settings.m_slot2On) || force) {
        reverseAPIKeys.append("slot2On");
    }
    if ((settings.m_demodGain != m_settings.m_demodGain) || force) {
        reverseAPIKeys.append("demodGain");
    }
    if ((settings.m_traceLengthMutliplier != m_settings.m_traceLengthMutliplier) || force) {
        reverseAPIKeys.append("traceLengthMutliplier");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        reverseAPIKeys.append("fmDeviation");
    }
    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force) {
        reverseAPIKeys.append("squelchGate");
    }
    if ((settings.m_squelch != m_settings.m_squelch) || force) {
        reverseAPIKeys.append("squelch");
    }
    if ((settings.m_volume != m_settings.m_volume) || force) {
        reverseAPIKeys.append("volume");
    }
    if ((settings.m_baudRate != m_settings.m_baudRate) || force) {
        reverseAPIKeys.append("baudRate");
    }
    if ((settings.m_enableCosineFiltering != m_settings.m_enableCosineFiltering) || force) {
        reverseAPIKeys.append("enableCosineFiltering");
    }
    if ((settings.m_tdmaStereo != m_settings.m_tdmaStereo) || force) {
        reverseAPIKeys.append("tdmaStereo");
    }
    if ((settings.m_pllLock != m_settings.m_pllLock) || force) {
        reverseAPIKeys.append("pllLock");
    }
    if ((settings.m_highPassFilter != m_settings.m_highPassFilter) || force) {
        reverseAPIKeys.append("highPassFilter");
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

    DSDDemodBaseband::MsgConfigureDSDDemodBaseband *msg = DSDDemodBaseband::MsgConfigureDSDDemodBaseband::create(settings, force);
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

    QList<MessageQueue*> *messageQueues = MainCore::instance()->getMessagePipes().getMessageQueues(this, "settings");

    if (messageQueues) {
        sendChannelSettings(messageQueues, reverseAPIKeys, settings, force);
    }

    m_settings = settings;
}

QByteArray DSDDemod::serialize() const
{
    return m_settings.serialize();
}

bool DSDDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureDSDDemod *msg = MsgConfigureDSDDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureDSDDemod *msg = MsgConfigureDSDDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void DSDDemod::sendSampleRateToDemodAnalyzer()
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

int DSDDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDsdDemodSettings(new SWGSDRangel::SWGDSDDemodSettings());
    response.getDsdDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int DSDDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    DSDDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureDSDDemod *msg = MsgConfigureDSDDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("DSDDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDSDDemod *msgToGUI = MsgConfigureDSDDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void DSDDemod::webapiUpdateChannelSettings(
        DSDDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getDsdDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getDsdDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getDsdDemodSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("demodGain")) {
        settings.m_demodGain = response.getDsdDemodSettings()->getDemodGain();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getDsdDemodSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("baudRate")) {
        settings.m_baudRate = response.getDsdDemodSettings()->getBaudRate();
    }
    if (channelSettingsKeys.contains("squelchGate")) {
        settings.m_squelchGate = response.getDsdDemodSettings()->getSquelchGate();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getDsdDemodSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getDsdDemodSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("enableCosineFiltering")) {
        settings.m_enableCosineFiltering = response.getDsdDemodSettings()->getEnableCosineFiltering() != 0;
    }
    if (channelSettingsKeys.contains("syncOrConstellation")) {
        settings.m_syncOrConstellation = response.getDsdDemodSettings()->getSyncOrConstellation() != 0;
    }
    if (channelSettingsKeys.contains("slot1On")) {
        settings.m_slot1On = response.getDsdDemodSettings()->getSlot1On() != 0;
    }
    if (channelSettingsKeys.contains("slot2On")) {
        settings.m_slot2On = response.getDsdDemodSettings()->getSlot2On() != 0;
    }
    if (channelSettingsKeys.contains("tdmaStereo")) {
        settings.m_tdmaStereo = response.getDsdDemodSettings()->getTdmaStereo() != 0;
    }
    if (channelSettingsKeys.contains("pllLock")) {
        settings.m_pllLock = response.getDsdDemodSettings()->getPllLock() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getDsdDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getDsdDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getDsdDemodSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("highPassFilter")) {
        settings.m_highPassFilter = response.getDsdDemodSettings()->getHighPassFilter() != 0;
    }
    if (channelSettingsKeys.contains("traceLengthMutliplier")) {
        settings.m_traceLengthMutliplier = response.getDsdDemodSettings()->getTraceLengthMutliplier();
    }
    if (channelSettingsKeys.contains("traceStroke")) {
        settings.m_traceStroke = response.getDsdDemodSettings()->getTraceStroke();
    }
    if (channelSettingsKeys.contains("traceDecay")) {
        settings.m_traceDecay = response.getDsdDemodSettings()->getTraceDecay();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getDsdDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getDsdDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getDsdDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getDsdDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getDsdDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getDsdDemodSettings()->getReverseApiChannelIndex();
    }
}

int DSDDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDsdDemodReport(new SWGSDRangel::SWGDSDDemodReport());
    response.getDsdDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void DSDDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DSDDemodSettings& settings)
{
    response.getDsdDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getDsdDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getDsdDemodSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getDsdDemodSettings()->setDemodGain(settings.m_demodGain);
    response.getDsdDemodSettings()->setVolume(settings.m_volume);
    response.getDsdDemodSettings()->setBaudRate(settings.m_baudRate);
    response.getDsdDemodSettings()->setSquelchGate(settings.m_squelchGate);
    response.getDsdDemodSettings()->setSquelch(settings.m_squelch);
    response.getDsdDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getDsdDemodSettings()->setEnableCosineFiltering(settings.m_enableCosineFiltering ? 1 : 0);
    response.getDsdDemodSettings()->setSyncOrConstellation(settings.m_syncOrConstellation ? 1 : 0);
    response.getDsdDemodSettings()->setSlot1On(settings.m_slot1On ? 1 : 0);
    response.getDsdDemodSettings()->setSlot2On(settings.m_slot2On ? 1 : 0);
    response.getDsdDemodSettings()->setTdmaStereo(settings.m_tdmaStereo ? 1 : 0);
    response.getDsdDemodSettings()->setPllLock(settings.m_pllLock ? 1 : 0);
    response.getDsdDemodSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getDsdDemodSettings()->getTitle()) {
        *response.getDsdDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getDsdDemodSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getDsdDemodSettings()->getAudioDeviceName()) {
        *response.getDsdDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getDsdDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getDsdDemodSettings()->setHighPassFilter(settings.m_highPassFilter ? 1 : 0);
    response.getDsdDemodSettings()->setTraceLengthMutliplier(settings.m_traceLengthMutliplier);
    response.getDsdDemodSettings()->setTraceStroke(settings.m_traceStroke);
    response.getDsdDemodSettings()->setTraceDecay(settings.m_traceDecay);
    response.getDsdDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getDsdDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getDsdDemodSettings()->getReverseApiAddress()) {
        *response.getDsdDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getDsdDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getDsdDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getDsdDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getDsdDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

}

void DSDDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getDsdDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getDsdDemodReport()->setAudioSampleRate(m_basebandSink->getAudioSampleRate());
    response.getDsdDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
    response.getDsdDemodReport()->setSquelch(m_basebandSink->getSquelchOpen() ? 1 : 0);
    response.getDsdDemodReport()->setPllLocked(getDecoder().getSymbolPLLLocked() ? 1 : 0);
    response.getDsdDemodReport()->setSlot1On(getDecoder().getVoice1On() ? 1 : 0);
    response.getDsdDemodReport()->setSlot2On(getDecoder().getVoice2On() ? 1 : 0);
    response.getDsdDemodReport()->setSyncType(new QString(getDecoder().getFrameTypeText()));
    response.getDsdDemodReport()->setInLevel(getDecoder().getInLevel());
    response.getDsdDemodReport()->setCarierPosition(getDecoder().getCarrierPos());
    response.getDsdDemodReport()->setZeroCrossingPosition(getDecoder().getZeroCrossingPos());
    response.getDsdDemodReport()->setSyncRate(getDecoder().getSymbolSyncQuality());
    response.getDsdDemodReport()->setStatusText(new QString(updateAndGetStatusText()));
}

void DSDDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const DSDDemodSettings& settings, bool force)
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

void DSDDemod::sendChannelSettings(
    QList<MessageQueue*> *messageQueues,
    QList<QString>& channelSettingsKeys,
    const DSDDemodSettings& settings,
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

void DSDDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DSDDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setDsdDemodSettings(new SWGSDRangel::SWGDSDDemodSettings());
    SWGSDRangel::SWGDSDDemodSettings *swgDSDDemodSettings = swgChannelSettings->getDsdDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgDSDDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgDSDDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgDSDDemodSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("demodGain") || force) {
        swgDSDDemodSettings->setDemodGain(settings.m_demodGain);
    }
    if (channelSettingsKeys.contains("volume") || force) {
        swgDSDDemodSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("baudRate") || force) {
        swgDSDDemodSettings->setBaudRate(settings.m_baudRate);
    }
    if (channelSettingsKeys.contains("squelchGate") || force) {
        swgDSDDemodSettings->setSquelchGate(settings.m_squelchGate);
    }
    if (channelSettingsKeys.contains("squelch") || force) {
        swgDSDDemodSettings->setSquelch(settings.m_squelch);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgDSDDemodSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("enableCosineFiltering") || force) {
        swgDSDDemodSettings->setEnableCosineFiltering(settings.m_enableCosineFiltering ? 1 : 0);
    }
    if (channelSettingsKeys.contains("syncOrConstellation") || force) {
        swgDSDDemodSettings->setSyncOrConstellation(settings.m_syncOrConstellation ? 1 : 0);
    }
    if (channelSettingsKeys.contains("slot1On") || force) {
        swgDSDDemodSettings->setSlot1On(settings.m_slot1On ? 1 : 0);
    }
    if (channelSettingsKeys.contains("slot2On") || force) {
        swgDSDDemodSettings->setSlot2On(settings.m_slot2On ? 1 : 0);
    }
    if (channelSettingsKeys.contains("tdmaStereo") || force) {
        swgDSDDemodSettings->setTdmaStereo(settings.m_tdmaStereo ? 1 : 0);
    }
    if (channelSettingsKeys.contains("pllLock") || force) {
        swgDSDDemodSettings->setPllLock(settings.m_pllLock ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgDSDDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgDSDDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgDSDDemodSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("highPassFilter") || force) {
        swgDSDDemodSettings->setHighPassFilter(settings.m_highPassFilter ? 1 : 0);
    }
    if (channelSettingsKeys.contains("traceLengthMutliplier") || force) {
        swgDSDDemodSettings->setTraceLengthMutliplier(settings.m_traceLengthMutliplier);
    }
    if (channelSettingsKeys.contains("traceStroke") || force) {
        swgDSDDemodSettings->setTraceStroke(settings.m_traceStroke);
    }
    if (channelSettingsKeys.contains("traceDecay") || force) {
        swgDSDDemodSettings->setTraceDecay(settings.m_traceDecay);
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgDSDDemodSettings->setStreamIndex(settings.m_streamIndex);
    }
}

void DSDDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "DSDDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("DSDDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void DSDDemod::handleChannelMessages()
{
	Message* message;

	while ((message = m_channelMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}
