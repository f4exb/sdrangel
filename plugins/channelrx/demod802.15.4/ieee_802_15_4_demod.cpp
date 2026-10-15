///////////////////////////////////////////////////////////////////////////////////
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
#include <QDataStream>
#include <QThread>
#include <QtEndian>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGIEEE_802_15_4_DemodSettings.h"
#include "SWGChannelReport.h"

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "util/ieee_802_15_4.h"
#include "maincore.h"

#include "ieee_802_15_4_demod.h"

MESSAGE_CLASS_DEFINITION(IEEE_802_15_4_Demod::MsgConfigureIEEE_802_15_4_Demod, Message)
MESSAGE_CLASS_DEFINITION(IEEE_802_15_4_Demod::MsgFrame, Message)

const char * const IEEE_802_15_4_Demod::m_channelIdURI = "sdrangel.channelrx.demod802.15.4";
const char * const IEEE_802_15_4_Demod::m_channelId = "IEEE_802_15_4_Demod";

IEEE_802_15_4_Demod::IEEE_802_15_4_Demod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0),
        m_centerFrequency(0),
        m_zepSequenceNumber(0),
        m_logPCAP(false),
        m_framesOK(0),
        m_framesBad(0)
{
    setObjectName(m_channelId);
    m_thread.setObjectName("802154DemodBB");

    m_basebandSink = new IEEE_802_15_4_DemodBaseband(this);
    m_basebandSink->setMessageQueueToChannel(getInputMessageQueue());
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);

    applySettings(QStringList(), m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &IEEE_802_15_4_Demod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &IEEE_802_15_4_Demod::handleIndexInDeviceSetChanged
    );
}

IEEE_802_15_4_Demod::~IEEE_802_15_4_Demod()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &IEEE_802_15_4_Demod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;

    if (m_logFile.isOpen())
    {
        m_logStream.flush();
        m_logFile.close();
    }
}

void IEEE_802_15_4_Demod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this, false);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

uint32_t IEEE_802_15_4_Demod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void IEEE_802_15_4_Demod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void IEEE_802_15_4_Demod::start()
{
    qDebug("IEEE_802_15_4_Demod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    IEEE_802_15_4_DemodBaseband::MsgConfigureIEEE_802_15_4_DemodBaseband *msg = IEEE_802_15_4_DemodBaseband::MsgConfigureIEEE_802_15_4_DemodBaseband::create(QStringList(), m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void IEEE_802_15_4_Demod::stop()
{
    qDebug("IEEE_802_15_4_Demod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool IEEE_802_15_4_Demod::handleMessage(const Message& cmd)
{
    if (MsgConfigureIEEE_802_15_4_Demod::match(cmd))
    {
        MsgConfigureIEEE_802_15_4_Demod& cfg = (MsgConfigureIEEE_802_15_4_Demod&) cmd;
        qDebug() << "IEEE_802_15_4_Demod::handleMessage: MsgConfigureIEEE_802_15_4_Demod";
        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "IEEE_802_15_4_Demod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);

        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MsgFrame::match(cmd))
    {
        const MsgFrame& frame = (const MsgFrame&) cmd;

        if (frame.getFCSOK()) {
            m_framesOK++;
        } else {
            m_framesBad++;
        }

        if (!frame.getFCSOK() && !m_settings.m_showBadFCS) {
            return true;
        }

        // Forward to GUI
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new MsgFrame(frame));
        }

        // Forward via UDP
        if (m_settings.m_udpEnabled)
        {
            if (m_settings.m_udpFormat == IEEE_802_15_4_DemodSettings::ZEP)
            {
                int channel = getChannelNumber(m_centerFrequency + m_settings.m_inputFrequencyOffset);
                int lqi = std::max(0, std::min(255, (int) std::round(frame.getLQI() * 255.0f)));
                QByteArray zep = encodeZEP(frame.getPSDU(), frame.getDateTime(), channel < 0 ? 0 : channel, lqi, m_zepSequenceNumber++);
                m_udpSocket.writeDatagram(zep, QHostAddress(m_settings.m_udpAddress), m_settings.m_udpPort);
            }
            else
            {
                m_udpSocket.writeDatagram(frame.getPSDU(), QHostAddress(m_settings.m_udpAddress), m_settings.m_udpPort);
            }
        }

        // Write to log file
        if (m_logFile.isOpen()) {
            logFrame(frame);
        }

        return true;
    }
    else
    {
        return false;
    }
}

int IEEE_802_15_4_Demod::getChannelNumber(qint64 frequency)
{
    // 10.1.2.2 - Channel numbering for 868 MHz, 915 MHz, and 2450 MHz bands
    const qint64 tolerance = 100000;

    if (std::abs(frequency - 868300000LL) <= tolerance) {
        return 0;
    }
    for (int k = 1; k <= 10; k++)
    {
        if (std::abs(frequency - (906000000LL + 2000000LL * (k - 1))) <= tolerance) {
            return k;
        }
    }
    for (int k = 11; k <= 26; k++)
    {
        if (std::abs(frequency - (2405000000LL + 5000000LL * (k - 11))) <= tolerance) {
            return k;
        }
    }

    return -1;
}

QByteArray IEEE_802_15_4_Demod::encodeZEP(const QByteArray& psdu, const QDateTime& dateTime, int channel, int lqi, quint32 sequenceNumber)
{
    QByteArray zep;
    QDataStream s(&zep, QIODevice::WriteOnly);
    s.setByteOrder(QDataStream::BigEndian);

    // NTP timestamp: seconds since 1900 and fraction
    const quint64 ntpEpochOffset = 2208988800ULL;
    qint64 ms = dateTime.toMSecsSinceEpoch();
    quint32 seconds = (quint32) (ms / 1000 + ntpEpochOffset);
    quint32 fraction = (quint32) (((ms % 1000) * 4294967296ULL) / 1000);

    s << (quint8) 'E' << (quint8) 'X';  // Preamble
    s << (quint8) 2;                    // Version
    s << (quint8) 1;                    // Type: Data
    s << (quint8) channel;              // Channel ID
    s << (quint16) 0;                   // Device ID
    s << (quint8) 1;                    // CRC mode: PSDU includes FCS
    s << (quint8) lqi;                  // LQI
    s << seconds << fraction;           // Timestamp
    s << sequenceNumber;
    for (int i = 0; i < 10; i++) {
        s << (quint8) 0;                // Reserved
    }
    s << (quint8) psdu.size();          // Length
    zep.append(psdu);

    return zep;
}

void IEEE_802_15_4_Demod::openLog(const QString& filename)
{
    m_logFile.setFileName(filename);
    m_logPCAP = filename.endsWith(".pcap", Qt::CaseInsensitive);

    // Only append to an existing PCAP file if it has the same link type, otherwise
    // the frames would be misinterpreted, or the file wouldn't be readable at all
    if (m_logPCAP && (m_logFile.size() > 0))
    {
        bool valid = false;
        QFile existing(filename);

        if (existing.open(QIODevice::ReadOnly))
        {
            QByteArray header = existing.read(24);

            if (header.size() == 24)
            {
                quint32 magic = qFromLittleEndian<quint32>(header.constData());
                quint32 linkType = qFromLittleEndian<quint32>(header.constData() + 20);
                valid = (magic == 0xa1b2c3d4) && (linkType == m_pcapLinkTypeIEEE802154WithFCS);
            }
        }

        if (!valid)
        {
            qWarning() << "IEEE_802_15_4_Demod::openLog - Not logging to" << filename << "as it is not a PCAP file with IEEE 802.15.4 link type";
            return;
        }
    }

    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | (m_logPCAP ? QIODevice::NotOpen : QIODevice::Text)))
    {
        qDebug() << "IEEE_802_15_4_Demod::openLog - Logging to: " << filename;
        bool newFile = m_logFile.size() == 0;

        if (m_logPCAP)
        {
            if (newFile)
            {
                // PCAP global header
                QDataStream s(&m_logFile);
                s.setByteOrder(QDataStream::LittleEndian);
                s << (quint32) 0xa1b2c3d4 << (quint16) 2 << (quint16) 4 << (qint32) 0 << (quint32) 0
                  << (quint32) 65535 << m_pcapLinkTypeIEEE802154WithFCS;
            }
        }
        else
        {
            m_logStream.setDevice(&m_logFile);

            if (newFile) {
                m_logStream << "Date,Time,Data,FCS,Type,Seq,Dst PAN,Dst Addr,Src PAN,Src Addr,Info,Payload,Power,SNR,LQI,Freq Offset\n";
            }
        }
    }
    else
    {
        qDebug() << "IEEE_802_15_4_Demod::openLog - Unable to open log file: " << filename;
    }
}

void IEEE_802_15_4_Demod::logFrame(const MsgFrame& frame)
{
    if (m_logPCAP)
    {
        QDataStream s(&m_logFile);
        s.setByteOrder(QDataStream::LittleEndian);
        qint64 ms = frame.getDateTime().toMSecsSinceEpoch();
        s << (quint32) (ms / 1000) << (quint32) ((ms % 1000) * 1000)
          << (quint32) frame.getPSDU().size() << (quint32) frame.getPSDU().size();
        s.writeRawData(frame.getPSDU().data(), frame.getPSDU().size());
        m_logFile.flush();
    }
    else
    {
        IEEE_802_15_4_FrameDecoder mac;
        mac.decode(frame.getPSDU());

        m_logStream << frame.getDateTime().date().toString() << ","
            << frame.getDateTime().time().toString() << ","
            << frame.getPSDU().toHex() << ","
            << (frame.getFCSOK() ? "OK" : "Bad") << ","
            << mac.getFrameTypeString() << ","
            << (mac.m_hasSeqNo ? QString::number(mac.m_seqNo) : "") << ","
            << mac.getDestPANIDString() << ","
            << mac.getDestAddressString() << ","
            << mac.getSrcPANIDString() << ","
            << mac.getSrcAddressString() << ","
            << "\"" << mac.toString() << "\","
            << mac.m_payload.toHex() << ","
            << QString::number(frame.getPowerDB(), 'f', 1) << ","
            << QString::number(frame.getSNR(), 'f', 1) << ","
            << QString::number(frame.getLQI(), 'f', 2) << ","
            << QString::number(frame.getFrequencyOffset(), 'f', 0) << "\n";
        m_logStream.flush();
    }
}

ScopeVis *IEEE_802_15_4_Demod::getScopeSink()
{
    return m_basebandSink->getScopeSink();
}

void IEEE_802_15_4_Demod::setCenterFrequency(qint64 frequency)
{
    IEEE_802_15_4_DemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList("inputFrequencyOffset"), settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureIEEE_802_15_4_Demod *msgToGUI = MsgConfigureIEEE_802_15_4_Demod::create(QStringList("inputFrequencyOffset"), settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void IEEE_802_15_4_Demod::applySettings(const QStringList& settingsKeys, const IEEE_802_15_4_DemodSettings& settings, bool force)
{
    qDebug() << "IEEE_802_15_4_Demod::applySettings:" << settings.getDebugString(settingsKeys, force);

    if (settingsKeys.contains("streamIndex") && (m_settings.m_streamIndex != settings.m_streamIndex))
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSinkAPI(this);
            m_deviceAPI->removeChannelSink(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSink(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSinkAPI(this);
            m_settings.m_streamIndex = settings.m_streamIndex; // make sure ChannelAPI::getStreamIndex() is consistent
            emit streamIndexChanged(settings.m_streamIndex);
        }
    }

    IEEE_802_15_4_DemodBaseband::MsgConfigureIEEE_802_15_4_DemodBaseband *msg = IEEE_802_15_4_DemodBaseband::MsgConfigureIEEE_802_15_4_DemodBaseband::create(settingsKeys, settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    // Forward all settings changes when reverse API is enabled, not just changes to the reverse API settings
    bool useReverseAPI = (settingsKeys.contains("useReverseAPI") || force) ? settings.m_useReverseAPI : m_settings.m_useReverseAPI;

    if (useReverseAPI)
    {
        bool fullUpdate = ((settingsKeys.contains("useReverseAPI") && (m_settings.m_useReverseAPI != settings.m_useReverseAPI)) && settings.m_useReverseAPI) ||
                (settingsKeys.contains("reverseAPIAddress") && (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress)) ||
                (settingsKeys.contains("reverseAPIPort") && (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort)) ||
                (settingsKeys.contains("reverseAPIDeviceIndex") && (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex)) ||
                (settingsKeys.contains("reverseAPIChannelIndex") && (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex));
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if ((settingsKeys.contains("logEnabled") && (settings.m_logEnabled != m_settings.m_logEnabled))
        || (settingsKeys.contains("logFilename") && (settings.m_logFilename != m_settings.m_logFilename))
        || force)
    {
        if (m_logFile.isOpen())
        {
            m_logStream.flush();
            m_logFile.close();
        }
        if (settings.m_logEnabled && !settings.m_logFilename.isEmpty()) {
            openLog(settings.m_logFilename);
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

QByteArray IEEE_802_15_4_Demod::serialize() const
{
    return m_settings.serialize();
}

bool IEEE_802_15_4_Demod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureIEEE_802_15_4_Demod *msg = MsgConfigureIEEE_802_15_4_Demod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureIEEE_802_15_4_Demod *msg = MsgConfigureIEEE_802_15_4_Demod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int IEEE_802_15_4_Demod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIeee802154DemodSettings(new SWGSDRangel::SWGIEEE_802_15_4_DemodSettings());
    response.getIeee802154DemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int IEEE_802_15_4_Demod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int IEEE_802_15_4_Demod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    if (channelSettingsKeys.contains("phy"))
    {
        IEEE_802_15_4_PHY phy;
        QString *phyName = response.getIeee802154DemodSettings()->getPhy();

        if (!phyName || !phy.fromString(*phyName) || phy.toString().isEmpty())
        {
            errorMessage = QString("phy must be one of: %1").arg(IEEE_802_15_4_PHY::getPHYNames().join(", "));
            return 400;
        }
    }

    IEEE_802_15_4_DemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureIEEE_802_15_4_Demod *msg = MsgConfigureIEEE_802_15_4_Demod::create(channelSettingsKeys, settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureIEEE_802_15_4_Demod *msgToGUI = MsgConfigureIEEE_802_15_4_Demod::create(channelSettingsKeys, settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int IEEE_802_15_4_Demod::webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setIeee802154DemodReport(new SWGSDRangel::SWGIEEE_802_15_4_DemodReport());
    response.getIeee802154DemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void IEEE_802_15_4_Demod::webapiUpdateChannelSettings(
        IEEE_802_15_4_DemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    SWGSDRangel::SWGIEEE_802_15_4_DemodSettings *s = response.getIeee802154DemodSettings();

    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = s->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("phy")) {
        settings.m_phy = *s->getPhy();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = s->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("samplesPerChip")) {
        settings.m_samplesPerChip = std::max(2, std::min(16, s->getSamplesPerChip()));
    }
    if (channelSettingsKeys.contains("maxFrequencyOffset")) {
        settings.m_maxFrequencyOffset = s->getMaxFrequencyOffset();
    }
    if (channelSettingsKeys.contains("threshold")) {
        settings.m_threshold = std::max(IEEE_802_15_4_DemodSettings::m_minThreshold,
            std::min(IEEE_802_15_4_DemodSettings::m_maxThreshold, s->getThreshold()));
    }
    if (channelSettingsKeys.contains("showBadFCS")) {
        settings.m_showBadFCS = s->getShowBadFcs() != 0;
    }
    if (channelSettingsKeys.contains("filterFrameType")) {
        settings.m_filterFrameType = (IEEE_802_15_4_DemodSettings::FrameTypeFilter) std::max(0, std::min(5, s->getFilterFrameType()));
    }
    if (channelSettingsKeys.contains("filterPANID")) {
        settings.m_filterPANID = *s->getFilterPanid();
    }
    if (channelSettingsKeys.contains("filterAddress")) {
        settings.m_filterAddress = *s->getFilterAddress();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = s->getUdpEnabled() != 0;
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *s->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = s->getUdpPort();
    }
    if (channelSettingsKeys.contains("udpFormat")) {
        settings.m_udpFormat = s->getUdpFormat() == 1 ? IEEE_802_15_4_DemodSettings::Raw : IEEE_802_15_4_DemodSettings::ZEP;
    }
    if (channelSettingsKeys.contains("logFilename")) {
        settings.m_logFilename = *s->getLogFilename();
    }
    if (channelSettingsKeys.contains("logEnabled")) {
        settings.m_logEnabled = s->getLogEnabled() != 0;
    }
    if (channelSettingsKeys.contains("useFileTime")) {
        settings.m_useFileTime = s->getUseFileTime() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = s->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *s->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = s->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = s->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *s->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = s->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = s->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = s->getReverseApiChannelIndex();
    }
    if (settings.m_scopeGUI && channelSettingsKeys.contains("scopeConfig")) {
        settings.m_scopeGUI->updateFrom(channelSettingsKeys, s->getScopeConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, s->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, s->getRollupState());
    }
}

static void setString(QString *(SWGSDRangel::SWGIEEE_802_15_4_DemodSettings::*get)(),
                      void (SWGSDRangel::SWGIEEE_802_15_4_DemodSettings::*set)(QString*),
                      SWGSDRangel::SWGIEEE_802_15_4_DemodSettings *s,
                      const QString& value)
{
    if ((s->*get)()) {
        *(s->*get)() = value;
    } else {
        (s->*set)(new QString(value));
    }
}

void IEEE_802_15_4_Demod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const IEEE_802_15_4_DemodSettings& settings)
{
    typedef SWGSDRangel::SWGIEEE_802_15_4_DemodSettings S;
    S *s = response.getIeee802154DemodSettings();

    s->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    setString(&S::getPhy, &S::setPhy, s, settings.m_phy);
    s->setRfBandwidth(settings.m_rfBandwidth);
    s->setSamplesPerChip(settings.m_samplesPerChip);
    s->setMaxFrequencyOffset(settings.m_maxFrequencyOffset);
    s->setThreshold(settings.m_threshold);
    s->setShowBadFcs(settings.m_showBadFCS ? 1 : 0);
    s->setFilterFrameType((int) settings.m_filterFrameType);
    setString(&S::getFilterPanid, &S::setFilterPanid, s, settings.m_filterPANID);
    setString(&S::getFilterAddress, &S::setFilterAddress, s, settings.m_filterAddress);
    s->setUdpEnabled(settings.m_udpEnabled ? 1 : 0);
    setString(&S::getUdpAddress, &S::setUdpAddress, s, settings.m_udpAddress);
    s->setUdpPort(settings.m_udpPort);
    s->setUdpFormat((int) settings.m_udpFormat);
    setString(&S::getLogFilename, &S::setLogFilename, s, settings.m_logFilename);
    s->setLogEnabled(settings.m_logEnabled ? 1 : 0);
    s->setUseFileTime(settings.m_useFileTime ? 1 : 0);
    s->setRgbColor(settings.m_rgbColor);
    setString(&S::getTitle, &S::setTitle, s, settings.m_title);
    s->setStreamIndex(settings.m_streamIndex);
    s->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);
    setString(&S::getReverseApiAddress, &S::setReverseApiAddress, s, settings.m_reverseAPIAddress);
    s->setReverseApiPort(settings.m_reverseAPIPort);
    s->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    s->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_scopeGUI)
    {
        if (s->getScopeConfig())
        {
            settings.m_scopeGUI->formatTo(s->getScopeConfig());
        }
        else
        {
            SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
            settings.m_scopeGUI->formatTo(swgGLScope);
            s->setScopeConfig(swgGLScope);
        }
    }

    if (settings.m_channelMarker)
    {
        if (s->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(s->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            s->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (s->getRollupState())
        {
            settings.m_rollupState->formatTo(s->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            s->setRollupState(swgRollupState);
        }
    }
}

void IEEE_802_15_4_Demod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getIeee802154DemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getIeee802154DemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
    response.getIeee802154DemodReport()->setFramesOk((qint32) m_framesOK);
    response.getIeee802154DemodReport()->setFramesBadFcs((qint32) m_framesBad);
}

void IEEE_802_15_4_Demod::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const IEEE_802_15_4_DemodSettings& settings, bool force)
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

void IEEE_802_15_4_Demod::webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const IEEE_802_15_4_DemodSettings& settings,
        bool force
)
{
    typedef SWGSDRangel::SWGIEEE_802_15_4_DemodSettings S;

    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setIeee802154DemodSettings(new S());
    S *s = swgChannelSettings->getIeee802154DemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        s->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("phy") || force) {
        s->setPhy(new QString(settings.m_phy));
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        s->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("samplesPerChip") || force) {
        s->setSamplesPerChip(settings.m_samplesPerChip);
    }
    if (channelSettingsKeys.contains("maxFrequencyOffset") || force) {
        s->setMaxFrequencyOffset(settings.m_maxFrequencyOffset);
    }
    if (channelSettingsKeys.contains("threshold") || force) {
        s->setThreshold(settings.m_threshold);
    }
    if (channelSettingsKeys.contains("showBadFCS") || force) {
        s->setShowBadFcs(settings.m_showBadFCS ? 1 : 0);
    }
    if (channelSettingsKeys.contains("filterFrameType") || force) {
        s->setFilterFrameType((int) settings.m_filterFrameType);
    }
    if (channelSettingsKeys.contains("filterPANID") || force) {
        s->setFilterPanid(new QString(settings.m_filterPANID));
    }
    if (channelSettingsKeys.contains("filterAddress") || force) {
        s->setFilterAddress(new QString(settings.m_filterAddress));
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        s->setUdpEnabled(settings.m_udpEnabled ? 1 : 0);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        s->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        s->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("udpFormat") || force) {
        s->setUdpFormat((int) settings.m_udpFormat);
    }
    if (channelSettingsKeys.contains("logFilename") || force) {
        s->setLogFilename(new QString(settings.m_logFilename));
    }
    if (channelSettingsKeys.contains("logEnabled") || force) {
        s->setLogEnabled(settings.m_logEnabled ? 1 : 0);
    }
    if (channelSettingsKeys.contains("useFileTime") || force) {
        s->setUseFileTime(settings.m_useFileTime ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        s->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        s->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        s->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_scopeGUI && (channelSettingsKeys.contains("scopeConfig") || force))
    {
        SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
        settings.m_scopeGUI->formatTo(swgGLScope);
        s->setScopeConfig(swgGLScope);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        s->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        s->setRollupState(swgRollupState);
    }
}

void IEEE_802_15_4_Demod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "IEEE_802_15_4_Demod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("IEEE_802_15_4_Demod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void IEEE_802_15_4_Demod::handleIndexInDeviceSetChanged(int index)
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

int IEEE_802_15_4_Demod::getChannelSampleRate() const
{
    return m_basebandSink ? m_basebandSink->getChannelSampleRate() : 0;
}
