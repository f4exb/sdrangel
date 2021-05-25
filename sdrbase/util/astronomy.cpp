///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
// Copyright (C) 2004 Rutherford Appleton Laboratory                             //
// Copyright (C) 2012 Science and Technology Facilities Council.                 //
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

#include <QRegExp>
#include <QDateTime>
#include <QDebug>

#include "util/units.h"
#include "astronomy.h"

// Function prototypes
static void palRefz(double zu, double refa, double refb, double *zr);
static void palRefco (double hm, double tdk, double pmb, double rh,
                double wl, double phi, double tlr, double eps,
                double *refa, double *refb);
static void palRefro( double zobs, double hm, double tdk, double pmb,
               double rh, double wl, double phi, double tlr,
               double eps, double * ref);


// Calculate Julian date (days since January 1, 4713 BC) from Gregorian calendar date
double Astronomy::julianDate(int year, int month, int day, int hours, int minutes, int seconds)
{
    int julian_day;
    double julian_date;

    // From: https://en.wikipedia.org/wiki/Julian_day
    julian_day = (1461 * (year + 4800 + (month - 14)/12))/4 +(367 * (month - 2 - 12 * ((month - 14)/12)))/12 - (3 * ((year + 4900 + (month - 14)/12)/100))/4 + day - 32075;
    julian_date = julian_day + (hours/24.0 - 0.5) + minutes/(24.0*60.0) + seconds/(24.0*60.0*60.0);

    return julian_date;
}

// Calculate Julian date
double Astronomy::julianDate(QDateTime dt)
{
    QDateTime utc = dt.toUTC();
    QDate date = utc.date();
    QTime time = utc.time();
    return julianDate(date.year(), date.month(), date.day(), time.hour(), time.minute(), time.second());
}

// Get Julian date of J2000 Epoch
double Astronomy::jd_j2000(void)
{
    static double j2000 = 0.0;

    if (j2000 == 0.0) {
        j2000 = julianDate(2000, 1, 1, 12, 0, 0);
    }
    return j2000;
}

// Get Julian date of B1950 Epoch
double Astronomy::jd_b1950(void)
{
    static double b1950 = 0.0;

    if (b1950 == 0.0) {
        b1950 = julianDate(1949, 12, 31, 22, 9, 0);
    }
    return b1950;
}

// Get Julian date of current time Epoch
double Astronomy::jd_now(void)
{
    time_t system_time;
    struct tm *utc_time;

    // Get current time in seconds since Unix Epoch (1970)
    time(&system_time);
    // Convert to UTC (GMT)
    utc_time = gmtime(&system_time);

    // Convert to Julian date
    return julianDate(utc_time->tm_year + 1900, utc_time->tm_mon + 1, utc_time->tm_mday,
                      utc_time->tm_hour, utc_time->tm_min, utc_time->tm_sec);
}

// Precess a RA/DEC between two given Epochs
RADec Astronomy::precess(RADec rd_in, double jd_from, double jd_to)
{
    RADec rd_out;
    double x, y, z;
    double xp, yp, zp;
    double ra_rad, dec_rad;
    double rot[3][3];      // [row][col]
    double ra_deg;

    double days_per_century = 36524.219878;
    double t0 = (jd_from - jd_b1950())/days_per_century; // Tropical centuries since B1950.0
    double t = (jd_to - jd_from)/days_per_century;     // Tropical centuries from starting epoch to ending epoch

    // From: https://www.cloudynights.com/topic/561254-ra-dec-epoch-conversion/
    rot[0][0] = 1.0 - ((29696.0 + 26.0*t0)*t*t - 13.0*t*t*t)*.00000001;
    rot[1][0] = ((2234941.0 + 1355.0*t0)*t - 676.0*t*t + 221.0*t*t*t)*.00000001;
    rot[2][0] = ((971690.0 - 414.0*t0)*t + 207.0*t*t + 96.0*t*t*t)*.00000001;
    rot[0][1] = -rot[1][0];
    rot[1][1] = 1.0 - ((24975.0 + 30.0*t0)*t*t - 15.0*t*t*t)*.00000001;
    rot[2][1] = -((10858.0 + 2.0*t0)*t*t)*.00000001;
    rot[0][2] = -rot[2][0];
    rot[1][2] = rot[2][1];
    rot[2][2] = 1.0 - ((4721.0 - 4.0*t0)*t*t)*.00000001;

    // Hours to degrees
    ra_deg = rd_in.ra*(360.0/24.0);

    // Convert to rectangular coordinates
    ra_rad = Units::degreesToRadians(ra_deg);
    dec_rad = Units::degreesToRadians(rd_in.dec);
    x = cos(ra_rad) * cos(dec_rad);
    y = sin(ra_rad) * cos(dec_rad);
    z = sin(dec_rad);

    // Rotate
    xp = rot[0][0]*x + rot[0][1]*y + rot[0][2]*z;
    yp = rot[1][0]*x + rot[1][1]*y + rot[1][2]*z;
    zp = rot[2][0]*x + rot[2][1]*y + rot[2][2]*z;

    // Convert back to spherical coordinates
    rd_out.ra = Units::radiansToDegrees(atan(yp/xp));
    if (xp < 0.0) {
        rd_out.ra += 180.0;
    } else if ((yp < 0) && (xp > 0)) {
        rd_out.ra += 360.0;
    }
    rd_out.dec = Units::radiansToDegrees(asin(zp));

    // Degrees to hours
    rd_out.ra /= (360.0/24.0);

    return rd_out;
}

// Calculate local mean sidereal time (LMST) in degrees
double Astronomy::localSiderealTime(QDateTime dateTime, double longitude)
{
    double jd = julianDate(dateTime);

    double d = (jd - jd_j2000()); // Days since J2000 epoch (including fraction)
    double f = fmod(jd, 1.0); // Fractional part is decimal days
    double ut = (f+0.5)*24.0; // Universal time in decimal hours

    // https://astronomy.stackexchange.com/questions/24859/local-sidereal-time
    // 100.46 is offset for GMST at 0h UT on 1 Jan 2000
    // 0.985647 is number of degrees per day over 360 the Earth rotates in one solar day
    // Approx to 0.3 arcseconds
    return fmod(100.46 + 0.985647 * d + longitude + (360/24) * ut, 360.0);
}

// Convert from J2000 right ascension (decimal hours) and declination (decimal degrees) to altitude and azimuth, for location (decimal degrees) and time
AzAlt Astronomy::raDecToAzAlt(RADec rd, double latitude, double longitude, QDateTime dt, bool j2000)
{
    AzAlt aa;
    double ra_deg; // Right ascension in degrees
    double lst_deg; // Local sidereal time in degrees
    double ha; // Hour angle
    double a, az;
    double dec_rad, lat_rad, ha_rad, alt_rad; // Corresponding variables as radians

    double jd = julianDate(dt);

    // Precess RA/DEC from J2000 Epoch to desired (typically current) Epoch
    if (j2000)
        rd = precess(rd, jd_j2000(), jd);

    // Calculate local mean sidereal time (LMST) in degrees
    // https://astronomy.stackexchange.com/questions/24859/local-sidereal-time
    // 100.46 is offset for GMST at 0h UT on 1 Jan 2000
    // 0.985647 is number of degrees per day over 360 the Earth rotates in one solar day
    // Approx to 0.3 arcseconds
    lst_deg = Astronomy::localSiderealTime(dt, longitude);

    // Convert right ascension from hours to degrees
    ra_deg = rd.ra * (360.0/24.0);

    // Calculate hour angle
    ha = fmod(lst_deg - ra_deg, 360.0);

    // Convert degrees to radians
    dec_rad = Units::degreesToRadians(rd.dec);
    lat_rad = Units::degreesToRadians(latitude);
    ha_rad = Units::degreesToRadians(ha);

    // Calculate altitude and azimuth - no correction for atmospheric refraction
    // From: http://www.convertalot.com/celestial_horizon_co-ordinates_calculator.html
    alt_rad = asin(sin(dec_rad)*sin(lat_rad) + cos(dec_rad)*cos(lat_rad)*cos(ha_rad));
    a = Units::radiansToDegrees(acos((sin(dec_rad)-sin(alt_rad)*sin(lat_rad)) / (cos(alt_rad)*cos(lat_rad))));
    if (sin(ha_rad) < 0.0) {
        az = a;
    } else {
        az = 360.0 - a;
    }

    aa.alt = Units::radiansToDegrees(alt_rad);
    aa.az = az;
    return aa;
}

// Convert from altitude and azimuth, for location (decimal degrees) and time, to Jnow right ascension (decimal hours) and declination (decimal degrees)
// See: http://jonvoisey.net/blog/2018/07/data-converting-alt-az-to-ra-dec-example/
RADec Astronomy::azAltToRaDec(AzAlt aa, double latitude, double longitude, QDateTime dt)
{
    RADec rd;
    double lst_deg; // Local sidereal time
    double ha_rad, ha_deg; // Hour angle
    double alt_rad, az_rad, lat_rad, dec_rad; // Corresponding variables as radians

    // Calculate local mean sidereal time (LMST) in degrees (see raDecToAzAlt)
    lst_deg = Astronomy::localSiderealTime(dt, longitude);

    // Convert degrees to radians
    alt_rad = Units::degreesToRadians(aa.alt);
    az_rad = Units::degreesToRadians(aa.az);
    lat_rad = Units::degreesToRadians(latitude);

    // Calculate declination
    dec_rad = asin(sin(lat_rad)*sin(alt_rad)+cos(lat_rad)*cos(alt_rad)*cos(az_rad));

    // Calculate hour angle
    double quotient = (sin(alt_rad)-sin(lat_rad)*sin(dec_rad))/(cos(lat_rad)*cos(dec_rad));
    // At extreme altitudes, we seem to get small numerical errors that causes values to be out of range, so clip to [-1,1]
    if (quotient < -1.0) {
        ha_rad = acos(-1.0);
    } else if (quotient > 1.0) {
        ha_rad = acos(1.0);
    } else {
        ha_rad = acos(quotient);
    }

    // Convert radians to degrees
    rd.dec = Units::radiansToDegrees(dec_rad);
    ha_deg = Units::radiansToDegrees(ha_rad);

    // Calculate right ascension in decimal hours
    rd.ra = modulo((lst_deg - ha_deg) / (360.0/24.0), 24.0);

    return rd;
}

// Needs to work for negative a
double Astronomy::modulo(double a, double b)
{
    return a - b * floor(a/b);
}

// Calculate azimuth and altitude angles to the sun from the given latitude and longitude at the given time
// Refer to:
// https://en.wikipedia.org/wiki/Position_of_the_Sun
// https://www.aa.quae.nl/en/reken/zonpositie.html
// Said to be accurate to .01 degree (36") for dates between 1950 and 2050
// although we use slightly more accurate constants and an extra term in the equation
// of centre from the second link
// For an alternative, see: http://www.psa.es/sdg/sunpos.htm
// Most accurate algorithm is supposedly: https://midcdmz.nrel.gov/spa/
void Astronomy::sunPosition(AzAlt& aa, RADec& rd, double latitude, double longitude, QDateTime dt)
{
    double jd = julianDate(dt);
    double n = (jd - jd_j2000()); // Days since J2000 epoch (including fraction)

    double l = 280.461 + 0.9856474 * n; // Mean longitude of the Sun, corrected for the abberation of light
    double g = 357.5291 + 0.98560028 * n; // Mean anomaly of the Sun - how far around orbit from perihlion, in degrees

    l = modulo(l, 360.0);
    g = modulo(g, 360.0);

    double gr = Units::degreesToRadians(g);

    double la = l + 1.9148 * sin(gr) + 0.0200 * sin(2.0*gr) + 0.0003 * sin(3.0*gr); // Ecliptic longitude (Ecliptic latitude b set to 0)

    // Convert la, b=0, which give the position of the Sun in the ecliptic coordinate sytem, to
    // equatorial coordinates

    double e = 23.4393 - 3.563E-7 * n; // Obliquity of the ecliptic - tilt of Earth's axis of rotation

    double er = Units::degreesToRadians(e);
    double lr = Units::degreesToRadians(la);

    double a = atan2(cos(er) * sin(lr), cos(lr)); // Right ascension, radians
    double d = asin(sin(er) * sin(lr)); // Declination, radians

    rd.ra = modulo(Units::radiansToDegrees(a), 360.0) / (360.0/24.0); // Convert to hours
    rd.dec = Units::radiansToDegrees(d); // Convert to degrees

    aa = raDecToAzAlt(rd, latitude, longitude, dt, false);
}

double Astronomy::moonDays(QDateTime dt)
{
    QDateTime utc = dt.toUTC();
    QDate date = utc.date();
    QTime time = utc.time();

    int y = date.year();
    int m = date.month();
    int D = date.day();
    int d = 367 * y - 7 * (y + (m+9)/12) / 4 - 3 * ((y + (m-9)/7) / 100 + 1) / 4 + 275*m/9 + D - 730515;

    return d + time.hour()/24.0 + time.minute()/(24.0*60.0) + time.second()/(24.0*60.0*60.0);
}

// Calculate azimuth and altitude angles to the moon from the given latitude and longitude at the given time
// Refer to: https://stjarnhimlen.se/comp/ppcomp.html
// Accurate to 4 arcminute
void Astronomy::moonPosition(AzAlt& aa, RADec& rd, double latitude, double longitude, QDateTime dt)
{
    double d = moonDays(dt);

    double ecl = Units::degreesToRadians(23.4393 - 3.563E-7 * d); // Obliquity of the ecliptic - tilt of Earth's axis of rotation

    // Orbital elements for the Sun
    double ws = Units::degreesToRadians(282.9404 + 4.70935E-5 * d);
    double Ms = Units::degreesToRadians(356.0470 + 0.9856002585 * d);

    // Orbital elements for the Moon
    double Nm = Units::degreesToRadians(125.1228 - 0.0529538083 * d);           // longitude of the ascending node
    double im = Units::degreesToRadians(5.1454);                                // inclination to the ecliptic (plane of the Earth's orbit)
    double wm = Units::degreesToRadians(318.0634 + 0.1643573223 * d);           // argument of perihelion
    double am = 60.2666;                                                         // (Earth radii) semi-major axis, or mean distance from Sun
    double em = 0.054900;      // ecm                                                 // eccentricity (0=circle, 0-1=ellipse, 1=parabola)
    double Mm = Units::degreesToRadians(115.3654 + 13.0649929509 * d);          // mean anomaly (0 at perihelion; increases uniformly with time), degrees

    double Em = Mm + em * sin(Mm) * (1.0 + em * cos(Mm)); // Eccentric anomaly in radians

    double xv = am * (cos(Em) - em);
    double yv = am * (sqrt(1.0 - em*em) * sin(Em));

    double vm = atan2(yv, xv); // True anomaly
    double rm = sqrt(xv*xv + yv*yv); // Distance

    // Compute position in space (for the Moon, this is geocentric)
    double xh = rm * (cos(Nm) * cos(vm+wm) - sin(Nm) * sin(vm+wm) * cos(im));
    double yh = rm * (sin(Nm) * cos(vm+wm) + cos(Nm) * sin(vm+wm) * cos(im));
    double zh = rm * (sin(vm+wm) * sin(im));

    // Convert to ecliptic longitude and latitude
    double lonecl = atan2(yh, xh);
    double latecl = atan2(zh, sqrt(xh*xh+yh*yh));

    // Perturbations of the Moon

    double Ls = Ms + ws;                // Mean Longitude of the Sun  (Ns=0)
    double Lm = Mm + wm + Nm;           // Mean longitude of the Moon
    double D = Lm - Ls;                 // Mean elongation of the Moon
    double F = Lm - Nm;                 // Argument of latitude for the Moon

    double dlon;
    dlon = -1.274 * sin(Mm - 2*D);           // (the Evection)
    dlon += +0.658 * sin(2*D);               // (the Variation)
    dlon += -0.186 * sin(Ms);                // (the Yearly Equation)
    dlon += -0.059 * sin(2*Mm - 2*D);
    dlon += -0.057 * sin(Mm - 2*D + Ms);
    dlon += +0.053 * sin(Mm + 2*D);
    dlon += +0.046 * sin(2*D - Ms);
    dlon += +0.041 * sin(Mm - Ms);
    dlon += -0.035 * sin(D);                 // (the Parallactic Equation)
    dlon += -0.031 * sin(Mm + Ms);
    dlon += -0.015 * sin(2*F - 2*D);
    dlon += +0.011 * sin(Mm - 4*D);

    double dlat;
    dlat = -0.173 * sin(F - 2*D);
    dlat += -0.055 * sin(Mm - F - 2*D);
    dlat += -0.046 * sin(Mm + F - 2*D);
    dlat += +0.033 * sin(F + 2*D);
    dlat += +0.017 * sin(2*Mm + F);

    lonecl += Units::degreesToRadians(dlon);
    latecl += Units::degreesToRadians(dlat);

    rm += -0.58 * cos(Mm - 2*D);
    rm += -0.46 * cos(2*D);

    // Convert perturbed
    xh = rm * cos(lonecl) * cos(latecl);
    yh = rm * sin(lonecl) * cos(latecl);
    zh = rm               * sin(latecl);

    // Convert to geocentric coordinates (already the case for the Moon)
    double xg = xh;
    double yg = yh;
    double zg = zh;

    // Convert to equatorial cordinates
    double xe = xg;
    double ye = yg * cos(ecl) - zg * sin(ecl);
    double ze = yg * sin(ecl) + zg * cos(ecl);

    // Compute right ascension and declination
    double ra = atan2(ye, xe);
    double dec = atan2(ze, sqrt(xe*xe+ye*ye));

    rd.ra = modulo(Units::radiansToDegrees(ra), 360.0) / (360.0/24.0); // Convert to hours
    rd.dec = Units::radiansToDegrees(dec); // Convert to degrees

    // Convert from geocentric to topocentric
    double mpar = asin(1/rm);

    double lat = Units::degreesToRadians(latitude);
    double gclat = Units::degreesToRadians(latitude - 0.1924 * sin(2.0 * lat));
    double rho = 0.99833 + 0.00167 * cos(2*lat);

    QTime time = dt.toUTC().time();
    double UT = time.hour() + time.minute()/60.0 + time.second()/(60.0*60.0);

    double GMST0 = Units::radiansToDegrees(Ls)/15.0 + 12; // In hours
    double GMST = GMST0 + UT;
    double LST = GMST + longitude/15.0;

    double HA = Units::degreesToRadians(LST*15.0 - Units::radiansToDegrees(ra));    // Hour angle in radians
    double g = atan(tan(gclat) / cos(HA));

    double topRA = ra - mpar * rho * cos(gclat) * sin(HA) / cos(dec);
    double topDec;
    if (g != 0.0)
        topDec = dec - mpar * rho * sin(gclat) * sin(g - dec) / sin(g);
    else
        topDec = dec - mpar * rho * sin(-dec) * cos(HA);

    rd.ra = modulo(Units::radiansToDegrees(topRA), 360.0) / (360.0/24.0); // Convert to hours
    rd.dec = Units::radiansToDegrees(topDec); // Convert to degrees
    aa = raDecToAzAlt(rd, latitude, longitude, dt, false);
}

// Calculated adjustment to altitude angle from true to apparent due to atmospheric refraction using
// Saemundsson's formula (which is used by Stellarium and is primarily just for
// optical wavelengths)
// See: https://en.wikipedia.org/wiki/Atmospheric_refraction#Calculating_refraction
// Alt is in degrees. 90 = Zenith gives a factor of 0.
// Pressure in millibars
// Temperature in Celsuis
// We divide by 60.0 to get a value in degrees (as original formula is in arcminutes)
double Astronomy::refractionSaemundsson(double alt, double pressure, double temperature)
{
    double pt = (pressure/1010.0) * (283.0/(273.0+temperature));

    return pt * (1.02 / tan(Units::degreesToRadians(alt+10.3/(alt+5.11))) + 0.0019279) / 60.0;
}

// Calculated adjustment to altitude angle from true to apparent due to atmospheric refraction using
// code from Starlink Positional Astronomy Library. This is more accurate for
// radio than Saemundsson's formula, but also more complex.
// See: https://github.com/Starlink/pal
// Alt is in degrees. 90 = Zenith gives a factor of 0.
// Pressure in millibars
// Temperature in Celsuis
// Humdity in %
// Frequency in Hertz
// Latitude in decimal degrees
// HeightAboveSeaLevel in metres
// Temperature lapse rate in K/km
double Astronomy::refractionPAL(double alt, double pressure, double temperature, double humidity, double frequency,
                                double latitude, double heightAboveSeaLevel, double temperatureLapseRate)
{
    double tdk = Units::celsiusToKelvin(temperature);   // Ambient temperature at the observer (K)
    double wl = (299792458.0/frequency)*1000000.0;      // Wavelength in micrometres
    double rh = humidity/100.0;                         // Relative humidity in range 0-1
    double phi = Units::degreesToRadians(latitude);     // Latitude of the observer (radian, astronomical)
    double tlr = temperatureLapseRate/1000.0;           // Temperature lapse rate in the troposphere (K/metre)
    double refa, refb;
    double z = 90.0-alt;
    double zu = Units::degreesToRadians(z);
    double zr;

    palRefco(heightAboveSeaLevel, tdk, pressure, rh,
                wl, phi, tlr, 1E-10,
                &refa, &refb);
    palRefz(zu, refa, refb, &zr);

    return z-Units::radiansToDegrees(zr);
}

double Astronomy::raToDecimal(const QString& value)
{
    QRegExp decimal("^([0-9]+(\\.[0-9]+)?)");
    QRegExp hms("^([0-9]+)[ h]([0-9]+)[ m]([0-9]+(\\.[0-9]+)?)s?");

    if (decimal.exactMatch(value))
        return decimal.capturedTexts()[0].toDouble();
    else if (hms.exactMatch(value))
    {
        return Units::hoursMinutesSecondsToDecimal(
                    hms.capturedTexts()[1].toDouble(),
                    hms.capturedTexts()[2].toDouble(),
                    hms.capturedTexts()[3].toDouble());
    }
    return 0.0;
}

double Astronomy::decToDecimal(const QString& value)
{
    QRegExp decimal("^(-?[0-9]+(\\.[0-9]+)?)");
    QRegExp dms(QString("^(-?[0-9]+)[ %1d]([0-9]+)[ 'm]([0-9]+(\\.[0-9]+)?)[\"s]?").arg(QChar(0xb0)));

    if (decimal.exactMatch(value))
        return decimal.capturedTexts()[0].toDouble();
    else if (dms.exactMatch(value))
    {
        return Units::degreesMinutesSecondsToDecimal(
                    dms.capturedTexts()[1].toDouble(),
                    dms.capturedTexts()[2].toDouble(),
                    dms.capturedTexts()[3].toDouble());
    }
    return 0.0;
}

double Astronomy::lstAndRAToLongitude(double lst, double raHours)
{
    double longitude = lst - (raHours * 15.0); // Convert hours to degrees
    if (longitude < -180.0)
        longitude += 360.0;
    else if (longitude > 180.0)
        longitude -= 360.0;
    return -longitude; // East positive
}

// Return right ascension and declination of North Galactic Pole in J2000 Epoch
// http://www.lsc-group.phys.uwm.edu/lal/lsd/node1777.html
void Astronomy::northGalacticPoleJ2000(double& ra, double& dec)
{
    ra = 192.8594813/15.0;
    dec = 27.1282511;
}

// Convert from equatorial to Galactic coordinates, J2000 Epoch
void Astronomy::equatorialToGalactic(double ra, double dec, double& l, double& b)
{
    const double raRad = Units::degreesToRadians(ra * 15.0);
    const double decRad = Units::degreesToRadians(dec);

    // Calculate RA and dec for North Galactic pole, J2000
    double ngpRa, ngpDec;
    northGalacticPoleJ2000(ngpRa, ngpDec);
    const double ngpRaRad = Units::degreesToRadians(ngpRa * 15.0);
    const double ngpDecRad = Units::degreesToRadians(ngpDec);

    // Calculate galactic longitude in radians
    double bRad = asin(sin(ngpDecRad)*sin(decRad)+cos(ngpDecRad)*cos(decRad)*cos(raRad - ngpRaRad));

    // Calculate galactic latitiude in radians
    double lRad = atan2(sin(decRad)-sin(bRad)*sin(ngpDecRad), cos(decRad)*cos(ngpDecRad)*sin(raRad - ngpRaRad));

    // Ascending node of the galactic plane in degrees
    double lAscend = 33.0;

    // Convert to degrees in range -90,90 and 0,360
    b = Units::radiansToDegrees(bRad);
    l = Units::radiansToDegrees(lRad) + lAscend;
    if (l < 0.0) {
        l += 360.0;
    }
    if (l > 360.0) {
        l -= 360.0;
    }
}

// The following functions are from Starlink Positional Astronomy Library
// https://github.com/Starlink/pal

/* Pi */
static const double PAL__DPI = 3.1415926535897932384626433832795028841971693993751;

/* 2Pi */
static const double PAL__D2PI = 6.2831853071795864769252867665590057683943387987502;

/* pi/180:  degrees to radians */
static const double PAL__DD2R = 0.017453292519943295769236907684886127134428718885417;

/* Radians to degrees */
static const double PAL__DR2D = 57.295779513082320876798154814105170332405472466564;

/* DMAX(A,B) - return maximum value - evaluates arguments multiple times */
#define DMAX(A,B) ((A) > (B) ? (A) : (B) )

/* DMIN(A,B) - return minimum value - evaluates arguments multiple times */
#define DMIN(A,B) ((A) < (B) ? (A) : (B) )

// Normalize angle into range +/- pi
double palDrange( double angle ) {
   double result = fmod( angle, PAL__D2PI );
   if( result > PAL__DPI ) {
      result -= PAL__D2PI;
   } else if( result < -PAL__DPI ) {
      result += PAL__D2PI;
   }
   return result;
}

//  Calculate stratosphere parameters
static void pal1Atms ( double rt, double tt, double dnt, double gamal,
                double r, double * dn, double * rdndr ) {

  double b;
  double w;

  b = gamal / tt;
  w = (dnt - 1.0) * exp( -b * (r-rt) );
  *dn = 1.0 + w;
  *rdndr = -r * b * w;

}

// Calculate troposphere parameters
static void pal1Atmt ( double r0, double t0, double alpha, double gamm2,
                double delm2, double c1, double c2, double c3, double c4,
                double c5, double c6, double r,
                double *t, double *dn, double *rdndr ) {

  double tt0;
  double tt0gm2;
  double tt0dm2;

  *t = DMAX( DMIN( t0 - alpha*(r-r0), 320.0), 100.0 );
  tt0 = *t / t0;
  tt0gm2 = pow( tt0, gamm2 );
  tt0dm2 = pow( tt0, delm2 );
  *dn = 1.0 + ( c1 * tt0gm2 - ( c2 - c5 / *t ) * tt0dm2 ) * tt0;
  *rdndr = r * ( -c3 * tt0gm2 + ( c4 - c6 / tt0 ) * tt0dm2 );

}

// Adjust unrefracted zenith distance
static void palRefz ( double zu, double refa, double refb, double *zr ) {

  /* Constants */

  /* Largest usable ZD (deg) */
  const double D93 = 93.0;

  /* ZD at which one model hands over to the other (radians) */
  const double Z83 = 83.0 * PAL__DD2R;

  /* coefficients for high ZD model (used beyond ZD 83 deg) */
  const double C1 = +0.55445;
  const double C2 = -0.01133;
  const double C3 = +0.00202;
  const double C4 = +0.28385;
  const double C5 = +0.02390;

  /* High-ZD-model prefiction (deg) for that point */
  const double REF83 = (C1+C2*7.0+C3*49.0)/(1.0+C4*7.0+C5*49.0);

  double zu1,zl,s,c,t,tsq,tcu,ref,e,e2;

  /*  perform calculations for zu or 83 deg, whichever is smaller */
  zu1 = DMIN(zu,Z83);

  /*  functions of ZD */
  zl = zu1;
  s = sin(zl);
  c = cos(zl);
  t = s/c;
  tsq = t*t;
  tcu = t*tsq;

  /*  refracted zd (mathematically to better than 1 mas at 70 deg) */
  zl = zl-(refa*t+refb*tcu)/(1.0+(refa+3.0*refb*tsq)/(c*c));

  /*  further iteration */
  s = sin(zl);
  c = cos(zl);
  t = s/c;
  tsq = t*t;
  tcu = t*tsq;
  ref = zu1-zl+
    (zl-zu1+refa*t+refb*tcu)/(1.0+(refa+3.0*refb*tsq)/(c*c));

  /*  special handling for large zu */
  if (zu > zu1) {
    e = 90.0-DMIN(D93,zu*PAL__DR2D);
    e2 = e*e;
    ref = (ref/REF83)*(C1+C2*e+C3*e2)/(1.0+C4*e+C5*e2);
  }

  /*  return refracted zd */
  *zr = zu-ref;

}

// Determine constants in atmospheric refraction model
static void palRefco ( double hm, double tdk, double pmb, double rh,
                double wl, double phi, double tlr, double eps,
                double *refa, double *refb ) {

  double r1, r2;

  /*  Sample zenith distances: arctan(1) and arctan(4) */
  const double ATN1 = 0.7853981633974483;
  const double ATN4 = 1.325817663668033;

  /*  Determine refraction for the two sample zenith distances */
  palRefro(ATN1,hm,tdk,pmb,rh,wl,phi,tlr,eps,&r1);
  palRefro(ATN4,hm,tdk,pmb,rh,wl,phi,tlr,eps,&r2);

  /*  Solve for refraction constants */
  *refa = (64.0*r1-r2)/60.0;
  *refb = (r2-4.0*r1)/60.0;

}

// Atmospheric refraction for radio and optical/IR wavelengths
static void palRefro( double zobs, double hm, double tdk, double pmb,
               double rh, double wl, double phi, double tlr,
               double eps, double * ref ) {

  /*
   *  Fixed parameters
   */

  /*  93 degrees in radians */
  const double D93 = 1.623156204;
  /*  Universal gas constant */
  const double GCR = 8314.32;
  /*  Molecular weight of dry air */
  const double DMD = 28.9644;
  /*  Molecular weight of water vapour */
  const double DMW = 18.0152;
  /*  Mean Earth radius (metre) */
  const double S = 6378120.;
  /*  Exponent of temperature dependence of water vapour pressure */
  const double DELTA = 18.36;
  /*  Height of tropopause (metre) */
  const double HT = 11000.;
  /*  Upper limit for refractive effects (metre) */
  const double HS = 80000.;
  /*  Numerical integration: maximum number of strips. */
  const int ISMAX=16384l;

  /* Local variables */
  int is, k, n, i, j;

  int optic, loop; /* booleans */

  double zobs1,zobs2,hmok,tdkok,pmbok,rhok,wlok,alpha,
    tol,wlsq,gb,a,gamal,gamma,gamm2,delm2,
    tdc,psat,pwo,w,
    c1,c2,c3,c4,c5,c6,r0,tempo,dn0,rdndr0,sk0,f0,
    rt,tt,dnt,rdndrt,sine,zt,ft,dnts,rdndrp,zts,fts,
    rs,dns,rdndrs,zs,fs,refold,z0,zrange,fb,ff,fo,fe,
    h,r,sz,rg,dr,tg,dn,rdndr,t,f,refp,reft;

  /*  The refraction integrand */
#define refi(DN,RDNDR) RDNDR/(DN+RDNDR)

  /*  Transform ZOBS into the normal range. */
  zobs1 = palDrange(zobs);
  zobs2 = DMIN(fabs(zobs1),D93);

  /*  keep other arguments within safe bounds. */
  hmok = DMIN(DMAX(hm,-1e3),HS);
  tdkok = DMIN(DMAX(tdk,100.0),500.0);
  pmbok = DMIN(DMAX(pmb,0.0),10000.0);
  rhok = DMIN(DMAX(rh,0.0),1.0);
  wlok = DMAX(wl,0.1);
  alpha = DMIN(DMAX(fabs(tlr),0.001),0.01);

  /*  tolerance for iteration. */
  tol = DMIN(DMAX(fabs(eps),1e-12),0.1)/2.0;

  /*  decide whether optical/ir or radio case - switch at 100 microns. */
  optic = wlok < 100.0;

  /*  set up model atmosphere parameters defined at the observer. */
  wlsq = wlok*wlok;
  gb = 9.784*(1.0-0.0026*cos(phi+phi)-0.00000028*hmok);
  if (optic) {
    a = (287.6155+(1.62887+0.01360/wlsq)/wlsq) * 273.15e-6/1013.25;
  } else {
    a = 77.6890e-6;
  }
  gamal = (gb*DMD)/GCR;
  gamma = gamal/alpha;
  gamm2 = gamma-2.0;
  delm2 = DELTA-2.0;
  tdc = tdkok-273.15;
  psat = pow(10.0,(0.7859+0.03477*tdc)/(1.0+0.00412*tdc)) *
    (1.0+pmbok*(4.5e-6+6.0e-10*tdc*tdc));
  if (pmbok > 0.0) {
    pwo = rhok*psat/(1.0-(1.0-rhok)*psat/pmbok);
  } else {
    pwo = 0.0;
  }
  w = pwo*(1.0-DMW/DMD)*gamma/(DELTA-gamma);
  c1 = a*(pmbok+w)/tdkok;
  if (optic) {
    c2 = (a*w+11.2684e-6*pwo)/tdkok;
  } else {
    c2 = (a*w+6.3938e-6*pwo)/tdkok;
  }
  c3 = (gamma-1.0)*alpha*c1/tdkok;
  c4 = (DELTA-1.0)*alpha*c2/tdkok;
  if (optic) {
    c5 = 0.0;
    c6 = 0.0;
  } else {
    c5 = 375463e-6*pwo/tdkok;
    c6 = c5*delm2*alpha/(tdkok*tdkok);
  }

  /*  conditions at the observer. */
  r0 = S+hmok;
  pal1Atmt(r0,tdkok,alpha,gamm2,delm2,c1,c2,c3,c4,c5,c6,
           r0,&tempo,&dn0,&rdndr0);
  sk0 = dn0*r0*sin(zobs2);
  f0 = refi(dn0,rdndr0);

  /*  conditions in the troposphere at the tropopause. */
  rt = S+DMAX(HT,hmok);
  pal1Atmt(r0,tdkok,alpha,gamm2,delm2,c1,c2,c3,c4,c5,c6,
           rt,&tt,&dnt,&rdndrt);
  sine = sk0/(rt*dnt);
  zt = atan2(sine,sqrt(DMAX(1.0-sine*sine,0.0)));
  ft = refi(dnt,rdndrt);

  /*  conditions in the stratosphere at the tropopause. */
  pal1Atms(rt,tt,dnt,gamal,rt,&dnts,&rdndrp);
  sine = sk0/(rt*dnts);
  zts = atan2(sine,sqrt(DMAX(1.0-sine*sine,0.0)));
  fts = refi(dnts,rdndrp);

  /*  conditions at the stratosphere limit. */
  rs = S+HS;
  pal1Atms(rt,tt,dnt,gamal,rs,&dns,&rdndrs);
  sine = sk0/(rs*dns);
  zs = atan2(sine,sqrt(DMAX(1.0-sine*sine,0.0)));
  fs = refi(dns,rdndrs);

  /*  variable initialization to avoid compiler warning. */
  reft = 0.0;

  /*  integrate the refraction integral in two parts;  first in the
   *  troposphere (k=1), then in the stratosphere (k=2). */

  for (k=1; k<=2; k++) {

    /*     initialize previous refraction to ensure at least two iterations. */
    refold = 1.0;

    /*     start off with 8 strips. */
    is = 8;

    /*     start z, z range, and start and end values. */
    if (k==1) {
      z0 = zobs2;
      zrange = zt-z0;
      fb = f0;
      ff = ft;
    } else {
      z0 = zts;
      zrange = zs-z0;
      fb = fts;
      ff = fs;
    }

    /*     sums of odd and even values. */
    fo = 0.0;
    fe = 0.0;

    /*     first time through the loop we have to do every point. */
    n = 1;

    /*     start of iteration loop (terminates at specified precision). */
    loop = 1;
    while (loop) {

      /*        strip width. */
      h = zrange/((double)is);

      /*        initialize distance from earth centre for quadrature pass. */
      if (k == 1) {
        r = r0;
      } else {
        r = rt;
      }

      /*        one pass (no need to compute evens after first time). */
      for (i=1; i<is; i+=n) {

              /*           sine of observed zenith distance. */
        sz = sin(z0+h*(double)(i));

        /*           find r (to the nearest metre, maximum four iterations). */
        if (sz > 1e-20) {
          w = sk0/sz;
          rg = r;
          dr = 1.0e6;
          j = 0;
          while ( fabs(dr) > 1.0 && j < 4 ) {
            j++;
            if (k==1) {
              pal1Atmt(r0,tdkok,alpha,gamm2,delm2,
                       c1,c2,c3,c4,c5,c6,rg,&tg,&dn,&rdndr);
            } else {
              pal1Atms(rt,tt,dnt,gamal,rg,&dn,&rdndr);
            }
            dr = (rg*dn-w)/(dn+rdndr);
            rg = rg-dr;
          }
          r = rg;
        }

        /*           find the refractive index and integrand at r. */
        if (k==1) {
          pal1Atmt(r0,tdkok,alpha,gamm2,delm2,
                   c1,c2,c3,c4,c5,c6,r,&t,&dn,&rdndr);
        } else {
          pal1Atms(rt,tt,dnt,gamal,r,&dn,&rdndr);
        }
        f = refi(dn,rdndr);

        /*           accumulate odd and (first time only) even values. */
        if (n==1 && i%2 == 0) {
          fe += f;
        } else {
          fo += f;
        }
      }

      /*        evaluate the integrand using simpson's rule. */
      refp = h*(fb+4.0*fo+2.0*fe+ff)/3.0;

      /*        has the required precision been achieved (or can't be)? */
      if (fabs(refp-refold) > tol && is < ISMAX) {

        /*           no: prepare for next iteration.*/

        /*           save current value for convergence test. */
        refold = refp;

        /*           double the number of strips. */
        is += is;

        /*           sum of all current values = sum of next pass's even values. */
        fe += fo;

        /*           prepare for new odd values. */
        fo = 0.0;

        /*           skip even values next time. */
        n = 2;
      } else {

        /*           yes: save troposphere component and terminate the loop. */
        if (k==1) reft = refp;
        loop = 0;
      }
    }
  }

  /*  result. */
  *ref = reft+refp;
  if (zobs1 < 0.0) *ref = -(*ref);

}
