///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
#include <QThread>
#include <QBuffer>
#include <QNetworkReply>
#include <QNetworkAccessManager>

#include <stdio.h>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelAnalyzerSettings.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplesource.h"
#include "maincore.h"
#include "chanalyzer.h"

MESSAGE_CLASS_DEFINITION(ChannelAnalyzer::MsgConfigureChannelAnalyzer, Message)

const char* const ChannelAnalyzer::m_channelIdURI = "sdrangel.channel.chanalyzer";
const char* const ChannelAnalyzer::m_channelId = "ChannelAnalyzer";

ChannelAnalyzer::ChannelAnalyzer(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_spectrumVis(SDR_RX_SCALEF),
        m_basebandSampleRate(0)
{
    qDebug("ChannelAnalyzer::ChannelAnalyzer");
    setObjectName(m_channelId);
    getChannelSampleRate();
    m_basebandSink = new ChannelAnalyzerBaseband();
    m_basebandSink->moveToThread(&m_thread);

	applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &ChannelAnalyzer::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &ChannelAnalyzer::handleIndexInDeviceSetChanged
    );
}

ChannelAnalyzer::~ChannelAnalyzer()
{
    qDebug("ChannelAnalyzer::~ChannelAnalyzer");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &ChannelAnalyzer::networkManagerFinished
    );
    delete m_networkManager;

	m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
    qDebug("ChannelAnalyzer::~ChannelAnalyzer: done");
}

void ChannelAnalyzer::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t ChannelAnalyzer::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

int ChannelAnalyzer::getChannelSampleRate()
{
    DeviceSampleSource *source = m_deviceAPI->getSampleSource();

    if (source) {
        m_basebandSampleRate = source->getSampleRate();
    }

    return m_basebandSampleRate;
}

void ChannelAnalyzer::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) positiveOnly;
    m_basebandSink->feed(begin, end);
}

void ChannelAnalyzer::start()
{
    qDebug() << "ChannelAnalyzer::start";

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    ChannelAnalyzerBaseband::MsgConfigureChannelAnalyzerBaseband *msg =
        ChannelAnalyzerBaseband::MsgConfigureChannelAnalyzerBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if (getMessageQueueToGUI())
    {
        DSPSignalNotification *notifToGUI = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
        getMessageQueueToGUI()->push(notifToGUI);
    }
}

void ChannelAnalyzer::stop()
{
    qDebug() << "ChannelAnalyzer::stop";
	m_basebandSink->stopWork();
	m_thread.quit();
	m_thread.wait();
}

bool ChannelAnalyzer::handleMessage(const Message& cmd)
{
    if (MsgConfigureChannelAnalyzer::match(cmd))
    {
        qDebug("ChannelAnalyzer::handleMessage: MsgConfigureChannelAnalyzer");
        MsgConfigureChannelAnalyzer& cfg = (MsgConfigureChannelAnalyzer&) cmd;

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& cfg = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = cfg.getSampleRate();
        qDebug("ChannelAnalyzer::handleMessage: DSPSignalNotification: %d", m_basebandSampleRate);
        m_centerFrequency = cfg.getCenterFrequency();
        DSPSignalNotification *notif = new DSPSignalNotification(cfg);
        m_basebandSink->getInputMessageQueue()->push(notif);

        if (getMessageQueueToGUI())
        {
            DSPSignalNotification *notifToGUI = new DSPSignalNotification(cfg);
            getMessageQueueToGUI()->push(notifToGUI);
        }

        return true;
    }
	else
	{
	    return false;
	}
}

void ChannelAnalyzer::setCenterFrequency(qint64 frequency)
{
    ChannelAnalyzerSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings);
}

void ChannelAnalyzer::applySettings(const ChannelAnalyzerSettings& settings, bool force)
{
    qDebug() << "ChannelAnalyzer::applySettings:"
            << " m_rationalDownSample: " << settings.m_rationalDownSample
            << " m_rationalDownSamplerRate: " << settings.m_rationalDownSamplerRate
            << " m_rcc: " << settings.m_rrc
            << " m_rrcRolloff: " << settings.m_rrcRolloff / 100.0
            << " m_bandwidth: " << settings.m_bandwidth
            << " m_lowCutoff: " << settings.m_lowCutoff
            << " m_log2Decim: " << settings.m_log2Decim
            << " m_ssb: " << settings.m_ssb
            << " m_pll: " << settings.m_pll
            << " m_fll: " << settings.m_fll
            << " m_costasLoop: " << settings.m_costasLoop
            << " m_pllPskOrder: " << settings.m_pllPskOrder
            << " m_pllBandwidth: " << settings.m_pllBandwidth
            << " m_pllDampingFactor: " << settings.m_pllDampingFactor
            << " m_pllLoopGain: " << settings.m_pllLoopGain
            << " m_inputType: " << (int) settings.m_inputType
            << " m_useReverseAPI:" << settings.m_useReverseAPI
            << " m_reverseAPIAddress:" << settings.m_reverseAPIAddress
            << " m_reverseAPIPort:" << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex:" << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex:" << settings.m_reverseAPIChannelIndex;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_rationalDownSample != m_settings.m_rationalDownSample) || force) {
        reverseAPIKeys.append("rationalDownSample");
    }
    if ((settings.m_rationalDownSamplerRate != m_settings.m_rationalDownSamplerRate) || force) {
        reverseAPIKeys.append("rationalDownSamplerRate");
    }
    if ((settings.m_bandwidth != m_settings.m_bandwidth) || force) {
        reverseAPIKeys.append("bandwidth");
    }
    if ((settings.m_lowCutoff != m_settings.m_lowCutoff) || force) {
        reverseAPIKeys.append("lowCutoff");
    }
    if ((settings.m_log2Decim != m_settings.m_log2Decim) || force) {
        reverseAPIKeys.append("log2Decim");
    }
    if ((settings.m_lowCutoff != m_settings.m_lowCutoff) || force) {
        reverseAPIKeys.append("lowCutoff");
    }
    if ((settings.m_ssb != m_settings.m_ssb) || force) {
        reverseAPIKeys.append("ssb");
    }
    if ((settings.m_pll != m_settings.m_pll) || force) {
        reverseAPIKeys.append("pll");
    }
    if ((settings.m_fll != m_settings.m_fll) || force) {
        reverseAPIKeys.append("fll");
    }
    if ((settings.m_costasLoop != m_settings.m_costasLoop) || force) {
        reverseAPIKeys.append("costasLoop");
    }
    if ((settings.m_rrc != m_settings.m_rrc) || force) {
        reverseAPIKeys.append("rrc");
    }
    if ((settings.m_rrcRolloff != m_settings.m_rrcRolloff) || force) {
        reverseAPIKeys.append("rrcRolloff");
    }
    if ((settings.m_pllPskOrder != m_settings.m_pllPskOrder) || force) {
        reverseAPIKeys.append("pllPskOrder");
    }
    if ((settings.m_pllBandwidth != m_settings.m_pllBandwidth) || force) {
        reverseAPIKeys.append("pllBandwidth");
    }
    if ((settings.m_pllDampingFactor != m_settings.m_pllDampingFactor) || force) {
        reverseAPIKeys.append("pllDampingFactor");
    }
    if ((settings.m_pllLoopGain != m_settings.m_pllLoopGain) || force) {
        reverseAPIKeys.append("pllLoopGain");
    }
    if ((settings.m_inputType != m_settings.m_inputType) || force) {
        reverseAPIKeys.append("inputType");
    }
    if ((settings.m_rgbColor != m_settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((settings.m_title != m_settings.m_title) || force) {
        reverseAPIKeys.append("title");
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

    ChannelAnalyzerBaseband::MsgConfigureChannelAnalyzerBaseband *msg
        = ChannelAnalyzerBaseband::MsgConfigureChannelAnalyzerBaseband::create(settings, force);
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

int ChannelAnalyzer::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setChannelAnalyzerSettings(new SWGSDRangel::SWGChannelAnalyzerSettings());
    response.getChannelAnalyzerSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int ChannelAnalyzer::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int ChannelAnalyzer::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    ChannelAnalyzerSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureChannelAnalyzer *msg = MsgConfigureChannelAnalyzer::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("ChannelAnalyzer::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureChannelAnalyzer *msgToGUI = MsgConfigureChannelAnalyzer::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void ChannelAnalyzer::webapiUpdateChannelSettings(
        ChannelAnalyzerSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("frequency")) {
        settings.m_inputFrequencyOffset = response.getChannelAnalyzerSettings()->getFrequency();
    }
    if (channelSettingsKeys.contains("downSample")) {
        settings.m_rationalDownSample = response.getChannelAnalyzerSettings()->getDownSample() != 0;
    }
    if (channelSettingsKeys.contains("downSampleRate")) {
        settings.m_rationalDownSamplerRate = response.getChannelAnalyzerSettings()->getDownSampleRate();
    }
    if (channelSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = response.getChannelAnalyzerSettings()->getBandwidth();
    }
    if (channelSettingsKeys.contains("lowCutoff")) {
        settings.m_lowCutoff = response.getChannelAnalyzerSettings()->getLowCutoff();
    }
    if (channelSettingsKeys.contains("spanLog2")) {
        settings.m_log2Decim = response.getChannelAnalyzerSettings()->getSpanLog2();
    }
    if (channelSettingsKeys.contains("ssb")) {
        settings.m_ssb = response.getChannelAnalyzerSettings()->getSsb() != 0;
    }
    if (channelSettingsKeys.contains("pll")) {
        settings.m_pll = response.getChannelAnalyzerSettings()->getPll() != 0;
    }
    if (channelSettingsKeys.contains("fll")) {
        settings.m_fll = response.getChannelAnalyzerSettings()->getFll() != 0;
    }
    if (channelSettingsKeys.contains("costasLoop")) {
        settings.m_costasLoop = response.getChannelAnalyzerSettings()->getCostasLoop() != 0;
    }
    if (channelSettingsKeys.contains("rrc")) {
        settings.m_rrc = response.getChannelAnalyzerSettings()->getRrc() != 0;
    }
    if (channelSettingsKeys.contains("rrcRolloff")) {
        settings.m_rrcRolloff = response.getChannelAnalyzerSettings()->getRrcRolloff();
    }
    if (channelSettingsKeys.contains("pllPskOrder")) {
        settings.m_pllPskOrder = response.getChannelAnalyzerSettings()->getPllPskOrder();
    }
    if (channelSettingsKeys.contains("pllBandwidth")) {
        settings.m_pllBandwidth = response.getChannelAnalyzerSettings()->getPllBandwidth();
    }
    if (channelSettingsKeys.contains("pllDampingFactor")) {
        settings.m_pllDampingFactor = response.getChannelAnalyzerSettings()->getPllDampingFactor();
    }
    if (channelSettingsKeys.contains("pllLoopGain")) {
        settings.m_pllLoopGain = response.getChannelAnalyzerSettings()->getPllLoopGain();
    }
    if (channelSettingsKeys.contains("inputType")) {
        settings.m_inputType = (ChannelAnalyzerSettings::InputType) response.getChannelAnalyzerSettings()->getInputType();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getChannelAnalyzerSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getChannelAnalyzerSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getChannelAnalyzerSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getChannelAnalyzerSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getChannelAnalyzerSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getChannelAnalyzerSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getChannelAnalyzerSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getChannelAnalyzerSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_spectrumGUI && channelSettingsKeys.contains("spectrumConfig")) {
        settings.m_spectrumGUI->updateFrom(channelSettingsKeys, response.getChannelAnalyzerSettings()->getSpectrumConfig());
    }
    if (settings.m_scopeGUI && channelSettingsKeys.contains("scopeConfig")) {
        settings.m_scopeGUI->updateFrom(channelSettingsKeys, response.getChannelAnalyzerSettings()->getScopeConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getChannelAnalyzerSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getChannelAnalyzerSettings()->getRollupState());
    }
}

void ChannelAnalyzer::webapiFormatChannelSettings(
    SWGSDRangel::SWGChannelSettings& response,
    const ChannelAnalyzerSettings& settings
)
{
    response.getChannelAnalyzerSettings()->setFrequency(settings.m_inputFrequencyOffset);
    response.getChannelAnalyzerSettings()->setDownSample(settings.m_rationalDownSample ? 1 : 0);
    response.getChannelAnalyzerSettings()->setDownSampleRate(settings.m_rationalDownSamplerRate);
    response.getChannelAnalyzerSettings()->setBandwidth(settings.m_bandwidth);
    response.getChannelAnalyzerSettings()->setLowCutoff(settings.m_lowCutoff);
    response.getChannelAnalyzerSettings()->setSpanLog2(settings.m_log2Decim);
    response.getChannelAnalyzerSettings()->setSsb(settings.m_ssb ? 1 : 0);
    response.getChannelAnalyzerSettings()->setPll(settings.m_pll ? 1 : 0);
    response.getChannelAnalyzerSettings()->setFll(settings.m_fll ? 1 : 0);
    response.getChannelAnalyzerSettings()->setCostasLoop(settings.m_costasLoop ? 1 : 0);
    response.getChannelAnalyzerSettings()->setRrc(settings.m_rrc ? 1 : 0);
    response.getChannelAnalyzerSettings()->setRrcRolloff(settings.m_rrcRolloff);
    response.getChannelAnalyzerSettings()->setPllPskOrder(settings.m_pllPskOrder);
    response.getChannelAnalyzerSettings()->setPllBandwidth(settings.m_pllBandwidth);
    response.getChannelAnalyzerSettings()->setPllDampingFactor(settings.m_pllDampingFactor);
    response.getChannelAnalyzerSettings()->setPllLoopGain(settings.m_pllLoopGain);
    response.getChannelAnalyzerSettings()->setInputType((int) settings.m_inputType);
    response.getChannelAnalyzerSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getChannelAnalyzerSettings()->getTitle()) {
        *response.getChannelAnalyzerSettings()->getTitle() = settings.m_title;
    } else {
        response.getChannelAnalyzerSettings()->setTitle(new QString(settings.m_title));
    }

    response.getChannelAnalyzerSettings()->setStreamIndex(settings.m_streamIndex);
    response.getChannelAnalyzerSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getChannelAnalyzerSettings()->getReverseApiAddress()) {
        *response.getChannelAnalyzerSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getChannelAnalyzerSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getChannelAnalyzerSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getChannelAnalyzerSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getChannelAnalyzerSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_spectrumGUI)
    {
        if (response.getChannelAnalyzerSettings()->getSpectrumConfig())
        {
            settings.m_spectrumGUI->formatTo(response.getChannelAnalyzerSettings()->getSpectrumConfig());
        }
        else
        {
            SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
            settings.m_spectrumGUI->formatTo(swgGLSpectrum);
            response.getChannelAnalyzerSettings()->setSpectrumConfig(swgGLSpectrum);
        }
    }

    if (settings.m_scopeGUI)
    {
        if (response.getChannelAnalyzerSettings()->getScopeConfig())
        {
            settings.m_scopeGUI->formatTo(response.getChannelAnalyzerSettings()->getScopeConfig());
        }
        else
        {
            SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
            settings.m_scopeGUI->formatTo(swgGLScope);
            response.getChannelAnalyzerSettings()->setScopeConfig(swgGLScope);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getChannelAnalyzerSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getChannelAnalyzerSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getChannelAnalyzerSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getChannelAnalyzerSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getChannelAnalyzerSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getChannelAnalyzerSettings()->setRollupState(swgRollupState);
        }
    }
}

void ChannelAnalyzer::webapiReverseSendSettings(
    QList<QString>& channelSettingsKeys,
    const ChannelAnalyzerSettings& settings,
    bool force
)
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

void ChannelAnalyzer::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const ChannelAnalyzerSettings& settings,
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

void ChannelAnalyzer::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const ChannelAnalyzerSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setChannelAnalyzerSettings(new SWGSDRangel::SWGChannelAnalyzerSettings());
    SWGSDRangel::SWGChannelAnalyzerSettings *swgChannelAnalyzerSettings = swgChannelSettings->getChannelAnalyzerSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("frequency") || force) {
        swgChannelAnalyzerSettings->setFrequency(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("downSample")) {
        swgChannelAnalyzerSettings->setDownSample(settings.m_rationalDownSample ? 1 : 0);
    }
    if (channelSettingsKeys.contains("downSampleRate")) {
        swgChannelAnalyzerSettings->setDownSampleRate(settings.m_rationalDownSamplerRate);
    }
    if (channelSettingsKeys.contains("bandwidth")) {
        swgChannelAnalyzerSettings->setBandwidth(settings.m_bandwidth);
    }
    if (channelSettingsKeys.contains("lowCutoff")) {
        swgChannelAnalyzerSettings->setLowCutoff(settings.m_lowCutoff);
    }
    if (channelSettingsKeys.contains("spanLog2")) {
        swgChannelAnalyzerSettings->setSpanLog2(settings.m_log2Decim);
    }
    if (channelSettingsKeys.contains("ssb")) {
        swgChannelAnalyzerSettings->setSsb(settings.m_ssb ? 1 : 0);
    }
    if (channelSettingsKeys.contains("pll")) {
        swgChannelAnalyzerSettings->setPll(settings.m_pll ? 1 : 0);
    }
    if (channelSettingsKeys.contains("fll")) {
        swgChannelAnalyzerSettings->setFll(settings.m_fll ? 1 : 0);
    }
    if (channelSettingsKeys.contains("costasLoop")) {
        swgChannelAnalyzerSettings->setCostasLoop(settings.m_costasLoop ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rrc")) {
        swgChannelAnalyzerSettings->setRrc(settings.m_rrc ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rrcRolloff")) {
        swgChannelAnalyzerSettings->setRrcRolloff(settings.m_rrcRolloff);
    }
    if (channelSettingsKeys.contains("pllPskOrder")) {
        swgChannelAnalyzerSettings->setPllPskOrder(settings.m_pllPskOrder);
    }
    if (channelSettingsKeys.contains("pllBandwidth")) {
        swgChannelAnalyzerSettings->setPllBandwidth(settings.m_pllBandwidth);
    }
    if (channelSettingsKeys.contains("pllDampingFactor")) {
        swgChannelAnalyzerSettings->setPllDampingFactor(settings.m_pllDampingFactor);
    }
    if (channelSettingsKeys.contains("pllLoopGain")) {
        swgChannelAnalyzerSettings->setPllLoopGain(settings.m_pllLoopGain);
    }
    if (channelSettingsKeys.contains("inputType")) {
        swgChannelAnalyzerSettings->setInputType((int) settings.m_inputType);
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        swgChannelAnalyzerSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgChannelAnalyzerSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        swgChannelAnalyzerSettings->setStreamIndex(settings.m_streamIndex);
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        swgChannelAnalyzerSettings->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        swgChannelAnalyzerSettings->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        swgChannelAnalyzerSettings->setReverseApiPort(settings.m_reverseAPIPort);
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        swgChannelAnalyzerSettings->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        swgChannelAnalyzerSettings->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
    }

    if (settings.m_spectrumGUI && (channelSettingsKeys.contains("spectrunConfig") || force))
    {
        SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
        settings.m_spectrumGUI->formatTo(swgGLSpectrum);
        swgChannelAnalyzerSettings->setSpectrumConfig(swgGLSpectrum);
    }

    if (settings.m_scopeGUI && (channelSettingsKeys.contains("scopeConfig") || force))
    {
        SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
        settings.m_scopeGUI->formatTo(swgGLScope);
        swgChannelAnalyzerSettings->setScopeConfig(swgGLScope);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgChannelAnalyzerSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgChannelAnalyzerSettings->setRollupState(swgRollupState);
    }
}

void ChannelAnalyzer::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "ChannelAnalyzer::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("ChannelAnalyzer::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void ChannelAnalyzer::handleIndexInDeviceSetChanged(int index)
{
    if (index < 0) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
}
