///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <algorithm>
#include <cmath>

#include <QDebug>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDir>
#include <QProcess>
#include <QRandomGenerator>
#include <QGeoCoordinate>

#include "SWGMapItem.h"
#include "SWGMapAnimation.h"
#include "SWGMapAircraftState.h"

#include "channel/channelwebapiutils.h"
#include "feature/featurewebapiutils.h"
#include "maincore.h"
#include "util/units.h"
#include "util/azel.h"
#include "util/aircraftreport.h"
#include "util/aircraftenginesound.h"
#include "util/aircraft3dmodels.h"
#include "util/morse.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include "util/db.h"
#include "util/profiler.h"
#include "util/airlines.h"
#include "adsb.h"
#include "adsbdemod.h"
#include "adsbdemodreport.h"
#include "adsbdemodtracker.h"

const char ADSBDemodTracker::m_idMap[] = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ##### ############-##0123456789######";

const QString ADSBDemodTracker::m_categorySetA[] = {
    QStringLiteral("None"),
    QStringLiteral("Light"),
    QStringLiteral("Small"),
    QStringLiteral("Large"),
    QStringLiteral("High vortex"),
    QStringLiteral("Heavy"),
    QStringLiteral("High performance"),
    QStringLiteral("Rotorcraft")
};

const QString ADSBDemodTracker::m_categorySetB[] = {
    QStringLiteral("None"),
    QStringLiteral("Glider/sailplane"),
    QStringLiteral("Lighter-than-air"),
    QStringLiteral("Parachutist"),
    QStringLiteral("Ultralight"),
    QStringLiteral("Reserved"),
    QStringLiteral("UAV"),
    QStringLiteral("Space vehicle")
};

const QString ADSBDemodTracker::m_categorySetC[] = {
    QStringLiteral("None"),
    QStringLiteral("Emergency vehicle"),
    QStringLiteral("Service vehicle"),
    QStringLiteral("Ground obstruction"),
    QStringLiteral("Cluster obstacle"),
    QStringLiteral("Line obstacle"),
    QStringLiteral("Reserved"),
    QStringLiteral("Reserved")
};

const QString ADSBDemodTracker::m_emergencyStatus[] = {
    QStringLiteral("No emergency"),
    QStringLiteral("General emergency"),
    QStringLiteral("Lifeguard/Medical"),
    QStringLiteral("Minimum fuel"),
    QStringLiteral("No communications"),
    QStringLiteral("Unlawful interference"),
    QStringLiteral("Downed aircraft"),
    QStringLiteral("Reserved")
};

const QString ADSBDemodTracker::m_flightStatuses[] = {
    QStringLiteral("Airborne"),
    QStringLiteral("On-ground"),
    QStringLiteral("Alert, airborne"),
    QStringLiteral("Alert, on-ground"),
    QStringLiteral("Alert, SPI"),
    QStringLiteral("SPI"),
    QStringLiteral("Reserved"),
    QStringLiteral("Not assigned")
};

const QString ADSBDemodTracker::m_hazardSeverity[] = {
    "NIL", "Light", "Moderate", "Severe"
};

const QString ADSBDemodTracker::m_fomSources[] = {
    "Invalid", "INS", "GNSS", "DME/DME", "DME/VOR", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved"
};

const QString ADSBDemodTracker::m_nacvStrings[] = {
    ">= 10 m/s", "< 10 m/s", "< 3 m/s", "< 1 m/s", "< 0.3 m/s", "Reserved", "Reserved", "Reserved"
};

// Longitude zone (returns value in range [1,59]
static int cprNL(double lat)
{
    if (lat == 0.0)
    {
        return 59;
    }
    else if ((lat == 87.0) || (lat == -87.0))
    {
        return 2;
    }
    else if ((lat > 87.0) || (lat < -87.0))
    {
        return 1;
    }
    else
    {
        double nz = 15.0;
        double n = 1 - std::cos(M_PI / (2.0 * nz));
        double d = std::cos(std::fabs(lat) * M_PI/180.0);
        return std::floor((M_PI * 2.0) / std::acos(1.0 - (n/(d*d))));
    }
}

static int cprN(double lat, int odd)
{
    int nl = cprNL(lat) - odd;
    if (nl > 1) {
        return nl;
    } else {
        return 1;
    }
}

// Can't use std::fmod, as that works differently for negative numbers (See C.2.6.2)
static Real modulus(double x, double y)
{
    return x - y * std::floor(x/y);
}

static bool isSpeedInconsistent(float speed1, float speed2)
{
    return std::abs(speed1 - speed2) > 50.0f;
}

static bool isHeadingInconsistent(float heading1, float heading2)
{
    float difference = std::fmod(std::abs(heading1 - heading2), 360.0f);
    return std::min(difference, 360.0f - difference) > 45.0f;
}

static bool isVerticalRateInconsistent(int verticalRate1, int verticalRate2)
{
    return abs(verticalRate1 - verticalRate2) > 1500;
}

static bool isAltitudeInconsistent(int altitude1, int altitude2)
{
    return abs(altitude1 - altitude2) > 1500; // 30s at climb rate of 3kft/m
}

// Difference in longitude in degrees, accounting for wrap around +-180
static double longitudeDiff(double longitude1, double longitude2)
{
    double difference = std::abs(longitude1 - longitude2);
    return std::min(difference, 360.0 - difference);
}

// Difference in longitude in degrees that corresponds to the same distance as maxLatDiff degrees of latitude
// A degree of longitude covers cos(latitude) times the distance of a degree of latitude, so the same
// distance is a larger difference in longitude. Limited, to avoid divide by zero at the poles
static double maxLongitudeDiff(double latitude, double maxLatDiff)
{
    return maxLatDiff / std::max(cos(Units::degreesToRadians(latitude)), 0.01);
}

static bool isPositionInconsistent(double latitude1, double longitude1, double latitude2, double longitude2)
{
    return (std::abs(latitude1 - latitude2) > 0.2) || (longitudeDiff(longitude1, longitude2) > 0.3); // 1 deg lat is ~70 miles. 500mph is ~8miles per minute
}

// A previous emitter position is only an unambiguous local-CPR reference while an
// aircraft travelling at no more than 600 knots could not have crossed half a CPR
// zone (180 NM airborne or 45 NM on the surface) since that position.
static bool hasUsableCprReference(const ADSBAircraft *aircraft, bool onSurface, const QDateTime& dateTime)
{
    if (!aircraft->m_globalPosition || !aircraft->m_positionValid || !aircraft->m_positionDateTime.isValid()) {
        return false;
    }

    const qint64 age = aircraft->m_positionDateTime.msecsTo(dateTime);
    const qint64 maxAge = onSurface ? 270000 : 1080000;
    return (age >= 0) && (age <= maxAge);
}











// Initialise text to speech engine
// This takes 10 seconds on some versions of Linux, so only do it, if user actually
// has speech notifications configured

// ADS-B and Mode-S selected altitudes have slightly different precision, so values can jump around a bit
// And we end up with altitudes such as 38008 rather than 38000
// To remove this, we round to nearest 50 feet
int ADSBDemodTracker::roundTo50Feet(int alt)
{
    return ((alt + 25) / 50) * 50;
}

static void calcRadialEndPoint(float startLatitude, float startLongitude, float distance, float bearing, float &endLatitude, float &endLongitude)
{
    double startLatRad = startLatitude*M_PI/180.0;
    double startLongRad = startLongitude*M_PI/180.0;
    double theta = bearing*M_PI/180.0;
    double earthRadius = 6378137.0; // At equator
    double delta = distance/earthRadius;
    double endLatRad = std::asin(sin(startLatRad)*cos(delta) + cos(startLatRad)*sin(delta)*cos(theta));
    double endLongRad = startLongRad + std::atan2(sin(theta)*sin(delta)*cos(startLatRad), cos(delta) - sin(startLatRad)*sin(endLatRad));
    endLatitude = endLatRad*180.0/M_PI;
    endLongitude = endLongRad*180.0/M_PI;
}

void ADSBDemodTracker::clearFromMap(const QString& name)
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_adsbDemod, "mapitems", mapPipes);

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setImage(new QString(""));
        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_adsbDemod, swgMapItem);
        messageQueue->push(msg);
    }
}

void ADSBDemodTracker::queueMapUpdate(ADSBAircraft *aircraft, QList<SWGSDRangel::SWGMapAnimation *> *animations)
{
    auto existing = m_pendingMapUpdates.find(aircraft);

    if (existing == m_pendingMapUpdates.end())
    {
        m_pendingMapUpdates.insert(aircraft, animations);
    }
    else if (animations)
    {
        // Animations are one-offs, so they have to accumulate rather than be replaced
        if (*existing)
        {
            (*existing)->append(*animations);
            delete animations;
        }
        else
        {
            *existing = animations;
        }
    }
}

void ADSBDemodTracker::discardMapUpdate(ADSBAircraft *aircraft)
{
    auto pending = m_pendingMapUpdates.find(aircraft);

    if (pending != m_pendingMapUpdates.end())
    {
        if (*pending)
        {
            qDeleteAll(**pending);
            delete *pending;
        }
        m_pendingMapUpdates.erase(pending);
    }
}

// Every map item we send is owned and deleted by whoever receives it, and there can be
// more than one, so each needs animations of its own rather than a shared list
QList<SWGSDRangel::SWGMapAnimation *> *ADSBDemodTracker::copyAnimations(QList<SWGSDRangel::SWGMapAnimation *> *animations)
{
    if (!animations) {
        return nullptr;
    }

    QList<SWGSDRangel::SWGMapAnimation *> *copy = new QList<SWGSDRangel::SWGMapAnimation *>();

    for (auto *animation : *animations)
    {
        SWGSDRangel::SWGMapAnimation *item = new SWGSDRangel::SWGMapAnimation();

        if (animation->getName()) {
            item->setName(new QString(*animation->getName()));
        }
        if (animation->getStartDateTime()) {
            item->setStartDateTime(new QString(*animation->getStartDateTime()));
        }
        item->setReverse(animation->getReverse());
        item->setLoop(animation->getLoop());
        item->setStartOffset(animation->getStartOffset());
        item->setMultiplier(animation->getMultiplier());
        item->setDuration(animation->getDuration());
        item->setStop(animation->getStop());
        copy->append(item);
    }

    return copy;
}

void ADSBDemodTracker::sendToMap(ADSBAircraft *aircraft,
                                QList<SWGSDRangel::SWGMapAnimation *> *animations,
                                const QDateTime& aircraftStateDateTime)
{
    // When an ADSBAircraft feature is listening, it drives the Map: it gets reports
    // carrying the fully-built map item (so the 3D model and PFD aircraft state
    // reach the Map losslessly), combined with sightings from other protocols.
    // Without one, send to the Map directly as before.
    QList<ObjectPipe*> reportPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_adsbDemod, "aircraftreport", reportPipes);

    // Send to Map feature
    QList<ObjectPipe*> mapPipes;
    if (reportPipes.size() == 0) {
        MainCore::instance()->getMessagePipes().getMessagePipes(m_adsbDemod, "mapitems", mapPipes);
    }

    if ((mapPipes.size() > 0) || (reportPipes.size() > 0))
    {
        // Barometric altitude reported by ADS-B is relative to pressure of 1013.25
        // Correct using QNH setting to get altitude relative to sea level
        int altitudeFt = aircraft->m_altitude;

        if (aircraft->m_onSurfaceValid && aircraft->m_onSurface) {
            altitudeFt = 0; // Ignore Mode S altitude
        } else if (!aircraft->m_altitudeGNSS) {
            altitudeFt += (m_settings.m_qnh - 1013.25) * 30; // Adjust by 30ft per hPa
        }

        float altitudeM = Units::feetToMetres(altitudeFt);

        // On take-off, m_onSurface may go false before aircraft actually takes off
        // and due to inaccuracy in baro / QHN, it can appear to move down the runway in the air
        // So keep altitude at 0 until it starts to go above first airbourne value
        if (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude <= aircraft->m_runwayAltitude)) {
            altitudeM = 0;
        }

        const bool replay = m_loadingData && aircraftStateDateTime.isValid();
        // Only consume the boundary marker when an Aircraft feature will receive it.
        // If one is connected part way through a replay, its first report must still
        // start a track independent of any state it already holds for the airframe.
        const bool replayStart = replay
            && !m_replayAircraftReported.contains(aircraft->m_icao);
        if (replayStart) {
            m_replayAircraftReported.insert(aircraft->m_icao);
        }

        // The in-channel map starts a fresh trail for every replay and whenever a newly
        // unambiguous global CPR position proves the preceding local trail belonged to
        // another CPR zone. The external Map owns an independent automatic track, so it
        // must receive the same boundary. Removing and immediately re-adding the item is
        // the existing Map protocol for discarding all state associated with an object.
        const bool resetDirectMapTrack = reportPipes.isEmpty()
            && (replayStart || aircraft->m_mapTrailResetPending);
        if (resetDirectMapTrack)
        {
            qWarning() << "ADSB_MAP_TRACK_RESET" << aircraft->m_icaoHex
                       << (replayStart ? "replay-start" : "CPR-reference-change")
                       << aircraft->m_positionDateTime.toString(Qt::ISODateWithMs)
                       << aircraft->m_latitude << aircraft->m_longitude;
            clearFromMap(aircraft->m_icaoHex);
            aircraft->m_mapTrailResetPending = false;
        }

        for (const auto& pipe : (reportPipes.size() > 0 ? reportPipes : mapPipes))
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
            swgMapItem->setName(new QString(aircraft->m_icaoHex));
            swgMapItem->setLatitude(aircraft->m_latitude);
            swgMapItem->setLongitude(aircraft->m_longitude);
            swgMapItem->setAltitude(altitudeM);
            swgMapItem->setPositionDateTime(new QString(aircraft->m_positionDateTime.toString(Qt::ISODateWithMs)));
            swgMapItem->setAltitudeDateTime(new QString(aircraft->m_altitudeDateTime.toString(Qt::ISODateWithMs)));
            swgMapItem->setFixedPosition(false);
            swgMapItem->setAvailableUntil(new QString(aircraft->m_positionDateTime.addSecs(61).toString(Qt::ISODateWithMs))); // Changed from 60 to 61 due to https://github.com/CesiumGS/cesium/issues/12426
            QString image;
            if (m_settings.m_useLiveryIcons) {
                image = aircraft->getLiveryImage();
            }
            if (image.isEmpty()) {
                image = aircraft->getImage();
            }
            swgMapItem->setImage(new QString(QString("qrc:///map/%1").arg(image)));
            // Use track, in preference to heading, as we typically receive far fewer heading frames
            if (aircraft->m_trackValid) {
                swgMapItem->setImageRotation(aircraft->m_track);
            } else {
                swgMapItem->setImageRotation(aircraft->m_heading);
            }
            // Links only on the pass-through: this item is going to an Aircraft feature,
            // which is what makes them lead anywhere. Sent straight to the Map - no
            // Aircraft feature open - they would be dead, and the demodulator's own map
            // has no link handler in its delegate either.
            swgMapItem->setText(new QString(
                aircraft->getText(&m_settings, true, reportPipes.size() > 0)));

            if (!aircraft->m_aircraft3DModel.isEmpty()) {
                swgMapItem->setModel(new QString(aircraft->m_aircraft3DModel));
            } else {
                swgMapItem->setModel(new QString(aircraft->m_aircraftCat3DModel));
            }

            QDateTime labelDateTime;
            swgMapItem->setLabel(new QString(aircraft->getLabel(&m_settings, labelDateTime)));
            if (labelDateTime.isValid()) {
                swgMapItem->setLabelDateTime(new QString(labelDateTime.toString(Qt::ISODateWithMs)));
            }

            if (aircraft->m_trackValid)
            {
                swgMapItem->setOrientation(1);
                swgMapItem->setHeading(aircraft->m_track);
                swgMapItem->setPitch(aircraft->m_pitchEst);
                if (aircraft->m_rollValid) {
                    swgMapItem->setRoll(aircraft->m_roll);
                } else {
                    swgMapItem->setRoll(aircraft->m_rollEst);
                }
                swgMapItem->setOrientationDateTime(new QString(aircraft->m_positionDateTime.toString(Qt::ISODateWithMs)));
            }
            else
            {
                // Orient aircraft based on velocity calculated from position
                swgMapItem->setOrientation(0);
            }

            swgMapItem->setModelAltitudeOffset(aircraft->m_modelAltitudeOffset);
            swgMapItem->setLabelAltitudeOffset(aircraft->m_labelAltitudeOffset);
            swgMapItem->setAltitudeReference(3); // CLIP_TO_GROUND so aircraft don't go under runway
            swgMapItem->setAnimations(copyAnimations(animations));

            // A null track asks the Map to build one from successive positions. Leaving
            // the generated object's default empty list here instead says that the
            // sender owns an empty track, so no flight path is ever accumulated.
            swgMapItem->setTrack(nullptr);

            // Engine sound, looped by the 3D map, spatially located at the aircraft
            const QString engineSound = AircraftEngineSound::engineSound(
                aircraft->m_aircraftInfo ? aircraft->m_aircraftInfo->m_type : QString(),
                aircraft->m_emitterCategory,
                aircraft->m_aircraftInfo ? aircraft->m_aircraftInfo->m_model : QString());
            if (!engineSound.isEmpty()) {
                swgMapItem->setLoopSound(new QString(engineSound));
            }

            SWGSDRangel::SWGMapAircraftState *aircraftState = new SWGSDRangel::SWGMapAircraftState();
            if (aircraftStateDateTime.isValid()) {
                // Present only for unthrottled log replay. The Map uses this both as the
                // CZML sample time and as the instruction not to coalesce this state with
                // the next frame before it reaches the 3D display.
                aircraftState->setDateTime(new QString(aircraftStateDateTime.toString(Qt::ISODateWithMs)));
            }
            aircraftState->setIcao(new QString(QString::number(aircraft->m_icao, 16)));
            if (!aircraft->m_callsign.isEmpty()) {
                aircraftState->setCallsign(new QString(aircraft->m_callsign));
            }
            if (aircraft->m_aircraftInfo) {
                aircraftState->setAircraftType(new QString(aircraft->m_aircraftInfo->m_type));
            }
            if (aircraft->m_onSurfaceValid) {
                aircraftState->setOnSurface(aircraft->m_onSurface);
            } else {
                aircraftState->setOnSurface(-1);
            }
            if (aircraft->m_indicatedAirspeedValid)
            {
                aircraftState->setAirspeed(aircraft->m_indicatedAirspeed);
                aircraftState->setAirspeedDateTime(new QString(aircraft->m_indicatedAirspeedDateTime.toString(Qt::ISODateWithMs)));
            }
            else
            {
                aircraftState->setAirspeed(std::numeric_limits<float>::quiet_NaN());
            }
            if (aircraft->m_trueAirspeedValid) {
                aircraftState->setTrueAirspeed(aircraft->m_trueAirspeed);
            } else {
                aircraftState->setTrueAirspeed(std::numeric_limits<float>::quiet_NaN());
            }
            if (aircraft->m_groundspeedValid) {
                aircraftState->setGroundspeed(aircraft->m_groundspeed);
            } else {
                aircraftState->setGroundspeed(std::numeric_limits<float>::quiet_NaN());
            }
            if (aircraft->m_machValid) {
                aircraftState->setMach(aircraft->m_mach);
            } else {
                aircraftState->setMach(std::numeric_limits<float>::quiet_NaN());
            }
            if (aircraft->m_pressureAltitudeValid)
            {
                aircraftState->setAltitude(aircraft->m_pressureAltitude);
                aircraftState->setAltitudeDateTime(new QString(aircraft->m_altitudeDateTime.toString(Qt::ISODateWithMs)));
            }
            else
            {
                aircraftState->setAltitude(std::numeric_limits<float>::quiet_NaN());
            }
            if (aircraft->m_baroValid)
            {
                // What the crew has set in the altimeter, from the target state message
                aircraftState->setQnh(aircraft->m_baro);
            }
            else if (aircraft->m_pressureAltitudeValid)
            {
                // Not every aircraft reports it. The crew's setting would be standard
                // above the transition altitude and the local QNH below it, so that
                // stands in - without it the PFD shows no setting at all and reads a
                // flight level as though it were an altitude on QNH
                aircraftState->setQnh(aircraft->m_pressureAltitude >= m_settings.m_transitionAlt
                                      ? 1013.25f : m_settings.m_qnh);
            }
            else
            {
                aircraftState->setQnh(std::numeric_limits<float>::quiet_NaN());
            }
            if (aircraft->m_verticalRateValid) {
                aircraftState->setVerticalSpeed(aircraft->m_verticalRate);
            } else {
                aircraftState->setVerticalSpeed(std::numeric_limits<float>::quiet_NaN());
            }
            if (aircraft->m_headingValid) {
                aircraftState->setHeading(aircraft->m_heading);
            } else {
                aircraftState->setHeading(std::numeric_limits<float>::quiet_NaN());
            }
            if (aircraft->m_trackValid) {
                aircraftState->setTrack(aircraft->m_track);
            } else {
                aircraftState->setTrack(std::numeric_limits<float>::quiet_NaN());
            }
            //aircraftState->setSelectedAirspeed(aircraft->m_selSpeed); // Selected speed not available in ADS-B / Mode-S
            if (aircraft->m_selAltitudeValid) {
                aircraftState->setSelectedAltitude(roundTo50Feet(aircraft->m_selAltitude));
            } else {
                aircraftState->setSelectedAltitude(std::numeric_limits<float>::quiet_NaN());
            }
            if (aircraft->m_selHeadingValid) {
                aircraftState->setSelectedHeading(aircraft->m_selHeading);
            } else {
                aircraftState->setSelectedHeading(std::numeric_limits<float>::quiet_NaN());
            }
            if (aircraft->m_autopilotValid) {
                aircraftState->setAutopilot((int) aircraft->m_autopilot);
            } else {
                aircraftState->setAutopilot(-1);
            }
            if (aircraft->m_vnavModeValid && aircraft->m_vnavMode) {
                aircraftState->setVerticalMode(1);
            } else if (aircraft->m_altHoldModeValid && aircraft->m_altHoldMode) {
                aircraftState->setVerticalMode(2);
            } else if (aircraft->m_approachModeValid && aircraft->m_approachMode) {
                aircraftState->setVerticalMode(3);
            } else {
                aircraftState->setVerticalMode(0);
            }
            if (aircraft->m_lnavModeValid && aircraft->m_lnavMode) {
                aircraftState->setLateralMode(1);
            } else if (aircraft->m_approachModeValid && aircraft->m_approachMode) {
                aircraftState->setLateralMode(2);
            } else {
                aircraftState->setLateralMode(0);
            }

            // When RA Only, m_tcasOperational appears to be false
            QString acasCapability = aircraft->cellText(ADSB_COL_ACAS);
            if (acasCapability == "") {
                aircraftState->setTcasMode(0); // Unknown
            } else if (acasCapability == "No ACAS") {
                aircraftState->setTcasMode(1); // Off
            } else if (acasCapability == "No RA") {
                aircraftState->setTcasMode(2); // TA Only
            } else {
                aircraftState->setTcasMode(3); // TA/RA
            }

            // Active resolution advisory as a bitfield, 0 when there is none. The
            // clear-of-conflict bit is sent (briefly - see tick()) so the page can
            // announce it rather than just falling silent
            int tcasRA = 0;
            if (aircraft->m_raActive || aircraft->m_raTerminated)
            {
                tcasRA |= aircraft->m_raActive ? 1 : 0;
                tcasRA |= aircraft->m_raCorrective ? 2 : 0;
                tcasRA |= aircraft->m_raDownward ? 4 : 0;
                tcasRA |= aircraft->m_raPositive ? 8 : 0;
                tcasRA |= aircraft->m_raTerminated ? 16 : 0;
                tcasRA |= aircraft->m_raMultipleThreats ? 32 : 0;
                tcasRA |= aircraft->m_raOppositeSense ? 64 : 0;
            }
            aircraftState->setTcasRa(tcasRA);
            if (aircraft->m_raActive && aircraft->m_raThreatIcao) {
                aircraftState->setTcasThreatIcao(new QString(QString::number(aircraft->m_raThreatIcao, 16)));
            }
            // A squawk of 0 is never assigned, so it doubles as "not yet decoded"
            aircraftState->setSquawk(aircraft->m_squawk > 0 ? aircraft->m_squawk : -1);

            // Windspeed/direction are rarely available, so use calculated headwind as backup
            QVariant windSpeed = aircraft->cell(ADSB_COL_WIND_SPEED);
            QVariant windDirection = aircraft->cell(ADSB_COL_WIND_DIR);
            if (!windSpeed.isNull() && !windDirection.isNull())
            {
                aircraftState->setWindSpeed(windSpeed.toFloat());
                aircraftState->setWindDirection(windDirection.toFloat());
            }
            else
            {
                QVariant headwind = aircraft->cell(ADSB_COL_HEADWIND);
                if (!headwind.isNull() && aircraft->m_headingValid)
                {
                    aircraftState->setWindSpeed(headwind.toFloat());
                    aircraftState->setWindDirection(aircraft->m_heading);
                }
                else
                {
                    aircraftState->setWindSpeed(std::numeric_limits<float>::quiet_NaN());
                    aircraftState->setWindDirection(std::numeric_limits<float>::quiet_NaN());
                }
            }

            // Static air temperature rarely available, so use estimated as backup
            QVariant staticAirTemp = aircraft->cell(ADSB_COL_STATIC_AIR_TEMP);
            if (!staticAirTemp.isNull())
            {
                aircraftState->setStaticAirTemperature(staticAirTemp.toFloat());
            }
            else
            {
                QVariant estAirTemp = aircraft->cell(ADSB_COL_EST_AIR_TEMP);
                if (!estAirTemp.isNull()) {
                    aircraftState->setStaticAirTemperature(estAirTemp.toFloat());
                } else {
                    aircraftState->setStaticAirTemperature(std::numeric_limits<float>::quiet_NaN());
                }
            }

            swgMapItem->setAircraftState(aircraftState);

            if (reportPipes.size() > 0)
            {
                AircraftReport report;
                report.m_protocol = AircraftReport::ADSB;
                report.m_frequency = m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset;
                report.m_deviceSetIndex = m_adsbDemod->getDeviceSetIndex();
                report.m_channelIndex = m_adsbDemod->getIndexInDeviceSet();
                report.m_icao = (quint32) aircraft->m_icao;
                if (aircraft->m_aircraftInfo) {
                    report.m_registration = aircraft->m_aircraftInfo->m_registration;
                }
                report.m_flight = aircraft->m_callsign;
                report.m_positionValid = true;
                report.m_latitude = aircraft->m_latitude;
                report.m_longitude = aircraft->m_longitude;
                if (aircraft->m_altitudeValid)
                {
                    report.m_altitudeValid = true;
                    report.m_altitudeFt = altitudeFt;
                }
                // Track preferred over heading, as for the icon rotation above
                if (aircraft->m_trackValid)
                {
                    report.m_headingValid = true;
                    report.m_heading = aircraft->m_track;
                }
                else if (aircraft->m_headingValid)
                {
                    report.m_headingValid = true;
                    report.m_heading = aircraft->m_heading;
                }
                if (aircraft->m_groundspeedValid)
                {
                    report.m_speedValid = true;
                    report.m_speedKts = aircraft->m_groundspeed;
                }
                report.m_positionDateTime = aircraft->m_positionDateTime;
                report.m_altitudeDateTime = aircraft->m_altitudeDateTime;
                report.m_received = QDateTime::currentDateTime();
                report.m_replay = replay;
                report.m_replayStart = replayStart;
                report.m_mapItem = swgMapItem;  // Ownership transfers with the report
                messageQueue->push(MainCore::MsgAircraftReport::create(m_adsbDemod, report));
            }
            else
            {
                MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_adsbDemod, swgMapItem);
                messageQueue->push(msg);
            }
        }
    }

    // Each pipe was given a copy, so this is ours to free
    if (animations)
    {
        qDeleteAll(*animations);
        delete animations;
    }
}

// Find aircraft with icao, or create if it doesn't exist
ADSBAircraft *ADSBDemodTracker::getAircraft(int icao, bool &newAircraft)
{
    ADSBAircraft *aircraft;

    if (m_aircraft.contains(icao))
    {
        // Update existing aircraft info
        aircraft = m_aircraft.value(icao);
    }
    else
    {
        // Add new aircraft
        newAircraft = true;
        aircraft = new ADSBAircraft();
        aircraft->m_icao = icao;
        //aircraft->m_icaoHex = QString::number(aircraft->m_icao, 16);
        aircraft->m_icaoHex = QString("%1").arg(aircraft->m_icao, 6, 16, QChar('0'));
        m_aircraft.insert(icao, aircraft);
        aircraft->setCell(ADSB_COL_ICAO, aircraft->m_icaoHex);
        markDirty(aircraft);

        // Look aircraft up in database
        if (m_aircraftInfo != nullptr)
        {
            if (m_aircraftInfo->contains(icao))
            {
                aircraft->m_aircraftInfo = m_aircraftInfo->value(icao);
                aircraft->setCell(ADSB_COL_MODEL, aircraft->m_aircraftInfo->m_model);
                aircraft->setCell(ADSB_COL_TYPE, aircraft->m_aircraftInfo->m_type);
                aircraft->setCell(ADSB_COL_REGISTRATION, aircraft->m_aircraftInfo->m_registration);
                aircraft->setCell(ADSB_COL_MANUFACTURER, aircraft->m_aircraftInfo->m_manufacturerName);
                aircraft->setCell(ADSB_COL_OWNER, aircraft->m_aircraftInfo->m_owner);
                aircraft->setCell(ADSB_COL_OPERATOR_ICAO, aircraft->m_aircraftInfo->m_operatorICAO);
                aircraft->setCell(ADSB_COL_REGISTERED, aircraft->m_aircraftInfo->m_registered);
                // Try loading an airline logo based on operator ICAO
                bool hasAirlineIcon = false;
                if (aircraft->m_aircraftInfo->m_operatorICAO.size() > 0)
                {
                    const QString path = AircraftInformation::getAirlineIconPath(aircraft->m_aircraftInfo->m_operatorICAO);
                    hasAirlineIcon = !path.isEmpty();
                    if (hasAirlineIcon)
                    {
                        aircraft->m_airlineIconPath = path;
                        aircraft->m_airlineIconURL = AircraftInformation::resourcePathToURL(path);
                    }
                }
                if (!hasAirlineIcon)
                {
                    if (aircraft->m_aircraftInfo->m_operator.size() > 0)
                        aircraft->setCell(ADSB_COL_AIRLINE, aircraft->m_aircraftInfo->m_operator);
                    else
                        aircraft->setCell(ADSB_COL_AIRLINE, aircraft->m_aircraftInfo->m_owner);
                }
                // Try loading a sideview
                const QString sideviewPath = AircraftInformation::getSideviewIconPath(aircraft->m_aircraftInfo->m_registration, aircraft->m_aircraftInfo->m_operatorICAO, aircraft->m_aircraftInfo->m_type);
                if (!sideviewPath.isEmpty())
                {
                    aircraft->m_sideviewIconPath = sideviewPath;
                    aircraft->m_sideviewIconURL = AircraftInformation::resourcePathToURL(sideviewPath);
                }
                // Try loading a flag based on registration
                if (aircraft->m_aircraftInfo->m_registration.size() > 0)
                {
                    QString flag = aircraft->m_aircraftInfo->getFlag();
                    if (flag != "")
                    {
                        aircraft->m_flagName = flag;
                        aircraft->m_flagIconPath = AircraftInformation::getFlagIconPath(flag);
                        aircraft->m_flagIconURL = AircraftInformation::resourcePathToURL(aircraft->m_flagIconPath);
                    }
                }
                get3DModel(aircraft);
            }
        }

        if (aircraft->m_aircraft3DModel.isEmpty())
        {
            // Default to A320 until we get some more info
            aircraft->m_aircraftCat3DModel = Aircraft3DModels::instance().modelForType("A320");
            Aircraft3DModels::instance().offsetsForType("A320",
                aircraft->m_modelAltitudeOffset, aircraft->m_labelAltitudeOffset,
                aircraft->m_aircraftCat3DModel);
        }

        // Check to see if we need to emit a notification about this new aircraft
        checkStaticNotification(aircraft);
    }

    return aircraft;
}

void ADSBDemodTracker::setCallsign(ADSBAircraft *aircraft, const QString& callsign)
{
    aircraft->m_callsign = callsign;
    aircraft->setCell(ADSB_COL_CALLSIGN, aircraft->m_callsign);
    if (m_routeInfo && m_routeInfo->contains(aircraft->m_callsign))
    {
        AircraftRouteInformation *route = m_routeInfo->value(aircraft->m_callsign);
        aircraft->setCell(ADSB_COL_DEP, route->m_dep);
        aircraft->setCell(ADSB_COL_ARR, route->m_arr);
        aircraft->setCell(ADSB_COL_STOPS, route->m_stops);
    }
    atcCallsign(aircraft);
    callsignToFlight(aircraft);
}

void ADSBDemodTracker::atcCallsign(ADSBAircraft *aircraft)
{
    QString icao = aircraft->m_callsign.left(3);
    const Airline *airline = Airline::getByICAO(icao);
    if (airline)
    {
        aircraft->setCell(ADSB_COL_ATC_CALLSIGN, airline->m_callsign);
        // Create icons using data from Airline class, if it doesn't exist in database
        if (!aircraft->m_aircraftInfo)
        {
            // Airline logo
            QIcon *icon = nullptr;
            const QString airlinePath = AircraftInformation::getAirlineIconPath(airline->m_icao);
            if (!airlinePath.isEmpty())
            {
                aircraft->m_airlineIconPath = airlinePath;
                aircraft->m_airlineIconURL = AircraftInformation::resourcePathToURL(airlinePath);
            } else {
                aircraft->setCell(ADSB_COL_AIRLINE, airline->m_name);
            }
            // Flag
            QString flag = airline->m_country.toLower().replace(" ", "_");
            const QString flagPath = AircraftInformation::getFlagIconPath(flag);
            if (!flagPath.isEmpty())
            {
                aircraft->m_flagName = flag;
                aircraft->m_flagIconPath = flagPath;
                aircraft->m_flagIconURL = AircraftInformation::resourcePathToURL(flagPath);
            }
        }
    }
}

// Try to map callsign to flight number
void ADSBDemodTracker::callsignToFlight(ADSBAircraft *aircraft)
{
    if (!aircraft->m_callsign.isEmpty())
    {
        QRegularExpression flightNoExp("^[A-Z]{2,3}[0-9]{1,4}$");
        // Airlines line BA add a single character suffix that can be stripped
        // If the suffix is two characters, then it typically means a digit
        // has been replaced which I don't know how to guess
        // E.g Easyjet might use callsign EZY67JQ for flight EZY6267
        // BA use BAW90BG for BA890
        QRegularExpression suffixedFlightNoExp("^([A-Z]{2,3})([0-9]{1,4})[A-Z]?$");
        QRegularExpressionMatch suffixMatch;

        if (flightNoExp.match(aircraft->m_callsign).hasMatch())
        {
            aircraft->m_flight = aircraft->m_callsign;
        }
        else if ((suffixMatch = suffixedFlightNoExp.match(aircraft->m_callsign)).hasMatch())
        {
            aircraft->m_flight = QString("%1%2").arg(suffixMatch.captured(1)).arg(suffixMatch.captured(2));
        }
        else
        {
            // Don't guess, to save wasting API calls
            aircraft->m_flight = "";
        }
    }
    else
    {
        aircraft->m_flight = "";
    }
}

// Estimate outside air temperature (static temperature) from Mach number and true airspeed, assuming dry air
bool ADSBDemodTracker::calcAirTemp(ADSBAircraft *aircraft)
{
    if (aircraft->m_machValid && aircraft->m_trueAirspeedValid && (aircraft->m_mach > 0.0f))
    {
        // Calculate speed of sound
        float c = Units::knotsToMetresPerSecond(aircraft->m_trueAirspeed) / aircraft->m_mach;

        // Calculate temperature, given the speed of sound
        float a = c / 331.3f;
        float T = (a * a - 1.0f) * 273.15f;

        aircraft->setCell(ADSB_COL_EST_AIR_TEMP, (int)std::round(T));

        return true;
    }
    else
    {
        return false;
    }
}

void ADSBDemodTracker::resetStats()
{
    m_correlationAvg.reset();
    m_correlationOnesAvg.reset();
    m_rangeFails = 0;
    m_altFails = 0;
    m_frameRateTime = QDateTime::currentDateTime();
    m_adsbFrameRateCount = 0;
    m_modesFrameRateCount = 0;
    m_totalBytes = 0;
    m_maxRangeStat = 0.0f;
    m_maxAltitudeStat = 0.0f;
    m_maxRateState = 0.0f;
    for (int i = 0; i < 32; i++)
    {
        m_dfStats[i] = 0;
        m_tcStats[i] = 0;
    }
    m_stats.clear();
    m_statsChanged = true;

    ADSBDemod::MsgResetStats* message = ADSBDemod::MsgResetStats::create();
    m_adsbDemod->getInputMessageQueue()->push(message);
}

void ADSBDemodTracker::updateDFStats(int df)
{
    switch (df)
    {
    case 0:
        setStat(DF0, m_dfStats[df]);
        break;
    case 4:
        setStat(DF4, m_dfStats[df]);
        break;
    case 5:
        setStat(DF5, m_dfStats[df]);
        break;
    case 11:
        setStat(DF11, m_dfStats[df]);
        break;
    case 16:
        setStat(DF16, m_dfStats[df]);
        break;
    case 17:
        setStat(DF17, m_dfStats[df]);
        break;
    case 18:
        setStat(DF18, m_dfStats[df]);
        break;
    case 19:
        setStat(DF19, m_dfStats[df]);
        break;
    case 20:
    case 21:
        setStat(DF20_21, m_dfStats[20] + m_dfStats[21]);
        break;
    case 22:
        setStat(DF22, m_dfStats[df]);
        break;
    case 24:
    case 25:
    case 26:
    case 27:
        setStat(DF24, m_dfStats[24] + m_dfStats[25] + m_dfStats[26] + m_dfStats[27]);
        break;
    default:
        break;
    }
}

bool ADSBDemodTracker::updateTCStats(int tc, int row, int low, int high)
{
    if ((tc >= low) && (tc <= high))
    {
        qint64 sum = 0;

        for (int i = low; i <= high; i++) {
            sum += m_tcStats[i];
        }
        setStat(row, sum);
        return true;
    }
    else
    {
        return false;
    }
}

void ADSBDemodTracker::handleADSB(
    const QByteArray data,
    const QDateTime dateTime,
    float correlation,
    float correlationOnes,
    unsigned crc,
    bool updateModel)
{
    PROFILER_START();

    bool newAircraft = false;
    bool updatedCallsign = false;
    bool resetAnimation = false;
    bool mapUpdated = false;

    const int df = (data[0] >> 3) & ADS_B_DF_MASK; // Downlink format
    const int ca = data[0] & 0x7; // Capability (or Control Field for TIS-B)
    unsigned icao;
    ADSBAircraft *aircraft;

    if ((df == 11) || (df == 17))
    {
        icao = ((data[1] & 0xff) << 16) | ((data[2] & 0xff) << 8) | (data[3] & 0xff); // ICAO aircraft address
        aircraft = getAircraft(icao, newAircraft);
    }
    else if (df == 18)
    {
        const int cf = ca;
        const int tc = (data[4] >> 3) & 0x1f;
        const auto meBit = [&data](int bit) -> bool {
            const int offset = bit - 1;
            const unsigned octet = static_cast<unsigned char>(data[4 + offset / 8]);
            return (octet >> (7 - (offset % 8))) & 1;
        };

        // DF18 AA is only an ICAO address for the formats below. IMF is part of
        // the ME data and its position depends on both CF and the message type.
        // Formats with no IMF listed here are specified as ICAO-only.
        int imfBit = 0;
        if (cf == 0)
        {
            // ADS-B from a non-transponder device: AA is an ICAO address.
        }
        else if ((cf == 2) || (cf == 6))
        {
            if ((tc >= 1) && (tc <= 4)) {
                imfBit = 0; // Identification/category is ICAO-only.
            } else if ((tc >= 5) && (tc <= 8)) {
                imfBit = 21; // Surface position
            } else if ((tc == 0) || ((tc >= 9) && (tc <= 18)) || ((tc >= 20) && (tc <= 22))) {
                imfBit = 8; // Airborne position
            } else if (tc == 19) {
                imfBit = 9; // Airborne velocity
            } else if ((cf == 6) && (tc == 28) && ((data[4] & 0x7) == 1)) {
                imfBit = 56; // ADS-R emergency/priority status
            } else if ((cf == 6) && (tc == 29) && (((data[4] >> 1) & 0x3) == 1)) {
                imfBit = 51; // ADS-R target state and status, subtype 1
            } else if ((cf == 6) && (tc == 31)) {
                imfBit = 56; // ADS-R operational status
            } else {
                return; // Message format is not defined for this CF.
            }

            if ((imfBit != 0) && meBit(imfBit)) {
                return; // AA is not an ICAO address.
            }
        }
        else if (cf == 3)
        {
            if (meBit(1)) {
                return; // Coarse TIS-B track-file address, not ICAO.
            }
        }
        else
        {
            // CF1 and CF5 use non-ICAO addresses; CF4 is management and CF7
            // is reserved. None can be attached to an ICAO aircraft record.
            return;
        }
        icao = ((data[1] & 0xff) << 16) | ((data[2] & 0xff) << 8) | (data[3] & 0xff); // ICAO aircraft address
        aircraft = getAircraft(icao, newAircraft);
    }
    else
    {
        // Extract ICAO from parity
        int bytes = data.length();
        unsigned parity = ((data[bytes-3] & 0xff) << 16) | ((data[bytes-2] & 0xff) << 8) | (data[bytes-1] & 0xff);
        icao = (parity ^ crc) & 0xffffff;
        if (icao == 0)
        {
            // Appears to be the case often for DF=5
            qDebug() << "ADSBDemodTracker::handleADSB: iaco of 0 - df" << df;
            return;
        }
        aircraft = getAircraft(icao, newAircraft);
    }

    // ADS-B, non-transponder ADS-B, TIS-B, or rebroadcast of ADS-B (ADS-R)
    if ((df == 17) || ((df == 18) && (ca != 3) && (ca != 4)))
    {
        const int tc = (data[4] >> 3) & 0x1f; // Type code
        //unsigned surveillanceStatus = 0;

        m_tcStats[tc]++;
        if (updateTCStats(tc, TC_0, 0, 0)) {
        } else if (updateTCStats(tc, TC_1_4, 1, 4)) {
        } else if (updateTCStats(tc, TC_5_8, 5, 8)) {
        } else if (updateTCStats(tc, TC_9_18, 9, 18)) {
        } else if (updateTCStats(tc, TC_19, 19, 19)) {
        } else if (updateTCStats(tc, TC_20_22, 20, 22)) {
        } else if (updateTCStats(tc, TC_UNUSED, 23, 23)) {
        } else if (updateTCStats(tc, TC_24, 24, 24)) {
        } else if (updateTCStats(tc, TC_UNUSED, 25, 27)) {
        } else if (updateTCStats(tc, TC_28, 28, 28)) {
        } else if (updateTCStats(tc, TC_29, 29, 29)) {
        } else if (updateTCStats(tc, TC_UNUSED, 30, 30)) {
        } else if (updateTCStats(tc, TC_31, 31, 31)) {
        }

        if ((tc >= 1) && ((tc <= 4)))
        {
            // ADSBAircraft identification - BDS 0,8
            QString emitterCategory, callsign;

            decodeID(data, emitterCategory, callsign);

            QString prevEmitterCategory = aircraft->m_emitterCategory;
            aircraft->m_emitterCategory = emitterCategory;
            aircraft->setCell(ADSB_COL_CATEGORY, aircraft->m_emitterCategory);

            updatedCallsign = aircraft->m_callsign != callsign;
            if (updatedCallsign) {
                setCallsign(aircraft, callsign);
            }

            // Select 3D model based on category, if we don't already have one based on ICAO
            if (   aircraft->m_aircraft3DModel.isEmpty()
                && (   aircraft->m_aircraftCat3DModel.isEmpty()
                    || (prevEmitterCategory != aircraft->m_emitterCategory)
                   )
               )
            {
                get3DModelBasedOnCategory(aircraft);
                // As we're changing the model, we need to reset animations to
                // ensure gear/flaps are in correct position on new model
                resetAnimation = true;
            }
        }
        else if (((tc >= 5) && (tc <= 18)) || ((tc >= 20) && (tc <= 22)) || (tc == 0))
        {
            bool wasOnSurface = aircraft->m_onSurface;
            aircraft->m_onSurface = (tc >= 5) && (tc <= 8);
            aircraft->m_onSurfaceValid = true;

            if (wasOnSurface != aircraft->m_onSurface)
            {
                // Can't mix CPR values used on surface and those that are airborne
                aircraft->m_cprValid[0] = false;
                aircraft->m_cprValid[1] = false;
            }

            if (aircraft->m_onSurface)
            {
                // Surface position - BDS 0,6
                aircraft->setOnSurface(dateTime);

                int movement = ((data[4] & 0x7) << 4) | ((data[5] >> 4) & 0xf);
                if (movement == 0)
                {
                    // No information available
                    aircraft->m_groundspeedValid = false;
                    aircraft->setCell(ADSB_COL_GROUND_SPEED, "");
                }
                else if (movement == 1)
                {
                    // ADSBAircraft stopped
                    aircraft->setGroundspeed(0, m_settings);
                }
                else if ((movement >= 2) && (movement <= 123))
                {
                    float base, step; // In knts
                    int adjust;
                    if ((movement >= 2) && (movement <= 8))
                    {
                        base = 0.125f;
                        step = 0.125f;
                        adjust = 2;
                    }
                    else if ((movement >= 9) && (movement <= 12))
                    {
                        base = 1.0f;
                        step = 0.25f;
                        adjust = 9;
                    }
                    else if ((movement >= 13) && (movement <= 38))
                    {
                        base = 2.0f;
                        step = 0.5f;
                        adjust = 13;
                    }
                    else if ((movement >= 39) && (movement <= 93))
                    {
                        base = 15.0f;
                        step = 1.0f;
                        adjust = 39;
                    }
                    else if ((movement >= 94) && (movement <= 108))
                    {
                        base = 70.0f;
                        step = 2.0f;
                        adjust = 94;
                    }
                    else
                    {
                        base = 100.0f;
                        step = 5.0f;
                        adjust = 109;
                    }
                    aircraft->setGroundspeed(base + (movement - adjust) * step, m_settings);
                }
                else if (movement == 124)
                {
                    aircraft->setGroundspeed(175, m_settings); // Actually greater than this
                }

                int groundTrackStatus = (data[5] >> 3) & 1;
                int groundTrackValue = ((data[5] & 0x7) << 4) | ((data[6] >> 4) & 0xf);
                if (groundTrackStatus)
                {
                    float groundTrackValueFloat = groundTrackValue * 360.0/128.0;
                    clearOldHeading(aircraft, dateTime, groundTrackValueFloat);
                    aircraft->setTrack(groundTrackValueFloat, dateTime);
                }
            }
            else if (((tc >= 9) && (tc <= 18)) || ((tc >= 20) && (tc <= 22)) || (tc == 0))
            {
                // Airborne position (9-18 baro, 20-22 GNSS)

                //surveillanceStatus = (data[4] >> 1) & 0x3;

                // ADS-B: Version 0 is single antenna flag, version 2 is NIC supplement-B
                // TSI-B: ICAO/Mode A flag
                if (aircraft->m_adsbVersionValid && (aircraft->m_adsbVersion == 2) && (df == 17))
                {
                    aircraft->m_nicSupplementB = data[4] & 1;
                    aircraft->m_nicSupplementBValid = true;
                }

                int altFt;
                bool valid = decodeAltitude(data, altFt);
                if (valid)
                {
                    aircraft->setAltitude(altFt, (tc >= 20) && (tc <= 22), dateTime, m_settings);

                    // Assume runway elevation is at first reported airboune altitude
                    if (wasOnSurface)
                    {
                        aircraft->m_runwayAltitude = aircraft->m_altitude;
                        aircraft->m_runwayAltitudeValid = true;
                    }
                }
            }

            if (tc != 0)
            {
                int f;
                double latCpr, lonCpr;
                //unsigned t = (data[2] >> 3) & 1; // Not valid for TIS-B
                bool globalValid = false;
                double globalLatitude, globalLongitude;
                bool localValid = false;
                double localLatitude, localLongitude;
                const bool useAircraftReference = hasUsableCprReference(aircraft, aircraft->m_onSurface, dateTime);

                decodeCpr(data, f, latCpr, lonCpr);

                aircraft->m_cprValid[f] = true;
                aircraft->m_cprLat[f] = latCpr;
                aircraft->m_cprLong[f] = lonCpr;
                aircraft->m_cprTime[f] = dateTime;

                // CPR decoding
                // Refer to Technical Provisions  for Mode S Services and Extended Squitter - Appendix C2.6
                // See also: https://mode-s.org/decode/adsb/airborne-position.html
                // For global decoding, we need both odd and even frames
                // We also need to check that both frames aren't greater than 10s (airborne) or 50s (surface) apart in time (C.2.6.7), otherwise position may be out by ~10deg
                // I've reduced this to 8.5s, as problems have been seen where times are just 9s apart
                const int maxTimeDiff = aircraft->m_onSurface ? 48500 : 8500;
                if (aircraft->m_cprValid[0] && aircraft->m_cprValid[1]
                   && (std::abs(aircraft->m_cprTime[0].toMSecsSinceEpoch() - aircraft->m_cprTime[1].toMSecsSinceEpoch()) <= maxTimeDiff)
                   && !aircraft->m_onSurface)
                {
                    // Global decode using odd and even frames (C.2.6)
                    globalValid = decodeGlobalPosition(f, aircraft->m_cprLat, aircraft->m_cprLong, aircraft->m_cprTime, globalLatitude, globalLongitude, true);
                    if (!globalValid)
                    {
                        aircraft->m_cprValid[0] = false;
                        aircraft->m_cprValid[1] = false;
                    }
                }

                // Decode a single frame relative to the most recent emitter position once
                // a global decode has established the reference chain. Before that, use
                // the receiver position. A failed candidate must not discard an established
                // emitter reference and fall back to an unrelated receiver position.
                localValid = decodeLocalPosition(f, aircraft->m_cprLat[f], aircraft->m_cprLong[f], aircraft->m_onSurface, dateTime, aircraft, localLatitude, localLongitude, true);

                bool positionsInconsistent = false;
                if (globalValid && useAircraftReference)
                {
                    if (localValid)
                    {
                        double latDiff = std::abs(globalLatitude - localLatitude);
                        double lonDiff = longitudeDiff(globalLongitude, localLongitude);
                        double maxLatDiff = 5.1/111000.0;
                        double maxLonDiff = maxLongitudeDiff(globalLatitude, maxLatDiff);
                        positionsInconsistent = (latDiff > maxLatDiff) || (lonDiff > maxLonDiff);
                    }
                    else
                    {
                        // The new position failed the local reasonableness test, so it must
                        // not update the established track through a simultaneous global decode.
                        positionsInconsistent = true;
                    }

                    if (positionsInconsistent)
                    {
                        qDebug() << "positionsInconsistent" << aircraft->m_icaoHex << globalLatitude << localLatitude << globalLongitude << localLongitude;
                        aircraft->m_cprValid[0] = false;
                        aircraft->m_cprValid[1] = false;
                    }
                }
                if (!positionsInconsistent)
                {
                    if (globalValid)
                    {
                        if (!useAircraftReference && aircraft->m_positionValid)
                        {
                            double latDiff = std::abs(globalLatitude - aircraft->m_latitude);
                            double lonDiff = longitudeDiff(globalLongitude, aircraft->m_longitude);
                            double maxLatDiff = 50000/111000.0;
                            double maxLonDiff = maxLongitudeDiff(globalLatitude, maxLatDiff);
                            if ((latDiff > maxLatDiff) || (lonDiff > maxLonDiff))
                            {
                                qDebug() << "ADSBAircraft global position a long way from local - deleting track" << aircraft->m_icaoHex << globalLatitude << aircraft->m_latitude << globalLongitude << aircraft->m_longitude;
                                aircraft->m_trailCleared = true;
                                aircraft->m_mapTrailResetPending = true;
                                aircraft->m_positionUpdates.clear();
                            }
                        }
                        updateAircraftPosition(aircraft, globalLatitude, globalLongitude, dateTime);
                        aircraft->m_globalPosition = true;
                        aircraft->m_positionChanged = true;
                    }
                    else if (localValid)
                    {
                        updateAircraftPosition(aircraft, localLatitude, localLongitude, dateTime);
                        aircraft->m_positionChanged = true;
                    }
                }
            }
        }
        else if (tc == 19)
        {
            // Airborne velocity - BDS 0,9
            int st = data[4] & 0x7;   // Subtype

            if (df == 17)
            {
                int nacv = ((data[5] >> 3) & 0x7); // Navigation accuracy for velocity

                aircraft->setCell(ADSB_COL_NACV, m_nacvStrings[nacv]);
            }

            if ((st == 1) || (st == 2))
            {
                // Ground speed
                float v, h;

                if (decodeGroundspeed(data, v, h))
                {
                    clearOldHeading(aircraft, dateTime, h);
                    aircraft->setTrack(h, dateTime);
                    aircraft->setGroundspeed(v, m_settings);
                }
            }
            else if ((st == 3) || (st == 4))
            {
                // Airspeed (only likely to get this if an aircraft is unable to determine it's position)
                bool tas;
                int as;
                bool hdgValid;
                float hdg;

                bool asValid = decodeAirspeed(data, tas, as, hdgValid, hdg);

                if (hdgValid) {
                    aircraft->setHeading(hdg, dateTime);
                }

                if (asValid)
                {
                    if (tas) {
                        aircraft->setTrueAirspeed(as, m_settings);
                    } else {
                        aircraft->setIndicatedAirspeed(as, dateTime, m_settings);
                    }
                }
            }

            if ((st >= 1) && (st <= 4))
            {
                int verticalRate;

                if (decodeVerticalRate(data, verticalRate)) {
                    aircraft->setVerticalRate(verticalRate, m_settings);
                }
            }
        }
        else if (tc == 28)
        {
            // ADSBAircraft status - BDS 6,1
            int st = data[4] & 0x7;   // Subtype
            if (st == 1)
            {
                int es = (data[5] >> 5) & 0x7; // Emergency state
                int modeA =  ((data[5] << 8) & 0x1f00) | (data[6] & 0xff); // Mode-A code (squawk)
                aircraft->m_status = m_emergencyStatus[es];
                aircraft->setCell(ADSB_COL_STATUS, aircraft->m_status);
                aircraft->m_squawk = squawkDecode(modeA);
                // This message doesn't signal SPI, so use the state last seen in DF5/21, to keep the text consistent
                if (aircraft->m_spi) {
                    aircraft->setCell(ADSB_COL_SQUAWK, QString("%1 IDENT").arg(aircraft->m_squawk, 4, 10, QLatin1Char('0')));
                } else {
                    aircraft->setCell(ADSB_COL_SQUAWK, QString("%1").arg(aircraft->m_squawk, 4, 10, QLatin1Char('0')));
                }
            }
            else if (st == 2)
            {
                // TCAS/ACAS RA Broadcast - ME bits 9-56 carry the BDS 3,0 layout
                decodeRABroadcast(data, dateTime, aircraft);
            }
        }
        else if (tc == 29)
        {            
            if (aircraft->m_adsbVersionValid && (aircraft->m_adsbVersion == 0))
            {
                // Trajectory Change Point - according to the spec this shouldn't be transmitted 
            }
            else if (aircraft->m_adsbVersionValid && (aircraft->m_adsbVersion == 1))
            {
                // Target state and status - slightly different to version 2 below. Haven't ever seen version 1, so just ignore for now
            }
            else if (aircraft->m_adsbVersionValid && (aircraft->m_adsbVersion == 2))
            {
                // Target state and status                
                //bool selAltitudeType = (data[5] >> 7) & 0x1;
                int selAltitudeFix = ((data[5] & 0x7f) << 4) | ((data[6] >> 4) & 0xf);
                if (selAltitudeFix != 0)
                {
                    int selAltitude = (selAltitudeFix - 1) * 32; // Ft
                    aircraft->m_selAltitude = selAltitude;
                    aircraft->m_selAltitudeValid = true;
                    if (m_settings.m_siUnits) {
                        aircraft->setCell(ADSB_COL_SEL_ALTITUDE, Units::feetToIntegerMetres(aircraft->m_selAltitude));
                    }
                    else {
                        aircraft->setCell(ADSB_COL_SEL_ALTITUDE, roundTo50Feet(aircraft->m_selAltitude));
                    }
                }
                int baroFix = ((data[6] & 0xf) << 5) | ((data[7] >> 3) & 0x1f);
                if (baroFix != 0)
                {
                    float baro = (baroFix - 1) * 0.8f + 800.0f; // mb
                    aircraft->m_baro = baro;
                    aircraft->m_baroValid = true;
                    aircraft->setCell(ADSB_COL_BARO, std::round(aircraft->m_baro));
                    updateQNH(aircraft, baro);
                }
                bool selHeadingValid = (data[7] >> 2) & 0x1;
                if (selHeadingValid)
                {
                    int selHeadingFix = ((data[7] & 0x3) << 7) | ((data[8] >> 1) & 0x7f);
                    selHeadingFix = (selHeadingFix << 23) >> 23;
                    float selHeading = selHeadingFix * 180.0f / 256.0f;
                    if (selHeading < 0.0f) {
                        selHeading += 360.0f;
                    }
                    aircraft->m_selHeading = selHeading;
                    aircraft->m_selHeadingValid = true;
                    aircraft->setCell(ADSB_COL_SEL_HEADING, std::round(aircraft->m_selHeading));
                }

                bool modeValid = (data[9] >> 1) & 0x1;
                if (modeValid)
                {
                    bool autoPilot = data[9] & 0x1;
                    bool vnavMode = (data[10] >> 7) & 0x1;
                    bool altHoldMode = (data[10] >> 6) & 0x1;
                    bool approachMode = (data[10] >> 4) & 0x1;
                    bool tcasOperational = (data[10] >> 3) & 0x1;
                    bool lnavMode = (data[10] >> 2) & 0x1;

                    aircraft->m_autopilot = autoPilot;
                    aircraft->m_autopilotValid = true;
                    aircraft->setCell(ADSB_COL_AP, autoPilot ? QChar(0x2713) : QChar(0x2717)); // Tick or cross

                    aircraft->m_vnavMode = vnavMode;
                    aircraft->m_vnavModeValid = true;
                    aircraft->m_altHoldMode = altHoldMode;
                    aircraft->m_altHoldModeValid = true;
                    aircraft->m_approachMode = approachMode;
                    aircraft->m_approachModeValid = true;
                    aircraft->m_tcasOperational = tcasOperational;
                    aircraft->m_tcasOperationalValid = true;
                    aircraft->m_lnavMode = lnavMode;
                    aircraft->m_lnavModeValid = true;

                    if (aircraft->cellText(ADSB_COL_TCAS) != "RA") {
                        aircraft->setCell(ADSB_COL_TCAS, tcasOperational ? QChar(0x2713) : QChar(0x2717)); // Tick or cross
                    }

                    // Should only have one of these active
                    QString vMode = "";
                    if (vnavMode) {
                        vMode = vMode + "VNAV ";
                    }
                    if (altHoldMode) {
                        vMode = vMode + "HOLD ";
                    }
                    if (approachMode) {
                        vMode = vMode + "APP ";
                    }
                    vMode = vMode.trimmed();
                    aircraft->setCell(ADSB_COL_V_MODE, vMode);

                    QString lMode = "";
                    if (lnavMode) {
                        lMode = lMode + "LNAV ";
                    }
                    if (approachMode) {
                        lMode = lMode + "APP ";
                    }
                    lMode = lMode.trimmed();
                    aircraft->setCell(ADSB_COL_L_MODE, lMode);
                }
            }
        }
        else if (tc == 24)
        {
            // Surface system status
            // Get quite a few from heathrow data
        }
        else if (tc == 31)
        {
            // ADSBAircraft operational status
            int st = data[4] & 0x7;

            int adsbVersion = (data[9] >> 5) & 0x7;

            aircraft->m_adsbVersion = adsbVersion;
            aircraft->m_adsbVersionValid = true;
            aircraft->setCell(ADSB_COL_VERSION, aircraft->m_adsbVersion);

            if (adsbVersion == 2)
            {
                bool nicSupplementA = (data[9] >> 4) & 0x1;
                int nacp = data[9] & 0xf;
                int sil = (data[10] >> 4) & 0x3;
                //bool hrd = (data[10] >> 2) & 0x1;       // Whether headings are magnetic (false) or true North (true)
                bool silSuppliment = (data[10] >> 1) & 0x1; // 0 per hour / 1 per sample

                aircraft->m_nicSupplementA = nicSupplementA;
                aircraft->m_nicSupplementAValid = true;

                static const QStringList nacpStrings = {
                    ">= 10 NM", "< 10 NM", "< 4 NM", "< 2 NM", "< 1NM", "< 0.5NM", "< 0.3 NM", "< 0.1 NM",
                    "< 0.05 NM", "< 30 m", "< 10 m", "< 3 m", "Reserved", "Reserved", "Reserved", "Reserved"
                };
                static const QStringList silStrings = {
                    "> 1e-3", "<= 1e-3", "<= 1e-5", "<= 1e-7"
                };
                static const QStringList silSupplimentStrings = {
                    "ph", "ps"
                };
                QString silString = QString("%1 %2").arg(silStrings[sil]).arg(silSupplimentStrings[silSuppliment]);

                aircraft->setCell(ADSB_COL_NACP, nacpStrings[nacp]);
                aircraft->setCell(ADSB_COL_SIL, silString);

                if (st == 0)
                {
                    // Airborne
                    unsigned capacityClassCode = ((data[5] & 0xff) << 8) | (data[6] & 0xff);
                    bool tcasOperational = (capacityClassCode >> 13) & 1;
                    //bool adsbIn = (capacityClassCode >> 12) & 1;

                    aircraft->m_tcasOperational = tcasOperational;
                    aircraft->m_tcasOperationalValid = true;

                    int gva = (data[10] >> 6) & 0x3;
                    bool nicBaro = (data[10] >> 3) & 0x1;

                    static const QStringList gvaStrings = {
                        "> 150 m", "<= 150 m", "<= 45 m", "Reserved"
                    };

                    aircraft->setCell(ADSB_COL_GVA, gvaStrings[gva]);
                    aircraft->setCell(ADSB_COL_NIC_BARO, nicBaro ? QChar(0x2713) : QChar(0x2717)); // Tick or cross
                }
                else if (st == 1)
                {
                    // Surface
                    unsigned capacityClassCode = ((data[5] & 0xff) << 4) | ((data[6] >> 4) & 0xf);
                    unsigned lengthWidthCode = data[6] & 0xf;
                    int nacv = (capacityClassCode >> 1) & 0x7;
                    bool nicSupplementC = capacityClassCode & 1;

                    aircraft->m_nicSupplementC = nicSupplementC;
                    aircraft->m_nicSupplementCValid = true;

                    aircraft->setCell(ADSB_COL_NACV, m_nacvStrings[nacv]);

                    switch (lengthWidthCode)
                    {
                    case 0:
                        aircraft->setCell(ADSB_COL_LENGTH, "");
                        aircraft->setCell(ADSB_COL_WIDTH, "");
                        break;
                    case 1:
                        aircraft->setCell(ADSB_COL_LENGTH, 15);
                        aircraft->setCell(ADSB_COL_WIDTH, 23);
                        break;
                    case 2:
                        aircraft->setCell(ADSB_COL_LENGTH, 25);
                        aircraft->setCell(ADSB_COL_WIDTH, 28.5);
                        break;
                    case 3:
                        aircraft->setCell(ADSB_COL_LENGTH, 25);
                        aircraft->setCell(ADSB_COL_WIDTH, 34);
                        break;
                    case 4:
                        aircraft->setCell(ADSB_COL_LENGTH, 35);
                        aircraft->setCell(ADSB_COL_WIDTH, 33);
                        break;
                    case 5:
                        aircraft->setCell(ADSB_COL_LENGTH, 35);
                        aircraft->setCell(ADSB_COL_WIDTH, 38);
                        break;
                    case 6:
                        aircraft->setCell(ADSB_COL_LENGTH, 45);
                        aircraft->setCell(ADSB_COL_WIDTH, 39.5);
                        break;
                    case 7:
                        aircraft->setCell(ADSB_COL_LENGTH, 45);
                        aircraft->setCell(ADSB_COL_WIDTH, 45);
                        break;
                    case 8:
                        aircraft->setCell(ADSB_COL_LENGTH, 55);
                        aircraft->setCell(ADSB_COL_WIDTH, 45);
                        break;
                    case 9:
                        aircraft->setCell(ADSB_COL_LENGTH, 55);
                        aircraft->setCell(ADSB_COL_WIDTH, 52);
                        break;
                    case 10:
                        aircraft->setCell(ADSB_COL_LENGTH, 65);
                        aircraft->setCell(ADSB_COL_WIDTH, 59.5);
                        break;
                    case 11:
                        aircraft->setCell(ADSB_COL_LENGTH, 65);
                        aircraft->setCell(ADSB_COL_WIDTH, 67);
                        break;
                    case 12:
                        aircraft->setCell(ADSB_COL_LENGTH, 75);
                        aircraft->setCell(ADSB_COL_WIDTH, 72.5);
                        break;
                    case 13:
                        aircraft->setCell(ADSB_COL_LENGTH, 75);
                        aircraft->setCell(ADSB_COL_WIDTH, 80);
                        break;
                    case 14:
                        aircraft->setCell(ADSB_COL_LENGTH, 85);
                        aircraft->setCell(ADSB_COL_WIDTH, 80);
                        break;
                    case 15:
                        aircraft->setCell(ADSB_COL_LENGTH, 85);
                        aircraft->setCell(ADSB_COL_WIDTH, 90);
                        break;
                    }
                }
                else
                {
                    // Reserved sub-type
                }

                unsigned operationalMode = ((data[7] & 0xff) << 8) | (data[8] & 0xff);
                bool tcasActive = (operationalMode >> 13) & 1;
                bool identActive = (operationalMode >> 12) & 1;

                if (tcasActive)
                {
                    aircraft->m_tcasAlert = true;
                    aircraft->setCell(ADSB_COL_TCAS, "RA");
                }
                else if (aircraft->m_tcasOperationalValid)
                {
                    aircraft->m_tcasAlert = false;
                    aircraft->setCell(ADSB_COL_TCAS, aircraft->m_tcasOperational ? QChar(0x2713) : QChar(0x2717)); // Tick or cross
                }
                else
                {
                    aircraft->setCell(ADSB_COL_TCAS, "");
                }
                aircraft->setCell(ADSB_COL_IDENT, identActive ? QChar(0x2713) : QChar(0x2717)); // Tick or cross

            }
        }
        else
        {
            // 23, 25, 26, 27, 30 reserved
            //qDebug() << "ADSBDemodGUI: Unsupported tc" << tc << aircraft->m_icaoHex << aircraft->m_adsbFrameCount;
        }

        // Horizontal containment radius limit (Rc) and Navigation integrity category (NIC)
        if (tc == 0)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "?");
            aircraft->m_radius = -1.0f;
            aircraft->setCell(ADSB_COL_NIC, 0);
        }
        else if (tc == 5)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 7.5 m");
            aircraft->m_radius = 7.5f;
            aircraft->setCell(ADSB_COL_NIC, 11);
        }
        else if (tc == 6)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 25 m");
            aircraft->m_radius = 25.0f;
            aircraft->setCell(ADSB_COL_NIC, 10);
        }
        else if ((tc == 7) && aircraft->m_nicSupplementAValid && aircraft->m_nicSupplementA)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 75 m");
            aircraft->m_radius = 75.0f;
            aircraft->setCell(ADSB_COL_NIC, 9);
        }
        else if (tc == 7)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 0.1 NM");
            aircraft->m_radius = 185.2f;
            aircraft->setCell(ADSB_COL_NIC, 8);
        }
        else if (tc == 8)
        {
            aircraft->setCell(ADSB_COL_RADIUS, ">= 0.1 NM");
            aircraft->m_radius = 185.2f; // ?
            aircraft->setCell(ADSB_COL_NIC, 0);
        }
        else if (tc == 9)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 7.5 m");
            aircraft->m_radius = 7.5f;
            aircraft->setCell(ADSB_COL_NIC, 11);
        }
        else if (tc == 10)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 25 m");
            aircraft->m_radius = 25.0f;
            aircraft->setCell(ADSB_COL_NIC, 10);
        }
        else if ((tc == 11) && aircraft->m_nicSupplementAValid && aircraft->m_nicSupplementA && aircraft->m_nicSupplementBValid && aircraft->m_nicSupplementB)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 75 m");
            aircraft->m_radius = 75.0f;
            aircraft->setCell(ADSB_COL_NIC, 9);
        }
        else if (tc == 11)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 0.1 NM");
            aircraft->m_radius = 185.2f;
            aircraft->setCell(ADSB_COL_NIC, 8);
        }
        else if (tc == 12)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 0.2 NM");
            aircraft->m_radius = 370.4f;
            aircraft->setCell(ADSB_COL_NIC, 7);
        }
        else if ((tc == 13) && aircraft->m_nicSupplementAValid && !aircraft->m_nicSupplementA && aircraft->m_nicSupplementBValid && aircraft->m_nicSupplementB)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 0.3 NM");
            aircraft->m_radius = 555.6f;
            aircraft->setCell(ADSB_COL_NIC, 6);
        }
        else if ((tc == 13) && aircraft->m_nicSupplementAValid && aircraft->m_nicSupplementA && aircraft->m_nicSupplementBValid && aircraft->m_nicSupplementB)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 0.6 NM");
            aircraft->m_radius = 1111.2f;
            aircraft->setCell(ADSB_COL_NIC, 6);
        }
        else if (tc == 13)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 0.5 NM");
            aircraft->m_radius = 926.0f;
            aircraft->setCell(ADSB_COL_NIC, 6);
        }
        else if (tc == 14)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 1 NM");
            aircraft->m_radius = 1852.0f;
            aircraft->setCell(ADSB_COL_NIC, 5);
        }
        else if (tc == 15)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 2 NM");
            aircraft->m_radius = 3704.0f;
            aircraft->setCell(ADSB_COL_NIC, 4);
        }
        else if ((tc == 16) && aircraft->m_nicSupplementAValid && aircraft->m_nicSupplementA && aircraft->m_nicSupplementBValid && aircraft->m_nicSupplementB)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 4 NM");
            aircraft->m_radius = 7408.0f;
            aircraft->setCell(ADSB_COL_NIC, 3);
        }
        else if (tc == 16)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 8 NM");
            aircraft->m_radius = 14816.0f;
            aircraft->setCell(ADSB_COL_NIC, 2);
        }
        else if (tc == 17)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 20 NM");
            aircraft->m_radius = 37040.0f;
            aircraft->setCell(ADSB_COL_NIC, 1);
        }
        else if (tc == 18)
        {
            aircraft->setCell(ADSB_COL_RADIUS, ">= 20 NM");
            aircraft->m_radius = 37040.0f; // ?
            aircraft->setCell(ADSB_COL_NIC, 0);
        }
        else if (tc == 20)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 7.5 m");
            aircraft->m_radius = 7.5f;
            aircraft->setCell(ADSB_COL_NIC, 11);
        }
        else if (tc == 21)
        {
            aircraft->setCell(ADSB_COL_RADIUS, "< 25 m");
            aircraft->m_radius = 25.0f;
            aircraft->setCell(ADSB_COL_NIC, 10);
        }
        else if (tc == 22)
        {
            aircraft->setCell(ADSB_COL_RADIUS, ">= 25 m");
            aircraft->m_radius = 25.0f; // ?
            aircraft->setCell(ADSB_COL_NIC, 0);
        }

        // Update aircraft in map
        if (aircraft->m_positionValid)
        {
            // Check to see if we need to start any animations
            QList<SWGSDRangel::SWGMapAnimation *> *animations = animate(dateTime, aircraft);

            // Update map displayed in channel. Coalesced, see flushMapUpdates
            if (updateModel) {
                markDirty(aircraft);
            }

            if (m_loadingData) {
                // A log is already decoded as fast as the tracker can consume it. Do not
                // collapse its frames to one final map item: the intermediate positions,
                // PFD state and model animations are the flight being replayed.
                sendToMap(aircraft, animations, dateTime);
            } else {
                // Live traffic is coalesced on to the flush timer.
                queueMapUpdate(aircraft, animations);
            }
            mapUpdated = true;

            if (resetAnimation)
            {
                // Wait until after model has changed before resetting
                // otherwise animation might play on old model
                aircraft->m_gearDown = false;
                aircraft->m_tyresFlat = true;
                aircraft->m_flaps = 0.0;
                aircraft->m_engineStarted = false;
                aircraft->m_rotorStarted = false;
            }
        }
    }
    else if (df == 18)
    {
        // TIS-B that doesn't match ADS-B formats, such as TIS-B management or Coarse TIB-B position (TODO)
        qDebug() << "TIS B message cf=" << ca << " icao: " << QString::number(icao, 16);
    }
    else if ((df == 4) || (df == 5))
    {
        decodeModeS(data, dateTime, df, aircraft);
    }
    else if ((df == 20) || (df == 21))
    {
        decodeModeS(data, dateTime, df, aircraft);
        decodeCommB(data, dateTime, df, aircraft, updatedCallsign);
    }
    else if (df == 11)
    {
        // All call reply
        // Extract 6-bit IC (Interegator Code (II or SI)) which is the address of the Mode-S ground radar
        int bytes = data.length();
        unsigned parity = ((data[bytes-3] & 0xff) << 16) | ((data[bytes-2] & 0xff) << 8) | (data[bytes-1] & 0xff);
        unsigned ca = data[0] & 0x7;
        unsigned d = (parity ^ crc);
        unsigned ii = d & 0xf;
        unsigned cl = (d >> 4) & 0x7;
        bool siCode = cl > 0;

        if ((ca >= 1) && (ca <= 3)) {
            return;
        }

        if (cl <= 4)
        {
            unsigned si = ii + (16 * (cl - 1));
            unsigned ic = siCode ? si : ii;

            aircraft->setCell(ADSB_COL_IC, ic);
        }
        else
        {
            // This can happen quite frequency when running with a low correlation threshold
            // So the IC map probably not very reliable in that case
            //qDebug() << "ADSBDemodGUI: DF11 cl out of range" << Qt::hex << cl << d << parity << crc << aircraft->m_icaoHex << aircraft->m_adsbFrameCount;
            return;
        }
    }
    else if (df == 0)
    {
        // Short air-to-air ACAS (Airborne Collision Avoidance System)
        //bool onSurface = (data[0] >> 2) & 1;
        //unsigned crosslinkCapability = (data[0] >> 1) & 1;
        unsigned spare1 = data[0] & 1;
        //unsigned sensitivityLevel = (data[1] >> 5) & 0x7;
        unsigned spare2 = (data[1] >> 3) & 0x3;
        unsigned replyInfo = ((data[1] & 0x7) << 1) | ((data[2] >> 7) & 1);
        unsigned spare3 = (data[2] >> 5) & 0x3;

        if ((spare1 != 0) || (spare2 != 0) || (spare3 != 0)) {
            qDebug() << "df == 0, SPARE SET TO 1" << aircraft->m_icaoHex;
            return;
        }

        QString acasCapability;
        switch (replyInfo)
        {
        case 0:
            acasCapability = "No ACAS";
            break;
        case 2:
            acasCapability = "No RA";
            break;
        case 3:
            acasCapability = "V";
            break;
        case 4:
            acasCapability = "V+H";
            break;
        }
        if (!acasCapability.isEmpty()) {
            aircraft->setCell(ADSB_COL_ACAS, acasCapability);
        }

        QString maxSpeed;
        switch (replyInfo)
        {
        case 8:
            maxSpeed = "No data";
            break;
        case 9:
            maxSpeed = "<75";
            break;
        case 10:
            maxSpeed = "75-150";
            break;
        case 11:
            maxSpeed = "150-300";
            break;
        case 12:
            maxSpeed = "300-600";
            break;
        case 13:
            maxSpeed = "600-1200";
            break;
        case 14:
            maxSpeed = ">1200";
            break;
        }
        if (!maxSpeed.isEmpty()) {
            aircraft->setCell(ADSB_COL_MAX_SPEED, maxSpeed);
        }

        decodeModeSAltitude(data, dateTime, aircraft);
    }
    else if (df == 16)
    {
        // Long air-to-air ACAS (Airborne Collision Avoidance System)

        decodeModeSAltitude(data, dateTime, aircraft);

        unsigned dv = data[4] & 0xff;
        unsigned replyInfo = ((data[1] & 0x7) << 1) | ((data[2] >> 7) & 1);

        QString acasCapability;
        switch (replyInfo)
        {
        case 0:
            acasCapability = "No ACAS";
            break;
        case 2:
            acasCapability = "No RA";
            break;
        case 3:
            acasCapability = "V";
            break;
        case 4:
            acasCapability = "V+H";
            break;
        }

        if (!acasCapability.isEmpty()) {
            // The DF0 branch below writes this cell; this one computed it and dropped it
            aircraft->setCell(ADSB_COL_ACAS, acasCapability);
        }

        if (dv == 0x30)
        {
            decodeRABroadcast(data, dateTime, aircraft);
        }
        else
        {
            decodeCommB(data, dateTime, df, aircraft, updatedCallsign);
        }
    }
    else
    {
        qDebug() << "ADSBDemodGUI: Unsupported df" << df;
    }

    // Comm-B and other Mode-S replies carry several PFD values too. Live operation can
    // wait for the next coalesced update, but a replay must preserve the state produced
    // by every input frame, including a final Mode-S frame with no later ADS-B squitter.
    if (!mapUpdated && aircraft->m_positionValid)
    {
        QList<SWGSDRangel::SWGMapAnimation *> *animations = animate(dateTime, aircraft);
        if (updateModel) {
            markDirty(aircraft);
        }
        if (m_loadingData) {
            sendToMap(aircraft, animations, dateTime);
        } else {
            queueMapUpdate(aircraft, animations);
        }
    }

    // Update stats
    m_dfStats[df]++;
    updateDFStats(df);
    if ((df == 17) || (df == 18)) {
        m_adsbFrameRateCount++;
    } else {
        m_modesFrameRateCount++;
    }
    m_totalBytes += data.size();

    aircraft->m_rxTime = dateTime;
    aircraft->m_updateTime = QDateTime::currentDateTime();
    QTime time = aircraft->m_updateTime.time();
    aircraft->setCell(ADSB_COL_TIME, QString("%1:%2:%3").arg(time.hour(), 2, 10, QLatin1Char('0')).arg(time.minute(), 2, 10, QLatin1Char('0')).arg(time.second(), 2, 10, QLatin1Char('0')));
    if (df == 17)
    {
        aircraft->m_adsbFrameCount++;
        aircraft->setCell(ADSB_COL_ADSB_FRAMECOUNT, aircraft->m_adsbFrameCount);
    }
    else if (df == 18)
    {
        if (ca == 0)
        {
            aircraft->m_nonTransponderFrameCount++;
            aircraft->setCell(ADSB_COL_NON_TRANSPONDER, aircraft->m_nonTransponderFrameCount);
        }
        else if (ca == 6)
        {
            aircraft->m_adsrFrameCount++;
            aircraft->setCell(ADSB_COL_ADSR_FRAMECOUNT, aircraft->m_adsrFrameCount);
        }
        else
        {
            aircraft->m_tisBFrameCount++;
            aircraft->setCell(ADSB_COL_TIS_B_FRAMECOUNT, aircraft->m_tisBFrameCount);
        }
    }
    else
    {
        aircraft->m_modesFrameCount++;
        aircraft->setCell(ADSB_COL_MODES_FRAMECOUNT, aircraft->m_modesFrameCount);
    }
    aircraft->setCell(ADSB_COL_FRAMECOUNT, aircraft->m_adsbFrameCount + aircraft->m_tisBFrameCount + aircraft->m_modesFrameCount);

    if (correlation < aircraft->m_minCorrelation)
        aircraft->m_minCorrelation = correlation;
    if (correlation > aircraft->m_maxCorrelation)
        aircraft->m_maxCorrelation = correlation;
    m_correlationAvg(correlation);
    aircraft->m_correlationAvg(correlation);
    aircraft->m_correlation = aircraft->m_correlationAvg.instantAverage();
    aircraft->setCell(ADSB_COL_CORRELATION, QString("%1/%2/%3")
        .arg(CalcDb::dbPower(aircraft->m_minCorrelation), 3, 'f', 1)
        .arg(CalcDb::dbPower(aircraft->m_correlation), 3, 'f', 1)
        .arg(CalcDb::dbPower(aircraft->m_maxCorrelation), 3, 'f', 1));
    m_correlationOnesAvg(correlationOnes);
    aircraft->setCell(ADSB_COL_RSSI, QString("%1")
        .arg(CalcDb::dbPower(m_correlationOnesAvg.instantAverage()), 3, 'f', 1));

    // Check to see if we need to emit a notification about this aircraft
    checkDynamicNotification(aircraft);

    // Update text below photo if it's likely to have changed
    if (newAircraft || updatedCallsign) {
        emit photoRefresh((quint32) aircraft->m_icao);
    }

    PROFILER_STOP("ADS-B decode");
}

// Clear heading if much older than latest track figure or too different
void ADSBDemodTracker::clearOldHeading(ADSBAircraft *aircraft, const QDateTime& dateTime, float newTrack)
{
    if (aircraft->m_headingValid
        && (aircraft->m_heading != newTrack)
        && (   (std::abs(newTrack - aircraft->m_trackWhenHeadingSet) >= 5)
            || (aircraft->m_headingDateTime.secsTo(dateTime) >= 10)
           )
       )
    {
        aircraft->m_headingValid = false;
        aircraft->setCell(ADSB_COL_HEADING, "");
    }
}

void ADSBDemodTracker::decodeID(const QByteArray& data, QString& emitterCategory, QString& callsign)
{
    // ADSBAircraft identification - BDS 0,8
    unsigned tc = ((data[4] >> 3) & 0x1f); // Type code
    int ec = data[4] & 0x7;   // Emitter category

    if (tc == 4) {
        emitterCategory = m_categorySetA[ec];
    } else if (tc == 3) {
        emitterCategory = m_categorySetB[ec];
    } else if (tc == 2) {
        emitterCategory = m_categorySetC[ec];
    } else {
        emitterCategory = QStringLiteral("Reserved");
    }

    // Flight/callsign - Extract 8 6-bit characters from 6 8-bit bytes, MSB first
    unsigned char c[8];
    c[0] = (data[5] >> 2) & 0x3f; // 6
    c[1] = ((data[5] & 0x3) << 4) | ((data[6] & 0xf0) >> 4);  // 2+4
    c[2] = ((data[6] & 0xf) << 2) | ((data[7] & 0xc0) >> 6);  // 4+2
    c[3] = (data[7] & 0x3f); // 6
    c[4] = (data[8] >> 2) & 0x3f;
    c[5] = ((data[8] & 0x3) << 4) | ((data[9] & 0xf0) >> 4);
    c[6] = ((data[9] & 0xf) << 2) | ((data[10] & 0xc0) >> 6);
    c[7] = (data[10] & 0x3f);
    // Map to ASCII
    char callsignASCII[9];
    for (int i = 0; i < 8; i++)
        callsignASCII[i] = m_idMap[c[i]];
    callsignASCII[8] = '\0';
    callsign = QString(callsignASCII).trimmed();
}

bool ADSBDemodTracker::decodeGroundspeed(const QByteArray& data, float& v, float& h)
{
    int st = data[4] & 0x7;   // Subtype - 2 is supersonic, with 4kt LSB
    int lsb = (st == 2) ? 4 : 1;
    int s_ew = (data[5] >> 2) & 1; // East-west velocity sign
    int v_ew = ((data[5] & 0x3) << 8) | (data[6] & 0xff); // East-west velocity
    int s_ns = (data[7] >> 7) & 1; // North-south velocity sign
    int v_ns = ((data[7] & 0x7f) << 3) | ((data[8] >> 5) & 0x7); // North-south velocity

    // All-zeros means no velocity information available
    if ((v_ew == 0) || (v_ns == 0)) {
        return false;
    }

    int v_we;
    int v_sn;

    if (s_ew) {
        v_we = -1 * (v_ew - 1);
    } else {
        v_we = v_ew - 1;
    }
    if (s_ns) {
        v_sn = -1 * (v_ns - 1);
    } else {
        v_sn = v_ns - 1;
    }
    v_we *= lsb;
    v_sn *= lsb;
    v = std::round(std::sqrt(v_we*v_we + v_sn*v_sn));
    h = std::atan2(v_we, v_sn) * 360.0/(2.0*M_PI);
    if (h < 0.0) {
        h += 360.0;
    }
    return true;
}

bool ADSBDemodTracker::decodeAirspeed(const QByteArray& data, bool& tas, int& as, bool& hdgValid, float& hdg)
{
    int st = data[4] & 0x7;   // Subtype - 4 is supersonic, with 4kt LSB
    int lsb = (st == 4) ? 4 : 1;
    hdgValid = (data[5] >> 2) & 1; // Heading status
    int hdgFix =  ((data[5] & 0x3) << 8) | (data[6] & 0xff); // Heading
    hdg = hdgFix / 1024.0f * 360.0f;

    tas = (data[7] >> 7) & 1; // Airspeed type (true or indicated)
    int asFix = ((data[7] & 0x7f) << 3) | ((data[8] >> 5) & 0x7); // Airspeed

    // All-zeros means no airspeed information available
    if (asFix == 0) {
        return false;
    }
    as = (asFix - 1) * lsb;
    return true;
}

bool ADSBDemodTracker::decodeVerticalRate(const QByteArray& data, int& verticalRate)
{
    int s_vr = (data[8] >> 3) & 1; // Vertical rate sign
    int vr = ((data[8] & 0x7) << 6) | ((data[9] >> 2) & 0x3f); // Vertical rate

    // All-zeros means no vertical rate information available
    if (vr == 0) {
        return false;
    }
    verticalRate = (vr-1)*64*(s_vr?-1:1);
    return true;
}

// Called when we have both lat & long
void ADSBDemodTracker::updateAircraftPosition(ADSBAircraft *aircraft, double latitude, double longitude, const QDateTime& dateTime)
{
    // Calculate range, azimuth and elevation to aircraft from station
    m_azEl.setTarget(latitude, longitude, aircraft->m_altitudeValid ? Units::feetToMetres(aircraft->m_altitude) : 0);
    m_azEl.calculate();

    aircraft->m_latitude = latitude;
    aircraft->m_longitude = longitude;
    aircraft->setCell(ADSB_COL_LATITUDE, aircraft->m_latitude);
    aircraft->setCell(ADSB_COL_LONGITUDE, aircraft->m_longitude);
    aircraft->m_positionDateTime = dateTime;
    aircraft->m_positionOnSurface = aircraft->m_onSurface;
    aircraft->m_positionUpdates.append(ADSBAircraftPosition(
        aircraft->m_latitude,
        aircraft->m_longitude,
        aircraft->m_altitude,
        dateTime));

    if (!aircraft->m_positionValid)
    {
        aircraft->m_positionValid = true;
        // Now we have a position, add a plane to the map
        markDirty(aircraft);
    }

    aircraft->m_range = m_azEl.getDistance();
    aircraft->m_azimuth = m_azEl.getAzimuth();
    aircraft->m_elevation = m_azEl.getElevation();
    aircraft->setCell(ADSB_COL_RANGE, aircraft->m_range / 1000.0);
    aircraft->setCell(ADSB_COL_AZEL, QString("%1/%2").arg(std::round(aircraft->m_azimuth)).arg(std::round(aircraft->m_elevation)));
    if (aircraft == m_trackAircraft) {
        m_adsbDemod->setTarget(aircraft->targetName(), aircraft->m_azimuth, aircraft->m_elevation, aircraft->m_range);
    }
    if (m_azEl.getDistance() > m_maxRangeStat)
    {
        m_maxRangeStat = m_azEl.getDistance();
        setStat(MAX_RANGE, m_maxRangeStat / 1000.0);
    }
    if (aircraft->m_altitudeValid && (aircraft->m_altitude > m_maxAltitudeStat))
    {
        m_maxAltitudeStat = aircraft->m_altitude;
        setStat(MAX_ALTITUDE, m_maxAltitudeStat);
    }
    if (aircraft->m_altitudeValid) {
        updateCoverageMap(m_azEl.getAzimuth(), m_azEl.getElevation(), m_azEl.getDistance(), aircraft->m_altitude);
    }
}

bool ADSBDemodTracker::validateGlobalPosition(double latitude, double longitude, bool countFailure)
{
    // Log files could be from anywhere, so allow any position
    if (m_loadingData) {
        return true;
    }

    // Calculate range to aircraft from station
    m_azEl.setTarget(latitude, longitude, 0);
    m_azEl.calculate();

    // Reasonableness test
    if (m_azEl.getDistance() < 600000)
    {
        return true;
    }
    else if (countFailure)
    {
        m_rangeFails++;
        setStat(RANGE_FAILS, m_rangeFails);
    }
    return false;
}

// Called when we have lat & long from local decode and we need to check if it is in a valid range (<180nm/333km airborne or 45nm/83km for surface)
bool ADSBDemodTracker::validateLocalPosition(double latitude, double longitude, bool surfacePosition, bool countFailure)
{
    // Calculate range to aircraft from station
    m_azEl.setTarget(latitude, longitude, 0);
    m_azEl.calculate();

    // Don't use the full 333km, as there may be some error in station position
    if (m_azEl.getDistance() < (surfacePosition ? 80000 : 320000))
    {
        return true;
    }
    else if (countFailure)
    {
        m_rangeFails++;
        setStat(RANGE_FAILS, m_rangeFails);
    }
    return false;
}

bool ADSBDemodTracker::decodeGlobalPosition(int f, const double cprLat[2], const double cprLong[2], const QDateTime cprTime[2], double& latitude, double& longitude, bool countFailure)
{
    // Calculate latitude
    const double dLatEven = 360.0/60.0;
    const double dLatOdd = 360.0/59.0;
    double latEven, latOdd;
    int ni, m;

    int j = std::floor(59.0f*cprLat[0] - 60.0f*cprLat[1] + 0.5);
    latEven = dLatEven * (modulus(j, 60) + cprLat[0]);
    // Southern hemisphere is in range 270-360, so adjust to -90-0
    if (latEven >= 270.0f) {
        latEven -= 360.0f;
    }
    latOdd = dLatOdd * (modulus(j, 59) + cprLat[1]);
    if (latOdd >= 270.0f) {
        latOdd -= 360.0f;
    }
    if (cprTime[0] >= cprTime[1]) {
        latitude = latEven;
    } else {
        latitude = latOdd;
    }
    if ((latitude <= 90.0) && (latitude >= -90.0))
    {
        // Check if both frames in same latitude zone
        int latEvenNL = cprNL(latEven);
        int latOddNL = cprNL(latOdd);

        if (latEvenNL == latOddNL)
        {
            // Calculate longitude
            if (!f)
            {
                ni = cprN(latEven, 0);
                m = std::floor(cprLong[0] * (latEvenNL - 1) - cprLong[1] * latEvenNL + 0.5f);
                longitude = (360.0f/ni) * (modulus(m, ni) + cprLong[0]);
            }
            else
            {
                ni = cprN(latOdd, 1);
                m = std::floor(cprLong[0] * (latOddNL - 1) - cprLong[1] * latOddNL + 0.5f);
                longitude = (360.0f/ni) * (modulus(m, ni) + cprLong[1]);
            }
            if (longitude > 180.0f) {
                longitude -= 360.0f;
            }
            return validateGlobalPosition(latitude, longitude, countFailure);
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

// Only valid if airborne within 180nm/333km (C.2.6.4) or 45nm for surface
bool ADSBDemodTracker::decodeLocalPosition(int f, double cprLat, double cprLong, bool onSurface, const QDateTime& dateTime, const ADSBAircraft *aircraft, double& latitude, double& longitude, bool countFailure)
{
    double refLatitude;
    double refLongitude;
    const bool useAircraftReference = hasUsableCprReference(aircraft, onSurface, dateTime);

    if (useAircraftReference)
    {
        refLatitude = aircraft->m_latitude;
        refLongitude = aircraft->m_longitude;
    }
    else if (aircraft->m_globalPosition)
    {
        // The globally anchored reference is stale (or the replay timestamp moved
        // backwards). Wait for another global pair instead of silently switching
        // to the receiver position and selecting a different CPR zone.
        return false;
    }
    else
    {
        refLatitude = m_azEl.getLocationSpherical().m_latitude;
        refLongitude = m_azEl.getLocationSpherical().m_longitude;
    }

    // Calculate latitude
    const double maxDeg = onSurface ? 90.0 : 360.0;
    const double dLatEven = maxDeg/60.0;
    const double dLatOdd = maxDeg/59.0;
    double dLat = f ? dLatOdd : dLatEven;

    int j = std::floor(refLatitude/dLat) + std::floor(modulus(refLatitude, dLat)/dLat - cprLat + 0.5);
    latitude = dLat * (j + cprLat);

    // Calculate longitude
    double dLong;
    int latNL = cprNL(latitude);
    if (f == 0)
    {
        if (latNL > 0) {
            dLong = maxDeg / latNL;
        } else {
            dLong = maxDeg;
        }
    }
    else
    {
        if ((latNL - 1) > 0) {
            dLong = maxDeg / (latNL - 1);
        } else {
            dLong = maxDeg;
        }
    }
    int m = std::floor(refLongitude/dLong) + std::floor(modulus(refLongitude, dLong)/dLong - cprLong + 0.5);
    longitude =  dLong * (m + cprLong);

    if (useAircraftReference)
    {
        // Doc 9871 C.2.6.10.3 applies this motion test only when the two
        // position reports are no more than 30 seconds apart.
        const qint64 age = aircraft->m_positionDateTime.msecsTo(dateTime);
        if (age <= 30000)
        {
            double maxDistanceNm;
            if (aircraft->m_positionOnSurface != onSurface) {
                maxDistanceNm = 2.5;
            } else {
                maxDistanceNm = onSurface ? 0.75 : 6.0;
            }

            const QGeoCoordinate reference(refLatitude, refLongitude);
            const QGeoCoordinate decoded(latitude, longitude);
            if (reference.distanceTo(decoded) >= maxDistanceNm * 1852.0) {
                return false;
            }
        }
        return true;
    }
    else
    {
        // Check position is within range of antenna
        return validateLocalPosition(latitude, longitude, onSurface, countFailure);
    }
}

void ADSBDemodTracker::decodeCpr(const QByteArray& data, int& f, double& latCpr, double& lonCpr) const
{
    f = (data[6] >> 2) & 1; // CPR odd/even frame - should alternate every 0.2s
    int latCprFix = ((data[6] & 3) << 15) | ((data[7] & 0xff) << 7) | ((data[8] >> 1) & 0x7f);
    int lonCprFix = ((data[8] & 1) << 16) | ((data[9] & 0xff) << 8) | (data[10] & 0xff);

    latCpr = latCprFix / 131072.0;
    lonCpr = lonCprFix / 131072.0;
}

// This is relative to pressure of 1013.25 - it isn't corrected according to aircraft baro setting - so we can appear underground
bool ADSBDemodTracker::decodeAltitude(const QByteArray& data, int& altFt) const
{
    int alt = ((data[5] & 0xff) << 4) | ((data[6] >> 4) & 0xf); // Altitude
    if (alt == 0) {
        return false;
    }
    int q = (alt & 0x10) != 0;
    int n = ((alt >> 1) & 0x7f0) | (alt & 0xf);  // Remove Q-bit

    if (q == 1) {
        altFt = n * ((alt & 0x10) ? 25 : 100) - 1000;
    } else {
        altFt = gillhamToFeet(n);
    }
    return true;
}

// Mode S pressure altitude reported in 100ft or 25ft increments
// Note that we can get Mode-S altitude when still on the ground
void ADSBDemodTracker::decodeModeSAltitude(const QByteArray& data, const QDateTime dateTime, ADSBAircraft *aircraft)
{
    int altitude = 0;   // Altitude in feet
    int altitudeCode = ((data[2] & 0x1f) << 8) | (data[3] & 0xff);

    if (altitudeCode & 0x40) // M bit indicates metres
    {
        int altitudeMetres = ((altitudeCode & 0x1f80) >> 1) | (altitudeCode & 0x3f);
        altitude = Units::metresToFeet(altitudeMetres);
    }
    else
    {
        // Remove M and Q bits
        int altitudeFix = ((altitudeCode & 0x1f80) >> 2) | ((altitudeCode & 0x20) >> 1) | (altitudeCode & 0xf);

        // Convert to feet
        if (altitudeCode & 0x10) {
            altitude = altitudeFix * 25 - 1000;
        } else {
            altitude = gillhamToFeet(altitudeFix);
        }
    }

    // We can frequently get false decodes of df 4 frames, so check altitude is reasonable, based on previous value
    // If greater than an unlikely climb rate, ignore, as we don't want the aircraft jumping around the 3D map
    bool altUnlikely = false;
    if (aircraft->m_altitudeValid)
    {
        int altDiff = abs(aircraft->m_altitude - altitude);
        if (altDiff > 500)
        {
            qint64 msecs = aircraft->m_altitudeDateTime.msecsTo(dateTime);
            float climbRate = altDiff / (msecs / (1000.0f * 60.0f));
            if (climbRate > 6000.0f)
            {
                altUnlikely = true;
                m_altFails++;
                setStat(ALT_FAILS, m_altFails);
            }
        }
    }
    if (!altUnlikely) {
        aircraft->setAltitude(altitude, false, dateTime, m_settings);
    }
}

void ADSBDemodTracker::decodeModeS(const QByteArray data, const QDateTime dateTime, int df, ADSBAircraft *aircraft)
{
    bool wasOnSurface = aircraft->m_onSurface;
    bool takenOff = false;

    int flightStatus = data[0] & 0x7;
    if ((flightStatus == 0) || (flightStatus == 2))
    {
        takenOff = wasOnSurface;
        aircraft->m_onSurface = false;
        aircraft->m_onSurfaceValid = true;
    }
    else if ((flightStatus == 1) || (flightStatus == 3))
    {
        aircraft->m_onSurface = true;
        aircraft->m_onSurfaceValid = true;
    }
    if (wasOnSurface != aircraft->m_onSurface)
    {
        // Can't mix CPR values used on surface and those that are airborne
        aircraft->m_cprValid[0] = false;
        aircraft->m_cprValid[1] = false;
    }

    if ((df == 4) || (df == 20))
    {
        decodeModeSAltitude(data, dateTime, aircraft);

        // Assume runway elevation is at first reported airboune altitude
        if (takenOff)
        {
            aircraft->m_runwayAltitude = aircraft->m_altitude;
            aircraft->m_runwayAltitudeValid = true;
        }
    }
    else if ((df == 5) || (df == 21))
    {
        // Squawk ident code
        int identCode = ((data[2] & 0x1f) << 8) | (data[3] & 0xff);
        int squawk = squawkDecode(identCode);
        // SPI (IDENT) is indicated in the flight status field, not the identity field
        bool spi = (flightStatus == 4) || (flightStatus == 5);

        if ((squawk != aircraft->m_squawk) || (spi != aircraft->m_spi))
        {
            aircraft->m_squawk = squawk;
            aircraft->m_spi = spi;
            if (spi) {
                aircraft->setCell(ADSB_COL_SQUAWK, QString("%1 IDENT").arg(aircraft->m_squawk, 4, 10, QLatin1Char('0')));
            } else {
                aircraft->setCell(ADSB_COL_SQUAWK, QString("%1").arg(aircraft->m_squawk, 4, 10, QLatin1Char('0')));
            }
        }
    }
}

// TCAS/ACAS active resolution advisory report - the 56-bit BDS 3,0 layout, which is
// carried in three different places: the MV field of a DF16 air-air reply (VDS=0x30),
// the MB field of a DF20/21 Comm-B reply, and the ME field of a DF17 TC=28 subtype 2
// RA broadcast. Byte 4 differs between them (VDS / BDS / TC+ST) but ARA, RACs, RAT,
// MTE, TTI and TID sit at the same offsets in all three, so one decoder serves them all
void ADSBDemodTracker::decodeRABroadcast(const QByteArray data, const QDateTime dateTime, ADSBAircraft *aircraft)
{
    unsigned activeRAs = ((data[5] & 0xff) << 6) | ((data[6] >> 2) & 0x3f);     // ARA, bits 41-54
    unsigned racs = ((data[6] & 0x3) << 2) | ((data[7] >> 6) & 0x3);
    bool raTerminated = (data[7] >> 5) & 1;
    bool multipleThreats = (data[7] >> 4) & 1;
    unsigned threatType = (data[7] >> 2) & 0x3;

    // ARA bit 41 selects the coding of bits 42-47; it is not by itself a
    // one-threat indication. With MTE set, it distinguishes same-sense RAs
    // (bit 41 set) from opposite-sense RAs (bit 41 clear).
    const bool singleOrSameSenseRA = (activeRAs >> 13) & 0x1;
    QString threats;
    if (multipleThreats) {
        threats = singleOrSameSenseRA ? "Multiple threats (same-sense RAs)" : "Multiple threats (opposite-sense RAs)";
    } else {
        threats = singleOrSameSenseRA ? "One threat" : "No threats";
    }

    QStringList racStrings;
    if (racs & 1) {
        racStrings.append("Do not turn right");
    }
    if (racs & 2) {
        racStrings.append("Do not turn left");
    }
    if (racs & 4) {
        racStrings.append("Do not pass above");
    }
    if (racs & 8) {
        racStrings.append("Do not pass below");
    }

    unsigned threatICAO = 0;
    QString threatData;
    if (threatType == 1)
    {
        threatICAO = ((data[7] & 0x3) << 22) | ((data[8] & 0xff) << 14) | ((data[9] & 0xff) << 6) | ((data[10] >> 2) & 0x3f);
        threatData = QString::number(threatICAO, 16);
    }

    if (raTerminated)
    {
        aircraft->setCell(ADSB_COL_RA, "RA terminated");
    }
    else
    {
        QStringList s;
        s.append(threats);
        s.append(racStrings.join(""));
        s.append(threatData);
        aircraft->setCell(ADSB_COL_RA, s.join(" "));
    }

    // Typed state for the map's PFD/ND. With ARA bit 41 set, bits 42-47
    // use the single/same-sense coding (bit 42 corrective, 43 downward sense,
    // 47 positive rather than a vertical speed limit). With bit 41 clear and
    // MTE set, bits 42-45 independently describe upward/downward corrections.
    const bool active = (singleOrSameSenseRA || multipleThreats) && !raTerminated;
    aircraft->m_raOppositeSense = multipleThreats && !singleOrSameSenseRA;
    if (singleOrSameSenseRA)
    {
        aircraft->m_raCorrective = (activeRAs >> 12) & 1;   // ARA bit 42
        aircraft->m_raDownward = (activeRAs >> 11) & 1;     // ARA bit 43
        aircraft->m_raPositive = (activeRAs >> 7) & 1;      // ARA bit 47
    }
    else if (multipleThreats)
    {
        const bool requiresClimb = (activeRAs >> 12) & 1;   // ARA bit 42
        const bool positiveClimb = (activeRAs >> 11) & 1;   // ARA bit 43
        const bool requiresDescend = (activeRAs >> 10) & 1; // ARA bit 44
        const bool positiveDescend = (activeRAs >> 9) & 1;  // ARA bit 45
        aircraft->m_raCorrective = requiresClimb || requiresDescend;
        aircraft->m_raDownward = requiresDescend && !requiresClimb;
        aircraft->m_raPositive = requiresClimb != requiresDescend
            ? (requiresClimb ? positiveClimb : positiveDescend)
            : false;
    }
    aircraft->m_raActive = active;
    aircraft->m_raTerminated = raTerminated;
    aircraft->m_raMultipleThreats = multipleThreats;
    aircraft->m_raThreatIcao = threatICAO;
    aircraft->m_raDateTime = dateTime;
    if (active) {
        aircraft->m_tcasAlert = true;
        aircraft->setCell(ADSB_COL_TCAS, "RA");
    }
}

void ADSBDemodTracker::decodeCommB(const QByteArray data, const QDateTime dateTime, int df, ADSBAircraft *aircraft, bool &updatedCallsign)
{
    // We only see downlink messages, so do not know the data format, so have to decode all possibilities
    // and then see which have legal values and values that are consistent with ADS-B data

    // All IFR aircraft should support ELS (Elementary Surveillance) which includes BDS 1,0 1,7 2,0 3,0
    // Large aircraft are also required to support EHS (Enhanced Surveillance) which adds BDS 4,0 5,0 6,0
    // There is also MRAR (Meteorological Routine Air Report) BDS 4,4 4,5, but only a small % of aircraft support this
    // See: https://www.icao.int/APAC/Documents/edocs/Mode%20S%20DAPs%20Implementation%20and%20Operations%20Guidance%20Document3.0.pdf

    // I've implemented a few extra BDSes, but these are rarely seen.
    // Figure 2 in "Mode S Transponder Comm-B Capabilities in Current Operational ADSBAircraft" gives a breakdown of what capabilities are reported by BDS 1,7:
    // https://www.researchgate.net/publication/346521949_Mode_S_Transponder_Comm-B_Capabilities_in_Current_Operational_Aircraft/link/5fc60b794585152e9be8571c/download

    // Skip messages that are all zeros
    if (data[4] || data[5] || data[6] || data[7] || data[8] || data[9] || data[10])
    {

        const int maxWind = 250; // Maximum expected head/tail wind in knts
        const int maxSpeedDiff = 50; // Maximum speed difference we allow before we assume message is inconsistent
        const int maxAlt = 46000; // Maximum expected altitude for commercial jet
        const float maxHeadingDiff = 20.0f; // Maximum difference in heading/track

        // BDS 0,5 - Extended squitter airborne position (tc=11 typically seen in DF16)

        int altitude_0_5;
        bool altitudeValid_0_5 = decodeAltitude(data, altitude_0_5);
        // Require an altitude having been received by other means, otherwise too high chance of it being incorrect
        bool altitudeInconsistent_0_5 = !altitudeValid_0_5 || (aircraft->m_altitudeValid && isAltitudeInconsistent(altitude_0_5, aircraft->m_altitude));

        const unsigned tc = ((data[4] >> 3) & 0x1f);
        bool tcInconsistent = !((tc == 0) || ((tc >= 5) && (tc <= 8)) || ((tc >= 9) && (tc <= 18)) || ((tc >= 20) && (tc <= 22))); // Only position type codes

        int f;
        double latCpr, lonCpr;
        decodeCpr(data, f, latCpr, lonCpr);
        bool cprValid[2] =  {f ? aircraft->m_cprValid[0] : true, f ? true : aircraft->m_cprValid[1]};
        double cprLat[2] = {f ? aircraft->m_cprLat[0] : latCpr, f ? latCpr : aircraft->m_cprLat[1]};
        double cprLon[2] = {f ? aircraft->m_cprLong[0] : lonCpr, f ? lonCpr : aircraft->m_cprLong[1]};
        QDateTime cprTime[2] = {f ? aircraft->m_cprTime[0] : dateTime, f ? dateTime : aircraft->m_cprTime[1]};
        double latitude_0_5, longitude_0_5;
        double positionValid_0_5 = false;
        if (cprValid[0] && cprValid[1]
            && (std::abs(cprTime[0].toMSecsSinceEpoch() - cprTime[1].toMSecsSinceEpoch()) <= 8500)
            && !aircraft->m_onSurface)
        {
            if (decodeGlobalPosition(f, cprLat, cprLon, cprTime, latitude_0_5, longitude_0_5, false)) {
                positionValid_0_5 = true;
            }
        }
        else
        {
            if (decodeLocalPosition(f, cprLat[f], cprLon[f], aircraft->m_onSurface, dateTime, aircraft, latitude_0_5, longitude_0_5, false)) {
                positionValid_0_5 = true;
            }
        }
        // Require a position having been received by other means, otherwise too high chance of it being incorrect
        bool positionInconsistent_0_5 = !positionValid_0_5 || (aircraft->m_positionValid && isPositionInconsistent(aircraft->m_latitude, aircraft->m_longitude, latitude_0_5, longitude_0_5));

        bool bds_0_5 = ((tc >= 9) && (tc <= 18)) && !positionInconsistent_0_5 && !altitudeInconsistent_0_5 && !tcInconsistent;

        // BDS 0,8 - ADSBAircraft identification and category

        QString emitterCategory, callsign_0_8;

        decodeID(data, emitterCategory, callsign_0_8);

        bool emitterCategoryInconsistent = (emitterCategory == "Reserved") || (emitterCategory != aircraft->m_emitterCategory);
        bool callsignInconsistent_0_8 = callsign_0_8.isEmpty() || callsign_0_8.contains('#'); // Callsign can change between flights

        bool bds_0_8 = ((tc >= 1) && (tc <= 4)) && !emitterCategoryInconsistent && !callsignInconsistent_0_8;

        // BDS 0,9 - Airborne Velocity

        int st = data[4] & 0x7;   // Subtype
        bool groundspeedSubType = (st == 1) || (st == 2);
        bool airspeedSubType = (st == 3) || (st == 4);
        float groundspeed_0_9 = 0.0f, track = 0.0f;
        bool airspeedType_0_9 = false;
        int airspeed = 0;
        bool headingValid = false;
        float heading = 0.0f;
        bool groundspeedValid_0_9 = false;
        bool airspeedValid_0_9 = false;
        if (groundspeedSubType)
        {
            // Ground speed
            groundspeedValid_0_9 = decodeGroundspeed(data, groundspeed_0_9, track);
        }
        else if (airspeedSubType)
        {
            // Airspeed
            airspeedValid_0_9 = decodeAirspeed(data, airspeedType_0_9, airspeed, headingValid, heading);
        }
        int verticalRate = 0;
        bool verticalRateValid_0_9 = decodeVerticalRate(data, verticalRate);

        bool groundspeedInconsistent = groundspeedSubType
            && groundspeedValid_0_9
            && ((aircraft->m_groundspeedValid && isSpeedInconsistent(groundspeed_0_9, aircraft->m_groundspeed))
                || (aircraft->m_trackValid && isHeadingInconsistent(track, aircraft->m_track)));
        bool airspeedInconsistent = airspeedSubType
            && ((airspeedValid_0_9
                    && (airspeedType_0_9 ? aircraft->m_trueAirspeedValid : aircraft->m_indicatedAirspeedValid)
                    && isSpeedInconsistent(airspeed, airspeedType_0_9 ? aircraft->m_trueAirspeed : aircraft->m_indicatedAirspeed))
                || (headingValid && aircraft->m_headingValid && isHeadingInconsistent(heading, aircraft->m_heading)));
        bool verticalRateInconsistent = verticalRateValid_0_9 && aircraft->m_verticalRateValid && isVerticalRateInconsistent(verticalRate, aircraft->m_verticalRate);

        // Require at least one valid subfield, so a frame that happens to have subtype bits 1-4 but
        // no usable content can't consume the single-match slot for other registers
        bool bds_0_9 = (tc == 19)
            && (groundspeedSubType || airspeedSubType)
            && (groundspeedValid_0_9 || airspeedValid_0_9 || (airspeedSubType && headingValid) || verticalRateValid_0_9)
            && !groundspeedInconsistent && !airspeedInconsistent && !verticalRateInconsistent;

        // BDS 1,0 - ELS

        bool bds_1_0 = ((data[4] & 0xff) == 0x10) && ((data[5] & 0x7c) == 0x00);

        // BDS 1,7 - Common usage GICB capability report - ELS

        bool cap_0_5 = (data[4] >> 7) & 0x1;
        bool cap_0_6 = (data[4] >> 6) & 0x1;
        bool cap_0_7 = (data[4] >> 5) & 0x1;
        bool cap_0_8 = (data[4] >> 4) & 0x1;
        bool cap_0_9 = (data[4] >> 3) & 0x1;
        bool cap_0_a = (data[4] >> 2) & 0x1;
        bool cap_2_0 = (data[4] >> 1) & 0x1;
        bool cap_2_1 = (data[4] >> 0) & 0x1;

        bool cap_4_0 = (data[5] >> 7) & 0x1;
        bool cap_4_1 = (data[5] >> 6) & 0x1;
        bool cap_4_2 = (data[5] >> 5) & 0x1;
        bool cap_4_3 = (data[5] >> 4) & 0x1;
        bool cap_4_4 = (data[5] >> 3) & 0x1;
        bool cap_4_5 = (data[5] >> 2) & 0x1;
        bool cap_4_8 = (data[5] >> 1) & 0x1;
        bool cap_5_0 = (data[5] >> 0) & 0x1;

        bool cap_5_1 = (data[6] >> 7) & 0x1;
        bool cap_5_2 = (data[6] >> 6) & 0x1;
        bool cap_5_3 = (data[6] >> 5) & 0x1;
        bool cap_5_4 = (data[6] >> 4) & 0x1;
        bool cap_5_5 = (data[6] >> 3) & 0x1;
        bool cap_5_6 = (data[6] >> 2) & 0x1;
        bool cap_5_f = (data[6] >> 1) & 0x1;
        bool cap_6_0 = (data[6] >> 0) & 0x1;

        QStringList caps;
        if (cap_0_5) {
            caps.append("0,5");
        }
        if (cap_0_6) {
            caps.append("0,6");
        }
        if (cap_0_7) {
            caps.append("0,7");
        }
        if (cap_0_8) {
            caps.append("0,8");
        }
        if (cap_0_9) {
            caps.append("0,9");
        }
        if (cap_0_a) {
            caps.append("0,A");
        }
        if (cap_2_0) {
            caps.append("2,0");
        }
        if (cap_2_1) {
            caps.append("2,1");
        }
        if (cap_4_0) {
            caps.append("4,0");
        }
        if (cap_4_1) {
            caps.append("4,1");
        }
        if (cap_4_2) {
            caps.append("4,2");
        }
        if (cap_4_3) {
            caps.append("4,3");
        }
        if (cap_4_4) {
            caps.append("4,4");
        }
        if (cap_4_5) {
            caps.append("4,5");
        }
        if (cap_4_8) {
            caps.append("4,8");
        }
        if (cap_5_0) {
            caps.append("5,0");
        }
        if (cap_5_1) {
            caps.append("5,1");
        }
        if (cap_5_2) {
            caps.append("5,2");
        }
        if (cap_5_3) {
            caps.append("5,3");
        }
        if (cap_5_4) {
            caps.append("5,4");
        }
        if (cap_5_5) {
            caps.append("5,5");
        }
        if (cap_5_6) {
            caps.append("5,6");
        }
        if (cap_5_f) {
            caps.append("5,F");
        }
        if (cap_6_0) {
            caps.append("6,0");
        }

        bool bds_1_7 = cap_2_0 && (data[7] == 0x0) && (data[8] == 0x0) && (data[9] == 0x0) && (data[10] == 0x0);

        // BDS 1,8 1,9 1,A 1,B 1,C 1,D 1,E 1,F Mode S specific services

        // Apart from 1,C and 1,F these can have any bits set/clear and specify BDS code capabilities
        // Don't decode for now

        // BDS 2,0 - ADSBAircraft identification - ELS

        // Flight/callsign - Extract 8 6-bit characters from 6 8-bit bytes, MSB first
        unsigned char c[9];
        char callsignASCII[9];
        c[0] = (data[5] >> 2) & 0x3f; // 6
        c[1] = ((data[5] & 0x3) << 4) | ((data[6] & 0xf0) >> 4);  // 2+4
        c[2] = ((data[6] & 0xf) << 2) | ((data[7] & 0xc0) >> 6);  // 4+2
        c[3] = (data[7] & 0x3f); // 6
        c[4] = (data[8] >> 2) & 0x3f;
        c[5] = ((data[8] & 0x3) << 4) | ((data[9] & 0xf0) >> 4);
        c[6] = ((data[9] & 0xf) << 2) | ((data[10] & 0xc0) >> 6);
        c[7] = (data[10] & 0x3f);
        // Map to ASCII
        for (int i = 0; i < 8; i++) {
            callsignASCII[i] = m_idMap[c[i]];
        }
        callsignASCII[8] = '\0';
        QString callsignTrimmed = QString(callsignASCII).trimmed();
        bool invalidCallsign = QString(callsignASCII).contains('#');
        bool bds_2_0 = ((data[4] & 0xff) == 0x20) && !invalidCallsign;

        // BDS 2,1 - ADSBAircraft and airline registration markings

        bool aircraftRegistrationStatus = (data[4] >> 7) & 0x1;
        char aircraftRegistration[8];
        c[0] = (data[4] >> 1) & 0x3f;
        c[1] = ((data[4] & 0x1) << 5) | ((data[5] >> 3) & 0x1f);
        c[2] = ((data[5] & 0x7) << 3) | ((data[6] >> 5) & 0x7);
        c[3] = ((data[6] & 0x1f) << 1) | ((data[7] >> 7) & 0x1);
        c[4] = ((data[7] >> 1) & 0x1f);
        c[5] = ((data[7] & 0x1) << 5) | ((data[8] >> 3) & 0x1f);
        c[6] = ((data[8] & 0x7) << 3) | ((data[9] >> 5) & 0x7);
        // Map to ASCII
        for (int i = 0; i < 7; i++) {
            aircraftRegistration[i] = m_idMap[c[i]];
        }
        aircraftRegistration[7] = '\0';
        QString aircraftRegistrationString = QString(aircraftRegistration).trimmed();
        bool aircraftRegistrationInvalid = QString(aircraftRegistrationString).contains('#');
        bool aircraftRegistrationInconsistent = !aircraftRegistrationStatus && (c[0] || c[1] || c[2] || c[3] || c[4] || c[5] || c[6]);

        bool airlineRegistrationStatus = (data[9] >> 4) & 0x1;
        char airlineRegistration[3];
        c[0] = ((data[9] & 0xf) << 2) | ((data[10] >> 6) & 0x3);
        c[1] = data[10] & 0x3f;
        // Map to ASCII
        for (int i = 0; i < 2; i++) {
            airlineRegistration[i] = m_idMap[c[i]];
        }
        airlineRegistration[2] = '\0';
        QString airlineRegistrationString = QString(airlineRegistration).trimmed();
        bool airlineRegistrationInvalid = QString(airlineRegistrationString).contains('#') || !QChar(QLatin1Char(airlineRegistration[0])).isLetter() || !QChar(QLatin1Char(airlineRegistration[1])).isLetter();
        bool airlineRegistrationInconsistent = !airlineRegistrationStatus && (c[0] || c[1]);

        bool bds_2_1 = !aircraftRegistrationInvalid && !aircraftRegistrationInconsistent && !airlineRegistrationInvalid && !airlineRegistrationInconsistent;

        // BDS 3,0 - ACAS active resolution advisory - ELS

        int acas = data[6] & 0x7f;
        int threatType = (data[7] >> 2) & 0x3;
        bool bds_3_0 = ((data[4] & 0xff) == 0x30) && (acas < 48) && (threatType != 3);

        // BDS 4,0 - Selected vertical information - EHS

        bool mcpSelectedAltStatus = (data[4] >> 7) & 0x1;
        int mcpSelectedAltFix = ((data[4] & 0x7f) << 5) | ((data[5] >> 3) & 0x1f);
        int mcpSelectedAlt = mcpSelectedAltFix * 16; // ft
        bool mcpSelectedAltInconsistent = (mcpSelectedAlt > maxAlt) || (!mcpSelectedAltStatus && (mcpSelectedAltFix != 0));

        bool fmsSelectedAltStatus = (data[5] >> 2) & 0x1;
        int fmsSelectedAltFix = ((data[5] & 0x3) << 10) | ((data[6] & 0xff) << 2) | ((data[7] >> 6) & 0x3);
        int fmsSelectedAlt = fmsSelectedAltFix * 16; // ft
        bool fmsSelectedAltInconsistent = (fmsSelectedAlt > maxAlt) || (!fmsSelectedAltStatus && (fmsSelectedAltFix != 0));

        bool baroSettingStatus = (data[7] >> 5) & 0x1;
        int baroSettingFix = ((data[7] & 0x1f) << 7) | ((data[8] >> 1) & 0x7f);
        float baroSetting = baroSettingFix * 0.1f + 800.0f; // mb
        bool baroSettingIncosistent = !baroSettingStatus && (baroSettingFix != 0);

        bool modeStatus = data[9] & 0x1;
        bool vnavMode = (data[10] >> 7) & 0x1;
        bool altHoldMode = (data[10] >> 6) & 0x1;
        bool approachMode = (data[10] >> 5) & 0x1;
        bool modeInconsistent = !modeStatus && (vnavMode || altHoldMode || approachMode);

        bool targetAltSourceStatus = (data[10] >> 2) & 0x1;
        int targetAltSource = data[10] & 0x3;
        bool targetAltSourceInconsistent = !targetAltSourceStatus && (targetAltSource != 0);

        bool bds_4_0 = ((data[8] & 0x01) == 0x0) && ((data[9] & 0xfe) == 0x0) && ((data[10] & 0x18) == 0x0)
                    && !mcpSelectedAltInconsistent && !fmsSelectedAltInconsistent && !baroSettingIncosistent && !modeInconsistent && !targetAltSourceInconsistent;

        // BDS 4,1 - Next waypoint

        bool waypointStatus = (data[4] >> 7) & 0x1;
        char waypoint[10];
        c[0] = (data[4] >> 1) & 0x3f;
        c[1] = ((data[4] & 0x1) << 5) | ((data[5] >> 3) & 0x1f);
        c[2] = ((data[5] & 0x7) << 3) | ((data[6] >> 5) & 0x7);
        c[3] = ((data[6] & 0x1f) << 1) | ((data[7] >> 7) & 0x1);
        c[4] = ((data[7] >> 1) & 0x1f);
        c[5] = ((data[7] & 0x1) << 5) | ((data[8] >> 3) & 0x1f);
        c[6] = ((data[8] & 0x7) << 3) | ((data[9] >> 5) & 0x7);
        c[7] = ((data[9] & 0x1f) << 1) | ((data[10] >> 7) & 0x1);
        c[8] = ((data[10] >> 1) & 0x3f);
        // Map to ASCII
        for (int i = 0; i < 9; i++) {
            waypoint[i] = m_idMap[c[i]];
        }
        waypoint[9] = '\0';
        QString waypointString = QString(waypoint).trimmed();
        bool waypointInvalid = QString(waypointString).contains('#');
        bool waypointInconsistent =   (waypointString.size() != 5) // Most navaid waypoints are 5 characters, and this prevents some incorrect matches, but is it correct? Need examples.
                                   || (!waypointStatus && (c[0] || c[1] || c[2] || c[3] || c[4] || c[5] || c[6] || c[7] || c[8]));

        bool bds_4_1 = !waypointInvalid && !waypointInconsistent;

        // BDS 4,4 - Meteorological routine air report - MRAR - (Appendix E suggests a slightly different format in the future)

        int fomSource = (data[4] >> 4) & 0xf;
        bool fomSourceInconsistent = fomSource > 4;

        bool windSpeedStatus = (data[4] >> 3) & 0x1;
        int windSpeedFix = ((data[4] & 0x7) << 6) | ((data[5] >> 2) & 0x3f);
        int windSpeed = windSpeedFix; // knots
        int windDirectionFix = ((data[5] & 0x3) << 7) | ((data[6] >> 1) & 0x7f);
        int windDirection = windDirectionFix * 180.0f / 256.0f; // Degrees
        bool windSpeedInconsistent = (windSpeed > 250.0f) || (!windSpeedStatus && ((windSpeedFix != 0) || (windDirectionFix != 0)));

        int staticAirTemperatureFix = ((data[6] & 0x1) << 10) | ((data[7] & 0xff) << 2) | ((data[8] >> 6) & 0x3);
        staticAirTemperatureFix = (staticAirTemperatureFix << 21) >> 21;
        float staticAirTemperature = staticAirTemperatureFix * 0.25f;
        bool staticAirTemperatureInconsistent = (staticAirTemperature < -80.0f) || (staticAirTemperature > 60.0f);

        bool averageStaticPressureStatus = (data[8] >> 5) & 0x1;
        int averageStaticPressureFix = ((data[8] & 0x1f) << 6) | ((data[9] >> 2) & 0x3f);
        int averageStaticPressure = averageStaticPressureFix; // hPa
        bool averageStaticPressureInconsistent = !averageStaticPressureStatus && (averageStaticPressureFix != 0);

        bool turbulenceStatus = (data[9] >> 1) & 0x1;
        int turbulence = ((data[9] & 0x1) << 1) | ((data[10] >> 7) & 0x1);
        bool turbulenceInconsistent = !turbulenceStatus && (turbulence != 0);

        bool humidityStatus = (data[10] >> 6) & 0x1;
        int humidityFix = data[10] & 0x3f;
        float humidity = humidityFix * 100.0f / 64.0f; // %
        bool humidityInconsistent = (humidity > 100.0) || (!humidityStatus && (humidityFix != 0));

        // Occasionally get frames: 20000000000000 or 08000000000000 - these seem unlikely to be BDS 4,4
        bool noMetData = (((data[4] & 0xff) == 0x20) || ((data[4] & 0xff) == 0x08)) && !(data[5] || data[6] || data[7] || data[8] || data[9] || data[10]);

        bool bds_4_4 = !noMetData && !fomSourceInconsistent && !windSpeedInconsistent && !staticAirTemperatureInconsistent && !averageStaticPressureInconsistent && !turbulenceInconsistent && !humidityInconsistent;

        // BDS 4,5 - Meteorological hazard report - MRAR - (Appendix E suggests a slightly different format in the future)

        bool hazardTurbulenceStatus = (data[4] >> 7) & 0x1;
        int hazardTurbulence = (data[4] >> 5) & 0x3;
        bool hazardTurbulenceInconsistent = !hazardTurbulenceStatus && (hazardTurbulence != 0);

        bool hazardWindShearStatus = (data[4] >> 4) & 0x1;
        int hazardWindShear = (data[4] >> 2) & 0x3;
        bool hazardWindShearInconsistent = !hazardWindShearStatus && (hazardWindShear != 0);

        bool hazardMicroburstStatus = (data[4] >> 1) & 0x1;
        int hazardMicroburst = ((data[4] & 0x1) << 1) | ((data[5] >> 7) & 0x1);
        bool hazardMicroburstInconsistent = !hazardMicroburstStatus && (hazardMicroburst != 0);

        bool hazardIcingStatus = (data[5] >> 6) & 0x1;
        int hazardIcing = ((data[5] >> 4) & 0x3);
        bool hazardIcingInconsistent = !hazardIcingStatus && (hazardIcing != 0);

        bool hazardWakeVortexStatus = (data[5] >> 3) & 0x1;
        int hazardWakeVortex = ((data[5] >> 1) & 0x3);
        bool hazardWakeVortexInconsistent = !hazardWakeVortexStatus && (hazardWakeVortex != 0);

        bool hazardStaticAirTemperatureStatus = data[5] & 0x1;
        int hazardStaticAirTemperatureFix = ((data[6] & 0xff) << 2) | ((data[7] >> 6) & 0x3);
        hazardStaticAirTemperatureFix = (hazardStaticAirTemperatureFix << 22) >> 22;
        float hazardStaticAirTemperature = hazardStaticAirTemperatureFix * 0.25f; // deg C
        bool hazardStaticAirTemperatureInconsistent = (hazardStaticAirTemperature < -80.0f) || (hazardStaticAirTemperature > 60.0f) || (!hazardStaticAirTemperatureStatus && (hazardStaticAirTemperatureFix != 0));

        bool hazardAverageStaticPressureStatus = (data[7] >> 5) & 0x1;
        int hazardAverageStaticPressureFix = ((data[7] & 0x1f) << 6) | ((data[8] >> 2) & 0x3f);
        int hazardAverageStaticPressure = hazardAverageStaticPressureFix; // hPa
        bool hazardAverageStaticPressureInconsistent = !hazardAverageStaticPressureStatus && (hazardAverageStaticPressureFix != 0);

        bool hazardRadioHeightStatus = (data[8] >> 1) & 0x1;
        int hazardRadioHeightFix = ((data[8] & 0x1) << 11) | ((data[9] & 0xff) << 3) | ((data[10] >> 5) & 0x7);
        int hazardRadioHeight = hazardRadioHeightFix * 16; // Ft
        bool hazardRadioHeightInconsistent = (hazardRadioHeight > maxAlt) || (!aircraft->m_onSurface && (hazardRadioHeight == 0)) || (!hazardRadioHeightStatus && (hazardRadioHeightFix != 0));

        bool hazardReserveredInconsistent = (data[10] & 0x1f) != 0;

        bool harzardIcingTempInconsistent = hazardIcingStatus && hazardStaticAirTemperatureStatus && (hazardIcing != 0) && ((hazardStaticAirTemperature >= 20.0f) || (hazardStaticAirTemperature <= -40.0f));

        bool bds_4_5 = !hazardTurbulenceInconsistent && !hazardWindShearInconsistent && !hazardMicroburstInconsistent && !hazardIcingInconsistent
                    && !hazardWakeVortexInconsistent && !hazardStaticAirTemperatureInconsistent && !hazardAverageStaticPressureInconsistent
                    && !hazardRadioHeightInconsistent && !hazardReserveredInconsistent && !harzardIcingTempInconsistent;

        // BDS 5,0 - Track and turn report - EHS

        bool rollAngleStatus = (data[4] >> 7) & 0x1;
        int rollAngleFix = ((data[4] & 0x7f) << 3) | ((data[5] >> 5) & 0x7);
        rollAngleFix = (rollAngleFix << 22) >> 22;
        float rollAngle = rollAngleFix * (45.0f/256.0f);
        bool rollAngleInvalid = (rollAngle < -50.0f) || (rollAngle > 50.0f); // More than 50 deg bank unlikely for airliners
        bool rollAngleInconsistent = ((abs(rollAngle) >= 1.0f) && aircraft->m_onSurface) || (!rollAngleStatus && (rollAngleFix != 0));

        bool trueTrackAngleStatus = (data[5] >> 4) & 0x1;
        int trueTrackAngleFix = ((data[5] & 0xf) << 7) | ((data[6] >> 1) & 0x7f);
        trueTrackAngleFix = (trueTrackAngleFix << 21) >> 21;
        float trueTrackAngle = trueTrackAngleFix * (90.0f/512.0f);
        if (trueTrackAngle < 0.0f) {
            trueTrackAngle += 360.0f;
        }
        bool trueTrackAngleInconsistent =  (aircraft->m_trackValid && (abs(trueTrackAngle - aircraft->m_track) > maxHeadingDiff)) // FIXME: compare to heading or track?
                                        || (!trueTrackAngleStatus && (trueTrackAngleFix != 0));

        bool groundSpeedStatus = data[6] & 0x1;
        int groundSpeedFix = ((data[7] & 0xff) << 2) | ((data[8] >> 6) & 0x3);
        int groundSpeed = groundSpeedFix * 2;
        bool groundSpeedInconsistent =    ((groundSpeed > 800) && (aircraft->m_emitterCategory != "High performance"))
                                       || (aircraft->m_groundspeedValid && (abs(aircraft->m_groundspeed - groundSpeed) > maxSpeedDiff))
                                       || (!groundSpeedStatus && (groundSpeedFix != 0));

        bool trackAngleRateStatus = (data[8] >> 5) & 0x1;
        int trackAngleRateFix = ((data[8] & 0x1f) << 5) | ((data[9] >> 3) & 0x1f);
        trackAngleRateFix = (trackAngleRateFix << 22) >> 22;
        float trackAngleRate = trackAngleRateFix * (8.0f/256.0f);
        bool trackAngleRateInconsistent = !trackAngleRateStatus && (trackAngleRateFix != 0);

        bool trueAirspeedStatus = (data[9] >> 2) & 0x1;
        int trueAirspeedFix = ((data[9] & 0x3) << 8) | (data[10] & 0xff);
        int trueAirspeed = trueAirspeedFix * 2;
        bool trueAirspeedInconsistent =    ((trueAirspeed > 575) && (aircraft->m_emitterCategory != "High performance"))
                                        || (aircraft->m_groundspeedValid && (abs(aircraft->m_groundspeed - trueAirspeed) > maxWind+maxSpeedDiff))
                                        || (!trueAirspeedStatus && (trueAirspeedFix != 0));

        bool headwindStatus = groundSpeedStatus && trueAirspeedStatus;
        int headwind = trueAirspeed - groundSpeed;
        bool headwindInconsistent = abs(headwind) > maxWind;

        bool bds_5_0 = !rollAngleInvalid && (!rollAngleInconsistent && !trueTrackAngleInconsistent && !groundSpeedInconsistent && !trackAngleRateInconsistent && !trueAirspeedInconsistent && !headwindInconsistent);

        // BDS 5,1 - Position report coarse

        bool positionValid = (data[4] >> 7) & 0x1;

        int latitudeFix = ((data[4] & 0x7f) << 13) | ((data[5] & 0xff) << 5) | ((data[6] >> 3) & 0x1f);
        latitudeFix = (latitudeFix << 12) >> 12;
        float latitude_5_1 = latitudeFix * (360.0f / 1048576.0f);

        int longitudeFix = ((data[6] & 0x7) << 17) | ((data[7] & 0xff) << 9) | ((data[8] & 0xff) << 1) | ((data[9] >> 7) & 0x1);
        longitudeFix = (longitudeFix << 12) >> 12;
        float longitude_5_1 = longitudeFix * (360.0f / 1048576.0f);

        bool positionInconsistent = !aircraft->m_positionValid
                                || (positionValid && aircraft->m_positionValid && isPositionInconsistent(latitude_5_1, longitude_5_1, aircraft->m_latitude, aircraft->m_longitude))
                                || (!positionValid && ((latitudeFix != 0) || (longitudeFix != 0)));

        int pressureAltFix = ((data[9] & 0x7f) << 8) | (data[10] & 0xff);
        pressureAltFix = (pressureAltFix << 17) >> 17;
        int pressureAlt = pressureAltFix * 8;
        bool pressureAltInconsistent = (pressureAlt > 50000) || (pressureAlt < -1000) || (positionValid && aircraft->m_altitudeValid && isAltitudeInconsistent(pressureAlt, aircraft->m_altitude))
                                    || (!positionValid && (pressureAltFix != 0));

        bool bds_5_1 = !positionInconsistent && !pressureAltInconsistent;

        // BDS 5,3 - Air-referenced state vector

        bool arMagHeadingStatus = (data[4] >> 7) & 1;
        int arMagHeadingFix = ((data[4] & 0x7f) << 4) | ((data[5] >> 4) & 0xf);
        float arMagHeading = arMagHeadingFix * 90.0f / 512.0f;
        bool arMagHeadingInconsistent =    (aircraft->m_headingValid && arMagHeadingStatus && (abs(aircraft->m_heading - arMagHeading) > maxHeadingDiff))
                                        || (!arMagHeadingStatus && (arMagHeadingFix != 0));

        bool arIndicatedAirspeedStatus = (data[5] >> 3) & 0x1;
        int arIndicatedAirspeedFix = ((data[5] & 0x7) << 7) | ((data[6] >> 1) & 0x7f);
        int arIndicatedAirspeed = arIndicatedAirspeedFix; // knots
        bool arIndicatedAirspeedInconsistent =    ((arIndicatedAirspeed > 550) && (aircraft->m_emitterCategory != "High performance"))
                                               || (aircraft->m_groundspeedValid && (abs(aircraft->m_groundspeed - arIndicatedAirspeed) > maxWind+maxSpeedDiff))
                                               || (aircraft->m_indicatedAirspeedValid && (abs(aircraft->m_indicatedAirspeed - arIndicatedAirspeed) > maxSpeedDiff))
                                               || (arIndicatedAirspeedStatus && (arIndicatedAirspeed < 100) && !aircraft->m_onSurface && (aircraft->m_emitterCategory != "Light"))
                                               || (!arIndicatedAirspeedStatus && (arIndicatedAirspeedFix != 0));

        bool arMachStatus = data[6] & 0x1;
        int arMachFix = ((data[7] & 0xff) << 1) | ((data[8] >> 7) & 0x1);
        float arMach = arMachFix * 0.008f;
        bool arMachInconsistent =  ((arMach >= 1.0f) && (aircraft->m_emitterCategory != "High performance"))
                                || (!arMachStatus && (arMachFix != 0));

        bool arTrueAirspeedStatus = (data[8] >> 6) & 0x1;
        int arTureAirspeedFix = ((data[8] & 0x3f) << 6) | ((data[9] >> 2) & 0x3f);
        float arTrueAirspeed = arTureAirspeedFix * 0.5f; // knots
        bool arTrueAirspeedInconsistent =   ((arTrueAirspeed > 550) && (aircraft->m_emitterCategory != "High performance"))
                                         || (aircraft->m_groundspeedValid && (abs(aircraft->m_groundspeed - arTrueAirspeed) > maxWind+maxSpeedDiff))
                                         || (aircraft->m_trueAirspeedValid && (abs(aircraft->m_trueAirspeed - arTrueAirspeed) > maxSpeedDiff))
                                         || (arTrueAirspeedStatus && (arTureAirspeedFix < 100) && !aircraft->m_onSurface && (aircraft->m_emitterCategory != "Light"))
                                         || (!arTrueAirspeedStatus && (arTureAirspeedFix != 0));

        bool arAltitudeRateStatus = (data[9] >> 1) & 0x1;
        int arAltitudeRateFix = ((data[9] & 0x1) << 8) | (data[10] & 0xff);
        arAltitudeRateFix = (arAltitudeRateFix << 23) >> 23; // Sign extend 9-bit two's complement value
        int arAltitudeRate = arAltitudeRateFix * 64; // Ft/min
        bool arAltitudeRateInconsistent = (abs(arAltitudeRate) > 6000) || (!arAltitudeRateStatus && (arAltitudeRateFix != 0));

        bool bds_5_3 = !arMagHeadingInconsistent && !arIndicatedAirspeedInconsistent && !arMachInconsistent && !arTrueAirspeedInconsistent && !arAltitudeRateInconsistent;

        // BDS 6,0 - Heading and speed report - EHS

        bool magHeadingStatus = (data[4] >> 7) & 1;
        int magHeadingFix = ((data[4] & 0x7f) << 4) | ((data[5] >> 4) & 0xf);
        magHeadingFix = (magHeadingFix << 21) >> 21;
        float magHeading = magHeadingFix * (90.0f / 512.0f);
        if (magHeading < 0.0f) {
            magHeading += 360.f;
        }
        bool magHeadingInconsistent =    (aircraft->m_headingValid && magHeadingStatus && (abs(aircraft->m_heading - magHeading) > maxHeadingDiff))
                                      || (!magHeadingStatus && (magHeadingFix != 0));

        bool indicatedAirspeedStatus = (data[5] >> 3) & 0x1;
        int indicatedAirspeedFix = ((data[5] & 0x7) << 7) | ((data[6] >> 1) & 0x7f);
        int indicatedAirspeed = indicatedAirspeedFix;
        bool indicatedAirspeedInconsistent =   ((indicatedAirspeed > 550) && (aircraft->m_emitterCategory != "High performance"))
                                            || (aircraft->m_groundspeedValid && (abs(aircraft->m_groundspeed - indicatedAirspeed) > maxWind+maxSpeedDiff))
                                            || (aircraft->m_indicatedAirspeedValid && (abs(aircraft->m_indicatedAirspeed - indicatedAirspeed) > maxSpeedDiff))
                                            || (indicatedAirspeedStatus && (indicatedAirspeed < 100) && !aircraft->m_onSurface && (aircraft->m_emitterCategory != "Light"))
                                            || (!indicatedAirspeedStatus && (indicatedAirspeedFix != 0));

        bool machStatus = data[6] & 0x1;
        int machFix = ((data[7] & 0xff) << 2) | ((data[8] >> 6) & 0x3);
        float mach = machFix * 2.048f / 512.0f;
        bool machInconsistent = ((mach >= 1.0f) && (aircraft->m_emitterCategory != "High performance"))
                                || (!machStatus && (machFix != 0));

        bool baroAltRateStatus = (data[8] >> 5) & 0x1;
        int baroAltRateFix = ((data[8] & 0x1f) << 5) | ((data[9] >> 3) & 0x1f);
        baroAltRateFix = (baroAltRateFix << 22) >> 22;
        int baroAltRate = baroAltRateFix * 32; // ft/min
        bool baroAltRateInconsistent = (abs(baroAltRate) > 6000) || (aircraft->m_verticalRateValid && abs(baroAltRate - aircraft->m_verticalRate) > 2000)
                                    || (!baroAltRateStatus && (baroAltRateFix != 0));

        bool verticalVelStatus = (data[9] >> 2) & 0x1;
        int verticalVelFix = ((data[9] & 0x3) << 8) | (data[10] & 0xff);
        verticalVelFix = (verticalVelFix << 22) >> 22;
        int verticalVel = verticalVelFix * 32; // ft/min
        bool verticalVelInconsistent = (abs(verticalVel) > 6000) || (aircraft->m_verticalRateValid && abs(verticalVel - aircraft->m_verticalRate) > 2000)
                                    || (!verticalVelStatus && (verticalVelFix != 0));

        bool bds_6_0 = !magHeadingInconsistent && !indicatedAirspeedInconsistent && !machInconsistent && !baroAltRateInconsistent && !verticalVelInconsistent;

        int possibleMatches = bds_0_5 + bds_0_8 + bds_0_9 + bds_1_0 + bds_1_7 + bds_2_0 + bds_2_1 + bds_3_0 + bds_4_0 + bds_4_1 + bds_4_4 + bds_4_5 + bds_5_0 + bds_5_1 + bds_5_3 + bds_6_0;

        if (possibleMatches == 1)
        {
            if (bds_0_5)
            {
                // Only use this data if position and altitude where compared against a previous position, otherwise there's a high chance it may be wrong
                if (aircraft->m_altitudeValid && aircraft->m_positionValid)
                {
                    if (altitudeValid_0_5) {
                        aircraft->setAltitude(altitude_0_5, (tc >= 20) && (tc <= 22), dateTime, m_settings);
                    }
                    if (positionValid_0_5) {
                        updateAircraftPosition(aircraft, latitude_0_5, longitude_0_5, dateTime);
                    }
                    if (altitudeValid_0_5 && positionValid_0_5) {
                        aircraft->m_positionChanged = true;
                    }
                }
            }
            if (bds_0_8)
            {
                updatedCallsign = aircraft->m_callsign != callsign_0_8;
                if (updatedCallsign) {
                    setCallsign(aircraft, callsign_0_8);
                }
            }
            if (bds_0_9)
            {
                if (groundspeedSubType)
                {
                    if (groundspeedValid_0_9)
                    {
                        aircraft->setGroundspeed(groundspeed_0_9, m_settings);
                        aircraft->setTrack(track, dateTime);
                    }
                }
                else
                {
                    if (airspeedValid_0_9)
                    {
                        if (airspeedType_0_9) {
                            aircraft->setTrueAirspeed(airspeed, m_settings);
                        } else {
                            aircraft->setIndicatedAirspeed(airspeed, dateTime, m_settings);
                        }
                    }
                    if (headingValid) {
                        aircraft->setHeading(heading, dateTime);
                    }
                }
                if (verticalRateValid_0_9) {
                    aircraft->setVerticalRate(verticalRate, m_settings);
                }
            }
            if (bds_1_7)
            {
                // Some of these bits are dynamic, so can't assume that because a bit isn't set,
                // that message is not ever supported
                aircraft->m_bdsCapabilitiesValid = true;
                aircraft->m_bdsCapabilities[0][5] |= cap_0_5;
                aircraft->m_bdsCapabilities[0][6] |= cap_0_6;
                aircraft->m_bdsCapabilities[0][7] |= cap_0_7;
                aircraft->m_bdsCapabilities[0][8] |= cap_0_8;
                aircraft->m_bdsCapabilities[0][9] |= cap_0_9;
                aircraft->m_bdsCapabilities[0][10] |= cap_0_a;
                aircraft->m_bdsCapabilities[2][0] |= cap_2_0;
                aircraft->m_bdsCapabilities[2][1] |= cap_2_1;
                aircraft->m_bdsCapabilities[4][0] |= cap_4_0;
                aircraft->m_bdsCapabilities[4][1] |= cap_4_1;
                aircraft->m_bdsCapabilities[4][2] |= cap_4_2;
                aircraft->m_bdsCapabilities[4][3] |= cap_4_3;
                aircraft->m_bdsCapabilities[4][4] |= cap_4_4;
                aircraft->m_bdsCapabilities[4][5] |= cap_4_5;
                aircraft->m_bdsCapabilities[4][8] |= cap_4_8;
                aircraft->m_bdsCapabilities[5][0] |= cap_5_0;
                aircraft->m_bdsCapabilities[5][1] |= cap_5_1;
                aircraft->m_bdsCapabilities[5][2] |= cap_5_2;
                aircraft->m_bdsCapabilities[5][3] |= cap_5_3;
                aircraft->m_bdsCapabilities[5][4] |= cap_5_4;
                aircraft->m_bdsCapabilities[5][5] |= cap_5_5;
                aircraft->m_bdsCapabilities[5][6] |= cap_5_6;
                aircraft->m_bdsCapabilities[5][15] |= cap_5_f;
                aircraft->m_bdsCapabilities[6][0] |= cap_6_0;
            }
            if (bds_2_0)
            {
                updatedCallsign = aircraft->m_callsign != callsignTrimmed;
                if (updatedCallsign) {
                    setCallsign(aircraft, callsignTrimmed);
                }
            }
            if (bds_2_1)
            {
                qDebug() << "BDS 2,1 - "
                        << "ICAO:" << aircraft->m_icaoHex
                        << "aircraftRegistration:" << aircraftRegistrationString
                        << "airlineRegistration:" << airlineRegistrationString
                        << "m_bdsCapabilities[2][1]: " << aircraft->m_bdsCapabilities[2][1]
                        << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;

                if (!aircraftRegistrationString.isEmpty()) {
                    aircraft->setCell(ADSB_COL_REGISTRATION, aircraftRegistrationString);
                }
            }
            if (bds_3_0)
            {
                decodeRABroadcast(data, dateTime, aircraft);
            }
            if (bds_4_0)
            {
                // Could use targetAltSource here, but yet to see it set to anything other than unknown
                if (mcpSelectedAltStatus)
                {
                    aircraft->m_selAltitude = mcpSelectedAlt;
                    aircraft->m_selAltitudeValid = true;
                    if (m_settings.m_siUnits) {
                        aircraft->setCell(ADSB_COL_SEL_ALTITUDE, Units::feetToIntegerMetres(aircraft->m_selAltitude));
                    } else {
                        aircraft->setCell(ADSB_COL_SEL_ALTITUDE, roundTo50Feet(aircraft->m_selAltitude));
                    }
                }
                else if (fmsSelectedAltStatus)
                {
                    aircraft->m_selAltitude = fmsSelectedAlt;
                    aircraft->m_selAltitudeValid = true;
                    if (m_settings.m_siUnits) {
                        aircraft->setCell(ADSB_COL_SEL_ALTITUDE, Units::feetToIntegerMetres(aircraft->m_selAltitude));
                    } else {
                        aircraft->setCell(ADSB_COL_SEL_ALTITUDE, roundTo50Feet(aircraft->m_selAltitude));
                    }
                }
                else
                {
                    aircraft->m_selAltitude = 0;
                    aircraft->m_selAltitudeValid = false;
                    aircraft->setCell(ADSB_COL_SEL_ALTITUDE, "");
                }

                if (baroSettingStatus)
                {
                    aircraft->m_baro = baroSetting;
                    aircraft->m_baroValid = true;
                    aircraft->setCell(ADSB_COL_BARO, std::round(aircraft->m_baro));
                    updateQNH(aircraft, baroSetting);
                }

                if (modeStatus)
                {
                    QString mode = "";
                    if (vnavMode) {
                        mode = mode + "VNAV ";
                    }
                    if (altHoldMode) {
                        mode = mode + "HOLD ";
                    }
                    if (approachMode) {
                        mode = mode + "APP ";
                    }
                    mode = mode.trimmed();
                    aircraft->setCell(ADSB_COL_V_MODE, mode);

                    aircraft->m_vnavMode = vnavMode;
                    aircraft->m_vnavModeValid = true;
                    aircraft->m_altHoldMode = altHoldMode;
                    aircraft->m_altHoldModeValid = true;
                    aircraft->m_approachMode = approachMode;
                    aircraft->m_approachModeValid = true;
                }
            }
            if (bds_4_1)
            {
                qDebug() << "BDS 4,1 - "
                        << "ICAO:" << aircraft->m_icaoHex
                        << "waypoint:" << waypointString
                        << "m_bdsCapabilities[4][1]: " << aircraft->m_bdsCapabilities[4][1]
                        << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;
            }
            if (bds_4_4)
            {
                if (fomSource != 0) // Ignore FOM "invalid"
                {
                    if (windSpeedStatus)
                    {
                        if (m_settings.m_siUnits) {
                            aircraft->setCell(ADSB_COL_WIND_SPEED, Units::knotsToIntegerKPH(windSpeed));
                        } else {
                            aircraft->setCell(ADSB_COL_WIND_SPEED, windSpeed);
                        }
                        aircraft->setCell(ADSB_COL_WIND_DIR, windDirection);
                        // Not clear if air temp depends on the previous status bit
                        aircraft->setCell(ADSB_COL_STATIC_AIR_TEMP, (int)std::round(staticAirTemperature));
                    }
                    if (averageStaticPressureStatus) {
                        aircraft->setCell(ADSB_COL_STATIC_PRESSURE, averageStaticPressure);
                    }
                    if (humidityStatus) {
                        aircraft->setCell(ADSB_COL_HUMIDITY, (int)std::round(humidity));
                    }
                }
            }
            if (bds_4_5)
            {
                qDebug() << "BDS 4,5 - "
                        << "ICAO:" << aircraft->m_icaoHex
                        << "hazardTurbulence:" << m_hazardSeverity[hazardTurbulence]
                        << "hazardWindShear:" << m_hazardSeverity[hazardWindShear]
                        << "hazardMicroburst:" << m_hazardSeverity[hazardMicroburst]
                        << "hazardIcing:" << m_hazardSeverity[hazardIcing]
                        << "hazardWakeVortex:" << m_hazardSeverity[hazardWakeVortex]
                        << "hazardStaticAirTemperature:" << hazardStaticAirTemperature << "C"
                        << "hazardAverageStaticPressure:" << hazardAverageStaticPressure << "hPA"
                        << "hazardRadioHeight:" << hazardRadioHeight << "ft"
                        << "(ADSBAircraft Alt: " << aircraft->m_altitude << "ft)"
                        << "m_bdsCapabilities[4][5]: " << aircraft->m_bdsCapabilities[4][5]
                        << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;
                        ;
            }
            if (bds_5_0)
            {
                if (rollAngleStatus)
                {
                    aircraft->m_roll = rollAngle;
                    aircraft->m_rollValid = true;
                    aircraft->setCell(ADSB_COL_ROLL, std::round(aircraft->m_roll));
                }
                if (trueTrackAngleStatus)
                {
                    clearOldHeading(aircraft, dateTime, trueTrackAngle);
                    aircraft->setTrack(trueTrackAngle, dateTime);
                }
                if (groundSpeedStatus)
                {
                    aircraft->setGroundspeed(groundSpeed, m_settings);
                }
                if (trackAngleRateStatus)
                {
                    aircraft->m_turnRate = trackAngleRate;
                    aircraft->m_turnRateValid = true;
                    aircraft->setCell(ADSB_COL_TURNRATE, std::round(aircraft->m_turnRate*10.0f)/10.0f);
                }
                if (trueAirspeedStatus)
                {
                    aircraft->m_trueAirspeed = trueAirspeed;
                    aircraft->m_trueAirspeedValid = true;
                    if (m_settings.m_siUnits) {
                        aircraft->setCell(ADSB_COL_TRUE_AIRSPEED, Units::knotsToIntegerKPH(aircraft->m_trueAirspeed));
                    } else {
                        aircraft->setCell(ADSB_COL_TRUE_AIRSPEED, aircraft->m_trueAirspeed);
                    }
                }
                if (headwindStatus)
                {
                    if (m_settings.m_siUnits) {
                        aircraft->setCell(ADSB_COL_HEADWIND, Units::knotsToIntegerKPH(headwind));
                    } else {
                        aircraft->setCell(ADSB_COL_HEADWIND, headwind);
                    }
                }
            }
            if (bds_5_1)
            {
                // Position is specified as "coarse" (lsb = 90/131072 deg which is ~76m. CPR accuracy is 5.1m)
                /*qDebug() << "BDS 5,1 - "
                        << "ICAO:" << aircraft->m_icaoHex
                        << "latitude:" << latitude_5_1 << "(ADS-B:" << aircraft->m_latitude << ") "
                        << "longitude:" << longitude_5_1  << "(ADS-B:" << aircraft->m_longitude << ") "
                        << "pressureAlt:" << pressureAlt  << "(ADS-B:" << aircraft->m_altitude << ")"
                        << "m_bdsCapabilities[5][1]: " << aircraft->m_bdsCapabilities[5][1]
                        << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;
                        ;*/
            }
            if (bds_5_3)
            {
                qDebug() << "BDS 5,3 - "
                        << "ICAO:" << aircraft->m_icaoHex
                        << "arMagHeading:" << arMagHeading << "(ADS-B:" << aircraft->m_heading << ") "
                        << "arIndicatedAirspeed:" << arIndicatedAirspeed << "knts" << "(ADS-B GS: " << aircraft->m_groundspeed << ") "
                        << "arMach:" << arMach
                        << "arTrueAirspeed:" << arTrueAirspeed << "knts"
                        << "arAltitudeRate:" << arAltitudeRate << "ft/min"
                        << "m_bdsCapabilities[5][3]: " << aircraft->m_bdsCapabilities[5][3]
                        << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;
                        ;
            }
            if (bds_6_0)
            {
                if (arMagHeadingStatus) {
                    aircraft->setHeading(magHeading, dateTime);
                }
                if (indicatedAirspeedStatus)
                {
                    aircraft->m_indicatedAirspeed = indicatedAirspeed;
                    aircraft->m_indicatedAirspeedValid = true;
                    if (m_settings.m_siUnits) {
                        aircraft->setCell(ADSB_COL_INDICATED_AIRSPEED, Units::knotsToIntegerKPH(aircraft->m_indicatedAirspeed));
                    } else {
                        aircraft->setCell(ADSB_COL_INDICATED_AIRSPEED, aircraft->m_indicatedAirspeed);
                    }
                    aircraft->m_indicatedAirspeedDateTime = dateTime;
                }
                if (machStatus)
                {
                    aircraft->m_mach = mach;
                    aircraft->m_machValid = true;
                    aircraft->setCell(ADSB_COL_MACH, aircraft->m_mach);
                }
                if (verticalVelStatus) {
                    aircraft->setVerticalRate(verticalVel, m_settings);
                }
            }

            if (bds_5_0 || bds_6_0) {
                calcAirTemp(aircraft);
            }
        }
        else if (false) // Enable to debug multiple matching frames
        {
            qDebug() << "DF" << df
                    << "matches" << possibleMatches
                    << "bds_0_5" << bds_0_5
                    << "bds_0_8" << bds_0_8
                    << "bds_0_9" << bds_0_9
                    << "bds_1_0" << bds_1_0
                    << "bds_1_7" << bds_1_7
                    << "bds_2_0" << bds_2_0
                    << "bds_2_1" << bds_2_1
                    << "bds_3_0" << bds_3_0
                    << "bds_4_0" << bds_4_0
                    << "bds_4_1" << bds_4_1
                    << "bds_4_4" << bds_4_4
                    << "bds_4_5" << bds_4_5
                    << "bds_5_0" << bds_5_0
                    << "bds_5_1" << bds_5_1
                    << "bds_5_3" << bds_5_3
                    << "bds_6_0" << bds_6_0
                    ;

            qDebug() << data.toHex();

            qDebug() << (bds_0_5 ? "+" : "-")
                     << "BDS 0,5 - "
                     << "altitude:" << altitude_0_5 << "(ADS-B:" << aircraft->m_altitude << ") "
                     << "latitude:" << latitude_0_5 << "(ADS-B:" << aircraft->m_latitude << ") "
                     << "longitude:" << longitude_0_5 << "(ADS-B:" << aircraft->m_longitude << ") ";
            qDebug() << (bds_0_8 ? "+" : "-")
                     << "BDS 0,8 - "
                     << "emitterCategory:" << emitterCategory << "(ADS-B:" << aircraft->m_emitterCategory << ") "
                     << "callsign:" << callsign_0_8 << "(ADS-B:" << aircraft->m_callsign << ") ";
            qDebug() << (bds_0_9 ? "+" : "-")
                     << "BDS 0,9 - "
                     << "groundspeedSubType:" << groundspeedSubType
                     << "speed:" << (groundspeedSubType ? groundspeed_0_9 : airspeed) << "(ADS-B:" << (groundspeedSubType ? aircraft->m_groundspeed : (airspeedType_0_9 ? aircraft->m_trueAirspeed : aircraft->m_indicatedAirspeed)) << ") "
                     << "verticalRate:" << verticalRate << "(ADS-B:" << aircraft->m_verticalRate << ") ";
            qDebug() << (bds_1_0 ? "+" : "-")
                     << "BDS 1,0 - ";
            qDebug() << (bds_1_7 ? "+" : "-")
                     << "BDS 1,7 - "
                     << "ICAO:" << aircraft->m_icaoHex
                     << "BDS:" << caps.join(" ");
            qDebug() << (bds_2_0 ? "+" : "-")
                     << "BDS 2,0 - "
                     << "ICAO:" << aircraft->m_icaoHex
                     << "Callsign:" << callsignTrimmed << "(ADS-B:" << aircraft->m_callsign << ") ";
            qDebug() << (bds_2_1 ? "+" : "-")
                     << "BDS 2,1 - "
                     << "ICAO:" << aircraft->m_icaoHex
                     << "aircraftRegistration:" << aircraftRegistrationString
                     << "airlineRegistration:" << airlineRegistrationString
                     << "m_bdsCapabilities[2][1]: " << aircraft->m_bdsCapabilities[2][1]
                     << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;
            qDebug() << (bds_3_0 ? "+" : "-")
                     << "BDS 3,0 - ";
            qDebug() << (bds_4_0 ? "+" : "-")
                     << "BDS 4,0 - "
                     << "ICAO:" << aircraft->m_icaoHex
                     << "mcpSelectedAlt:" << mcpSelectedAlt << "ft"
                     << "fmsSelectedAlt:" << fmsSelectedAlt << "ft"
                     << "baroSetting:" << (baroSettingStatus ? QString::number(baroSetting) : "invalid") << (baroSettingStatus ? "mb" : "")
                     << "vnav: " << vnavMode
                     << "altHold: " << altHoldMode
                     << "approach: " << approachMode
                     << "targetAltSource:" << targetAltSource
                     << "m_bdsCapabilities[4][0]: " << aircraft->m_bdsCapabilities[4][0]
                     << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;
            qDebug() << (bds_4_1 ? "+" : "-")
                     << "BDS 4,1 - "
                     << "ICAO:" << aircraft->m_icaoHex
                     << "waypoint:" << waypointString
                     << "m_bdsCapabilities[4][1]: " << aircraft->m_bdsCapabilities[4][1]
                     << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;
            qDebug() << (bds_4_4 ? "+" : "-")
                     << "BDS 4,4 - "
                     << "ICAO:" << aircraft->m_icaoHex
                     << "fomSource:" << m_fomSources[fomSource]
                     << "windSpeed:" << windSpeed << "knts"
                     << "windDirection:" << windDirection << "deg"
                     << "staticAirTemperature:" << staticAirTemperature << "C"
                     << "averageStaticPressure:" << averageStaticPressure << "hPa"
                     << "turbulence:" << turbulence
                     << "humidity:" << humidity << "%"
                     << "m_bdsCapabilities[4][4]: " << aircraft->m_bdsCapabilities[4][4]
                     << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;
            qDebug() << (bds_4_5 ? "+" : "-")
                     << "BDS 4,5 - "
                     << "ICAO:" << aircraft->m_icaoHex
                     << "hazardTurbulence:" << m_hazardSeverity[hazardTurbulence]
                     << "hazardWindShear:" << m_hazardSeverity[hazardWindShear]
                     << "hazardMicroburst:" << m_hazardSeverity[hazardMicroburst]
                     << "hazardIcing:" << m_hazardSeverity[hazardIcing]
                     << "hazardWakeVortex:" << m_hazardSeverity[hazardWakeVortex]
                     << "hazardStaticAirTemperature:" << hazardStaticAirTemperature << "C"
                     << "hazardAverageStaticPressure:" << hazardAverageStaticPressure << "hPA"
                     << "hazardRadioHeight:" << hazardRadioHeight << "ft"
                     << "m_bdsCapabilities[4][5]: " << aircraft->m_bdsCapabilities[4][5]
                     << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;
            qDebug() << (bds_5_0 ? "+" : "-")
                     << "BDS 5,0 - "
                     << "ICAO:" << aircraft->m_icaoHex
                     << "rollAngle:" << rollAngle
                     << "trueTrackAngle:" << trueTrackAngle
                     << "trackAngleRate: " << trackAngleRate
                     << "groundSpeed:" << groundSpeed
                     << "trueAirspeed:" << trueAirspeed
                     << "headwind:" << headwind
                     << "(ADS-B GS: " << aircraft->m_groundspeed << ")"
                     << "(ADS-B TAS: " << aircraft->m_trueAirspeed << ")"
                     << "(ADS-B heading:" << aircraft->m_heading << ")"
                     << "m_bdsCapabilities[5][0]: " << aircraft->m_bdsCapabilities[5][0]
                     << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;
            qDebug() << (bds_5_1 ? "+" : "-")
                     << "BDS 5,1 - "
                     << "ICAO:" << aircraft->m_icaoHex
                     << "latitude:" << latitude_5_1 << "(ADS-B:" << aircraft->m_latitude << ") "
                     << "longitude:" << longitude_5_1  << "(ADS-B:" << aircraft->m_longitude << ") "
                     << "pressureAlt:" << pressureAlt  << "(ADS-B:" << aircraft->m_altitude << ")"
                     << "m_bdsCapabilities[5][1]: " << aircraft->m_bdsCapabilities[5][1]
                     << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;
            qDebug() << (bds_5_3 ? "+" : "-")
                     << "BDS 5,3 - "
                     << "ICAO:" << aircraft->m_icaoHex
                     << "arMagHeading:" << arMagHeading << "(ADS-B:" << aircraft->m_heading << ") "
                     << "arIndicatedAirspeed:" << arIndicatedAirspeed << "knts" << "(ADS-B GS: " << aircraft->m_groundspeed << ") "
                     << "arMach:" << arMach
                     << "arTrueAirspeed:" << arTrueAirspeed << "knts"
                     << "arAltitudeRate:" << arAltitudeRate << "ft/min"
                     << "arMagHeadingInconsistent" << arMagHeadingInconsistent << "arIndicatedAirspeedInconsistent" << arIndicatedAirspeedInconsistent << "arMachInconsistent" << arMachInconsistent << "arTrueAirspeedInconsistent" << arTrueAirspeedInconsistent
                     << "m_bdsCapabilities[5][3]: " << aircraft->m_bdsCapabilities[5][3]
                     << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;
            qDebug() << (bds_6_0 ? "+" : "-")
                     << "BDS 6,0 - "
                     << "ICAO:" << aircraft->m_icaoHex
                     << "magHeading:" << magHeading << "(ADS-B:" << aircraft->m_heading << ") "
                     << "indicatedAirspeed:" << indicatedAirspeed << "knts" << "(ADS-B GS: " << aircraft->m_groundspeed << ") "
                     << "mach:" << mach
                     << "baroAltRate:" << baroAltRate << "ft/min"
                     << "verticalVel:" << verticalVel << "ft/min"
                     << "magHeadingInconsistent " << magHeadingInconsistent << "indicatedAirspeedInconsistent" << indicatedAirspeedInconsistent << "machInconsistent" << machInconsistent << "baroAltRateInconsistent" << baroAltRateInconsistent << "verticalVelInconsistent" << verticalVelInconsistent
                     << "m_bdsCapabilities[6][0]: " << aircraft->m_bdsCapabilities[6][0]
                     << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;
        }
    }
}

// The animations the Bluebell models were built with (X-Plane datarefs), and the
// helicopter's. Asked of a model only if it has them - see modelHasAnimation()
static const QString ANIMATION_GEAR = "libxplanemp/controls/gear_ratio";
static const QString ANIMATION_GEAR_ANGLE = "libxplanemp/gear/tire_vertical_deflection_mtr";
static const QString ANIMATION_FLAPS = "libxplanemp/controls/flap_ratio";
static const QString ANIMATION_SLATS = "libxplanemp/controls/slat_ratio";
static const QString ANIMATION_ROTOR = "Take 001";
static const QString ANIMATION_ENGINE = "libxplanemp/engines/engine_rotation_angle_deg%1";   // %1 is the engine number

bool ADSBDemodTracker::modelHasAnimation(const ADSBAircraft *aircraft, const QString &animation) const
{
    // The same choice of model as when the item is sent to the map
    const QString &model = !aircraft->m_aircraft3DModel.isEmpty() ? aircraft->m_aircraft3DModel : aircraft->m_aircraftCat3DModel;
    return Aircraft3DModels::instance().hasAnimation(model, animation);
}

// Height above the airport an aircraft is arriving at, which is what decides when the
// gear and flaps come out. m_runwayAltitude only ever covers a departure we watched -
// it is captured from the first airborne altitude and cleared again on climb out - so
// an arrival has no reference at all without this, and the animation had to guess from
// groundspeed, which carries the wind with it.
//
// Airports are bucketed by whole degree so the search looks at a handful rather than
// the eighty thousand in the database. Built once, on first use.
static QHash<int, QList<const AirportInformation *>> *airportGrid()
{
    static QHash<int, QList<const AirportInformation *>> grid;
    static bool built = false;

    if (!built)
    {
        built = true;
        QSharedPointer<const QHash<int, AirportInformation *>> airports = OurAirportsDB::getAirportsById();

        if (airports)
        {
            for (auto i = airports->cbegin(); i != airports->cend(); ++i)
            {
                const AirportInformation *airport = i.value();

                // A heliport can be on a building or a rig, so its elevation says
                // nothing about the ground an aeroplane is about to land on
                if (!airport || (airport->m_type == AirportInformation::Heliport)) {
                    continue;
                }
                const int key = (((int) std::floor(airport->m_latitude)) + 90) * 1000
                              + ((int) std::floor(airport->m_longitude)) + 180;
                grid[key].append(airport);
            }
        }
        qDebug() << "ADSBDemodTracker: airport elevation grid holds" << grid.size() << "cells";
    }

    return &grid;
}

// Sets m_fieldElevation to the elevation of the nearest airport, when the aircraft is
// low enough for it to matter. Kept on the aircraft and refreshed rarely: the elevation
// of whatever is underneath changes slowly, and this runs for every aircraft on every
// position report
static void updateFieldElevation(ADSBAircraft *aircraft, const QDateTime& dateTime)
{
    const int lookupCeiling = 10000;    // Feet, above which an arrival is not imminent
    const float searchRange = 20.0f;    // Nautical miles
    const int refreshSeconds = 60;

    if (!aircraft->m_positionValid || !aircraft->m_altitudeValid) {
        return;
    }

    // Too high to be landing anywhere near here, and a cruising aircraft passing over
    // airports should not pick up their elevations
    if (aircraft->m_altitude > lookupCeiling)
    {
        aircraft->m_fieldElevationValid = false;
        return;
    }

    if (aircraft->m_fieldElevationTime.isValid()
        && (aircraft->m_fieldElevationTime.secsTo(dateTime) < refreshSeconds)) {
        return;
    }
    aircraft->m_fieldElevationTime = dateTime;

    const QHash<int, QList<const AirportInformation *>> *grid = airportGrid();
    const int latDegree = (int) std::floor(aircraft->m_latitude);
    const int lonDegree = (int) std::floor(aircraft->m_longitude);
    // Nautical miles per degree of longitude shrinks towards the poles
    const double lonScale = 60.0 * std::cos(aircraft->m_latitude * M_PI / 180.0);

    const AirportInformation *nearest = nullptr;
    float nearestRange = searchRange;

    // The neighbouring cells as well as the one it is in, so an airport just over a
    // degree boundary is not missed
    for (int dLat = -1; dLat <= 1; dLat++)
    {
        for (int dLon = -1; dLon <= 1; dLon++)
        {
            const int key = ((latDegree + dLat) + 90) * 1000 + (lonDegree + dLon) + 180;
            const auto cell = grid->find(key);

            if (cell == grid->cend()) {
                continue;
            }
            for (const AirportInformation *airport : *cell)
            {
                const double north = (airport->m_latitude - aircraft->m_latitude) * 60.0;
                const double east = (airport->m_longitude - aircraft->m_longitude) * lonScale;
                const float range = (float) std::sqrt((north * north) + (east * east));

                if (range < nearestRange)
                {
                    nearestRange = range;
                    nearest = airport;
                }
            }
        }
    }

    if (nearest)
    {
        aircraft->m_fieldElevation = nearest->m_elevation;
        aircraft->m_fieldElevationValid = true;
    }
    else
    {
        aircraft->m_fieldElevationValid = false;
    }
}

QList<SWGSDRangel::SWGMapAnimation *> * ADSBDemodTracker::animate(QDateTime dateTime, ADSBAircraft *aircraft)
{
    QList<SWGSDRangel::SWGMapAnimation *> *animations = new QList<SWGSDRangel::SWGMapAnimation *>();

    // What this aircraft's model can play. An animation it lacks would still have been
    // sent - engines are re-asserted every few minutes - for the map to catch and log,
    // for the whole session. The state below is tracked regardless, so an aircraft whose
    // model cannot show its gear still knows where its gear is.
    const bool hasGear = modelHasAnimation(aircraft, ANIMATION_GEAR);
    const bool hasGearAngle = modelHasAnimation(aircraft, ANIMATION_GEAR_ANGLE);
    const bool hasFlaps = modelHasAnimation(aircraft, ANIMATION_FLAPS);
    const bool hasSlats = modelHasAnimation(aircraft, ANIMATION_SLATS);
    const bool hasRotor = modelHasAnimation(aircraft, ANIMATION_ROTOR);
    const bool hasEngine1 = modelHasAnimation(aircraft, ANIMATION_ENGINE.arg(1));
    const bool hasEngine2 = modelHasAnimation(aircraft, ANIMATION_ENGINE.arg(2));

    updateFieldElevation(aircraft, dateTime);

    // Height above whatever the aircraft is flying over, when that is known. For a
    // departure the runway altitude is exact; for an arrival the nearest airport is as
    // close as we can get, and good to the few tens of feet that matter here
    bool heightValid = false;
    int height = 0;

    if (aircraft->m_altitudeValid)
    {
        if (aircraft->m_runwayAltitudeValid)
        {
            height = aircraft->m_altitude - (int) aircraft->m_runwayAltitude;
            heightValid = true;
        }
        else if (aircraft->m_fieldElevationValid)
        {
            height = aircraft->m_altitude - (int) aircraft->m_fieldElevation;
            heightValid = true;
        }
    }

    const int gearDownHeight = 2000;            // Above the runway, as most operators fly it
    const int gearDownAirspeed = 180;           // Indicated, which is what a crew flies to
    const int gearDownSpeed = 150;              // Groundspeed, the last resort    
    const int gearDownVerticalRate = -300;      // A settled descent.
    const int gearUpAltitude = 200;
    const int gearUpVerticalRate = 1000;
    const int accelerationHeight = 1500;
    const int minPitchGroundspeed = 50;         // Below which a flight path angle means nothing
    const float ftPerMinutePerKnot = 101.269f;  // One knot, in feet per minute
    // An airliner flies a few degrees nose up of its own flight path - that is the angle
    // of attack, and it is what makes the wing lift. Nothing reported gives it, so a
    // single representative figure stands in: a jet sits at around 2.5 degrees in the
    // cruise and 5 in the climb. Without it the flight path angle alone leaves the models
    // and the attitude indicator looking flat, which is what the old estimate was
    // compensating for by treating the vertical rate as an attitude
    const float angleOfAttack = 3.0f;
    // Flown at Vref with the flaps out, an airliner sits at a noticeably higher body
    // angle than it does clean. Pitch is this plus the flight path angle, so on a three
    // degree glideslope it comes out around +1.5, which is what an approach looks like
    const float landingAngleOfAttack = 4.5f;
    const int flapsRetractAltitude = 2000;
    const int flapsCleanSpeed = 200;

    bool debug = false;

    // Landing gear should be down when on surface
    // Check speed in case we get a mixture of surface and airborne positions
    // during take-off
    if (   aircraft->m_onSurface
        && !aircraft->m_gearDown
        && (   (aircraft->m_groundspeedValid && (aircraft->m_groundspeed < 80))
            || !aircraft->m_groundspeedValid
           )
       )
    {
        if (debug) {
            qDebug() << "Gear down as on surface " << aircraft->m_icaoHex;
        }
        if (hasGear) {
            animations->append(gearAnimation(dateTime, false));
        }
        if (!aircraft->m_tyresFlat)
        {
            if (hasGearAngle) {
                animations->append(gearAngle(dateTime, true));
            }
            aircraft->m_tyresFlat = true;
        }
        aircraft->m_gearDown = true;
    }

    // Flaps when on the surface
    if (aircraft->m_onSurface && aircraft->m_groundspeedValid)
    {
        if ((aircraft->m_groundspeed <= 20) && (aircraft->m_flaps != 0.0))
        {
            // No flaps when stationary / taxiing
            if (debug) {
                qDebug() << "Parking flaps " << aircraft->m_icaoHex;
            }
            if (hasFlaps) {
                animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.0));
            }
            if (hasSlats) {
                animations->append(slatsAnimation(dateTime, true));
            }
            aircraft->m_flaps = 0.0;
        }
        else if ((aircraft->m_groundspeed >= 30) && (aircraft->m_flaps < 0.25))
        {
            // Flaps for takeoff
            if (debug) {
                qDebug() << "Takeoff flaps " << aircraft->m_icaoHex;
            }
            if (hasFlaps) {
                animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.25));
            }
            if (hasSlats) {
                animations->append(slatsAnimation(dateTime, false));
            }
            aircraft->m_flaps = 0.25;
        }
    }

    // Pitch up on take-off
    if (   aircraft->m_gearDown
        && !aircraft->m_onSurface
        && (   (aircraft->m_verticalRateValid && (aircraft->m_verticalRate > 300))
            || (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude > (aircraft->m_runwayAltitude + gearUpAltitude/2)))
           )
        )
    {
        if (debug) {
            qDebug() << "Pitch up " << aircraft->m_icaoHex;
        }
        aircraft->m_pitchEst = 5.0;
        // Only as the aircraft leaves the ground. This runs on every update while it
        // climbs slower than the rate that retracts the gear, so without the state the
        // animation was asked for several times a second for as long as that lasted -
        // one aircraft asked 2,485 times in 16 minutes. Every request that reaches a
        // loaded model stays on it, and the map walks them all each frame
        if (aircraft->m_tyresFlat)
        {
            if (hasGearAngle) {
                animations->append(gearAngle(dateTime, false));
            }
            aircraft->m_tyresFlat = false;
        }
    }

    // Retract landing gear after take-off
    if (   aircraft->m_gearDown
        && !aircraft->m_onSurface
        && (   (aircraft->m_verticalRateValid && (aircraft->m_verticalRate > 1000))
            || (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude > (aircraft->m_runwayAltitude + gearUpAltitude)))
           )
        )
    {
        if (debug) {
            qDebug() << "Gear up " << aircraft->m_icaoHex
                    << " VR " << (aircraft->m_verticalRateValid && (aircraft->m_verticalRate > gearUpVerticalRate))
                    << " Alt " << (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude > (aircraft->m_runwayAltitude + gearUpAltitude)))
                    <<  "m_altitude " << aircraft->m_altitude   << " aircraft->m_runwayAltitude " << aircraft->m_runwayAltitude;
        }
        aircraft->m_pitchEst = 10.0;
        if (hasGear) {
            animations->append(gearAnimation(dateTime.addSecs(2), true));
        }
        aircraft->m_gearDown = false;
    }

    // Reduce pitch at acceleration height
    if (   (aircraft->m_flaps > 0.0)
        && (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude > (aircraft->m_runwayAltitude + accelerationHeight)))
       )
    {
        aircraft->m_pitchEst = 5.0;
    }

    // Retract flaps/slats after take-off
    // Should be after acceleration altitude (1500-3000ft AAL)
    // And before max speed for flaps is reached (215knt for A320, 255KIAS for 777)
    if (   (aircraft->m_flaps > 0.0)
        && (   (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude > (aircraft->m_runwayAltitude + flapsRetractAltitude)))
            || (aircraft->m_groundspeedValid && (aircraft->m_groundspeed > flapsCleanSpeed))
           )
       )
    {
        if (debug) {
            qDebug() << "Retract flaps " << aircraft->m_icaoHex
                    << " Spd " << (aircraft->m_groundspeedValid && (aircraft->m_groundspeed > flapsCleanSpeed))
                    << " Alt " << (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude > (aircraft->m_runwayAltitude + flapsRetractAltitude)));
        }
        if (hasFlaps) {
            animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.0));
        }
        if (hasSlats) {
            animations->append(slatsAnimation(dateTime, true));
        }
        aircraft->m_flaps = 0.0;
        // Clear runway information
        aircraft->m_runwayAltitudeValid = false;
        aircraft->m_runwayAltitude = 0.0;
    }

    // Extend flaps for approach and landing
    // We don't know airport elevation, so just base on speed and descent rate
    // Vertical rate can go negative during take-off, so we check m_runwayAltitudeValid
    if (!aircraft->m_onSurface
        && !aircraft->m_runwayAltitudeValid
        && (aircraft->m_verticalRateValid && (aircraft->m_verticalRate < 0))
        && aircraft->m_groundspeedValid
       )
    {
        if ((aircraft->m_groundspeed < flapsCleanSpeed) && (aircraft->m_flaps < 0.25))
        {
            // Extend flaps for approach
            if (debug) {
                qDebug() << "Extend flaps for approach" << aircraft->m_icaoHex;
            }
            if (hasFlaps) {
                animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.25));
            }
            //animations->append(slatsAnimation(dateTime, false));
            aircraft->m_flaps = 0.25;
            // Only until the next report, where the flight path angle takes over
            aircraft->m_pitchEst = 1.0;
        }
    }

    // Gear down for landing, at the height above the runway most operators use.
    //
    // Where no airport is near enough to give an elevation this still has to guess
    // m_runwayAltitudeValid still excludes a departure being watched: vertical rate can
    // go briefly negative after take-off, and without this the gear would drop again
    // moments after retracting
    if (!aircraft->m_gearDown
        && !aircraft->m_onSurface
        && !aircraft->m_runwayAltitudeValid
        && (aircraft->m_verticalRateValid && (aircraft->m_verticalRate < gearDownVerticalRate))
        && (   (heightValid && (height < gearDownHeight))
            || (   !heightValid
                && (   (aircraft->m_indicatedAirspeedValid && (aircraft->m_indicatedAirspeed < gearDownAirspeed))
                    || (!aircraft->m_indicatedAirspeedValid && aircraft->m_groundspeedValid && (aircraft->m_groundspeed < gearDownSpeed))
                   )
               )
           )
       )
    {
        if (debug) {
            qDebug() << "Flaps/Gear down for landing " << aircraft->m_icaoHex;
        }
        if (hasFlaps) {
            animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.5));
        }
        if (hasSlats) {
            animations->append(slatsAnimation(dateTime, false));
        }
        if (hasGear) {
            animations->append(gearAnimation(dateTime.addSecs(8), false));
        }
        if (hasFlaps) {
            animations->append(flapsAnimation(dateTime.addSecs(16), 0.5, 1.0));
        }
        aircraft->m_gearDown = true;
        aircraft->m_flaps = 1.0;
        // Only until the next report, where the flight path angle takes over. A three
        // degree slope at the landing angle of attack settles around +1.5
        aircraft->m_pitchEst = 1.5;
    }

    // Engine control. The running animations are re-asserted every few minutes: the
    // start is otherwise a one-shot event, so a map opened, reloaded or reconnected
    // after it had fired showed the propellors frozen. The map replaces a looped
    // animation that is already playing, so nothing accumulates.
    const bool reassert = aircraft->m_engineAnimationTime.isValid()
        && (aircraft->m_engineAnimationTime.secsTo(dateTime) >= 300);
    if (aircraft->m_emitterCategory == "Rotorcraft")
    {
        // Helicopter rotors
        if (!aircraft->m_rotorStarted && !aircraft->m_onSurface)
        {
            // Start rotors
            if (debug) {
                qDebug() << "Start rotors " << aircraft->m_icaoHex;
            }
            if (hasRotor) {
                animations->append(rotorAnimation(dateTime, false));
            }
            aircraft->m_rotorStarted = true;
            aircraft->m_engineAnimationTime = dateTime;
        }
        else if (aircraft->m_rotorStarted && aircraft->m_onSurface)
        {
            if (debug) {
                qDebug() << "Stop rotors " << aircraft->m_icaoHex;
            }
            if (hasRotor) {
                animations->append(rotorAnimation(dateTime, true));
            }
            aircraft->m_rotorStarted = false;
        }
        else if (aircraft->m_rotorStarted && reassert)
        {
            if (hasRotor) {
                animations->append(rotorAnimation(dateTime, false));
            }
            aircraft->m_engineAnimationTime = dateTime;
        }
    }
    else
    {
        // Propellors/fans
        if (!aircraft->m_engineStarted && aircraft->m_groundspeedValid && (aircraft->m_groundspeed > 0))
        {
            if (debug) {
                qDebug() << "Start engines " << aircraft->m_icaoHex;
            }
            if (hasEngine1) {
                animations->append(engineAnimation(dateTime, 1, false));
            }
            if (hasEngine2) {
                animations->append(engineAnimation(dateTime, 2, false));
            }
            aircraft->m_engineStarted = true;
            aircraft->m_engineAnimationTime = dateTime;
        }
        else if (aircraft->m_engineStarted && aircraft->m_groundspeedValid && (aircraft->m_groundspeed == 0))
        {
            if (debug) {
                qDebug() << "Stop engines " << aircraft->m_icaoHex;
            }
            if (hasEngine1) {
                animations->append(engineAnimation(dateTime, 1, true));
            }
            if (hasEngine2) {
                animations->append(engineAnimation(dateTime, 2, true));
            }
            aircraft->m_engineStarted = false;
        }
        else if (aircraft->m_engineStarted && reassert)
        {
            if (hasEngine1) {
                animations->append(engineAnimation(dateTime, 1, false));
            }
            if (hasEngine2) {
                animations->append(engineAnimation(dateTime, 2, false));
            }
            aircraft->m_engineAnimationTime = dateTime;
        }
    }

    // Estimate pitch, so it looks a little more realistic
    if (aircraft->m_onSurface)
    {
        // Check speed so we don't set pitch to 0 immediately on touch-down
        // Should probably record time of touch-down and reduce over time
        if (aircraft->m_groundspeedValid)
        {
            if (aircraft->m_groundspeed < 80) {
                aircraft->m_pitchEst = 0.0;
            } else if ((aircraft->m_groundspeed < 130) && (aircraft->m_pitchEst >= 2)) {
                aircraft->m_pitchEst = 1;
            }
        }
    }
    else if (aircraft->m_verticalRateValid)
    {
        // In the climb or the descent, the flight path angle: how fast the aircraft is
        // climbing against how fast it is going forwards. This replaced an estimate made
        // from the vertical rate alone, which took no account of speed - so the same rate
        // gave the same attitude at 150 knots as at 500 - and, being an absolute value,
        // pitched the nose up in the descent as well as the climb.
        // The angle of attack above turns that path angle into the body attitude the
        // model is drawn at and the attitude indicator reads.
        //
        // This used to stop as soon as the approach flaps came out, leaving the attitude
        // frozen at whatever the configuration change had set - so an aircraft levelling
        // off with the gear down was drawn as nose up as one descending at a thousand
        // feet a minute. The flaps only change which angle of attack applies
        if (aircraft->m_groundspeedValid && (aircraft->m_groundspeed > minPitchGroundspeed))
        {
            const float aoa = (aircraft->m_flaps >= 0.25) ? landingAngleOfAttack : angleOfAttack;

            // Groundspeed rather than airspeed overstates the path angle into a
            // headwind, by well under the degree the fixed values were out by
            aircraft->m_pitchEst = aoa + Units::radiansToDegrees(
                std::atan2((float) aircraft->m_verticalRate, aircraft->m_groundspeed * ftPerMinutePerKnot));
        }
    }

    // Estimate some roll
    if (aircraft->m_onSurface
        || (aircraft->m_runwayAltitudeValid && (aircraft->m_altitude < (aircraft->m_runwayAltitude + accelerationHeight))))
    {
        aircraft->m_rollEst = 0.0;
    }
    else if (aircraft->m_trackValid)
    {
        // Really need to use more data points for this - or better yet, get it from Mode-S frames
        if (aircraft->m_prevTrackDateTime.isValid())
        {
             qint64 msecs = aircraft->m_prevTrackDateTime.msecsTo(aircraft->m_headingDateTime);
             if (msecs > 0)
             {
                 float trackDiff = fmod(aircraft->m_track - aircraft->m_prevTrack + 540.0, 360.0) - 180.0;
                 float roll = trackDiff / (msecs / 1000.0);
                 //qDebug() << "Track Diff " << trackDiff << " msecs " << msecs << " roll " << roll;
                 roll = std::min(roll, 15.0f);
                 roll = std::max(roll, -15.0f);
                 aircraft->m_rollEst = roll;
             }
        }
        aircraft->m_prevTrackDateTime = aircraft->m_trackDateTime;
        aircraft->m_prevTrack = aircraft->m_track;
    }

    return animations;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodTracker::gearAnimation(QDateTime startDateTime, bool up)
{
    // Gear up (0.0) / down (1.0)
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString(ANIMATION_GEAR));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(up);
    animation->setLoop(0);
    animation->setDuration(5);
    animation->setMultiplier(0.2f);
    animation->setSound(new QString("A320_gear.wav"));
    return animation;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodTracker::gearAngle(QDateTime startDateTime, bool flat)
{
    // Gear deflection - on ground it should be flat (0.0) in the air at an angle (1.0)
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString(ANIMATION_GEAR_ANGLE));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(flat);
    animation->setLoop(0);
    animation->setDuration(1);
    animation->setMultiplier(1);
    return animation;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodTracker::flapsAnimation(QDateTime startDateTime, float currentFlaps, float flaps)
{
    // Extend / retract flags
    bool retract = flaps < currentFlaps;
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString(ANIMATION_FLAPS));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(retract);
    animation->setLoop(0);
    animation->setDuration(5*std::abs(flaps-currentFlaps));
    animation->setMultiplier(0.2f);
    if (retract) {
        animation->setStartOffset(1.0 - currentFlaps);
    } else {
        animation->setStartOffset(currentFlaps);
    }
    animation->setSound(new QString("A320_flaps.wav"));
    return animation;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodTracker::slatsAnimation(QDateTime startDateTime, bool retract)
{
    // Extend / retract slats
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString(ANIMATION_SLATS));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(retract);
    animation->setLoop(0);
    animation->setDuration(5);
    animation->setMultiplier(0.2f);
    return animation;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodTracker::rotorAnimation(QDateTime startDateTime, bool stop)
{
    // Turn rotors in helicopter.glb model
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString(ANIMATION_ROTOR));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(false);
    animation->setLoop(true);
    animation->setMultiplier(1);
    animation->setStop(stop);
    return animation;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodTracker::engineAnimation(QDateTime startDateTime, int engine, bool stop)
{
    // Turn propellors. ModelConverterX bakes the engine_rotation_angle_deg dataref as
    // one degree of rotation per second of animation, so it takes a large multiplier
    // to look like a turning propellor - 720 is two revolutions a second
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString(ANIMATION_ENGINE.arg(engine)));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(false);
    animation->setLoop(true);
    animation->setMultiplier(720);
    animation->setStop(stop);
    return animation;
}

void ADSBDemodTracker::checkStaticNotification(ADSBAircraft *aircraft)
{
    for (int i = 0; i < m_settings.m_notificationSettings.size(); i++)
    {
        QString match;
        switch (m_settings.m_notificationSettings[i]->m_matchColumn)
        {
        case ADSB_COL_ICAO:
            match = aircraft->cell(ADSB_COL_ICAO).toString();
            break;
        case ADSB_COL_MODEL:
            match = aircraft->cell(ADSB_COL_MODEL).toString();
            break;
        case ADSB_COL_TYPE:
            match = aircraft->cell(ADSB_COL_TYPE).toString();
            break;
        case ADSB_COL_REGISTRATION:
            match = aircraft->cell(ADSB_COL_REGISTRATION).toString();
            break;
        case ADSB_COL_MANUFACTURER:
            match = aircraft->cell(ADSB_COL_MANUFACTURER).toString();
            break;
        case ADSB_COL_OWNER:
            match = aircraft->cell(ADSB_COL_OWNER).toString();
            break;
        case ADSB_COL_OPERATOR_ICAO:
            match = aircraft->cell(ADSB_COL_OPERATOR_ICAO).toString();
            break;
        default:
            break;
        }
        if (!match.isEmpty())
        {
            if (m_settings.m_notificationSettings[i]->m_regularExpression.isValid())
            {
                if (m_settings.m_notificationSettings[i]->m_regularExpression.match(match).hasMatch())
                {
                    emit highlightRequest((quint32) aircraft->m_icao);

                    if (!m_settings.m_notificationSettings[i]->m_speech.isEmpty()) {
                        emit speechRequest(subAircraftString(aircraft, m_settings.m_notificationSettings[i]->m_speech));
                    }
                    if (!m_settings.m_notificationSettings[i]->m_command.isEmpty()) {
                        commandNotification(aircraft, m_settings.m_notificationSettings[i]->m_command);
                    }
                    if (m_settings.m_notificationSettings[i]->m_autoTarget) {
                        emit targetRequest((quint32) aircraft->m_icao);
                    }

                    aircraft->m_notified = true;
                }
            }
        }
    }
}

void ADSBDemodTracker::checkDynamicNotification(ADSBAircraft *aircraft)
{
    if (!aircraft->m_notified)
    {
        for (int i = 0; i < m_settings.m_notificationSettings.size(); i++)
        {
            if (   (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_CALLSIGN)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_ALTITUDE)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_GROUND_SPEED)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_RANGE)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_CATEGORY)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_STATUS)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_SQUAWK)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_DEP)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_ARR)
                || (m_settings.m_notificationSettings[i]->m_matchColumn == ADSB_COL_STOPS)
                )
            {
                QString match;
                switch (m_settings.m_notificationSettings[i]->m_matchColumn)
                {
                case ADSB_COL_CALLSIGN:
                    match = aircraft->cell(ADSB_COL_CALLSIGN).toString();
                    break;
                case ADSB_COL_ALTITUDE:
                    match = aircraft->cell(ADSB_COL_ALTITUDE).toString();
                    break;
                case ADSB_COL_GROUND_SPEED:
                    match = aircraft->cell(ADSB_COL_GROUND_SPEED).toString();
                    break;
                case ADSB_COL_RANGE:
                    match = ADSBAircraft::rangeText(aircraft->cell(ADSB_COL_RANGE));
                    break;
                case ADSB_COL_CATEGORY:
                    match = aircraft->cell(ADSB_COL_CATEGORY).toString();
                    break;
                case ADSB_COL_STATUS:
                    match = aircraft->cell(ADSB_COL_STATUS).toString();
                    break;
                case ADSB_COL_SQUAWK:
                    match = aircraft->cell(ADSB_COL_SQUAWK).toString();
                    break;
                case ADSB_COL_DEP:
                    match = aircraft->cell(ADSB_COL_DEP).toString();
                    break;
                case ADSB_COL_ARR:
                    match = aircraft->cell(ADSB_COL_ARR).toString();
                    break;
                case ADSB_COL_STOPS:
                    match = aircraft->cell(ADSB_COL_STOPS).toString();
                    break;
                default:
                    break;
                }
                if (!match.isEmpty())
                {
                    if (m_settings.m_notificationSettings[i]->m_regularExpression.isValid())
                    {
                        if (m_settings.m_notificationSettings[i]->m_regularExpression.match(match).hasMatch())
                        {
                            emit highlightRequest((quint32) aircraft->m_icao);

                            if (!m_settings.m_notificationSettings[i]->m_speech.isEmpty()) {
                                emit speechRequest(subAircraftString(aircraft, m_settings.m_notificationSettings[i]->m_speech));
                            }
                            if (!m_settings.m_notificationSettings[i]->m_command.isEmpty()) {
                                commandNotification(aircraft, m_settings.m_notificationSettings[i]->m_command);
                            }
                            if (m_settings.m_notificationSettings[i]->m_autoTarget) {
                                emit targetRequest((quint32) aircraft->m_icao);
                            }

                            aircraft->m_notified = true;
                        }
                    }
                }
            }
        }
    }
}

void ADSBDemodTracker::commandNotification(ADSBAircraft *aircraft, const QString &command)
{
#if QT_CONFIG(process)
    QString commandLine = subAircraftString(aircraft, command);
    QStringList allArgs = QProcess::splitCommand(commandLine);

    if (allArgs.size() > 0)
    {
        QString program = allArgs[0];
        allArgs.pop_front();
        QProcess::startDetached(program, allArgs);
    }
#else
    qWarning() << "ADSBDemodTracker::commandNotification: QProcess not supported. Can't run: " << command;
#endif
}

QString ADSBDemodTracker::subAircraftString(ADSBAircraft *aircraft, const QString &string)
{
    QString s = string;
    s = s.replace("${icao}", aircraft->cell(ADSB_COL_ICAO).toString());
    s = s.replace("${callsign}", aircraft->cell(ADSB_COL_CALLSIGN).toString());
    s = s.replace("${aircraft}", aircraft->cell(ADSB_COL_MODEL).toString());
    s = s.replace("${type}", aircraft->cell(ADSB_COL_TYPE).toString());
    s = s.replace("${speed}", aircraft->cell(ADSB_COL_GROUND_SPEED).toString()); // For backwards compatibility
    s = s.replace("${gs}", aircraft->cell(ADSB_COL_GROUND_SPEED).toString());
    s = s.replace("${tas}", aircraft->cell(ADSB_COL_TRUE_AIRSPEED).toString());
    s = s.replace("${ias}", aircraft->cell(ADSB_COL_INDICATED_AIRSPEED).toString());
    s = s.replace("${mach}", aircraft->cell(ADSB_COL_MACH).toString());
    s = s.replace("${selAltitude}", aircraft->cell(ADSB_COL_SEL_ALTITUDE).toString());
    s = s.replace("${altitude}", aircraft->cell(ADSB_COL_ALTITUDE).toString());
    s = s.replace("${verticalRate}", aircraft->cell(ADSB_COL_VERTICALRATE).toString());
    s = s.replace("${selHeading}", aircraft->cell(ADSB_COL_SEL_HEADING).toString());
    s = s.replace("${heading}", aircraft->cell(ADSB_COL_HEADING).toString());
    s = s.replace("${track}", aircraft->cell(ADSB_COL_TRACK).toString());
    s = s.replace("${turnRate}", aircraft->cell(ADSB_COL_TURNRATE).toString());
    s = s.replace("${roll}", aircraft->cell(ADSB_COL_ROLL).toString());
    s = s.replace("${range}", ADSBAircraft::rangeText(aircraft->cell(ADSB_COL_RANGE)));
    s = s.replace("${azel}", aircraft->cell(ADSB_COL_AZEL).toString());
    s = s.replace("${category}", aircraft->cell(ADSB_COL_CATEGORY).toString());
    s = s.replace("${status}", aircraft->cell(ADSB_COL_STATUS).toString());
    s = s.replace("${squawk}", aircraft->cell(ADSB_COL_SQUAWK).toString());
    s = s.replace("${ident}", aircraft->cell(ADSB_COL_IDENT).toString());
    s = s.replace("${registration}", aircraft->cell(ADSB_COL_REGISTRATION).toString());
    s = s.replace("${manufacturer}", aircraft->cell(ADSB_COL_MANUFACTURER).toString());
    s = s.replace("${owner}", aircraft->cell(ADSB_COL_OWNER).toString());
    s = s.replace("${operator}", aircraft->cell(ADSB_COL_OPERATOR_ICAO).toString());
    s = s.replace("${ap}", aircraft->cell(ADSB_COL_AP).toString());
    s = s.replace("${vMode}", aircraft->cell(ADSB_COL_V_MODE).toString());
    s = s.replace("${lMode}", aircraft->cell(ADSB_COL_L_MODE).toString());
    s = s.replace("${tcas}", aircraft->cell(ADSB_COL_TCAS).toString());
    s = s.replace("${acas}", aircraft->cell(ADSB_COL_ACAS).toString());
    s = s.replace("${ra}", aircraft->cell(ADSB_COL_RA).toString());
    s = s.replace("${maxSpeed}", aircraft->cell(ADSB_COL_MAX_SPEED).toString());
    s = s.replace("${version}", aircraft->cell(ADSB_COL_VERSION).toString());
    s = s.replace("${length}", aircraft->cell(ADSB_COL_LENGTH).toString());
    s = s.replace("${width}", aircraft->cell(ADSB_COL_WIDTH).toString());
    s = s.replace("${baro}", aircraft->cell(ADSB_COL_BARO).toString());
    s = s.replace("${headwind}", aircraft->cell(ADSB_COL_HEADWIND).toString());
    s = s.replace("${windSpeed}", aircraft->cell(ADSB_COL_WIND_SPEED).toString());
    s = s.replace("${windDirection}", aircraft->cell(ADSB_COL_WIND_DIR).toString());
    s = s.replace("${staticPressure}", aircraft->cell(ADSB_COL_STATIC_PRESSURE).toString());
    s = s.replace("${staticAirTemperature}", aircraft->cell(ADSB_COL_STATIC_AIR_TEMP).toString());
    s = s.replace("${humidity}", aircraft->cell(ADSB_COL_HUMIDITY).toString());
    s = s.replace("${latitude}", aircraft->cell(ADSB_COL_LATITUDE).toString());
    s = s.replace("${longitude}", aircraft->cell(ADSB_COL_LONGITUDE).toString());
    s = s.replace("${ic}", aircraft->cell(ADSB_COL_IC).toString());
    s = s.replace("${rssi}", aircraft->cell(ADSB_COL_RSSI).toString());
    s = s.replace("${flightstatus}", aircraft->cell(ADSB_COL_FLIGHT_STATUS).toString());
    s = s.replace("${departure}", aircraft->cell(ADSB_COL_DEP).toString());
    s = s.replace("${arrival}", aircraft->cell(ADSB_COL_ARR).toString());
    s = s.replace("${stops}", aircraft->cell(ADSB_COL_STOPS).toString());
    s = s.replace("${std}", aircraft->cell(ADSB_COL_STD).toString());
    s = s.replace("${etd}", aircraft->cell(ADSB_COL_ETD).toString());
    s = s.replace("${atd}", aircraft->cell(ADSB_COL_ATD).toString());
    s = s.replace("${sta}", aircraft->cell(ADSB_COL_STA).toString());
    s = s.replace("${eta}", aircraft->cell(ADSB_COL_ETA).toString());
    s = s.replace("${ata}", aircraft->cell(ADSB_COL_ATA).toString());
    return s;
}

// The matching itself lives in sdrbase now, shared with the Aircraft feature: see
// Aircraft3DModels. What stays here is only the ADS-B specific part - remembering the
// result on the aircraft, and the emitter category fallback below, which no other protocol
// can use because no other protocol transmits a category.
void ADSBDemodTracker::get3DModel(ADSBAircraft *aircraft)
{
    if (!aircraft->m_aircraftInfo) {
        return;
    }

    QString model;
    float altitudeOffset = aircraft->m_modelAltitudeOffset;
    float labelOffset = aircraft->m_labelAltitudeOffset;

    const Aircraft3DModels &models = Aircraft3DModels::instance();
    // Failing a model of its own type, a stand-in of the right shape for the type it is -
    // a business jet for a Premier, a twin turboprop for a King Air. That knows more than
    // the emitter category, so it is kept in preference to one: a category stand-in
    // names a single type, which may well not be installed
    if (models.modelFor(aircraft->m_aircraftInfo, m_settings.m_favourLivery,
                        m_settings.m_verboseModelMatching, model, altitudeOffset, labelOffset)
        || models.defaultModel(aircraft->m_aircraftInfo->m_type.trimmed().toUpper(),
                               model, altitudeOffset, labelOffset, true))
    {
        aircraft->m_aircraft3DModel = model;
        aircraft->m_modelAltitudeOffset = altitudeOffset;
        aircraft->m_labelAltitudeOffset = labelOffset;
    }
}

void ADSBDemodTracker::get3DModelBasedOnCategory(ADSBAircraft *aircraft)
{
    QString aircraftType;
    QStringList candidateTypes;     // Any of these will do, for Heavy and Large
    static const QStringList large = {"A319", "A320", "A321", "B737", "B738", "B739", "A20N", "A21N"};

    if (!aircraft->m_emitterCategory.compare("Heavy"))
    {
        static const QStringList heavy = {"B744", "B77W", "B788", "A388", "B789", "A359", "B772", "A333",
                                          "A332", "B763", "B748", "A346"};
        candidateTypes = heavy;
    }
    else if (!aircraft->m_emitterCategory.compare("Large"))
    {
        candidateTypes = large;
    }
    else if (!aircraft->m_emitterCategory.compare("Small") || !aircraft->m_emitterCategory.compare("Light"))
    {
        // Whatever of the right shape is installed: the Bluebell set has a Learjet 45 and
        // a Cessna 172, the FSLTL one neither but a Learjet 25, Caravan, TBM and DA62. A
        // single named type left these with no model at all with only the FSLTL set
        const bool light = !aircraft->m_emitterCategory.compare("Light");
        QString model;
        float altitudeOffset = aircraft->m_modelAltitudeOffset;
        float labelOffset = aircraft->m_labelAltitudeOffset;
        aircraft->m_aircraftCat3DModel = "";
        if (Aircraft3DModels::instance().defaultModel(light ? "C172" : "LJ45", model, altitudeOffset, labelOffset))
        {
            aircraft->m_aircraftCat3DModel = model;
            aircraft->m_modelAltitudeOffset = altitudeOffset;
            aircraft->m_labelAltitudeOffset = labelOffset;
        }
    }
    else if (!aircraft->m_emitterCategory.compare("Rotorcraft"))
    {
        aircraft->m_aircraftCat3DModel = "helicopter.glb";
        aircraft->m_modelAltitudeOffset = 4.0f;
        aircraft->m_labelAltitudeOffset = 4.0f;
    }
    else if (!aircraft->m_emitterCategory.compare("High performance"))
    {
        aircraft->m_aircraftCat3DModel = "f15.glb";
        aircraft->m_modelAltitudeOffset = 1.0f;
        aircraft->m_labelAltitudeOffset = 6.0f;
    }
    else if (!aircraft->m_emitterCategory.compare("Ultralight"))
    {
        aircraft->m_aircraftCat3DModel = "ultralight.glb";
        aircraft->m_modelAltitudeOffset = 0.55f;
        aircraft->m_labelAltitudeOffset = 0.75f;
    }
    else if (!aircraft->m_emitterCategory.compare("Glider/sailplane"))
    {
        aircraft->m_aircraftCat3DModel = "glider.glb";
        aircraft->m_modelAltitudeOffset = 1.0f;
        aircraft->m_labelAltitudeOffset = 1.5f;
    }
    else if (!aircraft->m_emitterCategory.compare("Space vehicle"))
    {
        aircraft->m_aircraftCat3DModel = "atlas_v.glb";
        aircraft->m_labelAltitudeOffset = 16.0f;
    }
    else if (!aircraft->m_emitterCategory.compare("UAV"))
    {
        aircraft->m_aircraftCat3DModel = "drone.glb";
        aircraft->m_labelAltitudeOffset = 1.0f;
    }
    else if (!aircraft->m_emitterCategory.compare("Emergency vehicle"))
    {
        aircraft->m_aircraftCat3DModel = "fire_truck.glb";
        aircraft->m_modelAltitudeOffset = 0.3f;
        aircraft->m_labelAltitudeOffset = 2.5f;
    }
    else if (!aircraft->m_emitterCategory.compare("Service vehicle"))
    {
        aircraft->m_aircraftCat3DModel = "airport_truck.glb";
        aircraft->m_labelAltitudeOffset = 3.0f;
    }
    else
    {
        // No category, or one with no stand-in of its own: most likely an airliner
        candidateTypes = large;
    }

    if (!candidateTypes.isEmpty())
    {
        // Any type of the category in the operator's livery, rather than a type chosen at
        // random and then the livery only if that type has it - which, with a BA callsign
        // and a 737-800 drawn, gave another airline's 737. An aircraft the database doesn't
        // know still has its operator in its callsign: the first three letters of an airline
        // callsign are its ICAO code
        const Aircraft3DModels& models = Aircraft3DModels::instance();
        QStringList operators;
        if (aircraft->m_aircraftInfo && !aircraft->m_aircraftInfo->m_operatorICAO.isEmpty()) {
            operators.append(aircraft->m_aircraftInfo->m_operatorICAO);
        }
        static const QRegularExpression airlineCallsign("^([A-Z]{3})[0-9]");
        const QRegularExpressionMatch match = airlineCallsign.match(aircraft->m_callsign);
        if (match.hasMatch() && !operators.contains(match.captured(1))) {
            operators.append(match.captured(1));
        }
        const int start = m_random.bounded(candidateTypes.size());
        QString model;
        for (const QString &op : operators)
        {
            for (int i = 0; (i < candidateTypes.size()) && model.isEmpty(); i++)
            {
                const QString &type = candidateTypes[(start + i) % candidateTypes.size()];
                model = models.modelForTypeAndOperator(type, op);
                if (!model.isEmpty()) {
                    aircraftType = type;
                }
            }
            if (!model.isEmpty()) {
                break;
            }
        }
        // Otherwise any livery of the first of them that is installed
        for (int i = 0; (i < candidateTypes.size()) && model.isEmpty(); i++)
        {
            const QString &type = candidateTypes[(start + i) % candidateTypes.size()];
            model = models.modelForType(type);
            if (!model.isEmpty()) {
                aircraftType = type;
            }
        }
        aircraft->m_aircraftCat3DModel = model;
        if (!model.isEmpty()) {
            models.offsetsForType(aircraftType, aircraft->m_modelAltitudeOffset, aircraft->m_labelAltitudeOffset, model);
        }
    }
}


int ADSBDemodTracker::countActiveAircraft()
{
    int total = 0;
    QDateTime now = QDateTime::currentDateTime();
    qint64 nowSecs = now.toSecsSinceEpoch();
    QHash<int, ADSBAircraft *>::iterator i = m_aircraft.begin();

    while (i != m_aircraft.end())
    {
        ADSBAircraft *aircraft = i.value();
        qint64 secondsSinceLastFrame = nowSecs - aircraft->m_updateTime.toSecsSinceEpoch();

        if (secondsSinceLastFrame < 10) {
            total++;
        }

        ++i;
    }

    return total;
}

void ADSBDemodTracker::removeAircraft(QHash<int, ADSBAircraft *>::iterator& i, ADSBAircraft *aircraft)
{

    // Don't try to track it anymore
    if (m_trackAircraft == aircraft)
    {
        m_adsbDemod->clearTarget();
        m_trackAircraft = nullptr;
    }

    // The GUI removes it from the table and the map when told
    m_dirtyAircraft.remove(aircraft);
    m_removedAircraft.append((quint32) aircraft->m_icao);
    // Remove aircraft from hash
    i = m_aircraft.erase(i);
    // Remove from map feature
    clearFromMap(aircraft->m_icaoHex);

    // And finally free its memory
    discardMapUpdate(aircraft);
    delete aircraft;
}

void ADSBDemodTracker::sendAircraftReport()
{
    ADSBDemod::MsgAircraftReport* message = ADSBDemod::MsgAircraftReport::create();

    QList<ADSBDemod::MsgAircraftReport::AircraftReport>& report = message->getReport();
    report.reserve(m_aircraft.size());

    QHash<int, ADSBAircraft *>::iterator i = m_aircraft.begin();
    while (i != m_aircraft.end())
    {
        ADSBAircraft *aircraft = i.value();

        ADSBDemod::MsgAircraftReport::AircraftReport aircraftReport {
            aircraft->m_icaoHex,
            aircraft->m_callsign,
            aircraft->m_latitude,
            aircraft->m_longitude,
            aircraft->m_altitude,
            aircraft->m_groundspeed
        };

        report.append(aircraftReport);

        ++i;
    }

    m_adsbDemod->getInputMessageQueue()->push(message);
}

ADSBAircraft* ADSBDemodTracker::findAircraftByFlight(const QString& flight)
{
    QHash<int, ADSBAircraft *>::iterator i = m_aircraft.begin();
    while (i != m_aircraft.end())
    {
        ADSBAircraft *aircraft = i.value();
        if (aircraft->m_flight == flight) {
            return aircraft;
        }
        ++i;
    }
    return nullptr;
}

int ADSBDemodTracker::squawkDecode(int modeA) const
{
    int a, b, c, d;
    c = ((modeA >> 12) & 1) | ((modeA >> (10-1)) & 0x2) | ((modeA >> (8-2)) & 0x4);
    a = ((modeA >> 11) & 1) | ((modeA >> (9-1)) & 0x2) | ((modeA >> (7-2)) & 0x4);
    b = ((modeA >> 5) & 1) | ((modeA >> (3-1)) & 0x2) | ((modeA << (1)) & 0x4);
    d = ((modeA >> 4) & 1) | ((modeA >> (2-1)) & 0x2) | ((modeA << (2)) & 0x4);
    return a*1000 + b*100 + c*10 + d;
}

// https://en.wikipedia.org/wiki/Gillham_code
int ADSBDemodTracker::gillhamToFeet(int n) const
{
    int c1 = (n >> 10) & 1;
    int a1 = (n >> 9) & 1;
    int c2 = (n >> 8) & 1;
    int a2 = (n >> 7) & 1;
    int c4 = (n >> 6) & 1;
    int a4 = (n >> 5) & 1;
    int b1 = (n >> 4) & 1;
    int b2 = (n >> 3) & 1;
    int d2 = (n >> 2) & 1;
    int b4 = (n >> 1) & 1;
    int d4 = n & 1;

    int n500 = grayToBinary((d2 << 7) | (d4 << 6) | (a1 << 5) | (a2 << 4) | (a4 << 3) | (b1 << 2) | (b2 << 1) | b4, 8);
    int n100 = grayToBinary((c1 << 2) | (c2 << 1) | c4, 3) - 1;

    if (n100 == 6) {
        n100 = 4;
    }
    if (n500 %2 != 0) {
        n100 = 4 - n100;
    }

    return -1200 + n500*500 + n100*100;
}

int ADSBDemodTracker::grayToBinary(int gray, int bits) const
{
    int binary = 0;
    for (int i = bits - 1; i >= 0; i--) {
        binary = binary | ((((1 << (i+1)) & binary) >> 1) ^ ((1 << i) & gray));
    }
    return binary;
}

void ADSBDemodTracker::updateQNH(const ADSBAircraft *aircraft, float qnh)
{
    if (!m_settings.m_manualQNH)
    {
        // Ignore aircraft that have QNH set to STD.
        if (   ((qnh < 1012) || (std::floor(qnh) > 1013))
            || (   (aircraft->m_altitudeValid && (aircraft->m_altitude < (m_settings.m_transitionAlt - 1000)))
                && (aircraft->m_selAltitudeValid && (aircraft->m_selAltitude < m_settings.m_transitionAlt))     // If we have qnh, we'll have selAltitude as well
               )
           ) {
            // Use moving average, otherwise it can jump around if we can receive aircraft from different airports with different QNH
            m_qnhAvg(qnh);

            // This runs for every aircraft that reports a barometric setting, so several
            // times a second. Reporting it unconditionally left QSpinBox::setValue() as
            // the only thing deciding whether anything happened, and the rounded average
            // sits near a boundary for long stretches - so the value oscillated between
            // two adjacent hectopascals and each change fanned a full settings update out
            // to the baseband, the sink, the worker and this tracker, logging five lines
            // a time. A whole hectopascal of hysteresis stops that: QNH is only quoted to
            // 1 hPa anyway, and it moves that much over tens of minutes, not milliseconds.
            const float average = m_qnhAvg.instantAverage();
            if ((m_qnhReported == 0) || (std::abs(average - m_qnhReported) >= 1.0f))
            {
                const int rounded = (int) round(average);
                if (rounded != m_qnhReported)
                {
                    m_qnhReported = rounded;
                    emit qnhEstimate(rounded);
                }
            }
        }
    }
}

// Called again whenever the station position changes, as the outline is drawn around it
void ADSBDemodTracker::initCoverageMap()
{
    float lat = m_azEl.getLocationSpherical().m_latitude;
    float lon = m_azEl.getLocationSpherical().m_longitude;
    for (int i = 0; i < 2; i++)
    {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        m_maxRange[i].resize(360/ADSBDemodTracker::m_maxRangeDeg, 0.0f);
#else
        for (int j = 0; j < 360/ADSBDemodTracker::m_maxRangeDeg; j++) {
            m_maxRange[i].append(0.0f);
        }
#endif
        m_coverageAirspace[i].m_polygon.resize(2 * 360/ADSBDemodTracker::m_maxRangeDeg);
        m_coverageAirspace[i].m_center.setX(lon);
        m_coverageAirspace[i].m_center.setY(lat);
        if (i == 0)
        {
            m_coverageAirspace[i].m_position.setX(lon);
            m_coverageAirspace[i].m_position.setY(lat);
            m_coverageAirspace[i].m_bottom.m_alt = 0;
            m_coverageAirspace[i].m_top.m_alt = 10000;
            m_coverageAirspace[i].m_name = "Coverage Map Low";
        }
        else
        {
            m_coverageAirspace[i].m_position.setX(lon);
            m_coverageAirspace[i].m_position.setY(lat + 0.01);
            m_coverageAirspace[i].m_bottom.m_alt = 10000;
            m_coverageAirspace[i].m_top.m_alt = 66000;
            m_coverageAirspace[i].m_name = "Coverage Map High";
        }
    }
    clearCoverageMap();
}

void ADSBDemodTracker::clearCoverageMap()
{
    for (int j = 0; j < 2; j++)
    {
        for (int i = 0; i < m_maxRange[j].size(); i++) {
            m_maxRange[j][i] = 0.0f;
        }
        const float d = 0.01f;
        for (int i = 0; i < m_coverageAirspace[j].m_polygon.size(); i+=2)
        {
            float f = i / (float) m_coverageAirspace[j].m_polygon.size();
            m_coverageAirspace[j].m_polygon[i].setX(m_coverageAirspace[j].m_center.x() + d * sin(f * 2*M_PI));
            m_coverageAirspace[j].m_polygon[i].setY(m_coverageAirspace[j].m_center.y() + d * cos(f * 2*M_PI));
            m_coverageAirspace[j].m_polygon[i+1].setX(m_coverageAirspace[j].m_center.x() + d * sin(f * 2*M_PI));
            m_coverageAirspace[j].m_polygon[i+1].setY(m_coverageAirspace[j].m_center.y() + d * cos(f * 2*M_PI));
        }
    }
}

void ADSBDemodTracker::updateCoverageMap(float azimuth, float elevation, float distance, float altitude)
{
    (void) elevation;

    int i = (int) (azimuth / ADSBDemodTracker::m_maxRangeDeg);
    int k = altitude >= 10000 ? 1 : 0;
    if (distance > m_maxRange[k][i])
    {
        m_maxRange[k][i] = distance;

        float lat;
        float lon;
        float b1 = i * ADSBDemodTracker::m_maxRangeDeg;
        calcRadialEndPoint(m_azEl.getLocationSpherical().m_latitude, m_azEl.getLocationSpherical().m_longitude, distance, b1, lat, lon);
        m_coverageAirspace[k].m_polygon[i*2].setX(lon);
        m_coverageAirspace[k].m_polygon[i*2].setY(lat);

        float b2 = (i + 1) * ADSBDemodTracker::m_maxRangeDeg;
        calcRadialEndPoint(m_azEl.getLocationSpherical().m_latitude, m_azEl.getLocationSpherical().m_longitude, distance, b2, lat, lon);
        m_coverageAirspace[k].m_polygon[i*2+1].setX(lon);
        m_coverageAirspace[k].m_polygon[i*2+1].setY(lat);

        emit coverageUpdated(k, m_coverageAirspace[k].m_polygon);
    }
}

void ADSBDemodTracker::clearCoverage()
{
    clearCoverageMap();
    for (int i = 0; i < 2; i++) {
        emit coverageUpdated(i, m_coverageAirspace[i].m_polygon);
    }
}


void ADSBDemodTracker::applyImportSettings()
{
    invalidateImportAuthentication();
    m_importTimer->setInterval(m_settings.m_importPeriod * 1000);
    if (m_settings.m_feedEnabled && m_settings.m_importEnabled) {
        m_importTimer->start();
    } else {
        m_importTimer->stop();
    }
}

bool ADSBDemodTracker::importCredentialsConfigured() const
{
    return !m_settings.m_importClientId.isEmpty() && !m_settings.m_importClientSecret.isEmpty();
}

bool ADSBDemodTracker::importAccessTokenValid() const
{
    return !m_importAccessToken.isEmpty()
        && QDateTime::currentDateTimeUtc() < m_importAccessTokenExpiry;
}

void ADSBDemodTracker::invalidateImportAuthentication()
{
    ++m_importRequestGeneration;
    m_importTokenReply.clear();
    m_importStatesReply.clear();
    m_importAccessToken.clear();
    m_importAccessTokenExpiry = QDateTime();
    m_importAuthenticationRetry = false;
}

void ADSBDemodTracker::requestImportAccessToken()
{
    static const QUrl tokenUrl(
        QStringLiteral("https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token"));

    if (!m_importNetworkManager || m_importTokenReply || m_importStatesReply) {
        return;
    }

    const auto encodeFormComponent = [](const QString& value) {
        QByteArray encoded = QUrl::toPercentEncoding(value);
        encoded.replace("%20", "+");
        return encoded;
    };
    QByteArray form("grant_type=client_credentials&client_id=");
    form.append(encodeFormComponent(m_settings.m_importClientId));
    form.append("&client_secret=");
    form.append(encodeFormComponent(m_settings.m_importClientSecret));

    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    QNetworkReply *reply = m_importNetworkManager->post(request, form);
    reply->setProperty("adsbImportReplyType", QStringLiteral("token"));
    reply->setProperty("adsbImportGeneration", m_importRequestGeneration);
    m_importTokenReply = reply;
}

void ADSBDemodTracker::sendImportRequest()
{
    if (!m_importNetworkManager || m_importTokenReply || m_importStatesReply) {
        return;
    }

    QString urlString = "https://";
    urlString = urlString + m_settings.m_importHost + "/api/states/all";
    QChar join = '?';
    if (!m_settings.m_importParameters.isEmpty())
    {
        urlString = urlString + join + m_settings.m_importParameters;
        join = '&';
    }
    if (!m_settings.m_importMinLatitude.isEmpty())
    {
        urlString = urlString + join + "lamin=" + m_settings.m_importMinLatitude;
        join = '&';
    }
    if (!m_settings.m_importMaxLatitude.isEmpty())
    {
        urlString = urlString + join + "lamax=" + m_settings.m_importMaxLatitude;
        join = '&';
    }
    if (!m_settings.m_importMinLongitude.isEmpty())
    {
        urlString = urlString + join + "lomin=" + m_settings.m_importMinLongitude;
        join = '&';
    }
    if (!m_settings.m_importMaxLongitude.isEmpty())
    {
        urlString = urlString + join + "lomax=" + m_settings.m_importMaxLongitude;
        join = '&';
    }
    QNetworkRequest request = QNetworkRequest(QUrl(urlString));
    if (importCredentialsConfigured() && importAccessTokenValid()) {
        request.setRawHeader("Authorization", "Bearer " + m_importAccessToken.toUtf8());
    }

    QNetworkReply *reply = m_importNetworkManager->get(request);
    reply->setProperty("adsbImportReplyType", QStringLiteral("states"));
    reply->setProperty("adsbImportGeneration", m_importRequestGeneration);
    m_importStatesReply = reply;
}

// Import ADS-B data from opensky-network via an API call
void ADSBDemodTracker::import()
{
    if (!m_importNetworkManager || m_importTokenReply || m_importStatesReply) {
        return;
    }

    m_importAuthenticationRetry = false;

    const bool clientIdSet = !m_settings.m_importClientId.isEmpty();
    const bool clientSecretSet = !m_settings.m_importClientSecret.isEmpty();
    if (clientIdSet != clientSecretSet)
    {
        qWarning() << "ADSBDemodTracker::import: OpenSky authentication requires both a client ID and client secret";
        return;
    }

    if (!clientIdSet || importAccessTokenValid()) {
        sendImportRequest();
    } else {
        requestImportAccessToken();
    }
}

void ADSBDemodTracker::handleImportNetworkReply(QNetworkReply *reply)
{
    if (!reply) {
        return;
    }

    if (m_importTokenReply == reply) {
        m_importTokenReply.clear();
    }
    if (m_importStatesReply == reply) {
        m_importStatesReply.clear();
    }

    const int generation = reply->property("adsbImportGeneration").toInt();
    if (generation != m_importRequestGeneration)
    {
        reply->deleteLater();
        return;
    }

    const QString replyType = reply->property("adsbImportReplyType").toString();
    if (replyType == QStringLiteral("token")) {
        handleImportTokenReply(reply);
    } else if (replyType == QStringLiteral("states")) {
        handleImportReply(reply);
    } else {
        qWarning() << "ADSBDemodTracker::handleImportNetworkReply: Unknown reply type";
    }

    reply->deleteLater();
}

void ADSBDemodTracker::handleImportTokenReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        qWarning() << "ADSBDemodTracker::handleImportTokenReply: OpenSky token request failed:"
                   << reply->errorString();
        m_importAuthenticationRetry = false;
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if ((parseError.error != QJsonParseError::NoError) || !document.isObject())
    {
        qWarning() << "ADSBDemodTracker::handleImportTokenReply: Invalid OpenSky token response:"
                   << parseError.errorString();
        m_importAuthenticationRetry = false;
        return;
    }

    const QJsonObject object = document.object();
    const QString accessToken = object.value(QStringLiteral("access_token")).toString();
    if (accessToken.isEmpty())
    {
        qWarning() << "ADSBDemodTracker::handleImportTokenReply: OpenSky token response has no access token";
        m_importAuthenticationRetry = false;
        return;
    }

    const int expiresIn = object.value(QStringLiteral("expires_in")).toInt(1800);
    const int refreshMargin = qMin(30, qMax(0, expiresIn / 10));
    m_importAccessToken = accessToken;
    m_importAccessTokenExpiry = QDateTime::currentDateTimeUtc().addSecs(qMax(1, expiresIn - refreshMargin));

    if (m_settings.m_feedEnabled && m_settings.m_importEnabled) {
        sendImportRequest();
    }
}

// Handle opensky-network API call reply
void ADSBDemodTracker::handleImportReply(QNetworkReply* reply)
{
    if (reply)
    {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if ((statusCode == 401) && importCredentialsConfigured() && !m_importAuthenticationRetry)
        {
            m_importAccessToken.clear();
            m_importAccessTokenExpiry = QDateTime();
            m_importAuthenticationRetry = true;
            requestImportAccessToken();
            return;
        }

        if (!reply->error())
        {
            QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
            if (document.isObject())
            {
                QJsonObject obj = document.object();
                if (obj.contains("time") && obj.contains("states"))
                {
                    int seconds = obj.value("time").toInt();
                    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(seconds);
                    QJsonArray states = obj.value("states").toArray();
                    for (int i = 0; i < states.size(); i++)
                    {
                        QJsonArray state = states[i].toArray();
                        int icao = state[0].toString().toInt(nullptr, 16);

                        bool newAircraft;
                        ADSBAircraft *aircraft = getAircraft(icao, newAircraft);

                        QString callsign = state[1].toString().trimmed();
                        if (!callsign.isEmpty()) {
                            setCallsign(aircraft, callsign);
                        }

                        QDateTime timePosition = dateTime;
                        if (state[3].isNull()) {
                            timePosition = dateTime.addSecs(-15); // At least 15 seconds old
                        } else {
                            timePosition = QDateTime::fromSecsSinceEpoch(state[3].toInt());
                        }
                        aircraft->m_rxTime = QDateTime::fromSecsSinceEpoch(state[4].toInt());
                        aircraft->m_updateTime = QDateTime::currentDateTime();
                        QTime time = aircraft->m_updateTime.time();
                        aircraft->setCell(ADSB_COL_TIME, QString("%1:%2:%3").arg(time.hour(), 2, 10, QLatin1Char('0')).arg(time.minute(), 2, 10, QLatin1Char('0')).arg(time.second(), 2, 10, QLatin1Char('0')));
                        aircraft->m_adsbFrameCount++;
                        aircraft->setCell(ADSB_COL_ADSB_FRAMECOUNT, aircraft->m_adsbFrameCount);

                        if (timePosition > aircraft->m_positionDateTime)
                        {
                            if (!state[5].isNull() && !state[6].isNull())
                            {
                                updateAircraftPosition(aircraft, state[6].toDouble(), state[5].toDouble(), timePosition);
                                aircraft->m_cprValid[0] = false;
                                aircraft->m_cprValid[1] = false;
                            }
                            if (!state[7].isNull())
                            {
                                aircraft->setAltitude((int)Units::metresToFeet(state[7].toDouble()), false, dateTime, m_settings);
                                aircraft->m_altitudeDateTime = timePosition;
                            }
                            if (!state[5].isNull() && !state[6].isNull() && !state[7].isNull())
                            {
                                /*QGeoCoordinate coord(aircraft->m_latitude, aircraft->m_longitude, aircraft->m_altitude);
                                aircraft->m_coordinates.push_back(QVariant::fromValue(coord));
                                aircraft->m_coordinateDateTimes.push_back(dateTime);
                                */
                                aircraft->m_positionChanged = true;
                            }
                        }
                        aircraft->m_onSurface = state[8].toBool(false);
                        aircraft->m_onSurfaceValid = true;
                        if (!state[9].isNull()) {
                            aircraft->setGroundspeed((int)std::round(Units::metresPerSecondToKnots(state[9].toDouble())), m_settings);
                        }
                        if (!state[10].isNull()) {
                            aircraft->setTrack((float)state[10].toDouble(), aircraft->m_rxTime);
                        }
                        if (!state[11].isNull()) {
                            aircraft->setVerticalRate((int)std::round(Units::metresPerSecondToFeetPerMin(state[11].toDouble())), m_settings);
                        }
                        if (!state[14].isNull())
                        {
                            aircraft->m_squawk = state[14].toString().toInt();
                            aircraft->setCell(ADSB_COL_SQUAWK, QString("%1").arg(aircraft->m_squawk, 4, 10, QLatin1Char('0')));
                        }

                        // Update aircraft in map
                        if (aircraft->m_positionValid)
                        {
                            // Check to see if we need to start any animations
                            QList<SWGSDRangel::SWGMapAnimation *> *animations = animate(dateTime, aircraft);


                            // Send to Map feature
                            sendToMap(aircraft, animations);
                        }

                        // Check to see if we need to emit a notification about this aircraft
                        checkDynamicNotification(aircraft);
                    }
                }
                else
                {
                    qDebug() << "ADSBDemodTracker::handleImportReply: Document object does not contain time and states: " << document;
                }
            }
            else
            {
                qDebug() << "ADSBDemodTracker::handleImportReply: Document is not an object: " << document;
            }
        }
        else
        {
            qWarning() << "ADSBDemodTracker::handleImportReply: OpenSky states request failed:"
                       << reply->errorString();
        }
        m_importAuthenticationRetry = false;
    }
}


QString ADSBDemodTracker::dataTimeToShortString(QDateTime dt)
{
    if (dt.isValid())
    {
        QDate currentDate = QDateTime::currentDateTimeUtc().date();
        if (dt.date() == currentDate)
        {
            return dt.time().toString("hh:mm");
        }
        else
        {
            int days = currentDate.daysTo(dt.date());
            if (days >= 0) {
                return QString("%1 +%2").arg(dt.time().toString("hh:mm")).arg(days);
            } else {
                return QString("%1 %2").arg(dt.time().toString("hh:mm")).arg(days);
            }
        }
    }
    else
    {
        return "";
    }
}


void ADSBDemodTracker::initFlightInformation()
{
    if (m_flightInformation)
    {
        disconnect(m_flightInformation, &FlightInformation::flightUpdated, this, &ADSBDemodTracker::flightInformationUpdated);
        delete m_flightInformation;
        m_flightInformation = nullptr;
    }
    if (!m_settings.m_aviationstackAPIKey.isEmpty())
    {
        m_flightInformation = FlightInformation::create(m_settings.m_aviationstackAPIKey);
        if (m_flightInformation) {
            connect(m_flightInformation, &FlightInformation::flightUpdated, this, &ADSBDemodTracker::flightInformationUpdated);
        }
    }
}


void ADSBDemodTracker::flightInformationUpdated(const FlightInformation::Flight& flight)
{
    ADSBAircraft *aircraft = findAircraftByFlight(flight.m_flightICAO);
    if (aircraft)
    {
        aircraft->setCell(ADSB_COL_FLIGHT_STATUS, flight.m_flightStatus);
        aircraft->setCell(ADSB_COL_DEP, flight.m_departureICAO);
        aircraft->setCell(ADSB_COL_ARR, flight.m_arrivalICAO);
        aircraft->setCell(ADSB_COL_STOPS, "");
        aircraft->setCell(ADSB_COL_STD, ADSBDemodTracker::dataTimeToShortString(flight.m_departureScheduled));
        aircraft->setCell(ADSB_COL_ETD, ADSBDemodTracker::dataTimeToShortString(flight.m_departureEstimated));
        aircraft->setCell(ADSB_COL_ATD, ADSBDemodTracker::dataTimeToShortString(flight.m_departureActual));
        aircraft->setCell(ADSB_COL_STA, ADSBDemodTracker::dataTimeToShortString(flight.m_arrivalScheduled));
        aircraft->setCell(ADSB_COL_ETA, ADSBDemodTracker::dataTimeToShortString(flight.m_arrivalEstimated));
        aircraft->setCell(ADSB_COL_ATA, ADSBDemodTracker::dataTimeToShortString(flight.m_arrivalActual));
        // The GUI refreshes the map label and the photo panel from the update
        markDirty(aircraft);
        emit photoRefresh((quint32) aircraft->m_icao);
    }
    else
    {
        qDebug() << "ADSBDemodTracker::flightInformationUpdated - Flight not found in ADS-B table: " << flight.m_flightICAO;
    }
}
