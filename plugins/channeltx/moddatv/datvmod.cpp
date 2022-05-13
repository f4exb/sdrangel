///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#define BOOST_CHRONO_HEADER_ONLY
#include <boost/chrono/chrono.hpp>

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
//#include "SWGDATVModReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

#include "datvmodbaseband.h"
#include "datvmod.h"

MESSAGE_CLASS_DEFINITION(DATVMod::MsgConfigureDATVMod, Message)
MESSAGE_CLASS_DEFINITION(DATVMod::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(DATVMod::MsgConfigureSourceCenterFrequency, Message)
MESSAGE_CLASS_DEFINITION(DATVMod::MsgConfigureTsFileName, Message)
MESSAGE_CLASS_DEFINITION(DATVMod::MsgConfigureTsFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(DATVMod::MsgConfigureTsFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(DATVMod::MsgGetUDPBitrate, Message)
MESSAGE_CLASS_DEFINITION(DATVMod::MsgGetUDPBufferUtilization, Message)

const char* const DATVMod::m_channelIdURI = "sdrangel.channeltx.moddatv";
const char* const DATVMod::m_channelId = "DATVMod";

DATVMod::DATVMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new DATVModBaseband();
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DATVMod::networkManagerFinished
    );
}

DATVMod::~DATVMod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DATVMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void DATVMod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t DATVMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

void DATVMod::start()
{
    qDebug("DATVMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void DATVMod::stop()
{
    qDebug("DATVMod::stop");
    m_thread->exit();
    m_thread->wait();
}

void DATVMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool DATVMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        qDebug() << "DATVMod::handleMessage: MsgConfigureChannelizer:"
                << " getSourceSampleRate: " << cfg.getSourceSampleRate()
                << " getSourceCenterFrequency: " << cfg.getSourceCenterFrequency();

        MsgConfigureChannelizer *msg
            = MsgConfigureChannelizer::create(cfg.getSourceSampleRate(), cfg.getSourceCenterFrequency());
        m_basebandSource->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (MsgConfigureSourceCenterFrequency::match(cmd))
    {
        MsgConfigureSourceCenterFrequency& cfg = (MsgConfigureSourceCenterFrequency&) cmd;
        qDebug() << "DATVMod::handleMessage: MsgConfigureSourceCenterFrequency:"
                << " getSourceCenterFrequency: " << cfg.getSourceCenterFrequency();

        MsgConfigureChannelizer *msg
            = MsgConfigureChannelizer::create(m_basebandSource->getChannelSampleRate(), cfg.getSourceCenterFrequency());
        m_basebandSource->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (MsgConfigureDATVMod::match(cmd))
    {
        MsgConfigureDATVMod& cfg = (MsgConfigureDATVMod&) cmd;
        qDebug() << "DATVMod::handleMessage: MsgConfigureDATVMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif);
        qDebug() << "DATVMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MsgConfigureTsFileName::match(cmd))
    {
        MsgConfigureTsFileName& cfg = (MsgConfigureTsFileName&) cmd;
        MsgConfigureTsFileName *msg = MsgConfigureTsFileName::create(
                cfg.getFileName());
        m_basebandSource->getInputMessageQueue()->push(msg);

        return true;
    }
    else if (MsgConfigureTsFileSourceSeek::match(cmd))
    {
        MsgConfigureTsFileSourceSeek& cfg = (MsgConfigureTsFileSourceSeek&) cmd;
        MsgConfigureTsFileSourceSeek *rep = MsgConfigureTsFileSourceSeek::create(cfg.getPercentage());
        m_basebandSource->getInputMessageQueue()->push(rep);

        return true;
    }
    else if (MsgConfigureTsFileSourceStreamTiming::match(cmd))
    {
        MsgConfigureTsFileSourceStreamTiming *rep = MsgConfigureTsFileSourceStreamTiming::create();
        m_basebandSource->getInputMessageQueue()->push(rep);

        return true;
    }
    else if (MsgGetUDPBitrate::match(cmd))
    {
        m_basebandSource->getInputMessageQueue()->push(DATVMod::MsgGetUDPBitrate::create());

        return true;
    }
    else if (MsgGetUDPBufferUtilization::match(cmd))
    {
        m_basebandSource->getInputMessageQueue()->push(DATVMod::MsgGetUDPBufferUtilization::create());

        return true;
    }
    else
    {
        return false;
    }
}

void DATVMod::setCenterFrequency(qint64 frequency)
{
    DATVModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgConfigureDATVMod::create(settings, false));
    }
}

void DATVMod::applySettings(const DATVModSettings& settings, bool force)
{
    qDebug() << "DATVMod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_standard: " << (int) settings.m_standard
            << " m_source: " << (int) settings.m_source
            << " m_modulation: " << (int) settings.m_modulation
            << " m_fec: " << (int) settings.m_fec
            << " m_symbolRate: " << settings.m_symbolRate
            << " m_source: " << settings.m_source
            << " m_tsFileName: " << settings.m_tsFileName
            << " m_tsFilePlayLoop: " << settings.m_tsFilePlayLoop
            << " m_tsFilePlay: " << settings.m_tsFilePlay
            << " m_udpAddress: " << settings.m_udpAddress
            << " m_udpPort: " << settings.m_udpPort
            << " m_channelMute: " << settings.m_channelMute
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_standard != m_settings.m_standard) || force) {
        reverseAPIKeys.append("standard");
    }
    if ((settings.m_modulation != m_settings.m_modulation) || force) {
        reverseAPIKeys.append("modulation");
    }
    if ((settings.m_modulation != m_settings.m_modulation) || force) {
        reverseAPIKeys.append("modulation");
    }
    if ((settings.m_fec != m_settings.m_fec) || force) {
        reverseAPIKeys.append("fec");
    }
    if ((settings.m_symbolRate != m_settings.m_symbolRate) || force) {
        reverseAPIKeys.append("symbolRate");
    }
    if ((settings.m_rollOff != m_settings.m_rollOff) || force) {
        reverseAPIKeys.append("rollOff");
    }
    if ((settings.m_tsFilePlayLoop != m_settings.m_tsFilePlayLoop) || force) {
        reverseAPIKeys.append("tsSource");
    }
    if ((settings.m_tsFileName != m_settings.m_tsFileName) || force) {
        reverseAPIKeys.append("tsFileName");
    }
    if ((settings.m_tsFilePlayLoop != m_settings.m_tsFilePlayLoop) || force) {
        reverseAPIKeys.append("tsFilePlayLoop");
    }
    if ((settings.m_tsFilePlay != m_settings.m_tsFilePlay) || force) {
        reverseAPIKeys.append("tsFilePlay");
    }
    if ((settings.m_udpAddress != m_settings.m_udpAddress) || force) {
        reverseAPIKeys.append("udpAddress");
    }
    if ((settings.m_udpPort != m_settings.m_udpPort) || force) {
        reverseAPIKeys.append("udpPort");
    }
    if ((settings.m_channelMute != m_settings.m_channelMute) || force) {
        reverseAPIKeys.append("channelMute");
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

    MsgConfigureDATVMod *msg = MsgConfigureDATVMod::create(settings, force);
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

QByteArray DATVMod::serialize() const
{
    return m_settings.serialize();
}

bool DATVMod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureDATVMod *msg = MsgConfigureDATVMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureDATVMod *msg = MsgConfigureDATVMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int DATVMod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDatvModSettings(new SWGSDRangel::SWGDATVModSettings());
    response.getDatvModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int DATVMod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int DATVMod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    DATVModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    if (m_settings.m_inputFrequencyOffset != settings.m_inputFrequencyOffset)
    {
        DATVMod::MsgConfigureChannelizer *msgChan = DATVMod::MsgConfigureChannelizer::create(
            m_basebandSource->getChannelSampleRate(), settings.m_inputFrequencyOffset);
        m_inputMessageQueue.push(msgChan);
    }

    MsgConfigureDATVMod *msg = MsgConfigureDATVMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgConfigureDATVMod::create(settings, force));
    }

    if (channelSettingsKeys.contains("tsFileName"))
    {
        m_basebandSource->getInputMessageQueue()->push(MsgConfigureTsFileName::create(
                *response.getDatvModSettings()->getTsFileName()));

        if (m_guiMessageQueue)
        {
            m_guiMessageQueue->push(MsgConfigureTsFileName::create(
                    *response.getDatvModSettings()->getTsFileName()));
        }
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void DATVMod::webapiUpdateChannelSettings(
        DATVModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getDatvModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getDatvModSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("standard")) {
        settings.m_standard = (DATVModSettings::DVBStandard) response.getDatvModSettings()->getStandard();
    }
    if (channelSettingsKeys.contains("modulation")) {
        settings.m_modulation = (DATVModSettings::DATVModulation) response.getDatvModSettings()->getModulation();
    }
    if (channelSettingsKeys.contains("fec")) {
        settings.m_fec = (DATVModSettings::DATVCodeRate) response.getDatvModSettings()->getFec();
    }
    if (channelSettingsKeys.contains("symbolRate")) {
        settings.m_symbolRate = response.getDatvModSettings()->getSymbolRate();
    }
    if (channelSettingsKeys.contains("rollOff")) {
        settings.m_rollOff = response.getDatvModSettings()->getRollOff();
    }
    if (channelSettingsKeys.contains("tsSource")) {
        settings.m_source = (DATVModSettings::DATVSource) response.getDatvModSettings()->getTsSource();
    }
    if (channelSettingsKeys.contains("tsFileName")) {
        settings.m_tsFileName = *response.getDatvModSettings()->getTsFileName();
    }
    if (channelSettingsKeys.contains("tsFilePlayLoop")) {
        settings.m_tsFilePlayLoop = response.getDatvModSettings()->getTsFilePlayLoop() != 0;
    }
    if (channelSettingsKeys.contains("tsFilePlay")) {
        settings.m_tsFilePlay = response.getDatvModSettings()->getTsFilePlay() != 0;
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getDatvModSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getDatvModSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getDatvModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getDatvModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getDatvModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getDatvModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getDatvModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getDatvModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getDatvModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getDatvModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getDatvModSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getDatvModSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getDatvModSettings()->getRollupState());
    }
}

int DATVMod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDatvModReport(new SWGSDRangel::SWGDATVModReport());
    response.getDatvModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void DATVMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DATVModSettings& settings)
{
    response.getDatvModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getDatvModSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getDatvModSettings()->setStandard((int)settings.m_standard);
    response.getDatvModSettings()->setModulation((int)settings.m_modulation);
    response.getDatvModSettings()->setFec((int)settings.m_fec);
    response.getDatvModSettings()->setSymbolRate(settings.m_symbolRate);
    response.getDatvModSettings()->setRollOff(settings.m_rollOff);
    response.getDatvModSettings()->setTsSource(settings.m_source);
    response.getDatvModSettings()->setTsFileName(new QString(settings.m_tsFileName));
    response.getDatvModSettings()->setTsFilePlayLoop(settings.m_tsFilePlayLoop ? 1 : 0);
    response.getDatvModSettings()->setTsFilePlay(settings.m_tsFilePlay ? 1 : 0);
    response.getDatvModSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getDatvModSettings()->setUdpPort(settings.m_udpPort);

    response.getDatvModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getDatvModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getDatvModSettings()->getTitle()) {
        *response.getDatvModSettings()->getTitle() = settings.m_title;
    } else {
        response.getDatvModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getDatvModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getDatvModSettings()->getReverseApiAddress()) {
        *response.getDatvModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getDatvModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getDatvModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getDatvModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getDatvModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getDatvModSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getDatvModSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getDatvModSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getDatvModSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getDatvModSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getDatvModSettings()->setRollupState(swgRollupState);
        }
    }
}

void DATVMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getDatvModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    int tsBitrate = 0, tsSize = 0, channelSampleRate = 0, dataRate = 0;
    int64_t udpBytes = 0;

    if (m_basebandSource)
    {
        channelSampleRate = m_basebandSource->getChannelSampleRate();
        m_basebandSource->geTsFileInfos(tsBitrate, tsSize);
        udpBytes = m_basebandSource->getUdpByteCount();
        dataRate = m_basebandSource->getDataRate();
    }

    response.getDatvModReport()->setChannelSampleRate(channelSampleRate);
    response.getDatvModReport()->setDataRate(dataRate);
    response.getDatvModReport()->setTsFileBitrate(tsBitrate);
    response.getDatvModReport()->setTsFileLength(tsSize);
    response.getDatvModReport()->setUdpByteCount(udpBytes);
}

void DATVMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const DATVModSettings& settings, bool force)
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

void DATVMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const DATVModSettings& settings,
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

void DATVMod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DATVModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setDatvModSettings(new SWGSDRangel::SWGDATVModSettings());
    SWGSDRangel::SWGDATVModSettings *swgDATVModSettings = swgChannelSettings->getDatvModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgDATVModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgDATVModSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("standard") || force) {
        swgDATVModSettings->setStandard((int) settings.m_standard);
    }
    if (channelSettingsKeys.contains("modulation") || force) {
        swgDATVModSettings->setModulation((int) settings.m_modulation);
    }
    if (channelSettingsKeys.contains("fec") || force) {
        swgDATVModSettings->setFec((int) settings.m_fec);
    }
    if (channelSettingsKeys.contains("symbolRate") || force) {
        swgDATVModSettings->setSymbolRate((int) settings.m_symbolRate);
    }
    if (channelSettingsKeys.contains("tsSource") || force) {
        swgDATVModSettings->setTsSource((int) settings.m_source);
    }
    if (channelSettingsKeys.contains("tsFileName") || force) {
        swgDATVModSettings->setTsFileName(new QString(settings.m_tsFileName));
    }
    if (channelSettingsKeys.contains("tsFilePlayLoop") || force) {
        swgDATVModSettings->setTsFilePlayLoop(settings.m_tsFilePlayLoop ? 1 : 0);
    }
    if (channelSettingsKeys.contains("tsFilePlay") || force) {
        swgDATVModSettings->setTsFilePlay(settings.m_tsFilePlay ? 1 : 0);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgDATVModSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgDATVModSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgDATVModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgDATVModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgDATVModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgDATVModSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgDATVModSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgDATVModSettings->setRollupState(swgRollupState);
    }
}

void DATVMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "DATVMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("DATVMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double DATVMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

int DATVMod::getEffectiveSampleRate() const
{
    return m_basebandSource->getEffectiveSampleRate();
}

void DATVMod::setMessageQueueToGUI(MessageQueue* queue) {
    ChannelAPI::setMessageQueueToGUI(queue);
    m_basebandSource->setMessageQueueToGUI(queue);
}
