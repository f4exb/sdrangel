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

#include <QDateTime>

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
    static double modifiedJulianDate(QDateTime dt);

    static double jd_j2000(void);
    static double jd_b1950(void);
    static double jd_now(void);

    static RADec precess(RADec rd_in, double jd_from, double jd_to);
    static AzAlt raDecToAzAlt(RADec rd, double latitude, double longitude, QDateTime dt, bool j2000=true);
    static RADec azAltToRaDec(AzAlt aa, double latitude, double longitude, QDateTime dt);

    static void azAltToXY85(AzAlt aa, double& x, double& y);
    static void azAltToXY30(AzAlt aa, double& x, double& y);
    static AzAlt xy85ToAzAlt(double x, double y);
    static AzAlt xy30ToAzAlt(double x, double y);

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
    static void galacticToEquatorial(double l, double b, double& ra, double& dec);

    static double dopplerToVelocity(double f, double f0);
    static double velocityToDoppler(double v, double f0);

    static double earthRotationVelocity(RADec rd, double latitude, double longitude, QDateTime dt);
    static double earthOrbitVelocityBCRS(RADec rd, QDateTime dt);
    static double sunVelocityLSRK(RADec rd);
    static double observerVelocityLSRK(RADec rd, double latitude, double longitude, QDateTime dt);

    static double noisePowerdBm(double temp, double bw);
    static double noiseTemp(double dBm, double bw);

    static double modulo(double a, double b);

    static const double m_boltzmann;
    static const double m_hydrogenLineFrequency;
    static const double m_hydroxylLineFrequency;
    static const double m_deuteriumLineFrequency;
    static const double m_speedOfLight;
    static const double m_hydrogenMass;
};

#endif // INCLUDE_ASTRONOMY_H
