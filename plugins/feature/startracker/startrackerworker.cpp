///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <cmath>

#include <QDebug>
#include <QAbstractSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QEventLoop>
#include <QTimer>
#include <QDateTime>

#include "SWGTargetAzimuthElevation.h"
#include "SWGMapItem.h"

#include "webapi/webapiadapterinterface.h"
#include "webapi/webapiutils.h"

#include "util/units.h"
#include "maincore.h"

#include "startracker.h"
#include "startrackerworker.h"
#include "startrackerreport.h"

MESSAGE_CLASS_DEFINITION(StarTrackerWorker::MsgConfigureStarTrackerWorker, Message)
MESSAGE_CLASS_DEFINITION(StarTrackerReport::MsgReportAzAl, Message)
MESSAGE_CLASS_DEFINITION(StarTrackerReport::MsgReportRADec, Message)

StarTrackerWorker::StarTrackerWorker(StarTracker* starTracker, WebAPIAdapterInterface *webAPIAdapterInterface) :
    m_starTracker(starTracker),
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_msgQueueToFeature(nullptr),
    m_msgQueueToGUI(nullptr),
    m_running(false),
    m_mutex(QMutex::Recursive),
    m_tcpServer(nullptr),
    m_clientConnection(nullptr)
{
    connect(&m_pollTimer, SIGNAL(timeout()), this, SLOT(update()));
}

StarTrackerWorker::~StarTrackerWorker()
{
    m_inputMessageQueue.clear();
}

void StarTrackerWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

bool StarTrackerWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_pollTimer.start((int)round(m_settings.m_updatePeriod*1000.0));
    m_running = true;
    return m_running;
}

void StarTrackerWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    restartServer(false, 0);
    m_pollTimer.stop();
    m_running = false;
}

void StarTrackerWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool StarTrackerWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureStarTrackerWorker::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureStarTrackerWorker& cfg = (MsgConfigureStarTrackerWorker&) cmd;

        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else
    {
        return false;
    }
}

void StarTrackerWorker::applySettings(const StarTrackerSettings& settings, bool force)
{
    qDebug() << "StarTrackerWorker::applySettings:"
            << " m_target: " << settings.m_target
            << " m_ra: " << settings.m_ra
            << " m_dec: " << settings.m_dec
            << " m_time: " << settings.m_dateTime
            << " m_enableServer: " << settings.m_enableServer
            << " m_serverPort: " << settings.m_serverPort
            << " m_updatePeriod: " << settings.m_updatePeriod
            << " force: " << force;

    if ((m_settings.m_target != settings.m_target)
        || (m_settings.m_latitude != settings.m_latitude)
        || (m_settings.m_longitude != settings.m_longitude)
        || (m_settings.m_dateTime != settings.m_dateTime)
        || (m_settings.m_refraction != settings.m_refraction)
        || (m_settings.m_pressure != settings.m_pressure)
        || (m_settings.m_temperature != settings.m_temperature)
        || (m_settings.m_ra != settings.m_ra)
        || (m_settings.m_dec != settings.m_dec) || force)
    {
        // Recalculate immediately
        QTimer::singleShot(1, this, &StarTrackerWorker::update);
        m_pollTimer.start((int)round(settings.m_updatePeriod*1000.0));
    }
    else if ((m_settings.m_updatePeriod != settings.m_updatePeriod) || force)
    {
        m_pollTimer.start((int)round(settings.m_updatePeriod*1000.0));
    }

    if (!settings.m_drawSunOnMap && m_settings.m_drawSunOnMap)
        removeFromMap("Sun");
    if (!settings.m_drawMoonOnMap && m_settings.m_drawMoonOnMap)
        removeFromMap("Moon");
    if ((!settings.m_drawStarOnMap && m_settings.m_drawStarOnMap)
        || (((settings.m_target == "Sun") || (settings.m_target == "Moon"))
            && ((m_settings.m_target != "Sun") && (m_settings.m_target != "Moon"))))
        removeFromMap("Star");

    if ((settings.m_serverPort != m_settings.m_serverPort) ||
        (settings.m_enableServer != m_settings.m_enableServer) || force)
    {
        restartServer(settings.m_enableServer, settings.m_serverPort);
    }

    m_settings = settings;
}

void StarTrackerWorker::restartServer(bool enabled, uint32_t port)
{
    if (m_tcpServer)
    {
        if (m_clientConnection)
        {
            m_clientConnection->close();
            delete m_clientConnection;
            m_clientConnection = nullptr;
        }

        disconnect(m_tcpServer, &QTcpServer::newConnection, this, &StarTrackerWorker::acceptConnection);
        m_tcpServer->close();
        delete m_tcpServer;
        m_tcpServer = nullptr;
    }

    if (enabled)
    {
        qDebug() << "StarTrackerWorker::restartServer: server enabled on port " << port;
        m_tcpServer = new QTcpServer(this);

        if (!m_tcpServer->listen(QHostAddress::Any, port)) {
            qWarning("Star Tracker failed to listen on port %u. Check it is not already in use.", port);
        } else {
            connect(m_tcpServer, &QTcpServer::newConnection, this, &StarTrackerWorker::acceptConnection);
        }
    }
}

void StarTrackerWorker::acceptConnection()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_clientConnection = m_tcpServer->nextPendingConnection();

    if (!m_clientConnection) {
        return;
    }

    connect(m_clientConnection, &QIODevice::readyRead, this, &StarTrackerWorker::readStellariumCommand);
    connect(m_clientConnection, SIGNAL(disconnected()), this, SLOT(disconnected()));
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(m_clientConnection, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &StarTrackerWorker::errorOccurred);
#else
    connect(m_clientConnection, &QAbstractSocket::errorOccurred, this, &StarTrackerWorker::errorOccurred);
#endif
    qDebug() << "StarTrackerWorker::acceptConnection: client connected";
}

void StarTrackerWorker::disconnected()
{
    QMutexLocker mutexLocker(&m_mutex);
    qDebug() << "StarTrackerWorker::disconnected";
    m_clientConnection->deleteLater();
    m_clientConnection = nullptr;
    /*if (m_msgQueueToFeature)
    {
        StarTrackerWorker::MsgReportWorker *msg = StarTrackerWorker::MsgReportWorker::create("Disconnected");
        m_msgQueueToFeature->push(msg);
    }*/
}

void StarTrackerWorker::errorOccurred(QAbstractSocket::SocketError socketError)
{
    qDebug() << "StarTrackerWorker::errorOccurred: " << socketError;
    /*if (m_msgQueueToFeature)
    {
        StarTrackerWorker::MsgReportWorker *msg = StarTrackerWorker::MsgReportWorker::create(m_socket.errorString() + " " + socketError);
        m_msgQueueToFeature->push(msg);
    }*/
}

// Get RA & Dec from Stellarium
// Protocol described here:
// http://svn.code.sf.net/p/stellarium/code/trunk/telescope_server/stellarium_telescope_protocol.txt
void StarTrackerWorker::readStellariumCommand()
{
    QMutexLocker mutexLocker(&m_mutex);

    unsigned char buf[64];
    qint64 len;

    len = m_clientConnection->read((char *)buf, sizeof(buf));
    if (len != -1)
    {
        int msg_len;
        int msg_type;
        unsigned char *msg;

        // Extract length and message type
        msg_len = buf[0] | (buf[1] << 8);
        msg_type = buf[2] | (buf[3] << 8);
        msg = &buf[4];

        if (msg_type == 0)  // MessageGoto
        {
            unsigned ra;
            int dec;

            if (msg_len == 20)
            {
                // Skip time
                msg += 8;
                // Extract RA LSB first
                ra = msg[0] | (msg[1] << 8) | (msg[2] << 16) | (msg[3] << 24);
                msg += 4;
                // Extract DEC LSB first
                dec = msg[0] | (msg[1] << 8) | (msg[2] << 16) | (msg[3] << 24);
                msg += 4;

                // Convert from integer to floating point
                double raDeg = ra*(24.0/4294967296.0);    // Convert to decimal hours
                double decDeg = dec*(360.0/4294967296.0);  // Convert to decimal degrees

                // Set as current target
                m_settings.m_ra = Units::decimalHoursToHoursMinutesAndSeconds(raDeg);
                m_settings.m_dec = Units::decimalDegreesToDegreeMinutesAndSeconds(decDeg);

                qDebug() << "StarTrackerWorker: New target from Stellarum: " << m_settings.m_ra << " " << m_settings.m_dec;

                // Forward to GUI for display
                if (getMessageQueueToGUI())
                    getMessageQueueToGUI()->push(StarTrackerReport::MsgReportRADec::create(raDeg, decDeg));
            }
            else
            {
                qDebug() << "StarTrackerWorker: Unexpected number of bytes received (" << len << ") for message type: " <<  msg_type;
            }
        }
        else
        {
            qDebug() << "StarTrackerWorker: Unsupported Stellarium message type: " << msg_type;
        }
    }
}

// Send our target to Stellarium (J2000 epoch)
void StarTrackerWorker::writeStellariumTarget(double ra, double dec)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_clientConnection != nullptr)
    {
        unsigned char buf[24];

        // Length
        buf[0] = sizeof(buf);
        buf[1] = 0;
        // Type (MessageCurrentPosition)
        buf[2] = 0;
        buf[3] = 0;
        // Time (unused)
        buf[4] = 0;
        buf[5] = 0;
        buf[6] = 0;
        buf[7] = 0;
        buf[8] = 0;
        buf[9] = 0;
        buf[10] = 0;
        buf[11] = 0;
        // RA
        unsigned raInt = ra * (4294967296.0/24.0);
        buf[12] = raInt & 0xff;
        buf[13] = (raInt >> 8) & 0xff;
        buf[14] = (raInt >> 16) & 0xff;
        buf[15] = (raInt >> 24) & 0xff;
        // Dec
        int decInt = dec * (4294967296.0/360.0);
        buf[16] = decInt & 0xff;
        buf[17] = (decInt >> 8) & 0xff;
        buf[18] = (decInt >> 16) & 0xff;
        buf[19] = (decInt >> 24) & 0xff;
        // Status (OK)
        buf[20] = 0;
        buf[21] = 0;
        buf[22] = 0;
        buf[23] = 0;

        m_clientConnection->write((char *)buf, sizeof(buf));
    }
}


void StarTrackerWorker::updateRaDec(RADec rd, QDateTime dt)
{
    RADec rdJ2000;
    double jd;

    jd = Astronomy::julianDate(dt);
    // Precess to J2000
    rdJ2000 = Astronomy::precess(rd, jd, Astronomy::jd_j2000());
    // Send to Stellarium
    writeStellariumTarget(rdJ2000.ra, rdJ2000.dec);
    // Send to GUI
    if (m_settings.m_target == "Sun" || m_settings.m_target == "Moon")
    {
        if (getMessageQueueToGUI())
        {
            if (m_settings.m_jnow)
                getMessageQueueToGUI()->push(StarTrackerReport::MsgReportRADec::create(rd.ra, rd.dec));
            else
                getMessageQueueToGUI()->push(StarTrackerReport::MsgReportRADec::create(rdJ2000.ra, rdJ2000.dec));
        }
    }
}

void StarTrackerWorker::removeFromMap(QString id)
{
    MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
    QList<MessageQueue*> *mapMessageQueues = messagePipes.getMessageQueues(m_starTracker, "mapitems");
    if (mapMessageQueues)
    {
        sendToMap(mapMessageQueues, id, "", "", 0.0, 0.0);
    }
}

void StarTrackerWorker::sendToMap(QList<MessageQueue*> *mapMessageQueues, QString name, QString image, QString text, double lat, double lon, double rotation)
{
    QList<MessageQueue*>::iterator it = mapMessageQueues->begin();

    for (; it != mapMessageQueues->end(); ++it)
    {
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setLatitude(lat);
        swgMapItem->setLongitude(lon);
        swgMapItem->setImage(new QString(image));
        swgMapItem->setImageRotation(rotation);
        swgMapItem->setText(new QString(text));
        swgMapItem->setImageMinZoom(3);

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_starTracker, swgMapItem);
        (*it)->push(msg);
    }
}

QString moonPhase(double sunLongitude, double moonLongitude, double observationLatitude, double &rotation)
{
    double difference = sunLongitude - moonLongitude;
    if (difference < -180.0)
        difference += 360.0;
    else if (difference > 180.0)
        difference -= 360.0;

    if (difference >= 0.0)
        rotation = observationLatitude - 90.0;
    else
        rotation = 90.0 - observationLatitude;

    // These probably shouldn't be divided equally
    if (difference < -157.5)
        return "full";
    else if (difference < -112.5)
        return "waxing-gibbous";
    else if (difference < -67.5)
        return "first-quarter";
    else if (difference < -22.5)
        return "waxing-crescent";
    else if (difference < 22.5)
        return "new";
    else if (difference < 67.5)
        return "waning-crescent";
    else if (difference < 112.5)
        return "third-quarter";
    else if (difference < 157.5)
        return "waning-gibbous";
    else if (difference < 202.5)
        return "full";
    else
        return "full";
}

void StarTrackerWorker::update()
{
    AzAlt aa, sunAA, moonAA;
    RADec rd, sunRD, moonRD;

    QDateTime dt;

    // Get date and time to calculate position at
    if (m_settings.m_dateTime == "")
        dt = QDateTime::currentDateTime();
    else
        dt = QDateTime::fromString(m_settings.m_dateTime, Qt::ISODateWithMs);

    // Calculate position
    if ((m_settings.m_target == "Sun") || (m_settings.m_drawSunOnMap))
        Astronomy::sunPosition(sunAA, sunRD, m_settings.m_latitude, m_settings.m_longitude, dt);
    if ((m_settings.m_target == "Moon") || (m_settings.m_drawMoonOnMap))
        Astronomy::moonPosition(moonAA, moonRD, m_settings.m_latitude, m_settings.m_longitude, dt);

    if (m_settings.m_target == "Sun")
    {
       rd = sunRD;
       aa = sunAA;
    }
    else if (m_settings.m_target == "Moon")
    {
       rd = moonRD;
       aa = moonAA;
    }
    else if (m_settings.m_target == "Custom Az/El")
    {
        // Convert Alt/Az to RA/Dec
        aa.alt = m_settings.m_el;
        aa.az = m_settings.m_az;
        rd = Astronomy::azAltToRaDec(aa, m_settings.m_latitude, m_settings.m_longitude, dt);
    }
    else
    {
        // Convert RA/Dec to Alt/Az
        rd.ra = Astronomy::raToDecimal(m_settings.m_ra);
        rd.dec = Astronomy::decToDecimal(m_settings.m_dec);
        aa = Astronomy::raDecToAzAlt(rd, m_settings.m_latitude, m_settings.m_longitude, dt, !m_settings.m_jnow);
    }
    updateRaDec(rd, dt);

    // Adjust for refraction
    if (m_settings.m_refraction == "Positional Astronomy Library")
    {
        aa.alt += Astronomy::refractionPAL(aa.alt, m_settings.m_pressure, m_settings.m_temperature, m_settings.m_humidity,
                                            m_settings.m_frequency, m_settings.m_latitude, m_settings.m_heightAboveSeaLevel,
                                            m_settings.m_temperatureLapseRate);
        if (aa.alt > 90.0)
            aa.alt = 90.0f;
    }
    else if (m_settings.m_refraction == "Saemundsson")
    {
        aa.alt += Astronomy::refractionSaemundsson(aa.alt, m_settings.m_pressure, m_settings.m_temperature);
        if (aa.alt > 90.0)
            aa.alt = 90.0f;
    }

    // Send to GUI
    if (getMessageQueueToGUI())
    {
        if (m_settings.m_target == "Custom Az/El") {
            getMessageQueueToGUI()->push(StarTrackerReport::MsgReportRADec::create(rd.ra, rd.dec));
        } else {
            getMessageQueueToGUI()->push(StarTrackerReport::MsgReportAzAl::create(aa.az, aa.alt));
        }
    }

    // Send Az/El to Rotator Controllers
    MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
    QList<MessageQueue*> *mapMessageQueues = messagePipes.getMessageQueues(m_starTracker, "target");
    if (mapMessageQueues)
    {
        QList<MessageQueue*>::iterator it = mapMessageQueues->begin();

        for (; it != mapMessageQueues->end(); ++it)
        {
            SWGSDRangel::SWGTargetAzimuthElevation *swgTarget = new SWGSDRangel::SWGTargetAzimuthElevation();
            swgTarget->setName(new QString(m_settings.m_target));
            swgTarget->setAzimuth(aa.az);
            swgTarget->setElevation(aa.alt);
            (*it)->push(MainCore::MsgTargetAzimuthElevation::create(m_starTracker, swgTarget));
        }
    }

    // Send to Map
    if (m_settings.m_drawSunOnMap || m_settings.m_drawMoonOnMap || m_settings.m_drawStarOnMap)
    {
        mapMessageQueues = messagePipes.getMessageQueues(m_starTracker, "mapitems");
        if (mapMessageQueues)
        {
            // Different between GMST(Lst at Greenwich) and RA
            double lst = Astronomy::localSiderealTime(dt, 0.0);
            double sunLongitude;
            double sunLatitude;

            if (m_settings.m_drawSunOnMap || m_settings.m_drawMoonOnMap)
            {
                sunLongitude = Astronomy::lstAndRAToLongitude(lst, sunRD.ra);
                sunLatitude = sunRD.dec;
                sendToMap(mapMessageQueues, "Sun", "qrc:///startracker/startracker/sun-40.png", "Sun", sunLatitude, sunLongitude);
            }
            if (m_settings.m_drawMoonOnMap)
            {
                double moonLongitude = Astronomy::lstAndRAToLongitude(lst, moonRD.ra);
                double moonLatitude = moonRD.dec;
                double moonRotation;
                QString phase = moonPhase(sunLongitude, moonLongitude, m_settings.m_latitude, moonRotation);
                sendToMap(mapMessageQueues, "Moon", QString("qrc:///startracker/startracker/moon-%1-32").arg(phase), "Moon",
                                moonLatitude, moonLongitude, moonRotation);
            }
            if ((m_settings.m_drawStarOnMap) && (m_settings.m_target != "Sun") && (m_settings.m_target != "Moon"))
            {
                double starLongitude = Astronomy::lstAndRAToLongitude(lst, rd.ra);
                double starLatitude = rd.dec;
                QString text = m_settings.m_target.startsWith("Custom") ? "Star" : m_settings.m_target;
                sendToMap(mapMessageQueues, "Star", "qrc:///startracker/startracker/pulsar-32.png", text, starLatitude, starLongitude);
            }
        }
    }

}
