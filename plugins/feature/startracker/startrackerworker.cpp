///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2026 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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
#include <QVector3D>

#include "SWGTargetAzimuthElevation.h"
#include "SWGMapItem.h"
#include "SWGStarTrackerTarget.h"
#include "SWGSkyMapTarget.h"

#include "webapi/webapiadapterinterface.h"
#include "channel/channelwebapiutils.h"

#include "util/units.h"
#include "util/profiler.h"
#include "maincore.h"

#include "startracker.h"
#include "startrackerworker.h"
#include "startrackerreport.h"
#include "spice.h"
#include "spiceephemerides.h"

MESSAGE_CLASS_DEFINITION(StarTrackerWorker::MsgConfigureStarTrackerWorker, Message)
MESSAGE_CLASS_DEFINITION(StarTrackerReport::MsgReportAzAl, Message)
MESSAGE_CLASS_DEFINITION(StarTrackerReport::MsgReportRADec, Message)
MESSAGE_CLASS_DEFINITION(StarTrackerReport::MsgReportGalactic, Message)
MESSAGE_CLASS_DEFINITION(StarTrackerReport::MsgReportAzElVsTime, Message)
MESSAGE_CLASS_DEFINITION(StarTrackerReport::MsgReportSolarSystemPositions, Message)
MESSAGE_CLASS_DEFINITION(StarTrackerReport::MsgReportJupiter, Message)
MESSAGE_CLASS_DEFINITION(StarTrackerReport::MsgReportJupiterData, Message)

static double linearInterpolate(double y1, double y2, double mu)
{
    return y1 + mu * (y2 - y1);
}

static double cubicInterpolate(double y0, double y1, double y2, double y3, double mu)
{
    double a0, a1, a2, a3, mu2;

    mu2 = mu * mu;
    a0 = y3 - y2 - y0 + y1;
    a1 = y0 - y1 - a0;
    a2 = y2 - y0;
    a3 = y1;

    return a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3;
}

StarTrackerWorker::StarTrackerWorker(StarTracker* starTracker, WebAPIAdapterInterface *webAPIAdapterInterface) :
    m_starTracker(starTracker),
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_msgQueueToFeature(nullptr),
    m_msgQueueToGUI(nullptr),
    m_pollTimer(this),
    m_tcpServer(nullptr),
    m_clientConnection(nullptr),
    m_solarFlux(0.0f),
    m_jplHorizons(nullptr),
    m_requestedEphemeridesLatitude(0.0),
    m_requestedEphemeridesLongitude(0.0),
    m_chartLatitude(0.0),
    m_chartLongitude(0.0),
    m_chartL(0.0f),
    m_chartB(0.0f)
{
    connect(&m_pollTimer, &QTimer::timeout, this, &StarTrackerWorker::update);
}

StarTrackerWorker::~StarTrackerWorker()
{
    stopWork();
    m_inputMessageQueue.clear();
    if (m_jplHorizons)
    {
        disconnect(m_jplHorizons, &JPLHorizons::ephemeridesUpdated, this, &StarTrackerWorker::horizonsEphemeridesUpdated);
        delete m_jplHorizons;
    }
}

void StarTrackerWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_pollTimer.start((int)round(m_settings.m_updatePeriod*1000.0));
    // Handle any messages already on the queue
    handleInputMessages();
}

void StarTrackerWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_pollTimer.stop();
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    if (m_settings.m_drawSunOnMap)
        removeFromMap("Sun");
    if (m_settings.m_drawMoonOnMap)
        removeFromMap("Moon");
    if (m_settings.m_drawStarOnMap && (m_settings.m_target != "Sun") && (m_settings.m_target != "Moon"))
        removeFromMap("Star");

    restartServer(false, 0);
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

        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());
        return true;
    }
    else if (StarTracker::MsgSetSolarFlux::match(cmd))
    {
        StarTracker::MsgSetSolarFlux& msg = (StarTracker::MsgSetSolarFlux&) cmd;
        m_solarFlux = msg.getFlux();
        return true;
    }
    else
    {
        return false;
    }
}

void StarTrackerWorker::applySettings(const StarTrackerSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "StarTrackerWorker::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    if (settingsKeys.contains("ra")
        || settingsKeys.contains("dec")
        || settingsKeys.contains("latitude")
        || settingsKeys.contains("longitude")
        || settingsKeys.contains("target")
        || settingsKeys.contains("dateTime")
        || settingsKeys.contains("refraction")
        || settingsKeys.contains("pressure")
        || settingsKeys.contains("temperature")
        || settingsKeys.contains("humidity")
        || settingsKeys.contains("heightAboveSeaLevel")
        || settingsKeys.contains("temperatureLapseRate")
        || settingsKeys.contains("frequency")
        || settingsKeys.contains("beamwidth")
        || settingsKeys.contains("azimuth")
        || settingsKeys.contains("elevation")
        || settingsKeys.contains("l")
        || settingsKeys.contains("b")
        || settingsKeys.contains("azimuthOffset")
        || settingsKeys.contains("elevationOffset")
        || settingsKeys.contains("night")
        || settingsKeys.contains("logScale")
        || settingsKeys.contains("utc")
        || settingsKeys.contains("chartSelect")
        || settingsKeys.contains("chartSubSelect")
        || force)
    {
        // Recalculate immediately
        QTimer::singleShot(1, this, &StarTrackerWorker::update);
        m_pollTimer.start((int)round(settings.m_updatePeriod*1000.0));
    }
    else if (settingsKeys.contains("updatePeriod") || force)
    {
        m_pollTimer.start((int)round(settings.m_updatePeriod*1000.0));
    }

    if (settingsKeys.contains("drawSunOnMap")
     || settingsKeys.contains("drawMoonOnMap")
     || settingsKeys.contains("drawStarOnMap")
     || settingsKeys.contains("target"))
    {
        if (!settings.m_drawSunOnMap && m_settings.m_drawSunOnMap)
            removeFromMap("Sun");
        if (!settings.m_drawMoonOnMap && m_settings.m_drawMoonOnMap)
            removeFromMap("Moon");
        if ((!settings.m_drawStarOnMap && m_settings.m_drawStarOnMap)
            || (((settings.m_target == "Sun") || (settings.m_target == "Moon"))
                && ((m_settings.m_target != "Sun") && (m_settings.m_target != "Moon"))))
            removeFromMap("Star");
    }

    if (settingsKeys.contains("serverPort") ||
        settingsKeys.contains("enableServer") || force)
    {
        restartServer(settings.m_enableServer, settings.m_serverPort);
    }

    if ((settingsKeys.contains("targetSource") || force) && (settings.m_targetSource == "Horizons"))
    {
        if (!m_jplHorizons)
        {
            m_jplHorizons = JPLHorizons::create();
            if (m_jplHorizons) {
                connect(m_jplHorizons, &JPLHorizons::ephemeridesUpdated, this, &StarTrackerWorker::horizonsEphemeridesUpdated);
            }
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
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
                if (getMessageQueueToGUI()) {
                    getMessageQueueToGUI()->push(StarTrackerReport::MsgReportRADec::create(raDeg, decDeg, "target"));
                }
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

void StarTrackerWorker::removeFromMap(QString id)
{
    QList<ObjectPipe*> mapMessagePipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_starTracker, "mapitems", mapMessagePipes);

    if (mapMessagePipes.size() > 0) {
        sendToMap(mapMessagePipes, id, "", "", 0.0, 0.0);
    }
}

void StarTrackerWorker::sendToMap(
    const QList<ObjectPipe*>& mapMessagePipes,
    QString name,
    QString image,
    QString text,
    double lat,
    double lon,
    double rotation
)
{
    for (const auto& pipe : mapMessagePipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setLatitude(lat);
        swgMapItem->setLongitude(lon);
        swgMapItem->setImage(new QString(image));
        swgMapItem->setImageRotation(rotation);
        swgMapItem->setText(new QString(text));

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_starTracker, swgMapItem);
        messageQueue->push(msg);
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

bool StarTrackerWorker::getAzElFromSatelliteTracker(double &azimuth, double &elevation)
{
    unsigned int satelliteTrackerFeatureSetIndex,satelliteTrackerFeatureIndex;

    if (MainCore::getFeatureIndexFromId(m_settings.m_target, satelliteTrackerFeatureSetIndex, satelliteTrackerFeatureIndex))
    {
        if (ChannelWebAPIUtils::getFeatureReportValue(satelliteTrackerFeatureSetIndex, satelliteTrackerFeatureIndex, "targetAzimuth", azimuth)
            && ChannelWebAPIUtils::getFeatureReportValue(satelliteTrackerFeatureSetIndex, satelliteTrackerFeatureIndex, "targetElevation", elevation))
        {
            return true;
        }
        else
        {
            qDebug() << "StarTrackerWorker::getAzElFromSatelliteTracker - Failed to get feature report value (targetAzimuth/targetElevation) from" << m_settings.m_target;
            return false;
        }
    }
    else
    {
        qDebug() << "StarTrackerWorker::getAzElFromSatelliteTracker - Failed to parse feature name" << m_settings.m_target;
        return false;
    }
}

bool StarTrackerWorker::getRAFromSkyMap(RADec &rdJ2000)
{
    double ra, dec;
    unsigned int featureSetIndex,featureIndex;

    if (MainCore::getFeatureIndexFromId(m_settings.m_target, featureSetIndex, featureIndex))
    {
        if (ChannelWebAPIUtils::getFeatureReportValue(featureSetIndex, featureIndex, "ra", ra)
            && ChannelWebAPIUtils::getFeatureReportValue(featureSetIndex, featureIndex, "dec", dec))
        {
            rdJ2000.ra = ra;
            rdJ2000.dec = dec;
            return true;
        }
        else
        {
            qDebug() << "StarTrackerWorker::getRAFromSkyMap - Failed to get feature report value (ra/dec) from" << m_settings.m_target;
            return false;
        }
    }
    else
    {
        qDebug() << "StarTrackerWorker::getRAFromSkyMap - Failed to parse feature name" << m_settings.m_target;
        return false;
    }
}

bool StarTrackerWorker::getRAFromHorizons(const QDateTime &dateTime, RADec &rdJ2000)
{

    if (   (m_requestedEphemeridesTarget != m_settings.m_target)
        || (dateTime < m_requestedEphemeridesStartTime)
        || (dateTime > m_requestedEphemeridesStopTime)
        || (m_requestedEphemeridesLatitude != m_settings.m_latitude)
        || (m_requestedEphemeridesLongitude != m_settings.m_longitude)
        )
    {
        // Clear current data
        m_ephemeridesTarget = "";
        m_horizonsEphemerides = {};
        // Request ephemerides
        m_requestedEphemeridesTarget = m_settings.m_target;
        m_requestedEphemeridesStartTime = QDateTime(dateTime.date(), QTime(0, 0));
        m_requestedEphemeridesStopTime = QDateTime(dateTime.date().addDays(1), QTime(0, 0));
        m_requestedEphemeridesLatitude =  m_settings.m_latitude;
        m_requestedEphemeridesLongitude = m_settings.m_longitude;
        m_jplHorizons->getData(m_requestedEphemeridesTarget, m_requestedEphemeridesStartTime, m_requestedEphemeridesStopTime, m_requestedEphemeridesLatitude, m_requestedEphemeridesLongitude, 0.0f);
    }
    else if (   (m_settings.m_target == m_ephemeridesTarget)
         && (   !m_horizonsEphemerides.isEmpty()
             && (   (dateTime >= m_horizonsEphemerides[0].m_dateTime)
                 || (dateTime < m_horizonsEphemerides[m_horizonsEphemerides.size() - 1].m_dateTime)
                )
            )
        )
    {
        // Find closest empheris
        for (int i = 0; i < m_horizonsEphemerides.size() - 1; i++)
        {
            if ((dateTime >= m_horizonsEphemerides[i].m_dateTime) && (dateTime < m_horizonsEphemerides[i+1].m_dateTime))
            {
                // Interpolate

                qint64 x = dateTime.toMSecsSinceEpoch();
                qint64 x1 = m_horizonsEphemerides[i].m_dateTime.toMSecsSinceEpoch();
                qint64 x2 = m_horizonsEphemerides[i+1].m_dateTime.toMSecsSinceEpoch();
                qint64 diff = x2 - x1;
                double mu = (x - x1) / (double) diff;

                if ((i == 0) || (i >= m_horizonsEphemerides.size() - 2))
                {
                    rdJ2000.ra = linearInterpolate(m_horizonsEphemerides[i].m_ra, m_horizonsEphemerides[i+1].m_ra, mu);
                    rdJ2000.dec = linearInterpolate(m_horizonsEphemerides[i].m_dec, m_horizonsEphemerides[i+1].m_dec, mu);
                }
                else
                {
                    rdJ2000.ra = cubicInterpolate(m_horizonsEphemerides[i-1].m_ra, m_horizonsEphemerides[i].m_ra, m_horizonsEphemerides[i+1].m_ra, m_horizonsEphemerides[i+2].m_ra, mu);
                    rdJ2000.dec = cubicInterpolate(m_horizonsEphemerides[i-1].m_dec, m_horizonsEphemerides[i].m_dec, m_horizonsEphemerides[i+1].m_dec, m_horizonsEphemerides[i+2].m_dec, mu);
                }
                return true;
            }
        }
    }
    return false;
}

void StarTrackerWorker::update()
{
    AzAlt aa, sunAA, moonAA;
    RADec rdJnow, sunRD, moonRD;
    bool rdJnowValid = false;
    double l, b;
    bool lbTarget = false;
    bool horizonsTarget = false;
    QDateTime dt;

    const QStringList raDecTargets = {
        "Custom RA/Dec", "PSR B0329+54", "PSR B0833-45", "Sagittarius A", "Cassiopeia A", "Cygnus A", "Taurus A (M1)", "Virgo A (M87)"
    };
    const QStringList lbTargets = {
        "Custom l/b", "S7", "S8", "S9"
    };

    PROFILER_START();

    spiceLock(SpiceEphemerides::getEphemeridesFiles(m_settings.m_spiceEphemerides));

    // Get date and time to calculate position at
    dt = m_settings.getDateTime();

    // Calculate position
    if ((m_settings.m_target == "Sun") || m_settings.m_drawSunOnMap || m_settings.m_drawSunOnSkyTempChart)
    {
        Astronomy::sunPosition(sunAA, sunRD, m_settings.m_latitude, m_settings.m_longitude, dt);
        getMessageQueueToGUI()->push(StarTrackerReport::MsgReportRADec::create(sunRD.ra, sunRD.dec, "sun"));
    }
    if ((m_settings.m_target == "Moon") || m_settings.m_drawMoonOnMap || m_settings.m_drawMoonOnSkyTempChart)
    {
        Astronomy::moonPosition(moonAA, moonRD, m_settings.m_latitude, m_settings.m_longitude, dt);
        getMessageQueueToGUI()->push(StarTrackerReport::MsgReportRADec::create(moonRD.ra, moonRD.dec, "moon"));
    }

    if ((m_settings.m_target == "Sun") && (m_settings.m_targetSource == "SDRangel"))
    {
        rdJnow = sunRD;
        rdJnowValid = true;
        aa = sunAA;
        RADec rdJ2000 = Astronomy::precess(rdJnow, Astronomy::julianDate(dt), Astronomy::jd_j2000());
        Astronomy::equatorialToGalactic(rdJ2000.ra, rdJ2000.dec, l, b);
    }
    else if ((m_settings.m_target == "Moon") && (m_settings.m_targetSource == "SDRangel"))
    {
        rdJnow = moonRD;
        rdJnowValid = true;
        aa = moonAA;
        RADec rdJ2000 = Astronomy::precess(rdJnow, Astronomy::julianDate(dt), Astronomy::jd_j2000());
        Astronomy::equatorialToGalactic(rdJ2000.ra, rdJ2000.dec, l, b);
    }
    else if (m_settings.m_target.contains("SatelliteTracker"))
    {
        // Get Az/El from Satellite Tracker
        double azimuth, elevation;
        if (getAzElFromSatelliteTracker(azimuth, elevation))
        {
            m_settings.m_el = elevation;
            m_settings.m_az = azimuth;
            // Convert Alt/Az to RA/Dec and l/b
            aa.alt = m_settings.m_el;
            aa.az = m_settings.m_az;
            rdJnow = Astronomy::azAltToRaDec(aa, m_settings.m_latitude, m_settings.m_longitude, dt);
            rdJnowValid = true;
            RADec rdJ2000 = Astronomy::precess(rdJnow, Astronomy::julianDate(dt), Astronomy::jd_j2000());
            Astronomy::equatorialToGalactic(rdJ2000.ra, rdJ2000.dec, l, b);
        }
    }
    else if (m_settings.m_target == "Custom Az/El")
    {
        // Convert Alt/Az to RA/Dec and l/b
        aa.alt = m_settings.m_el;
        aa.az = m_settings.m_az;
        rdJnow = Astronomy::azAltToRaDec(aa, m_settings.m_latitude, m_settings.m_longitude, dt);
        rdJnowValid = true;
        RADec rdJ2000 = Astronomy::precess(rdJnow, Astronomy::julianDate(dt), Astronomy::jd_j2000());
        Astronomy::equatorialToGalactic(rdJ2000.ra, rdJ2000.dec, l, b);
    }
    else if (lbTargets.contains(m_settings.m_target))
    {
        // Convert l/b to RA/Dec, then Alt/Az
        l = m_settings.m_l;
        b = m_settings.m_b;
        RADec rdJ2000;
        Astronomy::galacticToEquatorial(l, b, rdJ2000.ra, rdJ2000.dec);
        aa = Astronomy::raDecToAzAlt(rdJ2000, m_settings.m_latitude, m_settings.m_longitude, dt, true);
        lbTarget = true;
        rdJnow = Astronomy::precess(rdJ2000, Astronomy::jd_j2000(), Astronomy::julianDate(dt));
        rdJnowValid = true;
    }
    else if (m_settings.m_targetSource == "SPICE")
    {
        // Calculate Alt/Az and RA/Dec with SPICE, then convert to l/b
        double et;

        if (dateTimeToET(dt, et))
        {
            getAzElFromSPICE(m_settings.m_target, et, m_settings.m_latitude, m_settings.m_longitude, 0.0, aa.az, aa.alt);

            RADec rdJ2000;
            if (getRADecFromSPICE(m_settings.m_target, et, rdJ2000.ra, rdJ2000.dec))
            {
                rdJnow = Astronomy::precess(rdJ2000, Astronomy::jd_j2000(), Astronomy::julianDate(dt));
                rdJnowValid = true;
                Astronomy::equatorialToGalactic(rdJ2000.ra, rdJ2000.dec, l, b);
            }
        }
    }
    else
    {
        RADec rdJ2000;
        if (m_settings.m_target.contains("SkyMap"))
        {
            // Get RA/Dec from Sky Map
            if (getRAFromSkyMap(rdJ2000))
            {
                m_settings.m_ra = QString::number(rdJ2000.ra, 'f', 10);;
                m_settings.m_dec = QString::number(rdJ2000.dec, 'f', 10);;
                rdJnow = Astronomy::precess(rdJ2000, Astronomy::jd_j2000(), Astronomy::julianDate(dt));
                rdJnowValid = true;
            }
        }
        else if ((m_settings.m_targetSource == "Horizons") && !raDecTargets.contains(m_settings.m_target))
        {
            // Get RA/Dec from Horizons
            if (getRAFromHorizons(dt, rdJ2000))
            {
                m_settings.m_ra = QString::number(rdJ2000.ra, 'f', 10);;
                m_settings.m_dec = QString::number(rdJ2000.dec, 'f', 10);;
                rdJnow = Astronomy::precess(rdJ2000, Astronomy::jd_j2000(), Astronomy::julianDate(dt));
                rdJnowValid = true;
                horizonsTarget = true;
            }
        }
        else if ((m_settings.m_target == "Custom RA/Dec") && m_settings.m_jnow)
        {
            rdJnow.ra = Units::raToDecimal(m_settings.m_ra);
            rdJnow.dec = Units::decToDecimal(m_settings.m_dec);
            rdJnowValid = true;
            rdJ2000 = Astronomy::precess(rdJnow, Astronomy::julianDate(dt), Astronomy::jd_j2000());
        }
        else
        {
            rdJ2000.ra = Units::raToDecimal(m_settings.m_ra);
            rdJ2000.dec = Units::decToDecimal(m_settings.m_dec);
            rdJnowValid = true;
            rdJnow = Astronomy::precess(rdJ2000, Astronomy::jd_j2000(), Astronomy::julianDate(dt));
        }
        if (rdJnowValid)
        {
            aa = Astronomy::raDecToAzAlt(rdJnow, m_settings.m_latitude, m_settings.m_longitude, dt, false);
            Astronomy::equatorialToGalactic(rdJ2000.ra, rdJ2000.dec, l, b);
        }
    }

    if (rdJnowValid)
    {
        // Precess to J2000
        RADec rdJ2000 = Astronomy::precess(rdJnow, Astronomy::julianDate(dt), Astronomy::jd_j2000());

        // Send to Stellarium
        writeStellariumTarget(rdJ2000.ra, rdJ2000.dec);

        // Send to GUI
        if ((m_settings.m_target == "Sun") || (m_settings.m_target == "Moon") || (m_settings.m_target == "Custom Az/El")
            || m_settings.m_target.contains("SatelliteTracker") || m_settings.m_target.contains("SkyMap")
            || lbTarget || horizonsTarget || (m_settings.m_targetSource == "SPICE"))
        {
            if (getMessageQueueToGUI())
            {
                if (m_settings.m_jnow) {
                    getMessageQueueToGUI()->push(StarTrackerReport::MsgReportRADec::create(rdJnow.ra, rdJnow.dec, "target"));
                } else {
                    getMessageQueueToGUI()->push(StarTrackerReport::MsgReportRADec::create(rdJ2000.ra, rdJ2000.dec, "target"));
                }
            }
        }

        // Adjust for refraction
        if (m_settings.m_refraction == "Positional Astronomy Library")
        {
            aa.alt += Astronomy::refractionPAL(aa.alt, m_settings.m_pressure, m_settings.m_temperature, m_settings.m_humidity,
                                                m_settings.m_frequency, m_settings.m_latitude, m_settings.m_heightAboveSeaLevel,
                                                m_settings.m_temperatureLapseRate);
            if (aa.alt > 90.0) {
                aa.alt = 90.0f;
            }
        }
        else if (m_settings.m_refraction == "Saemundsson")
        {
            aa.alt += Astronomy::refractionSaemundsson(aa.alt, m_settings.m_pressure, m_settings.m_temperature);
            if (aa.alt > 90.0) {
                aa.alt = 90.0f;
            }
        }

        // Add user-adjustment
        aa.alt += m_settings.m_elevationOffset;
        aa.az += m_settings.m_azimuthOffset;

        // Send to GUI
        if (getMessageQueueToGUI())
        {
            // MsgReportRADec sent in updateRaDec()
            if (m_settings.m_target != "Custom Az/El") {
                getMessageQueueToGUI()->push(StarTrackerReport::MsgReportAzAl::create(aa.az, aa.alt));
            }
            if (!lbTarget) {
                getMessageQueueToGUI()->push(StarTrackerReport::MsgReportGalactic::create(l, b));
            }
        }

        // Send Az/El to Rotator Controllers
        // Unless we're receiving settings to display from a Radio Astronomy plugins
        if (!m_settings.m_link)
        {
            QList<ObjectPipe*> rotatorPipes;
            MainCore::instance()->getMessagePipes().getMessagePipes(m_starTracker, "target", rotatorPipes);

            for (const auto& pipe : rotatorPipes)
            {
                MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                SWGSDRangel::SWGTargetAzimuthElevation *swgTarget = new SWGSDRangel::SWGTargetAzimuthElevation();
                swgTarget->setName(new QString(m_settings.m_target));
                swgTarget->setAzimuth(aa.az);
                swgTarget->setElevation(aa.alt);
                messageQueue->push(MainCore::MsgTargetAzimuthElevation::create(m_starTracker, swgTarget));
            }
        }

        // Send Az/El, RA/Dec and Galactic b/l to Radio Astronomy plugins
        // Unless we're receiving settings to display from a Radio Astronomy plugins
        if (!m_settings.m_link)
        {
            QList<ObjectPipe*> starTrackerPipes;
            MainCore::instance()->getMessagePipes().getMessagePipes(m_starTracker, "startracker.target", starTrackerPipes);

            for (const auto& pipe : starTrackerPipes)
            {
                MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                SWGSDRangel::SWGStarTrackerTarget *swgTarget = new SWGSDRangel::SWGStarTrackerTarget();
                swgTarget->setName(new QString(m_settings.m_target));
                swgTarget->setAzimuth(aa.az);
                swgTarget->setElevation(aa.alt);
                swgTarget->setRa(rdJnow.ra);
                swgTarget->setDec(rdJnow.dec);
                swgTarget->setB(b);
                swgTarget->setL(l);
                swgTarget->setSolarFlux(m_solarFlux);
                swgTarget->setAirTemperature(m_settings.m_temperature);
                double temp;
                m_starTracker->calcSkyTemperature(m_settings.m_frequency, m_settings.m_beamwidth, rdJnow.ra, rdJnow.dec, temp);
                swgTarget->setSkyTemperature(temp);
                swgTarget->setHpbw(m_settings.m_beamwidth);
                // Calculate velocities
                double vRot = Astronomy::earthRotationVelocity(rdJnow, m_settings.m_latitude, m_settings.m_longitude, dt);
                swgTarget->setEarthRotationVelocity(vRot);
                double vOrbit = Astronomy::earthOrbitVelocityBCRS(rdJnow, dt);
                swgTarget->setEarthOrbitVelocityBcrs(vOrbit);
                double vLSRK = Astronomy::sunVelocityLSRK(rdJnow);
                swgTarget->setSunVelocityLsr(vLSRK);
                messageQueue->push(MainCore::MsgStarTrackerTarget::create(m_starTracker, swgTarget));
            }
        }

        // Send RA/Dec, position, beamwidth and date to Sky Map
        QList<ObjectPipe*> skyMapPipes;
        MainCore::instance()->getMessagePipes().getMessagePipes(m_starTracker, "skymap.target", skyMapPipes);
        for (const auto& pipe : skyMapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGSkyMapTarget *swgTarget = new SWGSDRangel::SWGSkyMapTarget();
            RADec rdJ2000 = Astronomy::precess(rdJnow, Astronomy::julianDate(dt), Astronomy::jd_j2000());
            swgTarget->setRa(rdJ2000.ra);
            swgTarget->setDec(rdJ2000.dec);
            swgTarget->setLatitude(m_settings.m_latitude);
            swgTarget->setLongitude(m_settings.m_longitude);
            swgTarget->setAltitude(m_settings.m_heightAboveSeaLevel);
            swgTarget->setDateTime(new QString(dt.toString(Qt::ISODateWithMs)));
            swgTarget->setHpbw(m_settings.m_beamwidth);
            messageQueue->push(MainCore::MsgSkyMapTarget::create(m_starTracker, swgTarget));
        }

        // Send to Map
        if (m_settings.m_drawSunOnMap || m_settings.m_drawMoonOnMap || m_settings.m_drawStarOnMap)
        {
            QList<ObjectPipe*> mapMessagePipes;
            MainCore::instance()->getMessagePipes().getMessagePipes(m_starTracker, "mapitems", mapMessagePipes);

            if (mapMessagePipes.size() > 0)
            {
                // Different between GMST(Lst at Greenwich) and RA
                double lst = Astronomy::localSiderealTime(dt, 0.0);
                double sunLongitude;
                double sunLatitude;

                if (m_settings.m_drawSunOnMap || m_settings.m_drawMoonOnMap)
                {
                    sunLongitude = Astronomy::lstAndRAToLongitude(lst, sunRD.ra);
                    sunLatitude = sunRD.dec;
                    sendToMap(mapMessagePipes, "Sun", "qrc:///startracker/startracker/sun-40.png", "Sun", sunLatitude, sunLongitude);
                }
                if (m_settings.m_drawMoonOnMap)
                {
                    double moonLongitude = Astronomy::lstAndRAToLongitude(lst, moonRD.ra);
                    double moonLatitude = moonRD.dec;
                    double moonRotation;
                    QString phase = moonPhase(sunLongitude, moonLongitude, m_settings.m_latitude, moonRotation);
                    sendToMap(mapMessagePipes, "Moon", QString("qrc:///startracker/startracker/moon-%1-32").arg(phase), "Moon",
                                    moonLatitude, moonLongitude, moonRotation);
                }
                if ((m_settings.m_drawStarOnMap) && (m_settings.m_target != "Sun") && (m_settings.m_target != "Moon"))
                {
                    double starLongitude = Astronomy::lstAndRAToLongitude(lst, rdJnow.ra);
                    double starLatitude = rdJnow.dec;
                    QString text = m_settings.m_target.startsWith("Custom") ? "Star" : m_settings.m_target;
                    sendToMap(mapMessagePipes, "Star", "qrc:///startracker/startracker/pulsar-32.png", text, starLatitude, starLongitude);
                }
            }
        }

        if (m_settings.m_chartSelect == StarTrackerSettings::CHART_SOLAR_SYSTEM) {
            calculateSolarSystemPositions(dt);
        }
        if (m_settings.m_chartSelect == StarTrackerSettings::CHART_JUPITER) {
            calculateJupiterParameters(dt);
        }

        // Calculate az/el of target over the current day for plotting on chart
        // Can't do this for satellite tracker, as we only have current az/el
        // Values from SkyMap map not be valid if object is moving (E.g. planets), as ra/dec is for one point in time

        // Start at midnight or noon
        QDateTime lt = dt.toLocalTime();
        if (m_settings.m_night)
        {
            if (lt.time() < QTime(12, 0)) {
                dt = QDateTime(lt.date().addDays(-1), QTime(12, 0));
            } else {
                dt = QDateTime(lt.date(), QTime(12, 0));
            }
        }
        else
        {
            dt = QDateTime(lt.date(), QTime(0, 0));
        }
        //dt.setTimeZone(QTimeZone::LocalTime());

        if (   (m_settings.m_target != m_chartTarget)
            || (m_settings.m_latitude != m_chartLatitude) || (m_settings.m_longitude != m_chartLongitude)
            || (dt != m_chartStartTime)
            || (m_settings.m_target.startsWith("Custom"))
            || (   (raDecTargets.contains(m_settings.m_target) || m_settings.m_target.contains("SkyMap"))  // When switching these targets, we get separate settings updates for RA and Dec, so need to redraw
                && ((m_settings.m_ra != m_chartRA) || (m_settings.m_dec != m_chartDec)) // We don't want to redraw when RA/Dec for Sun/Moon changes though, as that happens continuously
                )
            || (   lbTargets.contains(m_settings.m_target)
                && ((m_settings.m_l != m_chartL) || (m_settings.m_b != m_chartB))
               )
           )
        {
            m_chartTarget = m_settings.m_target;
            m_chartLatitude = m_settings.m_latitude;
            m_chartLongitude = m_settings.m_longitude;
            m_chartStartTime = dt;
            m_chartRA = m_settings.m_ra;
            m_chartDec = m_settings.m_dec;
            m_chartL = m_settings.m_l;
            m_chartB = m_settings.m_b;

            if (!m_settings.m_target.contains("SatelliteTracker"))
            {
                int timestep = 5*60; // 5 mins

                QList<double> azimuths;
                QList<double> elevations;
                QList<QDateTime> dateTimes;

                for (int step = 0; step <= 24*60*60/timestep; step++)
                {
                    AzAlt aa;
                    RADec rdJnow, rdJ2000;

                    // Calculate elevation of desired object
                    if ((m_settings.m_target == "Sun") && (m_settings.m_targetSource == "SDRangel"))
                    {
                        Astronomy::sunPosition(aa, rdJnow, m_settings.m_latitude, m_settings.m_longitude, dt);
                    }
                    else if ((m_settings.m_target == "Moon") && (m_settings.m_targetSource == "SDRangel"))
                    {
                        Astronomy::moonPosition(aa, rdJnow, m_settings.m_latitude, m_settings.m_longitude, dt);
                    }
                    else if (m_settings.m_target == "Custom Az/El")
                    {
                        aa.alt = m_settings.m_el;
                        aa.az = m_settings.m_az;
                    }
                    else if (lbTargets.contains(m_settings.m_target))
                    {
                        Astronomy::galacticToEquatorial(m_settings.m_l, m_settings.m_b, rdJ2000.ra, rdJ2000.dec);
                        aa = Astronomy::raDecToAzAlt(rdJ2000, m_settings.m_latitude, m_settings.m_longitude, dt, true);
                    }
                    else if (m_settings.m_targetSource == "SPICE")
                    {
                        double et;

                        if (dateTimeToET(dt, et)) {
                            getAzElFromSPICE(m_settings.m_target, et, m_settings.m_latitude, m_settings.m_longitude, 0.0, aa.az, aa.alt);
                        }
                    }
                    else
                    {
                        if (m_settings.m_target.contains("SkyMap"))
                        {
                            getRAFromSkyMap(rdJ2000);
                        }
                        else if ((m_settings.m_targetSource == "Horizons") && !raDecTargets.contains(m_settings.m_target))
                        {
                            getRAFromHorizons(dt, rdJ2000);
                        }
                        else if ((m_settings.m_target == "Custom RA/Dec") && m_settings.m_jnow)
                        {
                            rdJnow.ra = Units::raToDecimal(m_settings.m_ra);
                            rdJnow.dec = Units::decToDecimal(m_settings.m_dec);
                            rdJ2000 = Astronomy::precess(rdJnow, Astronomy::julianDate(dt), Astronomy::jd_j2000());
                        }
                        else
                        {
                            rdJ2000.ra = Units::raToDecimal(m_settings.m_ra);
                            rdJ2000.dec = Units::decToDecimal(m_settings.m_dec);
                        }
                        aa = Astronomy::raDecToAzAlt(rdJ2000, m_settings.m_latitude, m_settings.m_longitude, dt, !m_settings.m_jnow);
                    }

                    azimuths.append(aa.az);
                    elevations.append(aa.alt);
                    dateTimes.append(dt);

                    dt = dt.addSecs(timestep); // addSecs accounts for daylight savings jumps
                }

                getMessageQueueToGUI()->push(StarTrackerReport::MsgReportAzElVsTime::create(m_settings.m_target, azimuths, elevations, dateTimes));
            }
        }
    }

    spiceUnlock();

    PROFILER_STOP("StarTrackerWorker");
}

void StarTrackerWorker::calculateSolarSystemPositions(const QDateTime& dateTime)
{
    QStringList bodyNames;
    QList<QVector3D> bodyPositions;
    QList<QList<QPointF>> orbits;

    static const QStringList planets = {
        "MERCURY", "VENUS", "EARTH", "MARS", "JUPITER", "SATURN", "URANUS", "NEPTUNE",
        "MERCURY BARYCENTER", "VENUS BARYCENTER", "EARTH BARYCENTER", "MARS BARYCENTER", "JUPITER BARYCENTER", "SATURN BARYCENTER", "URANUS BARYCENTER", "NEPTUNE BARYCENTER"
    };

    double et;
    if (!dateTimeToET(dateTime, et)) {
        return;
    }

    for (const auto& body : m_settings.m_solarSystemBodies)
    {
        QVector3D positionKm;

        if (spicePosition(body, et, positionKm))
        {
            QVector3D position(positionKm[0], positionKm[1], positionKm[2]);

            bodyNames.append(body);
            bodyPositions.append(position);

            QList<QPointF> orbit;
            if (planets.contains(body)) {
                spiceOrbit(body, et, orbit);
            }
            orbits.append(orbit);
        }
    }

    if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(StarTrackerReport::MsgReportSolarSystemPositions::create(dateTime, bodyNames, bodyPositions, orbits));
    }
}

void StarTrackerWorker::calculateJupiterParameters(const QDateTime& dateTime)
{
    double cml;
    double ioPhase;
    double ganymedePhase;
    double et;
    double az, el;
    bool jupiterVisibleNow = false;
    QDateTime dt;
    bool done;

    QString moon = m_settings.m_chartSubSelect == 0 ? "IO" : "GANYMEDE";
    double moonPhase;

    QList<StarTrackerReport::JupiterData> jupiterData;
    QList<StarTrackerReport::JupiterMoonData> moonData;

    PROFILER_START();

    if (dateTimeToET(dateTime, et))
    {
        // Calculate Alt/El of Jupiter
        if (getAzElFromSPICE("JUPITER", et, m_settings.m_latitude, m_settings.m_longitude, 0.0, az, el))
        {
            // Calculate Jupiter CML and phase of Moons
            if (spiceJupiterMoonPhase("IO", et, m_settings.m_latitude, m_settings.m_longitude, 0.0, cml, ioPhase))
            {
                if (spiceJupiterMoonPhase("GANYMEDE", et, m_settings.m_latitude, m_settings.m_longitude, 0.0, cml, ganymedePhase))
                {
                    if (getMessageQueueToGUI()) {
                        getMessageQueueToGUI()->push(StarTrackerReport::MsgReportJupiter::create(dateTime, az, el, cml, ioPhase, ganymedePhase));
                    }
                }
            }
            jupiterVisibleNow = el > 0.0;
        }
    }

    if (jupiterVisibleNow)
    {
        // Step backwards in time
        dt = dateTime;
        dt.setTime(QTime(dateTime.time().hour(), 0));
        done = false;
        while (!done)
        {
            if (dateTimeToET(dt, et))
            {
                if (getAzElFromSPICE("JUPITER", et, m_settings.m_latitude, m_settings.m_longitude, 0.0, az, el))
                {
                    if (el > 0.0)
                    {
                        if (spiceJupiterMoonPhase(moon, et, m_settings.m_latitude, m_settings.m_longitude, 0.0, cml, moonPhase))
                        {
                            StarTrackerReport::JupiterData jd = {dt, az, el};
                            StarTrackerReport::JupiterMoonData md = {cml, moonPhase};

                            jupiterData.push_front(jd);
                            moonData.push_front(md);
                        }
                    }
                    else
                    {
                        done = true;
                    }
                }
            }
            dt = dt.addSecs(-60*60);
        }

        // Step forwards in time
        dt = dateTime;
        dt.setTime(QTime(dateTime.time().hour(), 0));
        done = false;
        while (!done)
        {
            dt = dt.addSecs(60*60);
            if (dateTimeToET(dt, et))
            {
                if (getAzElFromSPICE("JUPITER", et, m_settings.m_latitude, m_settings.m_longitude, 0.0, az, el))
                {
                    if (el > 0.0)
                    {
                        if (spiceJupiterMoonPhase(moon, et, m_settings.m_latitude, m_settings.m_longitude, 0.0, cml, moonPhase))
                        {
                            StarTrackerReport::JupiterData jd = {dt, az, el};
                            StarTrackerReport::JupiterMoonData id = {cml, moonPhase};

                            jupiterData.push_back(jd);
                            moonData.push_back(id);
                        }
                    }
                    else
                    {
                        done = true;
                    }
                }
            }
        }
    }
    else
    {
        // Step forwards in time until Jupiter is visible
        dt = dateTime;
        dt.setTime(QTime(dateTime.time().hour(), 0));
        done = false;
        while (!done)
        {
            dt = dt.addSecs(60*60);
            if (dateTimeToET(dt, et))
            {
                if (getAzElFromSPICE("JUPITER", et, m_settings.m_latitude, m_settings.m_longitude, 0.0, az, el))
                {
                    if (el > 0.0) {
                        done = true;
                    }
                }
            }
        }

        // Continue stepping forward until no longer visible
        done = false;
        while (!done)
        {
            if (dateTimeToET(dt, et))
            {
                if (getAzElFromSPICE("JUPITER", et, m_settings.m_latitude, m_settings.m_longitude, 0.0, az, el))
                {
                    if (el > 0.0)
                    {
                        if (spiceJupiterMoonPhase(moon, et, m_settings.m_latitude, m_settings.m_longitude, 0.0, cml, moonPhase))
                        {
                            StarTrackerReport::JupiterData jd = {dt, az, el};
                            StarTrackerReport::JupiterMoonData id = {cml, moonPhase};

                            jupiterData.push_back(jd);
                            moonData.push_back(id);
                        }
                    }
                    else
                    {
                        done = true;
                    }
                }
            }
            dt = dt.addSecs(60*60);
        }

    }

    if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(StarTrackerReport::MsgReportJupiterData::create(jupiterData, moonData));
    }

    PROFILER_STOP("calculateJupiterParameters");
}

void StarTrackerWorker::horizonsEphemeridesUpdated(const QString &target, const QList<JPLHorizons::Ephemeris>& ephemerides)
{
    m_ephemeridesTarget = target;
    m_horizonsEphemerides = ephemerides;
}
