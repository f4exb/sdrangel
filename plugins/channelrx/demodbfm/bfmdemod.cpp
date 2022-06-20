///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
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

#include "boost/format.hpp"

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGBFMDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGBFMDemodReport.h"
#include "SWGRDSReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

#include "bfmdemod.h"

MESSAGE_CLASS_DEFINITION(BFMDemod::MsgConfigureBFMDemod, Message)

const char* const BFMDemod::m_channelIdURI = "sdrangel.channel.bfm";
const char* const BFMDemod::m_channelId = "BFMDemod";
const int BFMDemod::m_udpBlockSize = 512;

BFMDemod::BFMDemod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_RX_SCALEF),
    m_basebandSampleRate(0)
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSink = new BFMDemodBaseband();
    m_basebandSink->setSpectrumSink(&m_spectrumVis);
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(m_thread);

	applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &BFMDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &BFMDemod::handleIndexInDeviceSetChanged
    );
}

BFMDemod::~BFMDemod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &BFMDemod::networkManagerFinished
    );
    delete m_networkManager;

    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);
    delete m_basebandSink;
    delete m_thread;
}

void BFMDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t BFMDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void BFMDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void BFMDemod::start()
{
    qDebug() << "BFMDemod::start";

    if (m_basebandSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_basebandSampleRate);
    }

    m_basebandSink->reset();
    m_thread->start();

    SpectrumSettings spectrumSettings = m_spectrumVis.getSettings();
    spectrumSettings.m_ssb = true;
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis.getInputMessageQueue()->push(msg);
}

void BFMDemod::stop()
{
    qDebug() << "BFMDemod::stop";
	m_thread->exit();
	m_thread->wait();
}

bool BFMDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureBFMDemod::match(cmd))
    {
        MsgConfigureBFMDemod& cfg = (MsgConfigureBFMDemod&) cmd;
        qDebug() << "BFMDemod::handleMessage: MsgConfigureBFMDemod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "BFMDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);

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

void BFMDemod::setCenterFrequency(qint64 frequency)
{
    BFMDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureBFMDemod *msgToGUI = MsgConfigureBFMDemod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void BFMDemod::applySettings(const BFMDemodSettings& settings, bool force)
{
    qDebug() << "BFMDemod::applySettings: MsgConfigureBFMDemod:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_afBandwidth: " << settings.m_afBandwidth
            << " m_volume: " << settings.m_volume
            << " m_squelch: " << settings.m_squelch
            << " m_audioStereo: " << settings.m_audioStereo
            << " m_lsbStereo: " << settings.m_lsbStereo
            << " m_showPilot: " << settings.m_showPilot
            << " m_rdsActive: " << settings.m_rdsActive
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_volume != m_settings.m_volume) || force) {
        reverseAPIKeys.append("volume");
    }
    if ((settings.m_audioStereo != m_settings.m_audioStereo) || force) {
        reverseAPIKeys.append("audioStereo");
    }
    if ((settings.m_lsbStereo != m_settings.m_lsbStereo) || force) {
        reverseAPIKeys.append("lsbStereo");
    }
    if ((settings.m_showPilot != m_settings.m_showPilot) || force) {
        reverseAPIKeys.append("showPilot");
    }
    if ((settings.m_rdsActive != m_settings.m_rdsActive) || force) {
        reverseAPIKeys.append("rdsActive");
    }
    if ((settings.m_afBandwidth != m_settings.m_afBandwidth) || force) {
        reverseAPIKeys.append("afBandwidth");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_squelch != m_settings.m_squelch) || force) {
        reverseAPIKeys.append("squelch");
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

    BFMDemodBaseband::MsgConfigureBFMDemodBaseband *msg = BFMDemodBaseband::MsgConfigureBFMDemodBaseband::create(settings, force);
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

QByteArray BFMDemod::serialize() const
{
    return m_settings.serialize();
}

bool BFMDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureBFMDemod *msg = MsgConfigureBFMDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureBFMDemod *msg = MsgConfigureBFMDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int BFMDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setBfmDemodSettings(new SWGSDRangel::SWGBFMDemodSettings());
    response.getBfmDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int BFMDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int BFMDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    BFMDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureBFMDemod *msg = MsgConfigureBFMDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("BFMDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureBFMDemod *msgToGUI = MsgConfigureBFMDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void BFMDemod::webapiUpdateChannelSettings(
        BFMDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getBfmDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getBfmDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("afBandwidth")) {
        settings.m_afBandwidth = response.getBfmDemodSettings()->getAfBandwidth();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getBfmDemodSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getBfmDemodSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("audioStereo")) {
        settings.m_audioStereo = response.getBfmDemodSettings()->getAudioStereo() != 0;
    }
    if (channelSettingsKeys.contains("lsbStereo")) {
        settings.m_lsbStereo = response.getBfmDemodSettings()->getLsbStereo() != 0;
    }
    if (channelSettingsKeys.contains("showPilot")) {
        settings.m_showPilot = response.getBfmDemodSettings()->getShowPilot() != 0;
    }
    if (channelSettingsKeys.contains("rdsActive")) {
        settings.m_rdsActive = response.getBfmDemodSettings()->getRdsActive() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getBfmDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getBfmDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getBfmDemodSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getBfmDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getBfmDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getBfmDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getBfmDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getBfmDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getBfmDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_spectrumGUI && channelSettingsKeys.contains("spectrumConfig")) {
        settings.m_spectrumGUI->updateFrom(channelSettingsKeys, response.getBfmDemodSettings()->getSpectrumConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getBfmDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getBfmDemodSettings()->getRollupState());
    }
}

int BFMDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setBfmDemodReport(new SWGSDRangel::SWGBFMDemodReport());
    response.getBfmDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void BFMDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const BFMDemodSettings& settings)
{
    response.getBfmDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getBfmDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getBfmDemodSettings()->setAfBandwidth(settings.m_afBandwidth);
    response.getBfmDemodSettings()->setVolume(settings.m_volume);
    response.getBfmDemodSettings()->setSquelch(settings.m_squelch);
    response.getBfmDemodSettings()->setAudioStereo(settings.m_audioStereo ? 1 : 0);
    response.getBfmDemodSettings()->setLsbStereo(settings.m_lsbStereo ? 1 : 0);
    response.getBfmDemodSettings()->setShowPilot(settings.m_showPilot ? 1 : 0);
    response.getBfmDemodSettings()->setRdsActive(settings.m_rdsActive ? 1 : 0);
    response.getBfmDemodSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getBfmDemodSettings()->getTitle()) {
        *response.getBfmDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getBfmDemodSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getBfmDemodSettings()->getAudioDeviceName()) {
        *response.getBfmDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getBfmDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getBfmDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getBfmDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getBfmDemodSettings()->getReverseApiAddress()) {
        *response.getBfmDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getBfmDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getBfmDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getBfmDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getBfmDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_spectrumGUI)
    {
        if (response.getBfmDemodSettings()->getSpectrumConfig())
        {
            settings.m_spectrumGUI->formatTo(response.getBfmDemodSettings()->getSpectrumConfig());
        }
        else
        {
            SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
            settings.m_spectrumGUI->formatTo(swgGLSpectrum);
            response.getBfmDemodSettings()->setSpectrumConfig(swgGLSpectrum);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getBfmDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getBfmDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getBfmDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getBfmDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getBfmDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getBfmDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void BFMDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getBfmDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getBfmDemodReport()->setSquelch(m_basebandSink->getSquelchState() > 0 ? 1 : 0);
    response.getBfmDemodReport()->setAudioSampleRate(m_basebandSink->getAudioSampleRate());
    response.getBfmDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
    response.getBfmDemodReport()->setPilotLocked(getPilotLock() ? 1 : 0);
    response.getBfmDemodReport()->setPilotPowerDb(CalcDb::dbPower(getPilotLevel()));

    if (m_settings.m_rdsActive)
    {
        response.getBfmDemodReport()->setRdsReport(new SWGSDRangel::SWGRDSReport());
        webapiFormatRDSReport(response.getBfmDemodReport()->getRdsReport());
    }
    else
    {
        response.getBfmDemodReport()->setRdsReport(0);
    }
}

void BFMDemod::webapiFormatRDSReport(SWGSDRangel::SWGRDSReport *report)
{
    report->setDemodStatus(round(getDemodQua()));
    report->setDecodStatus(round(getDecoderQua()));
    report->setRdsDemodAccumDb(CalcDb::dbPower(std::fabs(getDemodAcc())));
    report->setRdsDemodFrequency(getDemodFclk());
    report->setPid(new QString(str(boost::format("%04X") % getRDSParser().m_pi_program_identification).c_str()));
    report->setPiType(new QString(getRDSParser().pty_table[getRDSParser().m_pi_program_type].c_str()));
    report->setPiCoverage(new QString(getRDSParser().coverage_area_codes[getRDSParser().m_pi_area_coverage_index].c_str()));
    report->setProgServiceName(new QString(getRDSParser().m_g0_program_service_name));
    report->setMusicSpeech(new QString((getRDSParser().m_g0_music_speech ? "Music" : "Speech")));
    report->setMonoStereo(new QString((getRDSParser().m_g0_mono_stereo ? "Mono" : "Stereo")));
    report->setRadioText(new QString(getRDSParser().m_g2_radiotext));
    std::string time = str(boost::format("%4i-%02i-%02i %02i:%02i (%+.1fh)")\
        % (1900 + getRDSParser().m_g4_year) % getRDSParser().m_g4_month % getRDSParser().m_g4_day % getRDSParser().m_g4_hours % getRDSParser().m_g4_minutes % getRDSParser().m_g4_local_time_offset);
    report->setTime(new QString(time.c_str()));
    report->setAltFrequencies(new QList<SWGSDRangel::SWGRDSReport_altFrequencies*>);

    for (std::set<double>::iterator it = getRDSParser().m_g0_alt_freq.begin(); it != getRDSParser().m_g0_alt_freq.end(); ++it)
    {
        if (*it > 76.0)
        {
            report->getAltFrequencies()->append(new SWGSDRangel::SWGRDSReport_altFrequencies);
            report->getAltFrequencies()->back()->setFrequency(*it);
        }
    }
}

void BFMDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const BFMDemodSettings& settings, bool force)
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

void BFMDemod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const BFMDemodSettings& settings,
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

void BFMDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const BFMDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setBfmDemodSettings(new SWGSDRangel::SWGBFMDemodSettings());
    SWGSDRangel::SWGBFMDemodSettings *swgBFMDemodSettings = swgChannelSettings->getBfmDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgBFMDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgBFMDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("afBandwidth") || force) {
        swgBFMDemodSettings->setAfBandwidth(settings.m_afBandwidth);
    }
    if (channelSettingsKeys.contains("volume") || force) {
        swgBFMDemodSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("squelch") || force) {
        swgBFMDemodSettings->setSquelch(settings.m_squelch);
    }
    if (channelSettingsKeys.contains("audioStereo") || force) {
        swgBFMDemodSettings->setAudioStereo(settings.m_audioStereo ? 1 : 0);
    }
    if (channelSettingsKeys.contains("lsbStereo") || force) {
        swgBFMDemodSettings->setLsbStereo(settings.m_lsbStereo ? 1 : 0);
    }
    if (channelSettingsKeys.contains("showPilot") || force) {
        swgBFMDemodSettings->setShowPilot(settings.m_showPilot ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rdsActive") || force) {
        swgBFMDemodSettings->setRdsActive(settings.m_rdsActive ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgBFMDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgBFMDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgBFMDemodSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgBFMDemodSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_spectrumGUI && (channelSettingsKeys.contains("spectrunConfig") || force))
    {
        SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
        settings.m_spectrumGUI->formatTo(swgGLSpectrum);
        swgBFMDemodSettings->setSpectrumConfig(swgGLSpectrum);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgBFMDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgBFMDemodSettings->setRollupState(swgRollupState);
    }
}

void BFMDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "BFMDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("BFMDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void BFMDemod::handleIndexInDeviceSetChanged(int index)
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
