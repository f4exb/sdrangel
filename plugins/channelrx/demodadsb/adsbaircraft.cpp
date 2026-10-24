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

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "util/units.h"
#include "util/aircraft3dmodels.h"
#include "util/aircraftreport.h"

#include "adsbaircraft.h"
#include "adsb.h"

QString ADSBAircraft::getImage() const
{
    if (m_aircraftInfo && !m_aircraftInfo->m_type.isEmpty())
    {
        // https://www.icao.int/publications/doc8643/pages/search.aspx
        static const QStringList fourEngineTypes = {"A388", "B741", "B742", "B743", "B744", "B748", "B74R", "B74S", "BLCF", "A342", "A343", "A345", "A346", "C17", "K35R"};

        if (fourEngineTypes.contains(m_aircraftInfo->m_type)) {
            return QString("aircraft_heavy_4engine.png");
        } else if (m_aircraftInfo->m_type == "EUFI") {
            return QString("eurofighter.png");
        } else if (m_aircraftInfo->m_type == "VF35") {
            return QString("f35.png");
        } else if ((m_aircraftInfo->m_type == "SPIT") || (m_aircraftInfo->m_type == "HURI")) {
            return QString("spitfire.png");
        } else if ((m_aircraftInfo->m_type == "A400") || (m_aircraftInfo->m_type == "C30J")) {
            return QString("a400m.png");
        } else if (m_aircraftInfo->m_type == "H64") {
            return QString("apache.png");
        } else if (m_aircraftInfo->m_type == "H47") {
            return QString("chinook.png");
        }
    }

    if (m_emitterCategory.length() > 0)
    {
        if (!m_emitterCategory.compare("Heavy")) { // 777/787/A350 and 747/A380 - 4 engine handled above by type
            return QString("aircraft_heavy_2engine.png");
        } else if (!m_emitterCategory.compare("Large")) { // A320/737
            return QString("aircraft_large.png");
        } else if (!m_emitterCategory.compare("Small")) { // Private jets
            return QString("aircraft_small.png");
        } else if (!m_emitterCategory.compare("Rotorcraft")) {
            return QString("aircraft_helicopter.png");
        } else if (!m_emitterCategory.compare("High performance")) {
            return QString("aircraft_fighter.png");
        } else if (!m_emitterCategory.compare("Light")
            || !m_emitterCategory.compare("Ultralight")) {
            return QString("aircraft_light.png");
        } else if (!m_emitterCategory.compare("Glider/sailplane")) {
            return QString("aircraft_glider.png");
        } else if (!m_emitterCategory.compare("Space vehicle")) {
            return QString("aircraft_space.png");
        } else if (!m_emitterCategory.compare("UAV")) {
            return QString("aircraft_drone.png");
        } else if (!m_emitterCategory.compare("Emergency vehicle")
                || !m_emitterCategory.compare("Service vehicle")) {
            return QString("truck.png");
        } else {
            return QString("aircraft_large.png");
        }
    }
    else
    {
        return QString("aircraft_large.png");
    }
}

// The two model sets disagree on case - Bluebell names the 777-300ER B77W, FSLTL B77w -
// and Qt looks resources up case sensitively, so 22 FSLTL liveries found no icon although
// the same one was sitting there under the other spelling. The icons are indexed once by
// lower case name, and that index is what the name is looked up in
static QString liveryIconFile(const QString& name)
{
    static const QHash<QString, QString> icons = []
    {
        QHash<QString, QString> index;
        const QStringList files = QDir(":/map/liveries").entryList({"*.png"}, QDir::Files);
        index.reserve(files.size());
        for (const QString& file : files) {
            index.insert(QFileInfo(file).completeBaseName().toLower(), file);
        }
        return index;
    }();

    if (name.isEmpty()) {
        return QString();
    }
    const QString file = icons.value(name.toLower());
    return file.isEmpty() ? QString() : QString("liveries/" + file);
}

QString ADSBAircraft::getLiveryImage() const
{
    // The 3D model matcher has already picked a model such as
    // BB_Airbus_png/A320/A320_BAW.gltf from the type and operator, and the livery icons
    // are named after the models, so the icon just follows the model's basename
    for (const QString& model : {m_aircraft3DModel, m_aircraftCat3DModel})
    {
        if (!model.isEmpty())
        {
            const QString image = liveryIconFile(Aircraft3DModels::liveryIconName(model));
            if (!image.isEmpty()) {
                return image;
            }
        }
    }
    // No model match - use a white version of the plain icon, so the style stays
    // consistent with the livery icons rather than dropping back to black
    return liveryIconFile(QFileInfo(getImage()).completeBaseName());
}

QString ADSBAircraft::getText(const ADSBDemodSettings *settings, bool all, bool links) const
{
    // Written as a link only where one would work; plain text otherwise, rather than
    // something that looks clickable and is not
    auto identity = [links](const QString& kind, const QString& value)
    {
        return links ? aircraftMapLink(kind, value) : value.toHtmlEscaped();
    };
    QStringList list;
    if (all)
    {
        if (!m_flagIconURL.isEmpty() && !m_sideviewIconURL.isEmpty())
        {
            list.append(QString("<table width=100%><tr><td><img src=%1 width=85 height=20><td><img src=%2 align=right></table>").arg(m_sideviewIconURL).arg(m_flagIconURL));
        }
        else if (!m_flagIconURL.isEmpty() && !m_airlineIconURL.isEmpty())
        {
            list.append(QString("<table width=100%><tr><td><img src=%1><td><img src=%2 align=right></table>").arg(m_airlineIconURL).arg(m_flagIconURL));
        }
        else
        {
            if (!m_flagIconURL.isEmpty()) {
                list.append(QString("<img src=%1>").arg(m_flagIconURL));
            } else if (!m_sideviewIconURL.isEmpty()) {
                list.append(QString("<img src=%1>").arg(m_sideviewIconURL));
            } else if (!m_airlineIconURL.isEmpty()) {
                list.append(QString("<img src=%1>").arg(m_airlineIconURL));
            }
        }
        list.append(QString("ICAO: %1").arg(identity("icao", m_icaoHex)));
        if ((m_aircraftInfo != nullptr) && !m_aircraftInfo->m_registration.isEmpty()) {
            list.append(QString("Reg: %1").arg(identity("registration", m_aircraftInfo->m_registration)));
        }
        if (!m_callsign.isEmpty()) {
            list.append(QString("Callsign: %1").arg(identity("flight", m_callsign.trimmed())));
        }
        QString atcCallsign = cellText(ADSB_COL_ATC_CALLSIGN);
        if (!atcCallsign.isEmpty()) {
            list.append(QString("ATC Callsign: %1").arg(atcCallsign));
        }
        bool typeKnown = false;
        if (m_aircraftInfo != nullptr)
        {
            if (!m_aircraftInfo->m_model.isEmpty())
            {
                list.append(QString("Aircraft: %1").arg(m_aircraftInfo->m_model));
                typeKnown = true;
            }
        }
        // The emitter category is the coarse size class the aircraft broadcasts about
        // itself - "Large", "Heavy", "Rotorcraft". Where the model is known it says the
        // same thing less precisely, so it is only worth a line when it is not.
        if (!m_emitterCategory.isEmpty() && !typeKnown) {
            list.append(QString("Category: %1").arg(m_emitterCategory));
        }
        if (m_altitudeValid)
        {
            if (m_onSurface)
            {
                list.append(QString("Altitude: Surface"));
            }
            else
            {
                QString reference = m_altitudeGNSS ? "GNSS" : "Baro";
                if (settings->m_siUnits) {
                    list.append(QString("Altitude: %1 (m %2)").arg(Units::feetToIntegerMetres(m_altitude)).arg(reference));
                } else {
                    list.append(QString("Altitude: %1 (ft %2)").arg(m_altitude).arg(reference));
                }
            }
        }
        if (m_groundspeedValid)
        {
            if (settings->m_siUnits) {
                list.append(QString("GS: %1 (kph)").arg(Units::knotsToIntegerKPH(m_groundspeed)));
            } else {
                list.append(QString("GS: %1 (kn)").arg(m_groundspeed));
            }
        }
        if (m_verticalRateValid)
        {
            QString desc;
            Real rate;
            QString units;

            if (settings->m_siUnits)
            {
                rate = Units::feetPerMinToIntegerMetresPerSecond(m_verticalRate);
                units = QString("m/s");
            }
            else
            {
                rate = m_verticalRate;
                units = QString("ft/min");
            }
            if (m_verticalRate == 0) {
                desc = "Level flight";
            } else if (rate > 0) {
                desc = QString("Climbing: %1 (%2)").arg(rate).arg(units);
            } else {
                desc = QString("Descending: %1 (%2)").arg(rate).arg(units);
            }
            list.append(QString(desc));
        }
        if ((m_status.length() > 0) && m_status.compare("No emergency")) {
            list.append(m_status);
        }

        QString flightStatus = cellText(ADSB_COL_FLIGHT_STATUS);
        if (!flightStatus.isEmpty()) {
            list.append(QString("Flight status: %1").arg(flightStatus));
        }
        QString dep = cellText(ADSB_COL_DEP);
        if (!dep.isEmpty()) {
            list.append(QString("Departed: %1").arg(dep));
        }
        QString std = cellText(ADSB_COL_STD);
        if (!std.isEmpty()) {
            list.append(QString("STD: %1").arg(std));
        }
        QString atd = cellText(ADSB_COL_ATD);
        if (!atd.isEmpty())
        {
            list.append(QString("ATD: %1").arg(atd));
        }
        else
        {
            QString etd = cellText(ADSB_COL_ETD);
            if (!etd.isEmpty()) {
                list.append(QString("ETD: %1").arg(etd));
            }
        }
        QString arr = cellText(ADSB_COL_ARR);
        if (!arr.isEmpty()) {
            list.append(QString("Arrival: %1").arg(arr));
        }
        QString sta = cellText(ADSB_COL_STA);
        if (!sta.isEmpty()) {
            list.append(QString("STA: %1").arg(sta));
        }
        QString ata = cellText(ADSB_COL_ATA);
        if (!ata.isEmpty())
        {
            list.append(QString("ATA: %1").arg(ata));
        }
        else
        {
            QString eta = cellText(ADSB_COL_ETA);
            if (!eta.isEmpty()) {
                list.append(QString("ETA: %1").arg(eta));
            }
        }
    }
    else
    {
        QDateTime labelDateTime;
        list.append(getLabel(settings, labelDateTime));
    }
    return list.join("<br>");
}

// See https://nats.aero/blog/2017/07/drone-disruption-gatwick/ for example of UK ATC display
QString ADSBAircraft::getLabel(const ADSBDemodSettings *settings, QDateTime& dateTime) const
{
    QString id;
    if (!m_callsign.isEmpty())
    {
        QString atcCallsign = cellText(ADSB_COL_ATC_CALLSIGN);
        if (settings->m_atcCallsigns && !atcCallsign.isEmpty()) {
            id = QString("%1 %2").arg(atcCallsign).arg(m_callsign.mid(3));
        } else {
            id = m_callsign;
        }
    }
    else
    {
        id = m_icaoHex;
    }
    QStringList strings;
    strings.append(id);
    if (settings->m_atcLabels)
    {
        // Route
        QString dep = cellText(ADSB_COL_DEP);
        QString arr = cellText(ADSB_COL_ARR);
        QString stops = cellText(ADSB_COL_STOPS);
        if (!dep.isEmpty() && !arr.isEmpty())
        {
            if (stops.isEmpty()) {
                strings.append(QString("%1-%2").arg(dep).arg(arr));
            } else {
                strings.append(QString("%1-%2-%3").arg(dep).arg(stops).arg(arr));
            }
        }

        if (m_altitudeValid)
        {
            QStringList row1;
            QChar c = m_altitude >= settings->m_transitionAlt ? 'F' : 'A';
            // Convert altitude to flight level
            int fl = m_altitude / 100;
            row1.append(QString("%1%2").arg(c).arg(fl));
            // Indicate whether climbing or descending
            if (m_verticalRateValid && ((m_verticalRate != 0) || (m_selAltitudeValid && (m_altitude != m_selAltitude))))
            {
                QChar dir = m_verticalRate == 0 ? QChar('-') : (m_verticalRate > 0 ? QChar(0x2191) : QChar(0x2193));
                row1.append(dir);
            }
            if (m_selAltitudeValid && (m_altitude != m_selAltitude))
            {
                int selfl = m_selAltitude / 100;
                row1.append(QString::number(selfl));
            }
            strings.append(row1.join(" "));
            dateTime = m_altitudeDateTime;
        }

        QStringList row2;
        // Display speed
        if (m_groundspeedValid)
        {
            row2.append(QString("G%2").arg(m_groundspeed));
        }
        else if (m_indicatedAirspeedValid)
        {
            row2.append(QString("I%1").arg(m_indicatedAirspeed));
            if (!dateTime.isValid() || (m_indicatedAirspeedDateTime > dateTime)) {
                dateTime = m_indicatedAirspeedDateTime;
            }
        }
        // Aircraft type
        if (m_aircraftInfo && !m_aircraftInfo->m_type.isEmpty())
        {
            row2.append(m_aircraftInfo->m_type);
        }
        else if (m_aircraftInfo && !m_aircraftInfo->m_model.isEmpty())
        {
            QString name = m_aircraftInfo->m_model;
            int idx;
            idx = name.indexOf(' ');
            if (idx >= 0) {
                name = name.left(idx);
            }
            idx = name.indexOf('-');
            if (idx >= 0) {
                name = name.left(idx);
            }
            row2.append(name);
        }
        strings.append(row2.join(" "));
        // FIXME: Add ATC altitude and waypoint from ATC feature
    }
    return strings.join("<br>");
}

void ADSBAircraft::setOnSurface(const QDateTime& dateTime)
{
    // There are a few airports that are below 0 MSL
    // https://en.wikipedia.org/wiki/List_of_lowest_airports
    // So we could set altitude to a negative value here, which should
    // then get clipped to actual terrain elevation in 3D map
    // But then PFD altimeter reads negative elsewhere
    // Really, the altimeter would typically read airport elevation when on the surface
    m_altitude = 0;
    m_altitudeValid = true;
    m_altitudeGNSS = false;
    setCell(ADSB_COL_ALTITUDE, "Surface");
    m_altitudeDateTime = dateTime;
}

void ADSBAircraft::setAltitude(int altitudeFt, bool gnss, const QDateTime& dateTime, const ADSBDemodSettings& settings)
{
    m_altitude = altitudeFt;
    m_altitudeValid = true;

    if (!gnss)
    {
        m_pressureAltitude = altitudeFt;
        m_pressureAltitudeValid = true;
    }
    else
    {
        m_pressureAltitudeValid = false;
    }

    m_altitudeGNSS = gnss;
    setCell(ADSB_COL_ALTITUDE, settings.m_siUnits ? Units::feetToIntegerMetres(m_altitude) : m_altitude);
    m_altitudeDateTime = dateTime;
}

void ADSBAircraft::setVerticalRate(int verticalRate, const ADSBDemodSettings& settings)
{
    m_verticalRate = verticalRate;
    m_verticalRateValid = true;
    if (settings.m_siUnits) {
        setCell(ADSB_COL_VERTICALRATE, Units::feetPerMinToIntegerMetresPerSecond(verticalRate));
    } else {
        setCell(ADSB_COL_VERTICALRATE, m_verticalRate);
    }
}

void ADSBAircraft::setGroundspeed(float groundspeed, const ADSBDemodSettings& settings)
{
    m_groundspeed = groundspeed;
    m_groundspeedValid = true;
    setCell(ADSB_COL_GROUND_SPEED, settings.m_siUnits ? Units::knotsToIntegerKPH(m_groundspeed) : (int)std::round(m_groundspeed));
    // FIXME: dateTime?
}

void ADSBAircraft::setTrueAirspeed(int airspeed, const ADSBDemodSettings& settings)
{
    m_trueAirspeed = airspeed;
    m_trueAirspeedValid = true;
    setCell(ADSB_COL_TRUE_AIRSPEED, settings.m_siUnits ? Units::knotsToIntegerKPH(m_trueAirspeed) : m_trueAirspeed);
}

void ADSBAircraft::setIndicatedAirspeed(int airspeed, const QDateTime& dateTime, const ADSBDemodSettings& settings)
{
    m_indicatedAirspeed = airspeed;
    m_indicatedAirspeedValid = true;
    setCell(ADSB_COL_INDICATED_AIRSPEED, settings.m_siUnits ? Units::knotsToIntegerKPH(m_indicatedAirspeed) : m_indicatedAirspeed);
    m_indicatedAirspeedDateTime = dateTime;
}

void ADSBAircraft::setTrack(float track, const QDateTime& dateTime)
{
    m_track = track;
    m_trackValid = true;
    m_trackDateTime = dateTime;
    setCell(ADSB_COL_TRACK, std::round(m_track));
    m_orientationDateTime = dateTime;
}

void ADSBAircraft::setHeading(float heading, const QDateTime& dateTime)
{
    m_heading = heading;
    m_headingValid = true;
    m_headingDateTime = dateTime;
    setCell(ADSB_COL_HEADING, std::round(m_heading));
    m_orientationDateTime = dateTime;
    m_trackWhenHeadingSet = m_track;
}

