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

#include "droneiddemod.h"

#include <algorithm>
#include <limits>

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGWorkspaceInfo.h"
#include "SWGMapItem.h"
#include "SWGMapAnimation.h"

#include "dsp/dspcommands.h"
#include "util/db.h"
#include "util/droneid.h"
#include "settings/serializable.h"
#include "device/deviceapi.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(DroneIDDemod::MsgConfigureDroneIDDemod, Message)
MESSAGE_CLASS_DEFINITION(DroneIDDemod::MsgBurstResult, Message)

const char * const DroneIDDemod::m_channelIdURI = "sdrangel.channel.droneiddemod";
const char * const DroneIDDemod::m_channelId = "DroneIDDemod";


DroneIDDemod::DroneIDDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0),
        m_centerFrequency(0),
        m_burstsDecoded(0),
        m_dronesTracked(0),
        m_scanPosition(0),
        m_scanFrequency(0),
        m_scanTuneTime(0),
        m_scanBurstTime(-1),
        m_scanLocked(false)
{
    setObjectName(m_channelId);
    m_thread.setObjectName("DroneIDDeBB");

    m_basebandSink = new DroneIDDemodBaseband(this);
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
        &DroneIDDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &DroneIDDemod::handleIndexInDeviceSetChanged
    );

    connect(&m_mapTimer, &QTimer::timeout, this, &DroneIDDemod::removeOldDrones);
    m_mapTimer.start(10 * 1000);
    connect(&m_scanTimer, &QTimer::timeout, this, &DroneIDDemod::scanTimeout);
}

DroneIDDemod::~DroneIDDemod()
{
    qDebug("DroneIDDemod::~DroneIDDemod");
    m_mapTimer.stop();
    m_scanTimer.stop();
    for (auto it = m_drones.begin(); it != m_drones.end(); ++it)
    {
        if (it->m_onMap) {
            clearFromMap(it.key());
        }
    }
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DroneIDDemod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void DroneIDDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t DroneIDDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void DroneIDDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void DroneIDDemod::start()
{
    qDebug("DroneIDDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    DroneIDDemodBaseband::MsgConfigureDroneIDDemodBaseband *msg = DroneIDDemodBaseband::MsgConfigureDroneIDDemodBaseband::create(QStringList(), m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void DroneIDDemod::stop()
{
    qDebug("DroneIDDemod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool DroneIDDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureDroneIDDemod::match(cmd))
    {
        MsgConfigureDroneIDDemod& cfg = (MsgConfigureDroneIDDemod&) cmd;
        qDebug() << "DroneIDDemod::handleMessage: MsgConfigureDroneIDDemod";
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
        qDebug() << "DroneIDDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MsgBurstResult::match(cmd))
    {
        handleBurstResult((const MsgBurstResult&) cmd);

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "DroneIDDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

void DroneIDDemod::handleBurstResult(const MsgBurstResult& result)
{
    if (result.getDetected() && m_scanTimer.isActive())
    {
        qint64 now = m_scanClock.elapsed();

        if (now - m_scanTuneTime >= m_scanSettleTime)
        {
            m_scanBurstTime = now;

            // Remember frequency, so it's searched first in future
            qint64 frequency = scanFrequencyMatch(result.getFrequency());

            if (frequency != 0)
            {
                m_scanHistory.removeAll(frequency);
                m_scanHistory.prepend(frequency);
            }
        }
    }

    // Forward bursts with DroneID sync to GUI, so it can display SNR even if not decoded.
    // Other detections (E.g. from video link) aren't forwarded, as they can occur at a high rate.
    if (result.getDetected() && getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(new MsgBurstResult(result));
    }

    if (!result.getDecoded() || result.getMessage().isEmpty()) {
        return;
    }

    m_burstsDecoded++;

    const QByteArray& bytes = result.getMessage();
    DroneIDMessage *msg = DroneIDMessage::decode(bytes);

    // Forward to packet consumers (E.g. MCP)
    QList<ObjectPipe*> packetPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "packets", packetPipes);

    for (const auto& pipe : packetPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        messageQueue->push(MainCore::MsgPacket::create(this, bytes, result.getDateTime()));
    }

    // Forward via UDP
    if (m_settings.m_udpEnabled) {
        m_udpSocket.writeDatagram(bytes.data(), bytes.size(), QHostAddress(m_settings.m_udpAddress), m_settings.m_udpPort);
    }

    logMessage(result, msg);

    if (msg)
    {
        if (!msg->m_serial.isEmpty())
        {
            Drone& drone = m_drones[msg->m_serial];
            m_dronesTracked = m_drones.size();
            drone.m_lastUpdate = QDateTime::currentDateTime();
            drone.m_lastSeen = result.getDateTime().isValid() ? result.getDateTime() : drone.m_lastUpdate;
            if (msg->isFlightInformation())
            {
                sendToMap(msg, result.getDateTime(), drone);
                drone.m_onMap = true;
            }
        }
        delete msg;
    }
}

void DroneIDDemod::logMessage(const MsgBurstResult& result, const DroneIDMessage *msg)
{
    if (!m_logFile.isOpen()) {
        return;
    }

    m_logStream << result.getDateTime().date().toString() << ","
        << result.getDateTime().time().toString() << ","
        << result.getMessage().toHex() << ",";

    if (msg && msg->isFlightInformation())
    {
        m_logStream << msg->m_serial << ","
            << msg->getProductTypeName() << ","
            << msg->m_sequence << ","
            << QString::number(msg->m_latitude, 'f', 7) << ","
            << QString::number(msg->m_longitude, 'f', 7) << ","
            << msg->m_altitude << ","
            << msg->m_height << ","
            << msg->getSpeed() << ","
            << msg->m_vUp << ","
            << msg->m_yaw << ","
            << QString::number(msg->m_pilotLatitude, 'f', 7) << ","
            << QString::number(msg->m_pilotLongitude, 'f', 7) << ","
            << QString::number(msg->m_homeLatitude, 'f', 7) << ","
            << QString::number(msg->m_homeLongitude, 'f', 7) << ","
            << msg->m_uuid << ","
            << (msg->m_crcValid ? "OK" : "Bad") << ",";
    }
    else if (msg)
    {
        m_logStream << msg->m_serial << ",,,,,,,,,,,,,,," << (msg->m_crcValid ? "OK" : "Bad") << ",";
    }
    else
    {
        m_logStream << ",,,,,,,,,,,,,,,,";
    }
    m_logStream << result.getSNR() << "," << result.getCFO() << ","
        << QString::number(result.getFrequency() / 1e6, 'f', 3) << "\n";
}

// If model is specified, it is displayed on the 3D map, oriented by imageRotation.
// If animation is specified, that animation in the model is started (or stopped if stopAnimation is true).
// altitudeReference is 0 for altitude above WGS84 ellipsoid, 1 to clamp to ground, 2 for height above ground
// or 3 for altitude above WGS84 ellipsoid, but not below ground.
void DroneIDDemod::sendMapItem(const QString& name, double latitude, double longitude, float altitude, int altitudeReference,
    const QString& image, float imageRotation, const QString& label, const QString& text, const QDateTime& dateTime,
    const QString& model, const QString& animation, bool stopAnimation)
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "mapitems", mapPipes);

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setLatitude(latitude);
        swgMapItem->setLongitude(longitude);
        swgMapItem->setAltitude(altitude);
        swgMapItem->setAltitudeReference(altitudeReference);
        swgMapItem->setPositionDateTime(new QString(dateTime.toString(Qt::ISODateWithMs)));
        swgMapItem->setFixedPosition(false);
        swgMapItem->setImage(new QString(image));
        swgMapItem->setImageRotation(imageRotation);
        swgMapItem->setLabel(new QString(label));
        swgMapItem->setText(new QString(text));
        if (!model.isEmpty())
        {
            swgMapItem->setModel(new QString(model));
            swgMapItem->setOrientation(1); // Use heading, rather than velocity calculated from positions
            swgMapItem->setHeading(imageRotation);
            swgMapItem->setPitch(0.0f);
            swgMapItem->setRoll(0.0f);
            swgMapItem->setOrientationDateTime(new QString(dateTime.toString(Qt::ISODateWithMs)));
            swgMapItem->setLabelAltitudeOffset(1.0f);

            if (!animation.isEmpty())
            {
                // Each map item owns and deletes its animations, so create for each pipe
                SWGSDRangel::SWGMapAnimation *swgAnimation = new SWGSDRangel::SWGMapAnimation();
                swgAnimation->setName(new QString(animation));
                swgAnimation->setStartDateTime(new QString(dateTime.toString(Qt::ISODateWithMs)));
                swgAnimation->setReverse(0);
                swgAnimation->setLoop(1);
                swgAnimation->setMultiplier(1.0f);
                swgAnimation->setStop(stopAnimation ? 1 : 0);
                QList<SWGSDRangel::SWGMapAnimation *> *animations = new QList<SWGSDRangel::SWGMapAnimation *>();
                animations->append(swgAnimation);
                swgMapItem->setAnimations(animations);
            }
        }
        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(this, swgMapItem);
        messageQueue->push(msg);
    }
}

void DroneIDDemod::sendToMap(const DroneIDMessage *msg, const QDateTime& dateTime, Drone& drone)
{
    const QString model = msg->getProductTypeName();

    if (msg->hasPosition())
    {
        QString text = QString("DroneID\nSerial: %1\nModel: %2\nAltitude: %3 m (WGS84)\nHeight: %4 m (above take-off)\nSpeed: %5 m/s\nVertical speed: %6 m/s\nYaw: %7%8")
            .arg(msg->m_serial)
            .arg(model)
            .arg(msg->m_altitude)
            .arg(msg->m_height)
            .arg(msg->getSpeed(), 0, 'f', 1)
            .arg(msg->m_vUp, 0, 'f', 1)
            .arg(msg->m_yaw, 0, 'f', 0)
            .arg(QChar(0xb0));
        // Yaw is -180 to 180
        float heading = msg->m_yaw < 0.0f ? msg->m_yaw + 360.0f : msg->m_yaw;
        float altitude;
        int altitudeReference;

        if (m_settings.m_mapAltitude == DroneIDDemodSettings::MAP_ALTITUDE)
        {
            // Altitude appears to be GPS height above the WGS84 ellipsoid (not MSL), as used by the 3D map.
            // GPS error can put it slightly below ground on take-off and landing, so clip to ground.
            altitude = msg->m_altitude;
            altitudeReference = 3;
        }
        else
        {
            // Height above take-off point, displayed relative to the ground at the drone's position.
            // This is never below ground, but is offset by any difference in ground height from the take-off point.
            altitude = std::max(msg->m_height, 0.0f);
            altitudeReference = 2;
        }

        // Spin propellers in 3D model while motors are on. Animations are only sent when this changes.
        bool motorsOn = (msg->m_stateInfo & DroneIDMessage::MotorsOn) != 0;
        QString animation;

        if (motorsOn != drone.m_propellersSpinning)
        {
            animation = "Take 01";
            drone.m_propellersSpinning = motorsOn;
        }

        sendMapItem(msg->m_serial, msg->m_latitude, msg->m_longitude, altitude, altitudeReference,
            "qrc:///droneiddemod/map/drone.png", heading, model, text, dateTime, "drone.glb", animation, !motorsOn);
    }
    if (msg->hasPilotPosition())
    {
        QString text = QString("DroneID Pilot\nSerial: %1\nModel: %2").arg(msg->m_serial).arg(model);
        sendMapItem(msg->m_serial + " Pilot", msg->m_pilotLatitude, msg->m_pilotLongitude, 0.0f, 1,
            "qrc:///droneiddemod/map/pilot.png", 0.0f, QString("%1 Pilot").arg(model), text, dateTime);
    }
    if (msg->hasHomePosition())
    {
        QString text = QString("DroneID Home\nSerial: %1\nModel: %2").arg(msg->m_serial).arg(model);
        sendMapItem(msg->m_serial + " Home", msg->m_homeLatitude, msg->m_homeLongitude, 0.0f, 1,
            "qrc:///droneiddemod/map/home.png", 0.0f, QString("%1 Home").arg(model), text, dateTime);
    }
}

void DroneIDDemod::clearFromMap(const QString& serial)
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "mapitems", mapPipes);

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        for (const auto& name : {serial, serial + " Pilot", serial + " Home"})
        {
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
            swgMapItem->setName(new QString(name));
            swgMapItem->setImage(new QString(""));
            MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(this, swgMapItem);
            messageQueue->push(msg);
        }
    }
}

void DroneIDDemod::removeOldDrones()
{
    if (m_settings.m_mapTimeout <= 0) {
        return;
    }

    // When playing a recording faster or slower than real time, the message times from the file
    // determine whether a drone has stopped transmitting. Wall clock time is always also checked,
    // so drones are removed when the recording or transmissions stop
    QDateTime latest;
    for (const auto& drone : m_drones)
    {
        if (!latest.isValid() || (drone.m_lastSeen > latest)) {
            latest = drone.m_lastSeen;
        }
    }
    QDateTime now = QDateTime::currentDateTime();

    for (auto it = m_drones.begin(); it != m_drones.end(); )
    {
        bool expired = it->m_lastUpdate.secsTo(now) > m_settings.m_mapTimeout;

        if (m_settings.m_useFileTime) {
            expired = expired || (it->m_lastSeen.secsTo(latest) > m_settings.m_mapTimeout);
        }

        if (expired)
        {
            if (it->m_onMap) {
                clearFromMap(it.key());
            }
            it = m_drones.erase(it);
        }
        else
        {
            ++it;
        }
    }
    m_dronesTracked = m_drones.size();
}

void DroneIDDemod::setCenterFrequency(qint64 frequency)
{
    DroneIDDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList("inputFrequencyOffset"), settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDroneIDDemod *msgToGUI = MsgConfigureDroneIDDemod::create(QStringList("inputFrequencyOffset"), settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void DroneIDDemod::applySettings(const QStringList& settingsKeys, const DroneIDDemodSettings& settings, bool force)
{
    qDebug() << "DroneIDDemod::applySettings:" << settings.getDebugString(settingsKeys, force);

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

    DroneIDDemodBaseband::MsgConfigureDroneIDDemodBaseband *msg = DroneIDDemodBaseband::MsgConfigureDroneIDDemodBaseband::create(settingsKeys, settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if (settings.m_useReverseAPI)
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
        if (settings.m_logEnabled && !settings.m_logFilename.isEmpty())
        {
            m_logFile.setFileName(settings.m_logFilename);
            if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            {
                qDebug() << "DroneIDDemod::applySettings - Logging to: " << settings.m_logFilename;
                bool newFile = m_logFile.size() == 0;
                m_logStream.setDevice(&m_logFile);
                if (newFile)
                {
                    // Write header
                    m_logStream << "Date,Time,Data,Serial,Model,Seq,Lat,Lon,Alt (m),Height (m),Speed (m/s),V/S (m/s),Yaw,Pilot Lat,Pilot Lon,Home Lat,Home Lon,UUID,CRC,SNR (dB),CFO (Hz),Frequency (MHz)\n";
                }
            }
            else
            {
                qDebug() << "DroneIDDemod::applySettings - Unable to open log file: " << settings.m_logFilename;
            }
        }
    }

    bool scanChanged = (settingsKeys.contains("scan") && (settings.m_scan != m_settings.m_scan))
        || (settingsKeys.contains("scanBand") && (settings.m_scanBand != m_settings.m_scanBand))
        || force;

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (scanChanged)
    {
        if (m_settings.m_scan) {
            startScan();
        } else {
            stopScan();
        }
    }
}

// Scanning steps the device center frequency through the DroneID channels, so that the channel
// is on each of them for m_scanDwell seconds. When a burst with DroneID sync is detected, the scan
// pauses until no bursts have been detected for m_scanHold seconds.
// Each pass through the channels starts with those that bursts have previously been detected on,
// most recent first, as drones often reuse channels, followed by the remainder in frequency order.
void DroneIDDemod::startScan()
{
    m_scanFrequencies = DroneIDDemodSettings::getScanFrequencies(m_settings.m_scanBand);
    m_scanClock.start();
    m_scanBurstTime = -1;
    m_scanLocked = false;

    // Start on the current channel, if it's one of those being scanned
    m_scanFrequency = scanFrequencyMatch(m_centerFrequency + m_settings.m_inputFrequencyOffset);
    scanNewPass();

    if (m_scanFrequency != 0) {
        m_scanTuneTime = m_scanClock.elapsed() - m_scanSettleTime;
    } else {
        m_scanTuneTime = std::numeric_limits<qint64>::min() / 2; // Tune on first timeout when device is running
    }

    m_scanTimer.start(m_scanTimerInterval);
}

void DroneIDDemod::stopScan()
{
    m_scanTimer.stop();
    m_scanLocked = false;
}

// Returns the scanned frequency that matches the given frequency, or 0 if none
qint64 DroneIDDemod::scanFrequencyMatch(qint64 frequency) const
{
    for (const auto f : m_scanFrequencies)
    {
        // Allow for small differences due to LO correction or tuning resolution
        if (std::abs(f - frequency) <= 1000) {
            return f;
        }
    }

    return 0;
}

// Create order of frequencies to visit: previously used frequencies first, then the rest.
// The current frequency is visited last, as we've just been listening to it.
void DroneIDDemod::scanNewPass()
{
    m_scanOrder.clear();

    for (const auto f : m_scanHistory)
    {
        if (m_scanFrequencies.contains(f) && (f != m_scanFrequency)) {
            m_scanOrder.append(f);
        }
    }
    for (const auto f : m_scanFrequencies)
    {
        if (!m_scanOrder.contains(f) && (f != m_scanFrequency)) {
            m_scanOrder.append(f);
        }
    }
    if (m_scanFrequency != 0) {
        m_scanOrder.append(m_scanFrequency);
    }

    m_scanPosition = 0;
}

void DroneIDDemod::scanTune(qint64 frequency)
{
    qint64 centerFrequency = frequency - m_settings.m_inputFrequencyOffset;

    m_scanFrequency = frequency;
    m_scanTuneTime = m_scanClock.elapsed();

    if (!ChannelWebAPIUtils::setCenterFrequency(getDeviceSetIndex(), centerFrequency)) {
        qWarning() << "DroneIDDemod::scanTune: Failed to set device center frequency to" << centerFrequency;
    }
}

void DroneIDDemod::scanTimeout()
{
    if (m_scanOrder.isEmpty()) {
        return;
    }

    qint64 now = m_scanClock.elapsed();
    bool locked = (m_scanBurstTime >= 0) && (now - m_scanBurstTime < (qint64) (m_settings.m_scanHold * 1000.0f));

    m_scanLocked = locked;

    // Don't retune while device isn't running, as no bursts can be received
    if (locked || !m_basebandSink->isRunning()) {
        return;
    }

    if (m_scanBurstTime >= 0)
    {
        // Bursts have stopped on this channel. Start a new pass, so other previously used channels are searched first
        m_scanBurstTime = -1;
        scanNewPass();
    }
    else if (now - m_scanTuneTime < (qint64) (m_settings.m_scanDwell * 1000.0f))
    {
        return;
    }

    if (m_scanPosition >= m_scanOrder.size()) {
        scanNewPass();
    }

    scanTune(m_scanOrder[m_scanPosition++]);
}

QByteArray DroneIDDemod::serialize() const
{
    return m_settings.serialize();
}

bool DroneIDDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureDroneIDDemod *msg = MsgConfigureDroneIDDemod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureDroneIDDemod *msg = MsgConfigureDroneIDDemod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void DroneIDDemod::sendSampleRateToDemodAnalyzer()
{
    QList<ObjectPipe*> reportPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", reportPipes);

    for (const auto& pipe : reportPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(
            this,
            DroneIDDemodSettings::DRONEIDDEMOD_CHANNEL_SAMPLE_RATE
        );
        messageQueue->push(msg);
    }
}

int DroneIDDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDroneIdDemodSettings(new SWGSDRangel::SWGDroneIDDemodSettings());
    response.getDroneIdDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int DroneIDDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int DroneIDDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDroneIdDemodReport(new SWGSDRangel::SWGDroneIDDemodReport());
    response.getDroneIdDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void DroneIDDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getDroneIdDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getDroneIdDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
    response.getDroneIdDemodReport()->setBurstsDetected(getBurstsDetected());
    response.getDroneIdDemodReport()->setBurstsDecoded(getBurstsDecoded());
    response.getDroneIdDemodReport()->setBurstsDropped(getBurstsDropped());
    response.getDroneIdDemodReport()->setDronesTracked(getDronesTracked());
    response.getDroneIdDemodReport()->setScanLocked(isScanLocked() ? 1 : 0);
}

int DroneIDDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    DroneIDDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureDroneIDDemod *msg = MsgConfigureDroneIDDemod::create(channelSettingsKeys, settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("DroneIDDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDroneIDDemod *msgToGUI = MsgConfigureDroneIDDemod::create(channelSettingsKeys, settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void DroneIDDemod::webapiUpdateChannelSettings(
        DroneIDDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getDroneIdDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("detectionThreshold")) {
        settings.m_detectionThreshold = std::clamp(response.getDroneIdDemodSettings()->getDetectionThreshold(), 0.05f, 0.95f);
    }
    if (channelSettingsKeys.contains("turboIterations")) {
        settings.m_turboIterations = std::clamp(response.getDroneIdDemodSettings()->getTurboIterations(), 1, 16);
    }
    if (channelSettingsKeys.contains("filterSerial")) {
        settings.m_filterSerial = *response.getDroneIdDemodSettings()->getFilterSerial();
    }
    if (channelSettingsKeys.contains("mapTimeout")) {
        settings.m_mapTimeout = std::clamp(response.getDroneIdDemodSettings()->getMapTimeout(), 0, 3600);
    }
    if (channelSettingsKeys.contains("mapAltitude")) {
        settings.m_mapAltitude = (DroneIDDemodSettings::MapAltitude) std::clamp(response.getDroneIdDemodSettings()->getMapAltitude(),
            (int) DroneIDDemodSettings::MAP_ALTITUDE, (int) DroneIDDemodSettings::MAP_HEIGHT);
    }
    if (channelSettingsKeys.contains("scan")) {
        settings.m_scan = response.getDroneIdDemodSettings()->getScan() != 0;
    }
    if (channelSettingsKeys.contains("scanBand")) {
        settings.m_scanBand = (DroneIDDemodSettings::ScanBand) std::clamp(response.getDroneIdDemodSettings()->getScanBand(),
            (int) DroneIDDemodSettings::BAND_2_4_GHZ, (int) DroneIDDemodSettings::BAND_2_4_AND_5_8_GHZ);
    }
    if (channelSettingsKeys.contains("scanDwell")) {
        settings.m_scanDwell = std::clamp(response.getDroneIdDemodSettings()->getScanDwell(), 0.5f, 60.0f);
    }
    if (channelSettingsKeys.contains("scanHold")) {
        settings.m_scanHold = std::clamp(response.getDroneIdDemodSettings()->getScanHold(), 0.5f, 600.0f);
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getDroneIdDemodSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getDroneIdDemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getDroneIdDemodSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("logFilename")) {
        settings.m_logFilename = *response.getDroneIdDemodSettings()->getLogFilename();
    }
    if (channelSettingsKeys.contains("logEnabled")) {
        settings.m_logEnabled = response.getDroneIdDemodSettings()->getLogEnabled();
    }
    if (channelSettingsKeys.contains("useFileTime")) {
        settings.m_useFileTime = response.getDroneIdDemodSettings()->getUseFileTime();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getDroneIdDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getDroneIdDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getDroneIdDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getDroneIdDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getDroneIdDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getDroneIdDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getDroneIdDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getDroneIdDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getDroneIdDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getDroneIdDemodSettings()->getRollupState());
    }
}

void DroneIDDemod::setString(QString *(SWGSDRangel::SWGDroneIDDemodSettings::*get)(), void (SWGSDRangel::SWGDroneIDDemodSettings::*set)(QString*),
    SWGSDRangel::SWGDroneIDDemodSettings *swg, const QString& value)
{
    if ((swg->*get)()) {
        *(swg->*get)() = value;
    } else {
        (swg->*set)(new QString(value));
    }
}

void DroneIDDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DroneIDDemodSettings& settings)
{
    SWGSDRangel::SWGDroneIDDemodSettings *swg = response.getDroneIdDemodSettings();

    swg->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    swg->setDetectionThreshold(settings.m_detectionThreshold);
    swg->setTurboIterations(settings.m_turboIterations);
    setString(&SWGSDRangel::SWGDroneIDDemodSettings::getFilterSerial, &SWGSDRangel::SWGDroneIDDemodSettings::setFilterSerial, swg, settings.m_filterSerial);
    swg->setMapTimeout(settings.m_mapTimeout);
    swg->setMapAltitude((int) settings.m_mapAltitude);
    swg->setScan(settings.m_scan ? 1 : 0);
    swg->setScanBand((int) settings.m_scanBand);
    swg->setScanDwell(settings.m_scanDwell);
    swg->setScanHold(settings.m_scanHold);
    swg->setUdpEnabled(settings.m_udpEnabled);
    setString(&SWGSDRangel::SWGDroneIDDemodSettings::getUdpAddress, &SWGSDRangel::SWGDroneIDDemodSettings::setUdpAddress, swg, settings.m_udpAddress);
    swg->setUdpPort(settings.m_udpPort);
    setString(&SWGSDRangel::SWGDroneIDDemodSettings::getLogFilename, &SWGSDRangel::SWGDroneIDDemodSettings::setLogFilename, swg, settings.m_logFilename);
    swg->setLogEnabled(settings.m_logEnabled);
    swg->setUseFileTime(settings.m_useFileTime);
    swg->setRgbColor(settings.m_rgbColor);
    setString(&SWGSDRangel::SWGDroneIDDemodSettings::getTitle, &SWGSDRangel::SWGDroneIDDemodSettings::setTitle, swg, settings.m_title);
    swg->setStreamIndex(settings.m_streamIndex);
    swg->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);
    setString(&SWGSDRangel::SWGDroneIDDemodSettings::getReverseApiAddress, &SWGSDRangel::SWGDroneIDDemodSettings::setReverseApiAddress, swg, settings.m_reverseAPIAddress);
    swg->setReverseApiPort(settings.m_reverseAPIPort);
    swg->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    swg->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (swg->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(swg->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            swg->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (swg->getRollupState())
        {
            settings.m_rollupState->formatTo(swg->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            swg->setRollupState(swgRollupState);
        }
    }
}

void DroneIDDemod::webapiReverseSendSettings(const QStringList& channelSettingsKeys, const DroneIDDemodSettings& settings, bool force)
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

void DroneIDDemod::webapiFormatChannelSettings(
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DroneIDDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    if (swgChannelSettings->getChannelType()) {
        *swgChannelSettings->getChannelType() = m_channelId;
    } else {
        swgChannelSettings->setChannelType(new QString(m_channelId));
    }
    swgChannelSettings->setDroneIdDemodSettings(new SWGSDRangel::SWGDroneIDDemodSettings());
    SWGSDRangel::SWGDroneIDDemodSettings *swg = swgChannelSettings->getDroneIdDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swg->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("detectionThreshold") || force) {
        swg->setDetectionThreshold(settings.m_detectionThreshold);
    }
    if (channelSettingsKeys.contains("turboIterations") || force) {
        swg->setTurboIterations(settings.m_turboIterations);
    }
    if (channelSettingsKeys.contains("filterSerial") || force) {
        swg->setFilterSerial(new QString(settings.m_filterSerial));
    }
    if (channelSettingsKeys.contains("mapTimeout") || force) {
        swg->setMapTimeout(settings.m_mapTimeout);
    }
    if (channelSettingsKeys.contains("mapAltitude") || force) {
        swg->setMapAltitude((int) settings.m_mapAltitude);
    }
    if (channelSettingsKeys.contains("scan") || force) {
        swg->setScan(settings.m_scan ? 1 : 0);
    }
    if (channelSettingsKeys.contains("scanBand") || force) {
        swg->setScanBand((int) settings.m_scanBand);
    }
    if (channelSettingsKeys.contains("scanDwell") || force) {
        swg->setScanDwell(settings.m_scanDwell);
    }
    if (channelSettingsKeys.contains("scanHold") || force) {
        swg->setScanHold(settings.m_scanHold);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swg->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swg->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swg->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("logFilename") || force) {
        swg->setLogFilename(new QString(settings.m_logFilename));
    }
    if (channelSettingsKeys.contains("logEnabled") || force) {
        swg->setLogEnabled(settings.m_logEnabled);
    }
    if (channelSettingsKeys.contains("useFileTime") || force) {
        swg->setUseFileTime(settings.m_useFileTime);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swg->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swg->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swg->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swg->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swg->setRollupState(swgRollupState);
    }
}

void DroneIDDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "DroneIDDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("DroneIDDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void DroneIDDemod::handleIndexInDeviceSetChanged(int index)
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

int DroneIDDemod::getChannelSampleRate() const
{
    return m_basebandSink ? m_basebandSink->getChannelSampleRate() : 0;
}
