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

#ifndef STARTRACKER_SPICE_H
#define STARTRACKER_SPICE_H

#include <QString>
#include <QDateTime>
#include <QVector3D>

void spiceInit();
bool spiceLock(QStringList ephemerides);
void spiceUnlock();

bool dateTimeToET(const QDateTime& dateTime, double &et);
QStringList getSPICETargets(const QString& file);
bool getAzElFromSPICE(const QString& target, double et, double latitude, double longitude, double altitudeKm, double &azimuth, double &elevation);
bool getRADecFromSPICE(const QString& target, double et, double &ra, double &dec);
bool spiceJupiterMoonPhase(const QString& moon, double et, double latitude, double longitude, double altitudeMetres, double& cml, double &phase);
bool spicePosition(const QString& name, double et, QVector3D &position);
bool spiceOrbit(const QString& name, double et, QList<QPointF> &orbit);

#endif // STARTRACKER_SPICE_H
