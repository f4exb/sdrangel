///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
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

#include "radiosondedemod.h"

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
#include "SWGChannelReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(RadiosondeDemod::MsgConfigureRadiosondeDemod, Message)
MESSAGE_CLASS_DEFINITION(RadiosondeDemod::MsgMessage, Message)

const char * const RadiosondeDemod::m_channelIdURI = "sdrangel.channel.radiosondedemod";
const char * const RadiosondeDemod::m_channelId = "RadiosondeDemod";

RadiosondeDemod::RadiosondeDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new RadiosondeDemodBaseband(this);
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
        &RadiosondeDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &RadiosondeDemod::handleIndexInDeviceSetChanged
    );
}

RadiosondeDemod::~RadiosondeDemod()
{
    qDebug("RadiosondeDemod::~RadiosondeDemod");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RadiosondeDemod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void RadiosondeDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t RadiosondeDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void RadiosondeDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void RadiosondeDemod::start()
{
    qDebug("RadiosondeDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    RadiosondeDemodBaseband::MsgConfigureRadiosondeDemodBaseband *msg = RadiosondeDemodBaseband::MsgConfigureRadiosondeDemodBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void RadiosondeDemod::stop()
{
    qDebug("RadiosondeDemod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool RadiosondeDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureRadiosondeDemod::match(cmd))
    {
        MsgConfigureRadiosondeDemod& cfg = (MsgConfigureRadiosondeDemod&) cmd;
        qDebug() << "RadiosondeDemod::handleMessage: MsgConfigureRadiosondeDemod";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "RadiosondeDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MsgMessage::match(cmd))
    {
        MsgMessage& report = (MsgMessage&)cmd;

        // Decode the message
        RS41Frame *frame = RS41Frame::decode(report.getMessage());
        RS41Subframe *subframe = nullptr;

        if (m_subframes.contains(frame->m_serial))
        {
            subframe = m_subframes.value(frame->m_serial);
        }
        else
        {
            subframe = new RS41Subframe();
            m_subframes.insert(frame->m_serial, subframe);
        }
        subframe->update(frame);

        // Forward to GUI
        if (getMessageQueueToGUI())
        {
            MsgMessage *msg = new MsgMessage(report);
            getMessageQueueToGUI()->push(msg);
        }

        // Forward to Radiosonde feature
        QList<ObjectPipe*> radiosondePipes;
        MainCore::instance()->getMessagePipes().getMessagePipes(this, "radiosonde", radiosondePipes);

        for (const auto& pipe : radiosondePipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgPacket *msg = MainCore::MsgPacket::create(this, report.getMessage(), report.getDateTime());
            messageQueue->push(msg);
        }

        // Forward via UDP
        if (m_settings.m_udpEnabled)
        {
            m_udpSocket.writeDatagram(report.getMessage().data(), report.getMessage().size(),
                                      QHostAddress(m_settings.m_udpAddress), m_settings.m_udpPort);
        }

        // Write to log file
        if (m_logFile.isOpen())
        {

            m_logStream << report.getDateTime().date().toString() << ","
                << report.getDateTime().time().toString() << ","
                << report.getMessage().toHex() << ",";

            if (frame->m_statusValid)
            {
                m_logStream << frame->m_serial << ","
                            << frame->m_frameNumber << ",";
            }
            else
            {
                m_logStream << ",,";
            }
            if (frame->m_posValid)
            {
                m_logStream << frame->m_latitude << ","
                            << frame->m_longitude << ","
                            << frame->m_height << ","
                            << frame->m_speed << ","
                            << frame->m_verticalRate << ","
                            << frame->m_heading << ","
                            ;
            }
            else
            {
                m_logStream << ",,,,,,";
            }
            if (frame->m_measValid)
            {
                m_logStream << frame->getPressureString(subframe) << ","
                            << frame->getTemperatureString(subframe) << ","
                            << frame->getHumidityString(subframe) << ",";
            }
            else
            {
                m_logStream << ",,,";
            }
            m_logStream << "\n";
        }

        delete frame;

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "RadiosondeDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

ScopeVis *RadiosondeDemod::getScopeSink()
{
    return m_basebandSink->getScopeSink();
}

void RadiosondeDemod::setCenterFrequency(qint64 frequency)
{
    RadiosondeDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRadiosondeDemod *msgToGUI = MsgConfigureRadiosondeDemod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void RadiosondeDemod::applySettings(const RadiosondeDemodSettings& settings, bool force)
{
    qDebug() << "RadiosondeDemod::applySettings:"
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

    if ((settings.m_baud != m_settings.m_baud) || force) {
        reverseAPIKeys.append("baud");
    }
    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        reverseAPIKeys.append("fmDeviation");
    }
    if ((settings.m_correlationThreshold != m_settings.m_correlationThreshold) || force) {
        reverseAPIKeys.append("correlationThreshold");
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

    RadiosondeDemodBaseband::MsgConfigureRadiosondeDemodBaseband *msg = RadiosondeDemodBaseband::MsgConfigureRadiosondeDemodBaseband::create(settings, force);
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
                qDebug() << "RadiosondeDemod::applySettings - Logging to: " << settings.m_logFilename;
                bool newFile = m_logFile.size() == 0;
                m_logStream.setDevice(&m_logFile);
                if (newFile)
                {
                    // Write header
                    m_logStream << "Date,Time,Data,Serial,Frame,Lat,Lon,Alt (m),Speed (m/s),V/R (m/s),Heading,P (hPa),T (C), U (%)\n";
                }
            }
            else
            {
                qDebug() << "RadiosondeDemod::applySettings - Unable to open log file: " << settings.m_logFilename;
            }
        }
    }

    m_settings = settings;
}

QByteArray RadiosondeDemod::serialize() const
{
    return m_settings.serialize();
}

bool RadiosondeDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureRadiosondeDemod *msg = MsgConfigureRadiosondeDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureRadiosondeDemod *msg = MsgConfigureRadiosondeDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void RadiosondeDemod::sendSampleRateToDemodAnalyzer()
{
    QList<ObjectPipe*> reportPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", reportPipes);

    for (const auto& pipe : reportPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(
            this,
            RadiosondeDemodSettings::RADIOSONDEDEMOD_CHANNEL_SAMPLE_RATE
        );
        messageQueue->push(msg);
    }
}

int RadiosondeDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRadiosondeDemodSettings(new SWGSDRangel::SWGRadiosondeDemodSettings());
    response.getRadiosondeDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int RadiosondeDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int RadiosondeDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    RadiosondeDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureRadiosondeDemod *msg = MsgConfigureRadiosondeDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("RadiosondeDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRadiosondeDemod *msgToGUI = MsgConfigureRadiosondeDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void RadiosondeDemod::webapiUpdateChannelSettings(
        RadiosondeDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("baud")) {
        settings.m_baud = response.getRadiosondeDemodSettings()->getBaud();
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getRadiosondeDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getRadiosondeDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getRadiosondeDemodSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("correlationThreshold")) {
        settings.m_correlationThreshold = response.getRadiosondeDemodSettings()->getCorrelationThreshold();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getRadiosondeDemodSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getRadiosondeDemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getRadiosondeDemodSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("logFilename")) {
        settings.m_logFilename = *response.getRadiosondeDemodSettings()->getLogFilename();
    }
    if (channelSettingsKeys.contains("logEnabled")) {
        settings.m_logEnabled = response.getRadiosondeDemodSettings()->getLogEnabled();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getRadiosondeDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getRadiosondeDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getRadiosondeDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRadiosondeDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRadiosondeDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRadiosondeDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getRadiosondeDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getRadiosondeDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_scopeGUI && channelSettingsKeys.contains("scopeConfig")) {
        settings.m_scopeGUI->updateFrom(channelSettingsKeys, response.getRadiosondeDemodSettings()->getScopeConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getRadiosondeDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getRadiosondeDemodSettings()->getRollupState());
    }
}

void RadiosondeDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const RadiosondeDemodSettings& settings)
{
    response.getRadiosondeDemodSettings()->setBaud(settings.m_baud);
    response.getRadiosondeDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getRadiosondeDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getRadiosondeDemodSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getRadiosondeDemodSettings()->setCorrelationThreshold(settings.m_correlationThreshold);
    response.getRadiosondeDemodSettings()->setUdpEnabled(settings.m_udpEnabled);
    response.getRadiosondeDemodSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getRadiosondeDemodSettings()->setUdpPort(settings.m_udpPort);
    response.getRadiosondeDemodSettings()->setLogFilename(new QString(settings.m_logFilename));
    response.getRadiosondeDemodSettings()->setLogEnabled(settings.m_logEnabled);

    response.getRadiosondeDemodSettings()->setRgbColor(settings.m_rgbColor);
    if (response.getRadiosondeDemodSettings()->getTitle()) {
        *response.getRadiosondeDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getRadiosondeDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getRadiosondeDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getRadiosondeDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRadiosondeDemodSettings()->getReverseApiAddress()) {
        *response.getRadiosondeDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRadiosondeDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRadiosondeDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRadiosondeDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getRadiosondeDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_scopeGUI)
    {
        if (response.getRadiosondeDemodSettings()->getScopeConfig())
        {
            settings.m_scopeGUI->formatTo(response.getRadiosondeDemodSettings()->getScopeConfig());
        }
        else
        {
            SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
            settings.m_scopeGUI->formatTo(swgGLScope);
            response.getRadiosondeDemodSettings()->setScopeConfig(swgGLScope);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getRadiosondeDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getRadiosondeDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getRadiosondeDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getRadiosondeDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getRadiosondeDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getRadiosondeDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void RadiosondeDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const RadiosondeDemodSettings& settings, bool force)
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

void RadiosondeDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const RadiosondeDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("RadiosondeDemod"));
    swgChannelSettings->setRadiosondeDemodSettings(new SWGSDRangel::SWGRadiosondeDemodSettings());
    SWGSDRangel::SWGRadiosondeDemodSettings *swgRadiosondeDemodSettings = swgChannelSettings->getRadiosondeDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("baud") || force) {
        swgRadiosondeDemodSettings->setBaud(settings.m_baud);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgRadiosondeDemodSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgRadiosondeDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgRadiosondeDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("correlationThreshold") || force) {
        swgRadiosondeDemodSettings->setCorrelationThreshold(settings.m_correlationThreshold);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgRadiosondeDemodSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgRadiosondeDemodSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgRadiosondeDemodSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("logFilename") || force) {
        swgRadiosondeDemodSettings->setLogFilename(new QString(settings.m_logFilename));
    }
    if (channelSettingsKeys.contains("logEnabled") || force) {
        swgRadiosondeDemodSettings->setLogEnabled(settings.m_logEnabled);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgRadiosondeDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgRadiosondeDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgRadiosondeDemodSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_scopeGUI && (channelSettingsKeys.contains("scopeConfig") || force))
    {
        SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
        settings.m_scopeGUI->formatTo(swgGLScope);
        swgRadiosondeDemodSettings->setScopeConfig(swgGLScope);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgRadiosondeDemodSettings->setChannelMarker(swgChannelMarker);
    }
}

void RadiosondeDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "RadiosondeDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("RadiosondeDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void RadiosondeDemod::handleIndexInDeviceSetChanged(int index)
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
