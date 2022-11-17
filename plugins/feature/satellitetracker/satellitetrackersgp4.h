///////////////////////////////////////////////////////////////////////////////////
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

#ifndef INCLUDE_FEATURE_SATELLITETRACKERSGP4_H_
#define INCLUDE_FEATURE_SATELLITETRACKERSGP4_H_

#include <QList>
#include <QDateTime>
#include <QGeoCoordinate>
#include <QtCharts/QLineSeries>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;
#endif

struct SatellitePass {
    QDateTime m_aos;
    QDateTime m_los;
    double m_maxElevation;              // Degrees
    double m_aosAzimuth;                // Degrees
    double m_losAzimuth;                // Degrees
    bool m_northToSouth;
};

struct SatelliteState {
    QString m_name;
    double m_latitude;                  // Degrees
    double m_longitude;                 // Degrees
    double m_altitude;                  // km
    double m_azimuth;                   // Degrees
    double m_elevation;                 // Degrees
    double m_range;                     // km
    double m_rangeRate;                 // km/s
    double m_speed;
    double m_period;
    QList<SatellitePass> m_passes;              // Used in worker and GUI threads
    QList<QGeoCoordinate *> m_groundTrack;      // These used only in worker thread
    QList<QDateTime *> m_groundTrackDateTime;
    QList<QGeoCoordinate *> m_predictedGroundTrack;
    QList<QDateTime *> m_predictedGroundTrackDateTime;
};

void getGroundTrack(QDateTime dateTime,
                        const QString& tle0, const QString& tle1, const QString& tle2,
                        int steps, QList<QGeoCoordinate>& coordinates);

void getSatelliteState(QDateTime dateTime,
                        const QString& tle0, const QString& tle1, const QString& tle2,
                        double latitude, double longitude, double altitude,
                        int predictionPeriod, int minAOSElevationDeg, int minPassElevationDeg,
                        QTime passStartTime, QTime passFinishTime, bool utc,
                        int noOfPasses, int groundTrackSteps, SatelliteState *satState);

void getPassAzEl(QLineSeries *azimuth, QLineSeries *elevation, QLineSeries *polar,
                        const QString& tle0, const QString& tle1, const QString& tle2,
                        double latitude, double longitude, double altitude,
                        const QDateTime& aos, const QDateTime& los);

bool getPassesThrough0Deg(const QString& tle0, const QString& tle1, const QString& tle2,
                          double latitude, double longitude, double altitude,
                          QDateTime& aos, QDateTime& los);

#endif // INCLUDE_FEATURE_SATELLITETRACKERSGP4_H_
