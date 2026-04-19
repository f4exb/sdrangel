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

#include "spice.h"
#include "util/units.h"
#include "util/profiler.h"

#include <cstring>

#include <QFileInfo>

#include <SpiceUsr.h>

static QMutex spiceMutex; // SPICE isn't thread-safe, so we use a global mutex that should be locked via spiceLock() before calling any function in this file.
static QStringList spiceEphemeridesLoaded;

void spiceInit()
{
    QMutexLocker locker(&spiceMutex);

    // Don't abort on error from SPICE
    erract_c("SET", 0, (char *) "RETURN");
}

bool spiceLock(QStringList ephemerides)
{
    spiceMutex.lock();

    QStringList files = ephemerides;
    bool changed = false;

    // Unload no longer used ephemerides
    QMutableListIterator<QString> itr(spiceEphemeridesLoaded);
    while (itr.hasNext())
    {
        QString file = itr.next();
        if (!files.contains(file))
        {
            unload_c(file.toStdString().c_str());
            itr.remove();
            changed = true;
        }
    }

    // Load new ephemerides - file may not have been downloaded yet
    for (const auto& file : files)
    {
        if (!spiceEphemeridesLoaded.contains(file) && QFileInfo::exists(file))
        {
            furnsh_c(file.toStdString().c_str());
            if (!failed_c())
            {
                spiceEphemeridesLoaded.append(file);
                changed = true;
            }
            else
            {
                qWarning() << "StarTracker: Failed to load SPICE ephemeris file: " << file;
                reset_c();
            }
        }
    }

    return changed;
}

void spiceUnlock()
{
    spiceMutex.unlock();
}

// Normalize angle to [0,360)
static SpiceDouble normalize360(SpiceDouble deg)
{
    SpiceDouble d = fmod(deg, 360.0);
    if (d < 0.0) {
        d += 360.0;
    }
    return d;
}

// Compute topocentric observer position in J2000 at ET
static bool computeTopocenterJ2000(SpiceDouble latDeg, SpiceDouble lonDeg, SpiceDouble altKm, SpiceDouble et, SpiceDouble obsJ2000[3])
{
    SpiceInt n;
    SpiceDouble radii[3];

    // Earth body radii for geodetic -> rectangular
    bodvrd_c("EARTH", "RADII", 3, &n, radii);
    if (n < 3)
    {
        reset_c();
        return false;
    }
    SpiceDouble re = radii[0];
    SpiceDouble rp = radii[2];
    SpiceDouble flatten = (re - rp) / re;

    // Geodetic to IAU_EARTH body-fixed rectangular coords
    SpiceDouble lonRad = Units::degreesToRadians(lonDeg);
    SpiceDouble latRad = Units::degreesToRadians(latDeg);
    SpiceDouble topoIAE[3];
    georec_c(lonRad, latRad, altKm, re, flatten, topoIAE);

    // Transform IAU_EARTH -> J2000 at ET
    SpiceDouble xformIAEToJ2000[3][3];
    pxform_c("IAU_EARTH", "J2000", et, xformIAEToJ2000);
    SpiceDouble topoJ2000[3];
    mxv_c(xformIAEToJ2000, topoIAE, topoJ2000);

    /// Earth center in J2000 w.r.t. SSB
    SpiceDouble earthPosJ2000[3], ltDummy;
    spkpos_c("EARTH", et, "J2000", "NONE", "SOLAR SYSTEM BARYCENTER", earthPosJ2000, &ltDummy);
    if (failed_c())
    {
        reset_c();
        return false;
    }

    // observer = earth_center + topo offset
    vadd_c(earthPosJ2000, topoJ2000, obsJ2000);

    return true;
}

// Convert QT date time to SPICE time (TBD seconds past J2000 epoch)
bool dateTimeToET(const QDateTime& dateTime, double &et)
{
    const QByteArray utcBA = dateTime.toUTC().toString(Qt::ISODateWithMs).toLatin1();
    const char *utc = utcBA.data();

    str2et_c(utc, &et);
    if (failed_c())
    {
        reset_c();
        return false;
    }
    return true;
}

// Get list of targets (bodies) from a SPICE SPK file. Non SPK files will be ignored.
QStringList getSPICETargets(const QString& file)
{
    QStringList targets;

    const QByteArray fileBA = file.toLatin1();
    const char *fileStr = fileBA.constData();

    // Check file is SPK
    SpiceChar arch[32];
    SpiceChar kernelType[32];

    getfat_c(fileStr, sizeof(arch), sizeof(kernelType), arch, kernelType);

    if (strcmp(arch, "DAF") || strcmp(kernelType, "SPK")) {
        return targets;
    }

    SPICEINT_CELL(ids, 1000);

    spkobj_c(fileStr, &ids);
    if (!failed_c())
    {
        for (int i = 0; i < card_c(&ids); i++)
        {
            SpiceBoolean found;
            SpiceInt id;
            char bodyName[33];

            id = SPICE_CELL_ELEM_I(&ids, i);
            bodc2n_c(id, sizeof(bodyName), bodyName, &found);
            if (!failed_c())
            {
                QString nameStr = QString::fromLatin1(bodyName);
                if (!targets.contains(nameStr)) {
                    targets.append(nameStr);
                }
            }
            else
            {
                reset_c();
            }
        }
    }
    else
    {
        reset_c();
    }

    return targets;
}

// Calculate azimuth and elevation of a target body from a position on Earth using SPICE
bool getAzElFromSPICE(const QString& target, double et, double latitude, double longitude, double altitudeKm, double &azimuth, double &elevation)
{
    QByteArray targetBA = target.toLatin1();
    const char *targetStr = targetBA.constData();

    // Get body radii for the observer's central body
    SpiceInt n;
    SpiceDouble radii[3];
    bodvrd_c("EARTH", "RADII", 3, &n, radii);

    // Convert geodetic coordinates to body-fixed rectangular coordinates
    SpiceDouble bodfix[3];
    SpiceDouble longitudeRad = Units::degreesToRadians(longitude);
    SpiceDouble latitudeRad = Units::degreesToRadians(latitude);
    georec_c(longitudeRad, latitudeRad, altitudeKm, radii[0], (radii[0] - radii[2]) / radii[0], bodfix);

    // Calculate Alz/Az
    SpiceDouble lt;
    SpiceDouble azlsta[6];
    azlcpo_c("ELLIPSOID", targetStr, et, "LT+S", SPICEFALSE, SPICETRUE, bodfix, "EARTH", "IAU_EARTH", azlsta, &lt);
    if (!failed_c())
    {
        azimuth = Units::radiansToDegrees(azlsta[1]);
        elevation = Units::radiansToDegrees(azlsta[2]);
    }
    else
    {
        reset_c();
        return false;
    }

    return true;
}

// Calculate RA and Dec of a target body from Earth using SPICE
bool getRADecFromSPICE(const QString& target, double et, double &ra, double &dec)
{
    QByteArray targetBA = target.toLatin1();
    const char *targetStr = targetBA.constData();

    // Get state (position & velocity) of a target body, relative to observing body (Earth), corrected for light time and stellar aberation
    SpiceDouble state[6];
    SpiceDouble lightTime;

    spkezr_c(targetStr, et, "J2000", "LT+S", "EARTH", state, &lightTime);
    if (!failed_c())
    {
        SpiceDouble radec[3];

        // Convert geocentric rectangular coordinates to RA/DEC (spherical coordinates)
        recrad_c(state, &radec[0], &radec[1], &radec[2]);

        ra = Units::radiansToDegrees(radec[1]);
        dec = Units::radiansToDegrees(radec[2]);

        // Normalize RA to 0-360 range
        if (ra < 0.0) {
            ra += 360.0;
        }

        // Convert from degrees to hours
        ra = ra / (360.0 / 24.0);
    }
    else
    {
        reset_c();
        return false;
    }

    return true;
}

// Calculate Jupiter's CML (Central Meridian Longitude) as seen from a location on Earth and the phase of the given Jovian moon (IO or GANYMEDE)
bool spiceJupiterMoonPhase(const QString& moon, double et, double latitude, double longitude, double altitudeMetres, double &cml, double &phase)
{
    const QByteArray moonBA = moon.toUpper().toLatin1();
    const char *moonName = moonBA.data();

    SpiceDouble te = et;        // Emission epoch
    SpiceDouble moonPosSSB[3];  // Io position w.r.t SSB at te
    SpiceDouble obsPosSSB[3];   // Pbserver position w.r.t SSB at reception (et)
    SpiceDouble lt = 0.0;       // Light time

    // Compute topocenter obsPosSSB at reception ET
    if (!computeTopocenterJ2000(latitude, longitude, altitudeMetres / 1000.0, et, obsPosSSB))
    {
        reset_c();
        return false;
    }

    // Iterate to find Io emission epoch te
    SpiceDouble moonPosGuess[3], ltDummy;
    spkpos_c(moonName, et, "J2000", "NONE", "SOLAR SYSTEM BARYCENTER", moonPosGuess, &ltDummy);
    if (failed_c())
    {
        reset_c();
        return false;
    }

    const SpiceDouble c = clight_c();
    SpiceDouble vec[3];
    vsub_c(obsPosSSB, moonPosGuess, vec);
    lt = vnorm_c(vec) / c;

    const int MAX_ITERS = 50;
    const SpiceDouble TOL = 1e-9;
    int iter;
    for (iter = 0; iter < MAX_ITERS; ++iter)
    {
        te = et - lt;
        spkpos_c(moonName, te, "J2000", "NONE", "SOLAR SYSTEM BARYCENTER", moonPosSSB, &ltDummy);
        if (failed_c())
        {
            reset_c();
            return false;
        }
        vsub_c(obsPosSSB, moonPosSSB, vec);
        SpiceDouble new_lt = vnorm_c(vec) / c;
        if (fabs(new_lt - lt) < TOL)
        {
            lt = new_lt;
            break;
        }
        lt = new_lt;
    }
    if (iter >= MAX_ITERS) {
        qDebug() << "calculateIoPhase: light-time iteration did not converge";
    }

    // Moon relative to Jupiter (J2000) at te
    SpiceDouble moonPosJupiterJ2000[3];
    spkpos_c(moonName, te, "J2000", "NONE", "JUPITER", moonPosJupiterJ2000, &ltDummy);
    if (failed_c())
    {
        reset_c();
        return false;
    }

    // Jupiter position (SSB) at te to compute observer-relative vector
    SpiceDouble jupPosSSBTe[3];
    spkpos_c("JUPITER", te, "J2000", "NONE", "SOLAR SYSTEM BARYCENTER", jupPosSSBTe, &ltDummy);
    if (failed_c())
    {
        reset_c();
        return false;
    }

    // Observer vector relative to Jupiter in J2000: obs_ssb (at et) - jup_pos_ssb_te
    SpiceDouble obsPosJupiterJ2000[3];
    vsub_c(obsPosSSB, jupPosSSBTe, obsPosJupiterJ2000);

    // Transform both vectors into IAU_JUPITER body-fixed frame evaluated at te
    SpiceDouble xform[3][3];
    pxform_c("J2000", "IAU_JUPITER", te, xform);

    SpiceDouble moonBodyFixed[3], obsBodyFixed[3];
    mxv_c(xform, moonPosJupiterJ2000, moonBodyFixed);
    mxv_c(xform, obsPosJupiterJ2000, obsBodyFixed);

    // Jupiter radii
    SpiceInt nrad;
    SpiceDouble radiiJupiter[3];
    bodvrd_c("JUPITER", "RADII", 3, &nrad, radiiJupiter);
    if (nrad < 3)
    {
        reset_c();
        return false;
    }
    SpiceDouble re_j = radiiJupiter[0];
    SpiceDouble rp_j = radiiJupiter[2];
    SpiceDouble f_j = (re_j - rp_j) / re_j;

    // Moon planetographic longitude using the same convention as CML
    SpiceDouble moonLonRad, moonLatRad, moonAltKm;
    recpgr_c("JUPITER", moonBodyFixed, re_j, f_j, &moonLonRad, &moonLatRad, &moonAltKm);
    SpiceDouble moonLonDeg = normalize360(Units::radiansToDegrees(moonLonRad));

    // CML (sub-observer planetographic longitude)
    SpiceDouble subObsLonRad, subObsLatRad, subObsAltKm;
    recpgr_c("JUPITER", obsBodyFixed, re_j, f_j, &subObsLonRad, &subObsLatRad, &subObsAltKm);
    cml = normalize360(Units::radiansToDegrees(subObsLonRad));

    // Moon phase (0 deg on far side)
    phase = normalize360(cml - moonLonDeg + 180);

    return true;
}

// Get position of named body relative to Sun, in ECLIPJ2000 reference frame. No abberation correction.
bool spicePosition(const QString& name, double et, QVector3D &positionKm)
{
    SpiceDouble pos[3];
    SpiceDouble lightTime;
    const QByteArray nameBA = name.toLatin1();
    const char *nameStr = nameBA.constData();

    // Get position of named body
    spkpos_c(nameStr, et, "ECLIPJ2000", "NONE", "SUN", pos, &lightTime);
    if (!failed_c())
    {
        positionKm.setX(pos[0]);
        positionKm.setY(pos[1]);
        positionKm.setZ(pos[2]);

        return true;
    }
    else
    {
        qDebug() << "spicePosition: Failed to get position for" << name;
        reset_c();
        return false;
    }
}

// Convert orbital elements to perifocal (PQW) coordinate system
static void orbitalElementsToPerifocal(SpiceDouble omega, SpiceDouble inc, SpiceDouble argp,
    SpiceDouble p[3], SpiceDouble q[3])
{
    SpiceDouble cO = cos(omega);
    SpiceDouble sO = sin(omega);
    SpiceDouble ci = cos(inc);
    SpiceDouble si = sin(inc);
    SpiceDouble cw = cos(argp);
    SpiceDouble sw = sin(argp);

    // Phat (toward periapsis)
    p[0] =  cO * cw - sO * sw * ci;
    p[1] =  sO * cw + cO * sw * ci;
    p[2] =  sw * si;

    // Qhat (90 deg ahead in orbit plane)
    q[0] = -cO * sw - sO * cw * ci;
    q[1] = -sO * sw + cO * cw * ci;
    q[2] =  cw * si;
}

// Get orbital path around the Sun, in XY plane of ECLIPJ2000 reference frame.
bool spiceOrbit(const QString& name, double et, QList<QPointF> &orbit)
{
    const QByteArray nameBA = name.toUpper().toLatin1();
    const char *target = nameBA.data();

    // Sun relative state, expressed in ECLIPJ2000
    SpiceDouble state[6], lt;
    spkezr_c(target, et, "ECLIPJ2000", "NONE", "SUN", state, &lt);

    // Sun GM (gravitational parameter = G * M)
    SpiceInt dim;
    SpiceDouble mu;
    bodvrd_c("SUN", "GM", 1, &dim, &mu);

    // Osculating conic elements
    SpiceDouble elts[8];
    oscelt_c(state, et, mu, elts);

    SpiceDouble rp    = elts[0]; // perifocal distance (km)
    SpiceDouble e     = elts[1]; // eccentricity
    SpiceDouble inc   = elts[2]; // inclination (rad)
    SpiceDouble omega = elts[3]; // longitude of ascending node (rad)
    SpiceDouble argp  = elts[4]; // argument of periapsis (rad)

    if (e >= 1.0)
    {
        qDebug() << "spiceOrbit: Not an ellipse" << name;
        return false;
    }

    SpiceDouble a = rp / (1.0 - e); // semi-major axis in orbital plane
    SpiceDouble b = a * sqrt(1.0 - e * e); // semi-minor axis in orbital plane

    SpiceDouble pHat[3], qHat[3];
    orbitalElementsToPerifocal(omega, inc, argp, pHat, qHat);

    const int N = 1000; // Could vary this with zoom level. 1k good enough when zoomed to orbit of the Moon around Earth
    for (int k = 0; k <= N; k++)
    {
        const double E = 2.0 * M_PI * (double) k / (double) N;

        // Ellipse in PQW with Sun at focus
        double xp = a * (cos(E) - e);
        double yp = b * sin(E);

        // Rotate into ECLIPJ2000
        double x = xp * pHat[0] + yp * qHat[0];
        double y = xp * pHat[1] + yp * qHat[1];

        QPointF point = {x, y};

        orbit.append(point);
    }

    return true;
}
