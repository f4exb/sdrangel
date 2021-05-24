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

#ifndef INCLUDE_ASTRONOMY_H
#define INCLUDE_ASTRONOMY_H

#include "export.h"

class QDateTime;

// Right ascension and declination
struct SDRBASE_API RADec {
    double ra;
    double dec;
};

// Azimuth and Altitude
struct SDRBASE_API AzAlt {
    double az;
    double alt;
};

class SDRBASE_API Astronomy {

public:
    static double julianDate(int year, int month, int day, int hours, int minutes, int seconds);
    static double julianDate(QDateTime dt);

    static double jd_j2000(void);
    static double jd_b1950(void);
    static double jd_now(void);

    static RADec precess(RADec rd_in, double jd_from, double jd_to);
    static AzAlt raDecToAzAlt(RADec rd, double latitude, double longitude, QDateTime dt, bool j2000=true);
    static RADec azAltToRaDec(AzAlt aa, double latitude, double longitude, QDateTime dt);

    static double localSiderealTime(QDateTime dateTime, double longitude);

    static void sunPosition(AzAlt& aa, RADec& rd, double latitude, double longitude, QDateTime dt);
    static double moonDays(QDateTime dt);
    static void moonPosition(AzAlt& aa, RADec& rd, double latitude, double longitude, QDateTime dt);

    static double refractionSaemundsson(double alt, double pressure, double temperature);
    static double refractionPAL(double alt, double pressure, double temperature, double humidity, double frequency, double latitude, double heightAboveSeaLevel, double temperatureLapseRate);

    static double raToDecimal(const QString& value);
    static double decToDecimal(const QString& value);

    static double lstAndRAToLongitude(double lst, double raHours);

    static void equatorialToGalactic(double ra, double dec, double& l, double& b);
    static void northGalacticPoleJ2000(double& ra, double& dec);

protected:
    static double modulo(double a, double b);
};

#endif // INCLUDE_ASTRONOMY_H
