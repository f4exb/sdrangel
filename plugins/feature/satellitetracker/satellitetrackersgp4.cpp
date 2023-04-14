///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
// Copyright (C) 2013 Daniel Warner <contact@danrw.com>                          //
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

#include <CoordTopocentric.h>
#include <CoordGeodetic.h>
#include <Observer.h>
#include <SGP4.h>

#include <QTimeZone>

#include "util/units.h"

#include "satellitetrackersgp4.h"

using namespace libsgp4;

// Convert QGP4 DateTime to Qt QDataTime
static QDateTime dateTimeToQDateTime(DateTime dt)
{
    QDateTime qdt(QDate(dt.Year(), dt.Month(), dt.Day()), QTime(dt.Hour(), dt.Minute(), dt.Second(), (int)(dt.Microsecond()/1000.0)), Qt::UTC);
    return qdt;
}

// Convert Qt QDataTime to QGP4 DateTime
static DateTime qDateTimeToDateTime(QDateTime qdt)
{
    QDateTime utc = qdt.toUTC();
    QDate date = utc.date();
    QTime time = utc.time();
    DateTime dt;
    dt.Initialise(date.year(), date.month(), date.day(), time.hour(), time.minute(), time.second(), time.msec() * 1000);
    return dt;
}

// Get ground track
// Throws SatelliteException, DecayedException and TleException
void getGroundTrack(QDateTime dateTime,
                        const QString& tle0, const QString& tle1, const QString& tle2,
                        int steps, bool forward,
                        QList<QGeoCoordinate *>& coordinates,
                        QList<QDateTime *>& coordinateDateTimes)
{
    Tle tle = Tle(tle0.toStdString(), tle1.toStdString(), tle2.toStdString());
    SGP4 sgp4(tle);
    OrbitalElements ele(tle);
    double periodMins;
    double timeStep;

    // For 3D map, we want to quantize to minutes, so we replace previous
    // position data, rather than insert additional positions alongside the old
    // which can result is the camera view jumping around
    dateTime = QDateTime(dateTime.date(), QTime(dateTime.time().hour(), dateTime.time().minute()), dateTime.timeZone());

    // Note 2D map doesn't support paths wrapping around Earth several times
    // So we just have a slight overlap here, with the future track being longer
    DateTime currentTime = qDateTimeToDateTime(dateTime);
    DateTime endTime;
    if (forward)
    {
        periodMins = ele.Period() * 0.9;
        endTime = currentTime.AddMinutes(periodMins);
        timeStep = periodMins / (steps * 0.9);
    }
    else
    {
        periodMins = ele.Period() * 0.4;
        endTime = currentTime.AddMinutes(-periodMins);
        timeStep = -periodMins / (steps * 0.4);
    }

    // Quantize time step to 30 seconds
    timeStep *= 2.0;
    if (timeStep > 0.0) {
        timeStep = std::max(timeStep, 1.0);
    } else if (timeStep < 0.0) {
        timeStep = std::min(timeStep, -1.0);
    }
    timeStep = round(timeStep);
    timeStep /= 2.0;

    while ((forward && (currentTime < endTime)) || (!forward && (currentTime > endTime)))
    {
        // Calculate satellite position
        Eci eci = sgp4.FindPosition(currentTime);

        // Convert satellite position to geodetic coordinates (lat and long)
        CoordGeodetic geo = eci.ToGeodetic();

        QGeoCoordinate *coord = new QGeoCoordinate(Units::radiansToDegrees(geo.latitude),
                                                   Units::radiansToDegrees(geo.longitude),
                                                   geo.altitude * 1000.0);
        coordinates.append(coord);

        QDateTime *coordDateTime = new QDateTime(dateTimeToQDateTime(currentTime));
        coordinateDateTimes.append(coordDateTime);

        // 2D map is stretched at poles, so use finer steps
        if (std::abs(Units::radiansToDegrees(geo.latitude)) >= 70)
            currentTime = currentTime.AddMinutes(timeStep/4);
        else
            currentTime = currentTime.AddMinutes(timeStep);
    }
}

// Find azimuth and elevation points during a pass
void getPassAzEl(QLineSeries* azimuth, QLineSeries* elevation, QLineSeries* polar,
                        const QString& tle0, const QString& tle1, const QString& tle2,
                        double latitude, double longitude, double altitude,
                        const QDateTime& aos, const QDateTime& los)
{
    try
    {
        Tle tle = Tle(tle0.toStdString(), tle1.toStdString(), tle2.toStdString());
        SGP4 sgp4(tle);
        Observer obs(latitude, longitude, altitude);

        DateTime aosTime = qDateTimeToDateTime(aos);
        DateTime losTime = qDateTimeToDateTime(los);
        DateTime currentTime(aosTime);
        int steps = 150; // Needs to be high enough, so rotator intersect with satellite position

        double timeStep = (losTime - aosTime).TotalSeconds() / steps;
        if (timeStep <= 0.0)
        {
            qDebug() << "getPassAzEl: AOS is the same as or after LOS";
            return;
        }

        while (currentTime <= losTime)
        {
            // Calculate satellite position
            Eci eci = sgp4.FindPosition(currentTime);

            // Calculate angle to satellite from antenna
            CoordTopocentric topo = obs.GetLookAngle(eci);

            // Save azimuth and elevation in series
            QDateTime qdt = dateTimeToQDateTime(currentTime);
            if (azimuth != nullptr)
                azimuth->append(qdt.toMSecsSinceEpoch(), Units::radiansToDegrees(topo.azimuth));
            if (elevation != nullptr)
                elevation->append(qdt.toMSecsSinceEpoch(), Units::radiansToDegrees(topo.elevation));
            if (polar != nullptr)
                polar->append(Units::radiansToDegrees(topo.azimuth), 90.0-Units::radiansToDegrees(topo.elevation));

            currentTime = currentTime.AddSeconds(timeStep);
        }
    }
    catch (SatelliteException& se)
    {
        qDebug() << se.what();
    }
    catch (DecayedException& de)
    {
        qDebug() << de.what();
    }
    catch (TleException& tlee)
    {
        qDebug() << tlee.what();
    }
}

// Get whether a pass passes through 0 degreees
bool getPassesThrough0Deg(const QString& tle0, const QString& tle1, const QString& tle2,
                          double latitude, double longitude, double altitude,
                          QDateTime& aos, QDateTime& los)
{
    try
    {
        Tle tle = Tle(tle0.toStdString(), tle1.toStdString(), tle2.toStdString());
        SGP4 sgp4(tle);
        Observer obs(latitude, longitude, altitude);

        DateTime aosTime = qDateTimeToDateTime(aos);
        DateTime losTime = qDateTimeToDateTime(los);
        DateTime currentTime(aosTime);
        int steps = 20;

        double timeStep = (losTime - aosTime).TotalSeconds() / steps;

        double prevAz;
        for (int i = 0; i < steps; i++)
        {
            // Calculate satellite position
            Eci eci = sgp4.FindPosition(currentTime);

            // Calculate angle to satellite from antenna
            CoordTopocentric topo = obs.GetLookAngle(eci);

            double az = Units::radiansToDegrees(topo.azimuth);
            if (i == 0)
                prevAz = az;

            // Does it cross 0 degrees?
            if (((prevAz > 270.0) && (az < 90.0)) || ((prevAz < 90.0) && (az >= 270.0)))
                return true;

            prevAz = az;
            currentTime = currentTime.AddSeconds(timeStep);
        }
    }
    catch (SatelliteException& se)
    {
        qDebug() << se.what();
    }
    catch (DecayedException& de)
    {
        qDebug() << de.what();
    }
    catch (TleException& tlee)
    {
        qDebug() << tlee.what();
    }
    return false;
}

// Find maximum elevation in a pass
static double findMaxElevation(Observer& obs1, SGP4& sgp4, const DateTime& aos, const DateTime& los)
{
    Observer obs(obs1.GetLocation());
    bool running;
    double timeStep = (los - aos).TotalSeconds() / 9.0;
    DateTime currentTime(aos);
    DateTime time1(aos);
    DateTime time2(los);
    double maxElevation;

    do
    {
        running = true;
        maxElevation = -INFINITY;
        while (running && (currentTime < time2))
        {
            Eci eci = sgp4.FindPosition(currentTime);
            CoordTopocentric topo = obs.GetLookAngle(eci);
            if (topo.elevation > maxElevation)
            {
                maxElevation = topo.elevation;
                currentTime = currentTime.AddSeconds(timeStep);
                if (currentTime > time2)
                    currentTime = time2;
            }
            else
                running = false;
        }
        time1 = currentTime.AddSeconds(-2.0 * timeStep);
        time2 = currentTime;
        currentTime = time1;
        timeStep = (time2 - time1).TotalSeconds() / 9.0;
    }
    while (timeStep > 1.0);

    return Units::radiansToDegrees(maxElevation);
}

// Find the time at which the satellite crossed the minimum elevation required for AOS or LOS
static DateTime findCrossingPoint(Observer& obs, SGP4& sgp4, const DateTime& initialTime1, const DateTime& initialTime2, double minElevation, bool findingAOS)
{
    bool running;
    int cnt;
    DateTime time1(initialTime1);
    DateTime time2(initialTime2);
    DateTime middleTime;

    running = true;
    cnt = 0;
    while (running && (cnt++ < 16))
    {
        middleTime = time1.AddSeconds((time2 - time1).TotalSeconds() / 2.0);
        Eci eci = sgp4.FindPosition(middleTime);
        CoordTopocentric topo = obs.GetLookAngle(eci);
        if (topo.elevation > minElevation)
        {
            if (findingAOS)
                time2 = middleTime;
            else
                time1 = middleTime;
        }
        else
        {
            if (findingAOS)
                time1 = middleTime;
            else
                time2 = middleTime;
        }
        if ((time2 - time1).TotalSeconds() < 1.0)
        {
            running = false;
            int us = middleTime.Microsecond();
            middleTime = middleTime.AddMicroseconds(-us);
            middleTime = middleTime.AddSeconds(findingAOS ? 1 : -1);
        }
    }
    running = true;
    cnt = 0;
    while (running && (cnt++ < 6))
    {
        Eci eci = sgp4.FindPosition(middleTime);
        CoordTopocentric topo = obs.GetLookAngle(eci);
        if (topo.elevation > minElevation)
            middleTime = middleTime.AddSeconds(findingAOS ? -1 : 1);
        else
            running = false;
    }
    return middleTime;
}

// Find when AOS occured, by stepping backwards
static DateTime findAOSBackwards(Observer& obs, SGP4& sgp4, DateTime& startTime,
                                int predictionPeriod, double minElevation, bool& aosUnknown)
{
    DateTime previousTime(startTime);
    DateTime currentTime(startTime);
    DateTime endTime(startTime.AddDays(-predictionPeriod));

    while (currentTime >= endTime)
    {
        Eci eci = sgp4.FindPosition(currentTime);
        CoordTopocentric topo = obs.GetLookAngle(eci);
        if (topo.elevation < minElevation)
        {
            aosUnknown = false;
            return findCrossingPoint(obs, sgp4, currentTime, previousTime, minElevation, true);
        }
        previousTime = currentTime;
        currentTime = currentTime - TimeSpan(0, 0, 180);
    }
    aosUnknown = true;
    return currentTime;
}

bool inPassWindow(DateTime dateTime, QTime passStartTime, QTime passEndTime, bool utc)
{
    // Don't compare seconds as not currently settable in GUI
    QDateTime qdt = dateTimeToQDateTime(dateTime);
    if (!utc)
        qdt = qdt.toLocalTime();
    QTime qt(qdt.time().hour(), qdt.time().minute());
    passStartTime = QTime(passStartTime.hour(), passStartTime.minute());
    passEndTime = QTime(passEndTime.hour(), passEndTime.minute());
    // If passEndTime is before passStartTime, then we allow overnight passes
    if (passEndTime > passStartTime)
    {
        return (qt >= passStartTime) && (qt <= passEndTime);
    }
    else
    {
        return (qt <= passEndTime) || (qt >= passStartTime);
    }
}

// Create a list of satellite passes, between the given start and end times, that exceed the specified minimum elevation
// We return an uninitalised QDateTime if AOS or LOS is outside of predictionPeriod
static QList<SatellitePass> createPassList(Observer& obs, SGP4& sgp4, DateTime& startTime,
                                            int predictionPeriod, double minAOSElevation, double minPassElevationDeg,
                                            QTime passStartTime, QTime passEndTime, bool utc,
                                            int noOfPasses)
{
    QList<SatellitePass> passes;
    bool aos = false;
    bool aosUnknown = true;
    double aosAz;
    double losAz;
    DateTime previousTime(startTime);
    DateTime currentTime(startTime);
    DateTime endTime(startTime.AddDays(predictionPeriod));
    DateTime aosTime;
    DateTime losTime;

    while (currentTime < endTime)
    {
        bool endOfPass = false;
        Eci eci = sgp4.FindPosition(currentTime);
        CoordTopocentric topo = obs.GetLookAngle(eci);

        if (!aos && (topo.elevation > minAOSElevation))
        {
            if (startTime == currentTime)
            {
                // AOS is before startTime
                aosTime = findAOSBackwards(obs, sgp4, startTime, predictionPeriod, minAOSElevation, aosUnknown);
            }
            else
            {
                aosTime = findCrossingPoint(obs, sgp4, previousTime, currentTime, minAOSElevation, true);
                aosUnknown = false;
            }
            aos = true;
            eci = sgp4.FindPosition(aosTime);
            topo = obs.GetLookAngle(eci);
            aosAz = Units::radiansToDegrees(topo.azimuth);
        }
        else if (aos && (topo.elevation < minAOSElevation))
        {
            aos = false;
            endOfPass = true;
            losTime = findCrossingPoint(obs, sgp4, previousTime, currentTime, minAOSElevation, false);
            eci = sgp4.FindPosition(losTime);
            topo = obs.GetLookAngle(eci);
            losAz = Units::radiansToDegrees(topo.azimuth);
            double maxElevationDeg = findMaxElevation(obs, sgp4, aosTime, losTime);
            if ((maxElevationDeg >= minPassElevationDeg)
                && inPassWindow(aosTime, passStartTime, passEndTime, utc)
                && inPassWindow(losTime, passStartTime, passEndTime, utc))
            {
                SatellitePass pass;
                pass.m_aos = aosUnknown ? QDateTime() : dateTimeToQDateTime(aosTime);
                pass.m_los = dateTimeToQDateTime(losTime);
                pass.m_maxElevation = maxElevationDeg;
                pass.m_aosAzimuth = aosAz;
                pass.m_losAzimuth = losAz;
                pass.m_northToSouth = std::min(360.0-aosAz, aosAz-0.0) < std::min(360.0-losAz, losAz-0.0);
                passes.append(pass);
                noOfPasses--;
                if (noOfPasses <= 0)
                    return passes;
            }
        }
        previousTime = currentTime;
        if (endOfPass)
            currentTime = currentTime + TimeSpan(0, 30, 0); // 30 minutes - no orbit likely to be that fast
        else
            currentTime = currentTime + TimeSpan(0, 0, 180);
        if (currentTime > endTime)
            currentTime = endTime;
    }
    if (aos)
    {
        // Pass still in progress at end time
        Eci eci = sgp4.FindPosition(currentTime);
        CoordTopocentric topo = obs.GetLookAngle(eci);
        losAz = Units::radiansToDegrees(topo.azimuth);
        double maxElevationDeg = findMaxElevation(obs, sgp4, aosTime, losTime);
        if ((maxElevationDeg >= minPassElevationDeg)
            && inPassWindow(aosTime, passStartTime, passEndTime, utc)
            && inPassWindow(losTime, passStartTime, passEndTime, utc))
        {
            SatellitePass pass;
            pass.m_aos = aosUnknown ? QDateTime() : dateTimeToQDateTime(aosTime);
            pass.m_los = QDateTime();
            pass.m_aosAzimuth = aosAz;
            pass.m_losAzimuth = losAz;
            pass.m_maxElevation = maxElevationDeg;
            pass.m_northToSouth = std::min(360.0-aosAz, aosAz-0.0) < std::min(360.0-losAz, losAz-0.0);
            passes.append(pass);
        }
    }

    return passes;
}

void getSatelliteState(QDateTime dateTime,
                        const QString& tle0, const QString& tle1, const QString& tle2,
                        double latitude, double longitude, double altitude,
                        int predictionPeriod, int minAOSElevationDeg, int minPassElevationDeg,
                        QTime passStartTime, QTime passFinishTime, bool utc,
                        int noOfPasses, int groundTrackSteps, SatelliteState *satState)
{
    try
    {
        Tle tle = Tle(tle0.toStdString(), tle1.toStdString(), tle2.toStdString());
        SGP4 sgp4(tle);
        Observer obs(latitude, longitude, altitude);

        DateTime dt = qDateTimeToDateTime(dateTime);

        // Calculate satellite position
        Eci eci = sgp4.FindPosition(dt);

        // Calculate angle to satellite from antenna
        CoordTopocentric topo = obs.GetLookAngle(eci);

        // Convert satellite position to geodetic coordinates (lat and long)
        CoordGeodetic geo = eci.ToGeodetic();

        satState->m_latitude = Units::radiansToDegrees(geo.latitude);
        satState->m_longitude = Units::radiansToDegrees(geo.longitude);
        satState->m_altitude = geo.altitude;
        satState->m_azimuth = Units::radiansToDegrees(topo.azimuth);
        satState->m_elevation = Units::radiansToDegrees(topo.elevation);
        satState->m_range = topo.range;
        satState->m_rangeRate = topo.range_rate;
        OrbitalElements ele(tle);
        satState->m_speed = eci.Velocity().Magnitude();
        satState->m_period = ele.Period();
        if (noOfPasses > 0)
        {
            satState->m_passes.clear();
            satState->m_passes = createPassList(obs, sgp4, dt, predictionPeriod,
                                                Units::degreesToRadians((double)minAOSElevationDeg),
                                                minPassElevationDeg,
                                                passStartTime, passFinishTime, utc,
                                                noOfPasses);
        }

        qDeleteAll(satState->m_groundTrack);
        satState->m_groundTrack.clear();
        qDeleteAll(satState->m_groundTrackDateTime);
        satState->m_groundTrackDateTime.clear();
        qDeleteAll(satState->m_predictedGroundTrack);
        satState->m_predictedGroundTrack.clear();
        qDeleteAll(satState->m_predictedGroundTrackDateTime);
        satState->m_predictedGroundTrackDateTime.clear();
        getGroundTrack(dateTime, tle0, tle1, tle2, groundTrackSteps, false, satState->m_groundTrack, satState->m_groundTrackDateTime);
        getGroundTrack(dateTime, tle0, tle1, tle2, groundTrackSteps, true, satState->m_predictedGroundTrack, satState->m_predictedGroundTrackDateTime);
    }
    catch (SatelliteException& se)
    {
        qDebug() << "getSatelliteState:SatelliteException " << satState->m_name << ": " << se.what();
    }
    catch (DecayedException& de)
    {
        qDebug() << "getSatelliteState:DecayedException " << satState->m_name << ": " << de.what();
    }
    catch (TleException& tlee)
    {
        qDebug() << "getSatelliteState:TleException " << satState->m_name << ": " << tlee.what() << "\n" << tle0 << "\n" << tle1 << "\n" << tle2;
    }
}
