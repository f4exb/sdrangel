///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include "ilsdemod.h"

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include <stdio.h>
#include <complex.h>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGILSDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGMapItem.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/morsedemod.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(ILSDemod::MsgConfigureILSDemod, Message)
MESSAGE_CLASS_DEFINITION(ILSDemod::MsgAngleEstimate, Message)

const char * const ILSDemod::m_channelIdURI = "sdrangel.channel.ilsdemod";
const char * const ILSDemod::m_channelId = "ILSDemod";

ILSDemod::ILSDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_running(false),
        m_spectrumVis(SDR_RX_SCALEF),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new ILSDemodBaseband(this);
    m_basebandSink->setMessageQueueToChannel(getInputMessageQueue());
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &ILSDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &ILSDemod::handleIndexInDeviceSetChanged
    );
}

ILSDemod::~ILSDemod()
{
    qDebug("ILSDemod::~ILSDemod");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &ILSDemod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void ILSDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t ILSDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void ILSDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;

    if (m_running) {
        m_basebandSink->feed(begin, end);
    }
}

void ILSDemod::start()
{
    if (m_running) {
        return;
    }

    qDebug("ILSDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_basebandSink->setSpectrumSink(&m_spectrumVis);
    m_thread.start();
    // FIXME: Threading!! Compare to SSB

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    ILSDemodBaseband::MsgConfigureILSDemodBaseband *msg = ILSDemodBaseband::MsgConfigureILSDemodBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);

    m_running = true;
}

void ILSDemod::stop()
{
    if (!m_running) {
        return;
    }

    qDebug("ILSDemod::stop");
    m_running = false;
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool ILSDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureILSDemod::match(cmd))
    {
        MsgConfigureILSDemod& cfg = (MsgConfigureILSDemod&) cmd;
        qDebug() << "ILSDemod::handleMessage: MsgConfigureILSDemod";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        qDebug() << "ILSDemod::handleMessage: DSPSignalNotification";
        // Forward to the sink
        if (m_running)
        {
            DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
            m_basebandSink->getInputMessageQueue()->push(rep);
        }
        // Forward to GUI if any
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MorseDemod::MsgReportIdent::match(cmd))
    {
        MorseDemod::MsgReportIdent& report = (MorseDemod::MsgReportIdent&) cmd;

        // Forward to GUI
        if (m_guiMessageQueue)
        {
            MorseDemod::MsgReportIdent *msg = new MorseDemod::MsgReportIdent(report);
            m_guiMessageQueue->push(msg);
        }

        return true;
    }
    else if (ILSDemod::MsgAngleEstimate::match(cmd))
    {
        // Forward to GUI
        ILSDemod::MsgAngleEstimate& report = (ILSDemod::MsgAngleEstimate&)cmd;
        if (getMessageQueueToGUI())
        {
            ILSDemod::MsgAngleEstimate *msg = new ILSDemod::MsgAngleEstimate(report);
            getMessageQueueToGUI()->push(msg);
        }

        // Forward via UDP
        if (m_settings.m_udpEnabled)
        {
            QString ddm = QString::number(report.getDDM(), 'f', 3);
            QByteArray bytes = ddm.toUtf8();
            m_udpSocket.writeDatagram(bytes, bytes.size(),
                                      QHostAddress(m_settings.m_udpAddress), m_settings.m_udpPort);
        }

        // Write to log file
        if (m_logFile.isOpen())
        {
            float stationLatitude = MainCore::instance()->getSettings().getLatitude();
            float stationLongitude = MainCore::instance()->getSettings().getLongitude();
            float stationAltitude = MainCore::instance()->getSettings().getAltitude();

            QDateTime dateTime = QDateTime::currentDateTime();
            m_logStream << dateTime.date().toString()
                        << "," << dateTime.time().toString()
                        << "," << stationLatitude
                        << "," << stationLongitude
                        << "," << stationAltitude
                        << "," << report.getModDepth90()
                        << "," << report.getModDepth150()
                        << "," << report.getSDM()
                        << "," << report.getDDM()
                        << "," << report.getAngle()
                        << "," << report.getPowerCarrier()
                        << "," << report.getPower90()
                        << "," << report.getPower150()
                        << "\n";
        }

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "ILSDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

ScopeVis *ILSDemod::getScopeSink()
{
    return m_basebandSink->getScopeSink();
}

void ILSDemod::setCenterFrequency(qint64 frequency)
{
    ILSDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureILSDemod *msgToGUI = MsgConfigureILSDemod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void ILSDemod::applySettings(const ILSDemodSettings& settings, bool force)
{
    qDebug() << "ILSDemod::applySettings:"
            << " m_logEnabled: " << settings.m_logEnabled
            << " m_logFilename: " << settings.m_logFilename
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_mode != m_settings.m_mode) || force) {
        reverseAPIKeys.append("mode");
    }
    if ((settings.m_frequencyIndex != m_settings.m_frequencyIndex) || force) {
        reverseAPIKeys.append("frequencyIndex");
    }
    if ((settings.m_squelch != m_settings.m_squelch) || force) {
        reverseAPIKeys.append("squelch");
    }
    if ((settings.m_volume != m_settings.m_volume) || force) {
        reverseAPIKeys.append("volume");
    }
    if ((settings.m_audioMute != m_settings.m_audioMute) || force) {
        reverseAPIKeys.append("audioMute");
    }
    if ((settings.m_average != m_settings.m_average) || force) {
        reverseAPIKeys.append("average");
    }
    if ((settings.m_ddmUnits != m_settings.m_ddmUnits) || force) {
        reverseAPIKeys.append("ddmUnits");
    }
    if ((settings.m_identThreshold != m_settings.m_identThreshold) || force) {
        reverseAPIKeys.append("identThreshold");
    }
    if ((settings.m_ident != m_settings.m_ident) || force) {
        reverseAPIKeys.append("ident");
    }
    if ((settings.m_runway != m_settings.m_runway) || force) {
        reverseAPIKeys.append("runway");
    }
    if ((settings.m_trueBearing != m_settings.m_trueBearing) || force) {
        reverseAPIKeys.append("trueBearing");
    }
    if ((settings.m_latitude != m_settings.m_latitude) || force) {
        reverseAPIKeys.append("latitude");
    }
    if ((settings.m_longitude != m_settings.m_longitude) || force) {
        reverseAPIKeys.append("longitude");
    }
    if ((settings.m_elevation != m_settings.m_elevation) || force) {
        reverseAPIKeys.append("elevation");
    }
    if ((settings.m_glidePath != m_settings.m_glidePath) || force) {
        reverseAPIKeys.append("glidePath");
    }
    if ((settings.m_refHeight != m_settings.m_refHeight) || force) {
        reverseAPIKeys.append("refHeight");
    }
    if ((settings.m_courseWidth != m_settings.m_courseWidth) || force) {
        reverseAPIKeys.append("courseWidth");
    }
    if ((settings.m_udpEnabled != m_settings.m_udpEnabled) || force) {
        reverseAPIKeys.append("udpEnabled");
    }
    if ((settings.m_udpAddress != m_settings.m_udpAddress) || force) {
        reverseAPIKeys.append("udpAddress");
    }
    if ((settings.m_udpPort != m_settings.m_udpPort) || force) {
        reverseAPIKeys.append("udpPort");
    }
    if ((settings.m_logFilename != m_settings.m_logFilename) || force) {
        reverseAPIKeys.append("logFilename");
    }
    if ((settings.m_logEnabled != m_settings.m_logEnabled) || force) {
        reverseAPIKeys.append("logEnabled");
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

    if (m_running)
    {
        ILSDemodBaseband::MsgConfigureILSDemodBaseband *msg = ILSDemodBaseband::MsgConfigureILSDemodBaseband::create(settings, force);
        m_basebandSink->getInputMessageQueue()->push(msg);
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    if ((settings.m_logEnabled != m_settings.m_logEnabled)
        || (settings.m_logFilename != m_settings.m_logFilename)
        || force)
    {
        if (m_logFile.isOpen())
        {
            m_logStream.flush();
            m_logFile.close();
        }
        if (settings.m_logEnabled && !settings.m_logFilename.isEmpty())
        {
            m_logFile.setFileName(settings.m_logFilename);
            if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            {
                qDebug() << "ILSDemod::applySettings - Logging to: " << settings.m_logFilename;
                m_logStream.setDevice(&m_logFile);
                bool newFile = m_logFile.size() == 0;
                if (newFile)
                {
                    // Write header
                    m_logStream << "Date,Time,Latitude,Longitude,Height,MD90,MD150,SDM,DDM,Angle,Carrier(dB),90Hz(dB),150Hz(dB)\n";
                }
            }
            else
            {
                qDebug() << "ILSDemod::applySettings - Unable to open log file: " << settings.m_logFilename;
            }
        }
    }

    m_settings = settings;
}

void ILSDemod::sendSampleRateToDemodAnalyzer()
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(
                this,
                ILSDemodSettings::ILSDEMOD_CHANNEL_SAMPLE_RATE
            );
            messageQueue->push(msg);
        }
    }
}

QByteArray ILSDemod::serialize() const
{
    return m_settings.serialize();
}

bool ILSDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureILSDemod *msg = MsgConfigureILSDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureILSDemod *msg = MsgConfigureILSDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int ILSDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIlsDemodSettings(new SWGSDRangel::SWGILSDemodSettings());
    response.getIlsDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int ILSDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int ILSDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    ILSDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureILSDemod *msg = MsgConfigureILSDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("ILSDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureILSDemod *msgToGUI = MsgConfigureILSDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int ILSDemod::webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setIlsDemodReport(new SWGSDRangel::SWGILSDemodReport());
    response.getIlsDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void ILSDemod::webapiUpdateChannelSettings(
        ILSDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getIlsDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getIlsDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("mode")) {
        settings.m_mode = (ILSDemodSettings::Mode) response.getIlsDemodSettings()->getMode();
    }
    if (channelSettingsKeys.contains("frequencyIndex")) {
        settings.m_frequencyIndex = response.getIlsDemodSettings()->getFrequencyIndex();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getIlsDemodSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getIlsDemodSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getIlsDemodSettings()->getAudioMute();
    }
    if (channelSettingsKeys.contains("average")) {
        settings.m_average = response.getIlsDemodSettings()->getAverage();
    }
    if (channelSettingsKeys.contains("ddmUnits")) {
        settings.m_ddmUnits = (ILSDemodSettings::DDMUnits) response.getIlsDemodSettings()->getDdmUnits();
    }
    if (channelSettingsKeys.contains("identThreshold")) {
        settings.m_identThreshold = response.getIlsDemodSettings()->getIdentThreshold();
    }
    if (channelSettingsKeys.contains("ident")) {
        settings.m_ident = *response.getIlsDemodSettings()->getIdent();
    }
    if (channelSettingsKeys.contains("runway")) {
        settings.m_runway = *response.getIlsDemodSettings()->getRunway();
    }
    if (channelSettingsKeys.contains("trueBearing")) {
        settings.m_trueBearing = response.getIlsDemodSettings()->getTrueBearing();
    }
    if (channelSettingsKeys.contains("latitude")) {
        settings.m_latitude = *response.getIlsDemodSettings()->getLatitude();
    }
    if (channelSettingsKeys.contains("longitude")) {
        settings.m_longitude = *response.getIlsDemodSettings()->getLongitude();
    }
    if (channelSettingsKeys.contains("elevation")) {
        settings.m_elevation = response.getIlsDemodSettings()->getElevation();
    }
    if (channelSettingsKeys.contains("glidePath")) {
        settings.m_glidePath = response.getIlsDemodSettings()->getGlidePath();
    }
    if (channelSettingsKeys.contains("refHeight")) {
        settings.m_refHeight = response.getIlsDemodSettings()->getRefHeight();
    }
    if (channelSettingsKeys.contains("courseWidth")) {
        settings.m_courseWidth = response.getIlsDemodSettings()->getCourseWidth();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getIlsDemodSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getIlsDemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getIlsDemodSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("logFilename")) {
        settings.m_logFilename = *response.getAdsbDemodSettings()->getLogFilename();
    }
    if (channelSettingsKeys.contains("logEnabled")) {
        settings.m_logEnabled = response.getAdsbDemodSettings()->getLogEnabled();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getIlsDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getIlsDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getIlsDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getIlsDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getIlsDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getIlsDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getIlsDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getIlsDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_scopeGUI && channelSettingsKeys.contains("scopeConfig")) {
        settings.m_scopeGUI->updateFrom(channelSettingsKeys, response.getIlsDemodSettings()->getScopeConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getIlsDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getIlsDemodSettings()->getRollupState());
    }
}

void ILSDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const ILSDemodSettings& settings)
{
    response.getIlsDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getIlsDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getIlsDemodSettings()->setMode((int) settings.m_mode);
    response.getIlsDemodSettings()->setFrequencyIndex(settings.m_frequencyIndex);
    response.getIlsDemodSettings()->setSquelch(settings.m_squelch);
    response.getIlsDemodSettings()->setVolume(settings.m_volume);
    response.getIlsDemodSettings()->setAudioMute(settings.m_audioMute);
    response.getIlsDemodSettings()->setAverage(settings.m_average);
    response.getIlsDemodSettings()->setDdmUnits((int) settings.m_ddmUnits);
    response.getIlsDemodSettings()->setIdentThreshold(settings.m_identThreshold);
    response.getIlsDemodSettings()->setIdent(new QString(settings.m_ident));
    response.getIlsDemodSettings()->setRunway(new QString(settings.m_runway));
    response.getIlsDemodSettings()->setTrueBearing(settings.m_trueBearing);
    response.getIlsDemodSettings()->setLatitude(new QString(settings.m_latitude));
    response.getIlsDemodSettings()->setLatitude(new QString(settings.m_latitude));
    response.getIlsDemodSettings()->setElevation(settings.m_elevation);
    response.getIlsDemodSettings()->setGlidePath(settings.m_glidePath);
    response.getIlsDemodSettings()->setRefHeight(settings.m_refHeight);
    response.getIlsDemodSettings()->setCourseWidth(settings.m_courseWidth);
    response.getIlsDemodSettings()->setUdpEnabled(settings.m_udpEnabled);
    response.getIlsDemodSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getIlsDemodSettings()->setUdpPort(settings.m_udpPort);
    response.getIlsDemodSettings()->setLogFilename(new QString(settings.m_logFilename));
    response.getIlsDemodSettings()->setLogEnabled(settings.m_logEnabled);

    response.getIlsDemodSettings()->setRgbColor(settings.m_rgbColor);
    if (response.getIlsDemodSettings()->getTitle()) {
        *response.getIlsDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getIlsDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getIlsDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getIlsDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getIlsDemodSettings()->getReverseApiAddress()) {
        *response.getIlsDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getIlsDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getIlsDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getIlsDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getIlsDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_scopeGUI)
    {
        if (response.getIlsDemodSettings()->getScopeConfig())
        {
            settings.m_scopeGUI->formatTo(response.getIlsDemodSettings()->getScopeConfig());
        }
        else
        {
            SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
            settings.m_scopeGUI->formatTo(swgGLScope);
            response.getIlsDemodSettings()->setScopeConfig(swgGLScope);
        }
    }
    if (settings.m_channelMarker)
    {
        if (response.getIlsDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getIlsDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getIlsDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getIlsDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getIlsDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getIlsDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void ILSDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getIlsDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getIlsDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
}

void ILSDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const ILSDemodSettings& settings, bool force)
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

void ILSDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const ILSDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("ILSDemod"));
    swgChannelSettings->setIlsDemodSettings(new SWGSDRangel::SWGILSDemodSettings());
    SWGSDRangel::SWGILSDemodSettings *swgILSDemodSettings = swgChannelSettings->getIlsDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgILSDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgILSDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("mode") || force) {
        swgILSDemodSettings->setMode((int) settings.m_mode);
    }
    if (channelSettingsKeys.contains("frequencyIndex") || force) {
        swgILSDemodSettings->setFrequencyIndex(settings.m_frequencyIndex);
    }
    if (channelSettingsKeys.contains("squelch") || force) {
        swgILSDemodSettings->setSquelch(settings.m_squelch);
    }
    if (channelSettingsKeys.contains("volume") || force) {
        swgILSDemodSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgILSDemodSettings->setAudioMute(settings.m_audioMute);
    }
    if (channelSettingsKeys.contains("average") || force) {
        swgILSDemodSettings->setAverage(settings.m_average);
    }
    if (channelSettingsKeys.contains("ddmUnits") || force) {
        swgILSDemodSettings->setDdmUnits((int) settings.m_ddmUnits);
    }
    if (channelSettingsKeys.contains("identThreshold") || force) {
        swgILSDemodSettings->setIdentThreshold(settings.m_identThreshold);
    }
    if (channelSettingsKeys.contains("ident") || force) {
        swgILSDemodSettings->setIdent(new QString(settings.m_ident));
    }
    if (channelSettingsKeys.contains("runway") || force) {
        swgILSDemodSettings->setRunway(new QString(settings.m_runway));
    }
    if (channelSettingsKeys.contains("trueBearing") || force) {
        swgILSDemodSettings->setTrueBearing(settings.m_trueBearing);
    }
    if (channelSettingsKeys.contains("latitude") || force) {
        swgILSDemodSettings->setLatitude(new QString(settings.m_latitude));
    }
    if (channelSettingsKeys.contains("longitude") || force) {
        swgILSDemodSettings->setLongitude(new QString(settings.m_longitude));
    }
    if (channelSettingsKeys.contains("elevation") || force) {
        swgILSDemodSettings->setElevation(settings.m_elevation);
    }
    if (channelSettingsKeys.contains("glidePath") || force) {
        swgILSDemodSettings->setGlidePath(settings.m_glidePath);
    }
    if (channelSettingsKeys.contains("refHeight") || force) {
        swgILSDemodSettings->setRefHeight(settings.m_refHeight);
    }
    if (channelSettingsKeys.contains("courseWidth") || force) {
        swgILSDemodSettings->setCourseWidth(settings.m_courseWidth);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgILSDemodSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgILSDemodSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgILSDemodSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("logFilename") || force) {
        swgILSDemodSettings->setLogFilename(new QString(settings.m_logFilename));
    }
    if (channelSettingsKeys.contains("logEnabled") || force) {
        swgILSDemodSettings->setLogEnabled(settings.m_logEnabled);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgILSDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgILSDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgILSDemodSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_scopeGUI && (channelSettingsKeys.contains("scopeConfig") || force))
    {
        SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
        settings.m_scopeGUI->formatTo(swgGLScope);
        swgILSDemodSettings->setScopeConfig(swgGLScope);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgILSDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgILSDemodSettings->setRollupState(swgRollupState);
    }
}

void ILSDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "ILSDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("ILSDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void ILSDemod::handleIndexInDeviceSetChanged(int index)
{
    if (!m_running || (index < 0)) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
}

