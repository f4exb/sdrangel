///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

#include <stdio.h>
#include <complex.h>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGSELCALDemodSettings.h"
#include "SWGChannelReport.h"

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

#include "selcaldemod.h"

MESSAGE_CLASS_DEFINITION(SELCALDemod::MsgConfigureSELCALDemod, Message)

const char *const SELCALDemod::m_channelIdURI = "sdrangel.channel.selcaldemod";
const char *const SELCALDemod::m_channelId = "SELCALDemod";

SELCALDemod::SELCALDemod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_RX_SCALEF),
    m_basebandSampleRate(0),
    m_centerFrequency(0)
{
    setObjectName(m_channelId);
    m_thread.setObjectName("SELCALDemodBB");

    m_basebandSink = new SELCALDemodBaseband(this);
    m_basebandSink->setMessageQueueToChannel(getInputMessageQueue());
    m_basebandSink->setSpectrumSink(&m_spectrumVis);
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);

    applySettings(QStringList(), m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(m_networkManager, &QNetworkAccessManager::finished, this, &SELCALDemod::networkManagerFinished);
    QObject::connect(this, &ChannelAPI::indexInDeviceSetChanged, this, &SELCALDemod::handleIndexInDeviceSetChanged);
}

SELCALDemod::~SELCALDemod()
{
    qDebug("SELCALDemod::~SELCALDemod");
    QObject::disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &SELCALDemod::networkManagerFinished);
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true, m_settings.m_streamIndex);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void SELCALDemod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this, false, m_settings.m_streamIndex);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this, m_settings.m_streamIndex);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

uint32_t SELCALDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void SELCALDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) positiveOnly;
    m_basebandSink->feed(begin, end);
}

void SELCALDemod::start()
{
    qDebug("SELCALDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    SELCALDemodBaseband::MsgConfigureSELCALDemodBaseband *msg =
        SELCALDemodBaseband::MsgConfigureSELCALDemodBaseband::create(QStringList(), m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void SELCALDemod::stop()
{
    qDebug("SELCALDemod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool SELCALDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureSELCALDemod::match(cmd))
    {
        MsgConfigureSELCALDemod& cfg = (MsgConfigureSELCALDemod&)cmd;
        qDebug() << "SELCALDemod::handleMessage: MsgConfigureSELCALDemod";
        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&)cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        // Forward to the sink
        DSPSignalNotification *rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "SELCALDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MsgMessage::match(cmd))
    {
        const auto& call = static_cast<const MsgMessage&>(cmd).getMessage();
        {
            QMutexLocker lock(&m_reportMutex);
            m_lastCall = call;
            ++m_callCount;
        }
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(MsgMessage::create(call));
        }
        sendAircraftReport(call);
        if (m_settings.m_udpEnabled)
        {
            QJsonObject json;
            json["code"] = call.m_code;
            json["utc"] = call.m_dateTime.toString(Qt::ISODateWithMs);
            json["frequencyHz"] = call.m_frequency;
            json["rssi"] = call.m_rssi;
            json["snr"] = call.m_snr;
            json["toneOffsetHz"] = call.m_toneFrequencyOffset;
            if (m_udpHostAddress.isNull()) {
                qWarning() << "SELCALDemod: Unable to send UDP to unresolved address" << m_settings.m_udpAddress;
            } else {
                m_udpSocket.writeDatagram(QJsonDocument(json).toJson(QJsonDocument::Compact), m_udpHostAddress, m_settings.m_udpPort);
            }
        }
        if (m_logFile.isOpen())
        {
            m_logStream << call.m_dateTime.toString(Qt::ISODateWithMs) << "," << call.m_code << "," << call.m_frequency << ","
                        << call.m_rssi << "," << call.m_snr << "\n";
            m_logStream.flush();
        }
        return true;
    }
    else if (MsgStatus::match(cmd))
    {
        const auto& status = static_cast<const MsgStatus&>(cmd).getStatus();
        {
            QMutexLocker lock(&m_reportMutex);
            m_status = status;
        }
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(MsgStatus::create(status));
        }
        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "SELCALDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

void SELCALDemod::setCenterFrequency(qint64 frequency)
{
    SELCALDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList("inputFrequencyOffset"), settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSELCALDemod *msgToGUI = MsgConfigureSELCALDemod::create(QStringList("inputFrequencyOffset"), settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void SELCALDemod::applySettings(const QStringList& settingsKeys, const SELCALDemodSettings& incomingSettings, bool force)
{
    SELCALDemodSettings settings = incomingSettings;
    settings.normalize();
    settings.m_streamIndex = std::min(settings.m_streamIndex, std::max(0, static_cast<int>(getNumberOfDeviceStreams()) - 1));
    qDebug() << "SELCALDemod::applySettings:" << settings.getDebugString(settingsKeys, force);

    if ((settingsKeys.contains("streamIndex") || force) && (settings.m_streamIndex != m_settings.m_streamIndex)) {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSinkAPI(this);
            m_deviceAPI->removeChannelSink(this, false, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSink(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSinkAPI(this);
            m_settings.m_streamIndex = settings.m_streamIndex; // make sure ChannelAPI::getStreamIndex() is consistent
            emit streamIndexChanged(settings.m_streamIndex);
        }
    }

    SELCALDemodBaseband::MsgConfigureSELCALDemodBaseband *msg =
        SELCALDemodBaseband::MsgConfigureSELCALDemodBaseband::create(settingsKeys, settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate =
            ((settingsKeys.contains("useReverseAPI") && (settings.m_useReverseAPI != m_settings.m_useReverseAPI)) &&
             settings.m_useReverseAPI) ||
            (settingsKeys.contains("reverseAPIAddress") && (settings.m_reverseAPIAddress != m_settings.m_reverseAPIAddress)) ||
            (settingsKeys.contains("reverseAPIPort") && (settings.m_reverseAPIPort != m_settings.m_reverseAPIPort)) ||
            (settingsKeys.contains("reverseAPIDeviceIndex") && (settings.m_reverseAPIDeviceIndex != m_settings.m_reverseAPIDeviceIndex)) ||
            (settingsKeys.contains("reverseAPIChannelIndex") && (settings.m_reverseAPIChannelIndex != m_settings.m_reverseAPIChannelIndex));
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if ((settingsKeys.contains("logEnabled") && (settings.m_logEnabled != m_settings.m_logEnabled)) ||
        (settingsKeys.contains("logFilename") && (settings.m_logFilename != m_settings.m_logFilename)) || force)
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
                qDebug() << "SELCALDemod::applySettings - Logging to: " << settings.m_logFilename;
                bool newFile = m_logFile.size() == 0;
                m_logStream.setDevice(&m_logFile);
                if (newFile) {
                    // Write header
                    m_logStream << "UTC,Code,FrequencyHz,RSSI,SNR\n";
                }
            }
            else
            {
                qWarning() << "SELCALDemod::applySettings - Unable to open log file: " << settings.m_logFilename << m_logFile.errorString();
            }
        }
    }

    bool resolve = (settingsKeys.contains("udpAddress") && (settings.m_udpAddress != m_settings.m_udpAddress)) || force;

    if (force) {
        m_settings = settings;
    }
    else
    {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (resolve) {
        resolveUDPAddress(m_settings.m_udpAddress);
    }
}

// The UDP address may be a host name, which is looked up without blocking.
void SELCALDemod::resolveUDPAddress(const QString& address)
{
    m_udpHostAddress = QHostAddress(address);
    if (m_udpHostAddress.isNull() && !address.isEmpty()) {
        QHostInfo::lookupHost(address, this, &SELCALDemod::udpAddressResolved);
    }
}

void SELCALDemod::udpAddressResolved(const QHostInfo& info)
{
    // Ignore the result of a lookup for an address since replaced
    if (info.hostName() != m_settings.m_udpAddress) {
        return;
    }
    if (info.error() != QHostInfo::NoError || info.addresses().isEmpty())
    {
        qWarning() << "SELCALDemod::udpAddressResolved: Unable to resolve" << info.hostName() << info.errorString();
        return;
    }
    // Prefer IPv4, which receivers more commonly listen on
    m_udpHostAddress = info.addresses().first();
    for (const auto& address : info.addresses())
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol)
        {
            m_udpHostAddress = address;
            break;
        }
    }
}

void SELCALDemod::sendSampleRateToDemodAnalyzer()
{
    QList<ObjectPipe *> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    if (pipes.size() > 0) {
        for (const auto& pipe : pipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue *>(pipe->m_element);
            // Sample rate of the demod stream, which is the audio output rate.
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(this, getAudioSampleRate());
            messageQueue->push(msg);
        }
    }
}

// Tell the Aircraft feature about the call.
void SELCALDemod::sendAircraftReport(const SELCALMessages::Call& call)
{
    QList<ObjectPipe *> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "aircraftreport", pipes);

    if (pipes.isEmpty()) {
        return;
    }

    AircraftReport report;
    report.m_protocol = AircraftReport::SELCAL;
    report.m_selcalCode = call.m_code;
    report.m_frequency = call.m_frequency;
    report.m_received = call.m_dateTime;
    report.m_uplink = true;     // A ground station calling an aircraft
    report.m_label = "SELCAL";
    report.m_deviceSetIndex = getDeviceSetIndex();
    report.m_channelIndex = getIndexInDeviceSet();

    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue *>(pipe->m_element);
        messageQueue->push(MainCore::MsgAircraftReport::create(this, report));
    }
}

QByteArray SELCALDemod::serialize() const
{
    return m_settings.serialize();
}

bool SELCALDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureSELCALDemod *msg = MsgConfigureSELCALDemod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureSELCALDemod *msg = MsgConfigureSELCALDemod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int SELCALDemod::webapiSettingsGet(SWGSDRangel::SWGChannelSettings& response, QString& errorMessage)
{
    (void)errorMessage;
    response.setSelcalDemodSettings(new SWGSDRangel::SWGSELCALDemodSettings());
    response.getSelcalDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int SELCALDemod::webapiWorkspaceGet(SWGSDRangel::SWGWorkspaceInfo& response, QString& errorMessage)
{
    (void)errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int SELCALDemod::webapiSettingsPutPatch(bool force, const QStringList& channelSettingsKeys, SWGSDRangel::SWGChannelSettings& response,
                                        QString& errorMessage)
{
    (void)errorMessage;
    SELCALDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);
    settings.m_streamIndex = std::min(settings.m_streamIndex, std::max(0, static_cast<int>(getNumberOfDeviceStreams()) - 1));

    MsgConfigureSELCALDemod *msg = MsgConfigureSELCALDemod::create(channelSettingsKeys, settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("SELCALDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSELCALDemod *msgToGUI = MsgConfigureSELCALDemod::create(channelSettingsKeys, settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int SELCALDemod::webapiReportGet(SWGSDRangel::SWGChannelReport& response, QString& errorMessage)
{
    (void)errorMessage;
    response.setSelcalDemodReport(new SWGSDRangel::SWGSELCALDemodReport());
    response.getSelcalDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void SELCALDemod::webapiUpdateChannelSettings(SELCALDemodSettings& settings, const QStringList& channelSettingsKeys,
                                              SWGSDRangel::SWGChannelSettings& response)
{
    auto *model = response.getSelcalDemodSettings();
    if (!model) {
        return;
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = model->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = model->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("modulation")) {
        settings.m_modulation = model->getModulation();
    }
    if (channelSettingsKeys.contains("toneFrequencyOffset")) {
        settings.m_toneFrequencyOffset = model->getToneFrequencyOffset();
    }
    if (channelSettingsKeys.contains("autoToneFrequencyOffset")) {
        settings.m_autoToneFrequencyOffset = model->getAutoToneFrequencyOffset() != 0;
    }
    if (channelSettingsKeys.contains("threshold")) {
        settings.m_threshold = model->getThreshold();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = model->getVolume();
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = model->getAudioMute();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = model->getSquelch();
    }
    if (channelSettingsKeys.contains("audioDeviceName") && model->getAudioDeviceName()) {
        settings.m_audioDeviceName = *model->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("filter") && model->getFilter()) {
        settings.m_filter = *model->getFilter();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = model->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress") && model->getUdpAddress()) {
        settings.m_udpAddress = *model->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = model->getUdpPort();
    }
    if (channelSettingsKeys.contains("logFilename") && model->getLogFilename()) {
        settings.m_logFilename = *model->getLogFilename();
    }
    if (channelSettingsKeys.contains("logEnabled")) {
        settings.m_logEnabled = model->getLogEnabled();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = model->getRgbColor();
    }
    if (channelSettingsKeys.contains("title") && model->getTitle()) {
        settings.m_title = *model->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = model->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = model->getUseReverseApi();
    }
    if (channelSettingsKeys.contains("reverseAPIAddress") && model->getReverseApiAddress()) {
        settings.m_reverseAPIAddress = *model->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = model->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = model->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = model->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker") && model->getChannelMarker()) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, model->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState") && model->getRollupState()) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, model->getRollupState());
    }
    settings.normalize();
}

void SELCALDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const SELCALDemodSettings& settings)
{
    auto *model = response.getSelcalDemodSettings();
    model->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    model->setRfBandwidth(settings.m_rfBandwidth);
    model->setModulation(settings.m_modulation);
    model->setToneFrequencyOffset(settings.m_toneFrequencyOffset);
    model->setAutoToneFrequencyOffset(settings.m_autoToneFrequencyOffset);
    model->setThreshold(settings.m_threshold);
    model->setVolume(settings.m_volume);
    model->setAudioMute(settings.m_audioMute);
    model->setSquelch(settings.m_squelch);
    model->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    model->setFilter(new QString(settings.m_filter));
    model->setUdpEnabled(settings.m_udpEnabled);
    model->setUdpAddress(new QString(settings.m_udpAddress));
    model->setUdpPort(settings.m_udpPort);
    model->setLogFilename(new QString(settings.m_logFilename));
    model->setLogEnabled(settings.m_logEnabled);
    model->setRgbColor(settings.m_rgbColor);
    model->setTitle(new QString(settings.m_title));
    model->setStreamIndex(settings.m_streamIndex);
    model->setUseReverseApi(settings.m_useReverseAPI);
    model->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    model->setReverseApiPort(settings.m_reverseAPIPort);
    model->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    model->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
    if (settings.m_channelMarker)
    {
        auto *item = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(item);
        model->setChannelMarker(item);
    }
    if (settings.m_rollupState)
    {
        auto *item = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(item);
        model->setRollupState(item);
    }
}

void SELCALDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double avg, peak;
    int count;
    getMagSqLevels(avg, peak, count);
    auto *model = response.getSelcalDemodReport();
    model->setChannelPowerDb(CalcDb::dbPower(avg));
    model->setChannelSampleRate(getChannelSampleRate());
    model->setAudioSampleRate(getAudioSampleRate());
    QMutexLocker lock(&m_reportMutex);
    model->setState(new QString(SELCALDecoder::stateName(m_status.m_state)));
    model->setCarrierLocked(m_status.m_carrierLocked);
    model->setSquelchOpen(m_status.m_squelchOpen);
    model->setCarrierFrequencyOffset(m_status.m_carrierFrequencyOffset);
    model->setToneFrequencyOffset(m_status.m_toneFrequencyOffset);
    model->setLastCode(new QString(m_lastCall.m_code));
    model->setLastTimestamp(new QString(m_lastCall.m_dateTime.toString(Qt::ISODateWithMs)));
    model->setLastFrequency(m_lastCall.m_frequency);
    model->setLastRssi(m_lastCall.m_rssi);
    model->setLastSnr(m_lastCall.m_snr);
    model->setCallCount(m_callCount);
}

void SELCALDemod::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const SELCALDemodSettings& settings, bool force)
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

void SELCALDemod::webapiFormatChannelSettings(const QList<QString>& channelSettingsKeys,
                                              SWGSDRangel::SWGChannelSettings *swgChannelSettings, const SELCALDemodSettings& settings,
                                              bool force)
{
    swgChannelSettings->setDirection(0);
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("SELCALDemod"));
    auto *model = new SWGSDRangel::SWGSELCALDemodSettings();
    swgChannelSettings->setSelcalDemodSettings(model);
    if (force || channelSettingsKeys.contains("inputFrequencyOffset")) {
        model->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (force || channelSettingsKeys.contains("rfBandwidth")) {
        model->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (force || channelSettingsKeys.contains("modulation")) {
        model->setModulation(settings.m_modulation);
    }
    if (force || channelSettingsKeys.contains("toneFrequencyOffset")) {
        model->setToneFrequencyOffset(settings.m_toneFrequencyOffset);
    }
    if (force || channelSettingsKeys.contains("autoToneFrequencyOffset")) {
        model->setAutoToneFrequencyOffset(settings.m_autoToneFrequencyOffset);
    }
    if (force || channelSettingsKeys.contains("threshold")) {
        model->setThreshold(settings.m_threshold);
    }
    if (force || channelSettingsKeys.contains("volume")) {
        model->setVolume(settings.m_volume);
    }
    if (force || channelSettingsKeys.contains("audioMute")) {
        model->setAudioMute(settings.m_audioMute);
    }
    if (force || channelSettingsKeys.contains("squelch")) {
        model->setSquelch(settings.m_squelch);
    }
    if (force || channelSettingsKeys.contains("audioDeviceName")) {
        model->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (force || channelSettingsKeys.contains("filter")) {
        model->setFilter(new QString(settings.m_filter));
    }
    if (force || channelSettingsKeys.contains("udpEnabled")) {
        model->setUdpEnabled(settings.m_udpEnabled);
    }
    if (force || channelSettingsKeys.contains("udpAddress")) {
        model->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (force || channelSettingsKeys.contains("udpPort")) {
        model->setUdpPort(settings.m_udpPort);
    }
    if (force || channelSettingsKeys.contains("logFilename")) {
        model->setLogFilename(new QString(settings.m_logFilename));
    }
    if (force || channelSettingsKeys.contains("logEnabled")) {
        model->setLogEnabled(settings.m_logEnabled);
    }
    if (force || channelSettingsKeys.contains("rgbColor")) {
        model->setRgbColor(settings.m_rgbColor);
    }
    if (force || channelSettingsKeys.contains("title")) {
        model->setTitle(new QString(settings.m_title));
    }
    if (force || channelSettingsKeys.contains("streamIndex")) {
        model->setStreamIndex(settings.m_streamIndex);
    }
    if (settings.m_channelMarker && (force || channelSettingsKeys.contains("channelMarker")))
    {
        auto *item = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(item);
        model->setChannelMarker(item);
    }
    if (settings.m_rollupState && (force || channelSettingsKeys.contains("rollupState")))
    {
        auto *item = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(item);
        model->setRollupState(item);
    }
}

void SELCALDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError) {
        qWarning() << "SELCALDemod::networkManagerFinished:"
                   << " error(" << (int)replyError << "): " << replyError << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("SELCALDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void SELCALDemod::handleIndexInDeviceSetChanged(int index)
{
    if (index < 0) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]").arg(m_channelId).arg(m_deviceAPI->getDeviceSetIndex()).arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
}

int SELCALDemod::getChannelSampleRate() const
{
    return m_basebandSink ? m_basebandSink->getChannelSampleRate() : 0;
}

int SELCALDemod::getAudioSampleRate() const
{
    return m_basebandSink->getAudioSampleRate();
}
