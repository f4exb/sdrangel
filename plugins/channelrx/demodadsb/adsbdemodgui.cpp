///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020-2024 Jon Beniston, M7RCE                                   //
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

#include <QDockWidget>
#include <QMainWindow>
#include <QQuickItem>
#include <QGeoLocation>
#include <QQmlContext>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QDebug>
#include <QProcess>
#include <QClipboard>
#include <QFileDialog>
#include <QQmlProperty>
#include <QJsonDocument>
#include <QJsonObject>
#ifdef QT_LOCATION_FOUND
#include <QGeoServiceProvider>
#endif

#include "ui_adsbdemodgui.h"
#include "device/deviceuiset.h"
#include "device/deviceapi.h"
#include "dsp/devicesamplesource.h"
#include "channel/channelwebapiutils.h"
#include "feature/featurewebapiutils.h"
#include "plugin/pluginapi.h"
#include "util/airlines.h"
#include "util/crc.h"
#include "util/db.h"
#include "util/units.h"
#include "util/morse.h"
#include "util/profiler.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/crightclickenabler.h"
#include "gui/clickablelabel.h"
#include "gui/tabletapandhold.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "gui/checklist.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

#include "adsbdemodreport.h"
#include "adsbdemod.h"
#include "adsbdemodgui.h"
#include "adsbdemodfeeddialog.h"
#include "adsbdemoddisplaydialog.h"
#include "adsbdemodnotificationdialog.h"
#include "adsb.h"
#include "adsbosmtemplateserver.h"

const char ADSBDemodGUI::m_idMap[] = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ##### ############-##0123456789######";

const QString ADSBDemodGUI::m_categorySetA[] = {
    QStringLiteral("None"),
    QStringLiteral("Light"),
    QStringLiteral("Small"),
    QStringLiteral("Large"),
    QStringLiteral("High vortex"),
    QStringLiteral("Heavy"),
    QStringLiteral("High performance"),
    QStringLiteral("Rotorcraft")
};

const QString ADSBDemodGUI::m_categorySetB[] = {
    QStringLiteral("None"),
    QStringLiteral("Glider/sailplane"),
    QStringLiteral("Lighter-than-air"),
    QStringLiteral("Parachutist"),
    QStringLiteral("Ultralight"),
    QStringLiteral("Reserved"),
    QStringLiteral("UAV"),
    QStringLiteral("Space vehicle")
};

const QString ADSBDemodGUI::m_categorySetC[] = {
    QStringLiteral("None"),
    QStringLiteral("Emergency vehicle"),
    QStringLiteral("Service vehicle"),
    QStringLiteral("Ground obstruction"),
    QStringLiteral("Cluster obstacle"),
    QStringLiteral("Line obstacle"),
    QStringLiteral("Reserved"),
    QStringLiteral("Reserved")
};

const QString ADSBDemodGUI::m_emergencyStatus[] = {
    QStringLiteral("No emergency"),
    QStringLiteral("General emergency"),
    QStringLiteral("Lifeguard/Medical"),
    QStringLiteral("Minimum fuel"),
    QStringLiteral("No communications"),
    QStringLiteral("Unlawful interference"),
    QStringLiteral("Downed aircraft"),
    QStringLiteral("Reserved")
};

const QString ADSBDemodGUI::m_flightStatuses[] = {
    QStringLiteral("Airborne"),
    QStringLiteral("On-ground"),
    QStringLiteral("Alert, airborne"),
    QStringLiteral("Alert, on-ground"),
    QStringLiteral("Alert, SPI"),
    QStringLiteral("SPI"),
    QStringLiteral("Reserved"),
    QStringLiteral("Not assigned")
};

const QString ADSBDemodGUI::m_hazardSeverity[] = {
    "NIL", "Light", "Moderate", "Severe"
};

const QString ADSBDemodGUI::m_fomSources[] = {
    "Invalid", "INS", "GNSS", "DME/DME", "DME/VOR", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved"
};

const QString ADSBDemodGUI::m_nacvStrings[] = {
    ">= 10 m/s", "< 10 m/s", "< 3 m/s", "< 1 m/s", "< 0.3 m/s", "Reserved", "Reserved", "Reserved"
};

static const unsigned char colors[3*64] = {
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x55,
    0x00, 0x00, 0xaa,
    0x00, 0x00, 0xff,
    0x00, 0x55, 0x00,
    0x00, 0x55, 0x55,
    0x00, 0x55, 0xaa,
    0x00, 0x55, 0xff,
    0x00, 0xaa, 0x00,
    0x00, 0xaa, 0x55,
    0x00, 0xaa, 0xaa,
    0x00, 0xaa, 0xff,
    0x00, 0xff, 0x00,
    0x00, 0xff, 0x55,
    0x00, 0xff, 0xaa,
    0x00, 0xff, 0xff,
    0x55, 0x00, 0x00,
    0x55, 0x00, 0x55,
    0x55, 0x00, 0xaa,
    0x55, 0x00, 0xff,
    0x55, 0x55, 0x00,
    0x55, 0x55, 0x55,
    0x55, 0x55, 0xaa,
    0x55, 0x55, 0xff,
    0x55, 0xaa, 0x00,
    0x55, 0xaa, 0x55,
    0x55, 0xaa, 0xaa,
    0x55, 0xaa, 0xff,
    0x55, 0xff, 0x00,
    0x55, 0xff, 0x55,
    0x55, 0xff, 0xaa,
    0x55, 0xff, 0xff,
    0xaa, 0x00, 0x00,
    0xaa, 0x00, 0x55,
    0xaa, 0x00, 0xaa,
    0xaa, 0x00, 0xff,
    0xaa, 0x55, 0x00,
    0xaa, 0x55, 0x55,
    0xaa, 0x55, 0xaa,
    0xaa, 0x55, 0xff,
    0xaa, 0xaa, 0x00,
    0xaa, 0xaa, 0x55,
    0xaa, 0xaa, 0xaa,
    0xaa, 0xaa, 0xff,
    0xaa, 0xff, 0x00,
    0xaa, 0xff, 0x55,
    0xaa, 0xff, 0xaa,
    0xaa, 0xff, 0xff,
    0xff, 0x00, 0x00,
    0xff, 0x00, 0x55,
    0xff, 0x00, 0xaa,
    0xff, 0x00, 0xff,
    0xff, 0x55, 0x00,
    0xff, 0x55, 0x55,
    0xff, 0x55, 0xaa,
    0xff, 0x55, 0xff,
    0xff, 0xaa, 0x00,
    0xff, 0xaa, 0x55,
    0xff, 0xaa, 0xaa,
    0xff, 0xaa, 0xff,
    0xff, 0xff, 0x00,
    0xff, 0xff, 0x55,
    0xff, 0xff, 0xaa,
    0xff, 0xff, 0xff,
};

ADSBDemodGUI* ADSBDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    ADSBDemodGUI* gui = new ADSBDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void ADSBDemodGUI::destroy()
{
    delete this;
}

void ADSBDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings(QStringList(), true);
    applyAllSettings();
}

QByteArray ADSBDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool ADSBDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        updateChannelList();
        displaySettings(QStringList(), true);
        applyAllSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

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

QString Aircraft::getImage() const
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

QString Aircraft::getText(const ADSBDemodSettings *settings, bool all) const
{
    QStringList list;
    if (m_showAll || all)
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
        list.append(QString("ICAO: %1").arg(m_icaoHex));
        if (!m_callsign.isEmpty()) {
            list.append(QString("Callsign: %1").arg(m_callsign));
        }
        QString atcCallsign = m_atcCallsignItem->text();
        if (!atcCallsign.isEmpty()) {
            list.append(QString("ATC Callsign: %1").arg(atcCallsign));
        }
        if (m_aircraftInfo != nullptr)
        {
            if (!m_aircraftInfo->m_model.isEmpty()) {
                list.append(QString("Aircraft: %1").arg(m_aircraftInfo->m_model));
            }
        }
        if (!m_emitterCategory.isEmpty()) {
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
                if (m_gui->useSIUints()) {
                    list.append(QString("Altitude: %1 (m %2)").arg(Units::feetToIntegerMetres(m_altitude)).arg(reference));
                } else {
                    list.append(QString("Altitude: %1 (ft %2)").arg(m_altitude).arg(reference));
                }
            }
        }
        if (m_groundspeedValid)
        {
            if (m_gui->useSIUints()) {
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

            if (m_gui->useSIUints())
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

        QString flightStatus = m_flightStatusItem->text();
        if (!flightStatus.isEmpty()) {
            list.append(QString("Flight status: %1").arg(flightStatus));
        }
        QString dep = m_depItem->text();
        if (!dep.isEmpty()) {
            list.append(QString("Departed: %1").arg(dep));
        }
        QString std = m_stdItem->text();
        if (!std.isEmpty()) {
            list.append(QString("STD: %1").arg(std));
        }
        QString atd = m_atdItem->text();
        if (!atd.isEmpty())
        {
            list.append(QString("ATD: %1").arg(atd));
        }
        else
        {
            QString etd = m_etdItem->text();
            if (!etd.isEmpty()) {
                list.append(QString("ETD: %1").arg(etd));
            }
        }
        QString arr = m_arrItem->text();
        if (!arr.isEmpty()) {
            list.append(QString("Arrival: %1").arg(arr));
        }
        QString sta = m_staItem->text();
        if (!sta.isEmpty()) {
            list.append(QString("STA: %1").arg(sta));
        }
        QString ata = m_ataItem->text();
        if (!ata.isEmpty())
        {
            list.append(QString("ATA: %1").arg(ata));
        }
        else
        {
            QString eta = m_etaItem->text();
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
QString Aircraft::getLabel(const ADSBDemodSettings *settings, QDateTime& dateTime) const
{
    QString id;
    if (!m_callsign.isEmpty())
    {
        QString atcCallsign = m_atcCallsignItem->text();
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
        QString dep = m_depItem->text();
        QString arr = m_arrItem->text();
        QString stops = m_stopsItem->text();
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

void Aircraft::addCoordinate(const QDateTime& dateTime, AircraftModel *model)
{
    int color = std::min(7, m_altitude / 5000);

    bool showFullPath = (model->m_settings->m_flightPaths && m_isHighlighted) || model->m_settings->m_allFlightPaths;
    bool showATCPath = model->m_settings->m_atcLabels && !showFullPath;

    QGeoCoordinate coord(m_latitude, m_longitude, m_altitude);
    if (m_coordinates.isEmpty())
    {
        QVariantList list;
        list.push_back(QVariant::fromValue(coord));
        m_coordinates.push_back(list);
        m_coordinateColors.push_back(color);
        if (showFullPath) {
            model->aircraftCoordsAdded(this);
        }
    }
    else
    {
        if (color == m_lastColor)
        {
            QVariantList& list = m_coordinates.back();
            list.push_back(QVariant::fromValue(coord));
            if (showFullPath) {
                model->aircraftCoordsUpdated(this);
            }
        }
        else
        {
            QVariantList& prevList = m_coordinates.back();
            prevList.push_back(QVariant::fromValue(coord));
            m_coordinateDateTimes.push_back(dateTime);
            if (showFullPath) {
                model->aircraftCoordsUpdated(this);
            }

            QVariantList list;
            list.push_back(QVariant::fromValue(coord));
            m_coordinates.push_back(list);
            m_coordinateColors.push_back(color);
            if (showFullPath) {
                model->aircraftCoordsAdded(this);
            }
        }
    }

    if (m_recentCoordinates.isEmpty())
    {
        QVariantList list;
        list.push_back(QVariant::fromValue(coord));
        m_recentCoordinates.push_back(list);
        m_recentCoordinateColors.push_back(color);
        if (showATCPath) {
            model->aircraftCoordsAdded(this);
        }
    }
    else
    {
        if (color == m_lastColor)
        {
            QVariantList& list = m_recentCoordinates.back();
            list.push_back(QVariant::fromValue(coord));
            if (showATCPath) {
                model->aircraftCoordsUpdated(this);
            }
        }
        else
        {
            QVariantList& prevList = m_recentCoordinates.back();
            prevList.push_back(QVariant::fromValue(coord));
            if (showATCPath) {
                model->aircraftCoordsUpdated(this);
            }

            QVariantList list;
            list.push_back(QVariant::fromValue(coord));
            m_recentCoordinates.push_back(list);
            m_recentCoordinateColors.push_back(color);
            if (showATCPath) {
                model->aircraftCoordsAdded(this);
            }
        }
    }
    m_coordinateDateTimes.push_back(dateTime);
    m_lastColor = color;

    // Prune recent list so only last 25 seconds of coordinates
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-25);
    int keepCount = 0;
    for (int i = m_coordinateDateTimes.size() - 1; i >= 0; i--)
    {
        if (m_coordinateDateTimes[i] < cutoff) {
            break;
        }
        keepCount++;
    }
    bool removed = false;
    for (int i = m_recentCoordinates.size() - 1; i >= 0; i--)
    {
        int size = (int) m_recentCoordinates[i].size();

        if (keepCount <= 0)
        {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
            m_recentCoordinates.remove(0, i + 1);
            m_recentCoordinateColors.remove(0, i + 1);
#else
            for (int j = 0; j < i + 1; j++)
            {
                m_recentCoordinates.removeAt(0);
                m_recentCoordinateColors.removeAt(0);
            }
#endif
            removed = true;
            break;
        }
        else
        {
            if (size > keepCount)
            {
                int remove = size - keepCount + 1;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
                m_recentCoordinates[i].remove(0, remove);
#else
                for (int j = 0; j < remove; j++) {
                    m_recentCoordinates[i].removeAt(0);
                }
#endif
                removed = true;
                keepCount -= remove;
            }
            else
            {
                keepCount -= size;
            }
        }
    }
    if (showATCPath && removed) {
        model->aircraftCoordsRemoved(this);
    }
}

void Aircraft::clearCoordinates(AircraftModel *model)
{
    m_coordinates.clear();
    m_coordinateColors.clear();
    m_recentCoordinates.clear();
    m_recentCoordinateColors.clear();
    m_coordinateDateTimes.clear();
    model->clearCoords(this);
}

void AircraftPathModel::add()
{
    beginInsertRows(QModelIndex(), m_paths, m_paths);
    m_paths++;
    endInsertRows();
}

void AircraftPathModel::updateLast()
{
    QModelIndex idx = index(m_paths - 1);
    emit dataChanged(idx, idx);
}

void AircraftPathModel::clear()
{
    if (m_paths > 0)
    {
        beginRemoveRows(QModelIndex(), 0, m_paths - 1);
        m_paths = 0;
        endRemoveRows();
    }
}

void AircraftPathModel::removed()
{
    beginResetModel();
    // Coordinates are never removed from m_coordinates
    m_paths = m_aircraft->m_recentCoordinates.size();
    endResetModel();
}

void AircraftPathModel::settingsUpdated()
{
    if (!m_aircraftModel->m_settings) {
        return;
    }

    bool showFullPath = (m_aircraftModel->m_settings->m_flightPaths && m_aircraft->m_isHighlighted) || m_aircraftModel->m_settings->m_allFlightPaths;
    bool showATCPath = m_aircraftModel->m_settings->m_atcLabels && !showFullPath;

    if (showFullPath)
    {
        if (!m_showFullPath)
        {
            m_showFullPath = showFullPath;
            m_showATCPath = false;
            beginResetModel();
            m_paths = m_aircraft->m_coordinates.size();
            endResetModel();
        }
    }
    else if (showATCPath)
    {
        if (!m_showATCPath)
        {
            m_showFullPath = false;
            m_showATCPath = showATCPath;
            beginResetModel();
            m_paths = m_aircraft->m_recentCoordinates.size();
            endResetModel();
        }
    }
    else if (m_showFullPath || m_showATCPath)
    {
        m_showFullPath = false;
        m_showATCPath = false;
        beginResetModel();
        m_paths = 0;
        endResetModel();
    }
}

QVariant AircraftPathModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_paths)) {
        return QVariant();
    }
    /*qDebug() << "AircraftPathModel::data: row" << row << "m_paths" << m_paths << "m_showFullPath" << m_showFullPath << "m_showATCPath" << m_showATCPath
        << "m_aircraft->m_coordinates.size()" << m_aircraft->m_coordinates.size() << "m_aircraft->m_recentCoordinates" << m_aircraft->m_recentCoordinates.size()
        << "m_aircraft->m_coordinateColors.size()" << m_aircraft->m_coordinateColors.size() << "m_aircraft->m_recentCoordinateColors" << m_aircraft->m_recentCoordinateColors.size();*/

    if (role == AircraftPathModel::coordinatesRole)
    {
        if (m_showFullPath) {
            return m_aircraft->m_coordinates[row];
        } else if (m_showATCPath) {
            return m_aircraft->m_recentCoordinates[row];
        } else {
            return QVariantList();
        }
    }
    else if (role == AircraftPathModel::colorRole)
    {
        if (m_showFullPath) {
            return m_aircraftModel->m_settings->m_flightPathPalette[m_aircraft->m_coordinateColors[row]];
        } else if (m_showATCPath) {
            return m_aircraftModel->m_settings->m_flightPathPalette[m_aircraft->m_recentCoordinateColors[row]];
        } else {
            return QVariant();
        }
    }
    return QVariant();
}

QVariant AircraftModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_aircrafts.count()))
        return QVariant();
    if (role == AircraftModel::positionRole)
    {
        // Coordinates to display the aircraft icon at
        QGeoCoordinate coords;
        coords.setLatitude(m_aircrafts[row]->m_latitude);
        coords.setLongitude(m_aircrafts[row]->m_longitude);
        coords.setAltitude(Units::feetToMetres(m_aircrafts[row]->m_altitude));
        return QVariant::fromValue(coords);
    }
    else if (role == AircraftModel::headingRole)
    {
        // What rotation to draw the aircraft icon at
        if (m_aircrafts[row]->m_trackValid) {
            return QVariant::fromValue(m_aircrafts[row]->m_track);
        } else {
            return QVariant::fromValue(m_aircrafts[row]->m_heading);
        }
    }
    else if (role == AircraftModel::adsbDataRole)
    {
        // Create the text to go in the bubble next to the aircraft
        return QVariant::fromValue(m_aircrafts[row]->getText(m_settings));
    }
    else if (role == AircraftModel::aircraftImageRole)
    {
        // Select an image to use for the aircraft
        return QVariant::fromValue(m_aircrafts[row]->getImage());
    }
    else if (role == AircraftModel::bubbleColourRole)
    {
        // Select a background colour for the text bubble next to the aircraft
        if (m_aircrafts[row]->m_isTarget)
            return  QVariant::fromValue(QColor("lightgreen"));
        else if (m_aircrafts[row]->m_isHighlighted)
            return  QVariant::fromValue(QColor("orange"));
        else if ((m_aircrafts[row]->m_status.length() > 0) && m_aircrafts[row]->m_status.compare("No emergency"))
            return QVariant::fromValue(QColor("lightred"));
        else
            return QVariant::fromValue(QColor("lightblue"));
    }
    else if (role == AircraftModel::showAllRole)
        return QVariant::fromValue(m_aircrafts[row]->m_showAll);
    else if (role == AircraftModel::highlightedRole)
        return QVariant::fromValue(m_aircrafts[row]->m_isHighlighted);
    else if (role == AircraftModel::targetRole)
        return QVariant::fromValue(m_aircrafts[row]->m_isTarget);
    else if (role == AircraftModel::radiusRole)
        return QVariant::fromValue(m_aircrafts[row]->m_radius);
    else if (role == AircraftModel::aircraftPathModelRole)
        return QVariant::fromValue(m_pathModels[row]);
    return QVariant();
}

bool AircraftModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    int row = index.row();
    if ((row < 0) || (row >= m_aircrafts.count()))
        return false;
    if (role == AircraftModel::showAllRole)
    {
        bool showAll = value.toBool();
        if (showAll != m_aircrafts[row]->m_showAll)
        {
            m_aircrafts[row]->m_showAll = showAll;
            emit dataChanged(index, index);
        }
        return true;
    }
    else if (role == AircraftModel::highlightedRole)
    {
        bool highlight = value.toBool();
        if (highlight != m_aircrafts[row]->m_isHighlighted)
        {
            m_aircrafts[row]->m_gui->highlightAircraft(m_aircrafts[row]);
            emit dataChanged(index, index);
        }
        return true;
    }
    else if (role == AircraftModel::targetRole)
    {
        bool target = value.toBool();
        if (target != m_aircrafts[row]->m_isTarget)
        {
            m_aircrafts[row]->m_gui->targetAircraft(m_aircrafts[row]);
            emit dataChanged(index, index);
        }
        return true;
    }
    return true;
}

void AircraftModel::findOnMap(int index)
{
    if ((index < 0) || (index >= m_aircrafts.count())) {
        return;
    }
    FeatureWebAPIUtils::mapFind(m_aircrafts[index]->m_icaoHex);
}

// Get list of frequeny scanners to use in menu
QStringList AirportModel::getFreqScanners() const
{
    return MainCore::instance()->getChannelIds("sdrangel.channel.freqscanner");
}

// Send airport frequencies to frequency scanner with given id (Rn:n)
void AirportModel::sendToFreqScanner(int index, const QString& id)
{
    if ((index < 0) || (index >= m_airports.count())) {
        return;
    }
    const AirportInformation *airport = m_airports[index];
    unsigned int deviceSet, channelIndex;

    if (MainCore::getDeviceAndChannelIndexFromId(id, deviceSet, channelIndex))
    {
        QJsonArray array;
        for (const auto airportFrequency : airport->m_frequencies)
        {
            QJsonObject obj;
            QJsonValue frequency(airportFrequency->m_frequency * 1000000);
            QJsonValue enabled(1); // true doesn't work
            QJsonValue notes(QString("%1 %2").arg(airport->m_ident).arg(airportFrequency->m_description));
            obj.insert("frequency", frequency);
            obj.insert("enabled", enabled);
            obj.insert("notes", notes);
            QJsonValue value(obj);
            array.append(value);
        }
        ChannelWebAPIUtils::patchChannelSetting(deviceSet, channelIndex, "frequencies", array);
    }
    else
    {
        qDebug() << "AirportModel::sendToFreqScanner: Malformed id: " << id;
    }
}

QVariant AirportModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_airports.count()))
        return QVariant();
    if (role == AirportModel::positionRole)
    {
        // Coordinates to display the airport icon at
        QGeoCoordinate coords;
        coords.setLatitude(m_airports[row]->m_latitude);
        coords.setLongitude(m_airports[row]->m_longitude);
        coords.setAltitude(Units::feetToMetres(m_airports[row]->m_elevation));
        return QVariant::fromValue(coords);
    }
    else if (role == AirportModel::airportDataRole)
    {
        if (m_showFreq[row])
        {
            QString text = m_airportDataFreq[row];
            if (!m_metar[row].isEmpty()) {
                text = text + "\n" + m_metar[row];
            }
            return QVariant::fromValue(text);
        }
        else
            return QVariant::fromValue(m_airports[row]->m_ident);
    }
    else if (role == AirportModel::airportDataRowsRole)
    {
        if (m_showFreq[row])
        {
            int rows = m_airportDataFreqRows[row];
            if (!m_metar[row].isEmpty()) {
                rows += 1 + m_metar[row].count("\n");
            }
            return QVariant::fromValue(rows);
        }
        else
            return 1;
    }
    else if (role == AirportModel::airportImageRole)
    {
        // Select an image to use for the airport
        return QVariant::fromValue(m_airports[row]->getImageName());
    }
    else if (role == AirportModel::bubbleColourRole)
    {
        // Select a background colour for the text bubble next to the airport
        return QVariant::fromValue(QColor("lightyellow"));
    }
    else if (role == AirportModel::showFreqRole)
    {
        return QVariant::fromValue(m_showFreq[row]);
    }
    return QVariant();
}

bool AirportModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    int row = index.row();
    if ((row < 0) || (row >= m_airports.count()))
        return false;
    if (role == AirportModel::showFreqRole)
    {
        bool showFreq = value.toBool();
        if (showFreq != m_showFreq[row])
        {
            m_showFreq[row] = showFreq;
            emit dataChanged(index, index);
            if (showFreq) {
                emit requestMetar(m_airports[row]->m_ident);
            }
        }
        return true;
    }
    else if (role == AirportModel::selectedFreqRole)
    {
        int idx = value.toInt();
        if ((idx >= 0) && (idx < m_airports[row]->m_frequencies.size()))
        {
            // Do in two steps to avoid rounding errors
            qint64 freqkHz = (qint64) std::round(m_airports[row]->m_frequencies[idx]->m_frequency*1000.0);
            qint64 freqHz = freqkHz * 1000;
            m_gui->setFrequency(freqHz);
        }
        else if (idx == m_airports[row]->m_frequencies.size())
        {
            // Set airport as target
            m_gui->target(m_airports[row]->m_name, m_azimuth[row], m_elevation[row], m_range[row]);
            emit dataChanged(index, index);
        }
        return true;
    }
    return true;
}

QVariant AirspaceModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_airspaces.count())) {
        return QVariant();
    }
    if (role == AirspaceModel::nameRole)
    {
        // Airspace name
        return QVariant::fromValue(m_airspaces[row]->m_name);
    }
    else if (role == AirspaceModel::detailsRole)
    {
        // Airspace name and altitudes
        QString details;
        details.append(m_airspaces[row]->m_name);
        if (m_airspaces[row]->m_bottom.m_alt != -1)
        {
            details.append(QString("\n%1 - %2")
                .arg(m_airspaces[row]->getAlt(&m_airspaces[row]->m_bottom))
                .arg(m_airspaces[row]->getAlt(&m_airspaces[row]->m_top)));
        }
        return QVariant::fromValue(details);
    }
    else if (role == AirspaceModel::positionRole)
    {
        // Coordinates to display the airspace name at
        QGeoCoordinate coords;
        coords.setLatitude(m_airspaces[row]->m_position.y());
        coords.setLongitude(m_airspaces[row]->m_position.x());
        coords.setAltitude(m_airspaces[row]->topHeightInMetres());
        return QVariant::fromValue(coords);
    }
    else if (role == AirspaceModel::airspaceBorderColorRole)
    {
        if (m_airspaces[row]->m_name == "Coverage Map Low") {
            return QVariant::fromValue(QColor(0x00, 0xff, 0x00, 0x00));
        } else if (m_airspaces[row]->m_name == "Coverage Map High") {
            return QVariant::fromValue(QColor(0xff, 0xff, 0x00, 0x00));
        } else if ((m_airspaces[row]->m_category == "D")
            || (m_airspaces[row]->m_category == "G")
            || (m_airspaces[row]->m_category == "CTR")) {
            return QVariant::fromValue(QColor(0x00, 0x00, 0xff, 0x00));
        } else {
            return QVariant::fromValue(QColor(0xff, 0x00, 0x00, 0x00));
        }
    }
    else if (role == AirspaceModel::airspaceFillColorRole)
    {
        if (m_airspaces[row]->m_name.startsWith("IC")) {
            int ic = m_airspaces[row]->m_name.mid(3).toInt();
            int i = (ic & 0x3f) * 3;

            return QVariant::fromValue(QColor(colors[i], colors[i+1], colors[i+2], 0x40));
        } else if (m_airspaces[row]->m_name == "Coverage Map Low") {
            return QVariant::fromValue(QColor(0x00, 0xff, 0x00, 0x40));
        } else if (m_airspaces[row]->m_name == "Coverage Map High") {
            return QVariant::fromValue(QColor(0xff, 0xff, 0x00, 0x40));
        } else if ((m_airspaces[row]->m_category == "D")
            || (m_airspaces[row]->m_category == "G")
            || (m_airspaces[row]->m_category == "CTR")) {
            return QVariant::fromValue(QColor(0x00, 0x00, 0xff, 0x10));
        } else {
            return QVariant::fromValue(QColor(0xff, 0x00, 0x00, 0x10));
        }
    }
    else if (role == AirspaceModel::airspacePolygonRole)
    {
       return m_polygons[row];
    }
    return QVariant();
}

bool AirspaceModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    (void) value;
    (void) role;

    int row = index.row();
    if ((row < 0) || (row >= m_airspaces.count())) {
        return false;
    }
    return true;
}

QVariant NavAidModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_navAids.count())) {
        return QVariant();
    }
    if (role == NavAidModel::positionRole)
    {
        // Coordinates to display the VOR icon at
        QGeoCoordinate coords;
        coords.setLatitude(m_navAids[row]->m_latitude);
        coords.setLongitude(m_navAids[row]->m_longitude);
        coords.setAltitude(Units::feetToMetres(m_navAids[row]->m_elevation));
        return QVariant::fromValue(coords);
    }
    else if (role == NavAidModel::navAidDataRole)
    {
        // Create the text to go in the bubble next to the VOR
        if (m_selected[row])
        {
            QStringList list;
            list.append(QString("Name: %1").arg(m_navAids[row]->m_name));
            if (m_navAids[row]->m_type == "NDB") {
                list.append(QString("Frequency: %1 kHz").arg(m_navAids[row]->m_frequencykHz, 0, 'f', 1));
            } else {
                list.append(QString("Frequency: %1 MHz").arg(m_navAids[row]->m_frequencykHz / 1000.0f, 0, 'f', 2));
            }
            if (m_navAids[row]->m_channel != "") {
                list.append(QString("Channel: %1").arg(m_navAids[row]->m_channel));
            }
            list.append(QString("Ident: %1 %2").arg(m_navAids[row]->m_ident).arg(Morse::toSpacedUnicodeMorse(m_navAids[row]->m_ident)));
            list.append(QString("Range: %1 nm").arg(m_navAids[row]->m_range));
            if (m_navAids[row]->m_alignedTrueNorth) {
                list.append(QString("Magnetic declination: Aligned to true North"));
            } else if (m_navAids[row]->m_magneticDeclination != 0.0f) {
                list.append(QString("Magnetic declination: %1%2").arg(std::round(m_navAids[row]->m_magneticDeclination)).arg(QChar(0x00b0)));
            }
            QString data = list.join("\n");
            return QVariant::fromValue(data);
        }
        else
        {
            return QVariant::fromValue(m_navAids[row]->m_name);
        }
    }
    else if (role == NavAidModel::navAidImageRole)
    {
        // Select an image to use for the NavAid
        return QVariant::fromValue(QString("%1.png").arg(m_navAids[row]->m_type));
    }
    else if (role == NavAidModel::bubbleColourRole)
    {
        // Select a background colour for the text bubble next to the NavAid
        return QVariant::fromValue(QColor("lightgreen"));
    }
    else if (role == NavAidModel::selectedRole)
    {
        return QVariant::fromValue(m_selected[row]);
    }
    return QVariant();
}

bool NavAidModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    int row = index.row();
    if ((row < 0) || (row >= m_navAids.count())) {
        return false;
    }
    if (role == NavAidModel::selectedRole)
    {
        bool selected = value.toBool();
        m_selected[row] = selected;
        emit dataChanged(index, index);
        return true;
    }
    return true;
}

// Set selected AM Demod to the given frequency (used to tune to ATC selected from airports on map)
bool ADSBDemodGUI::setFrequency(qint64 targetFrequencyHz)
{
    unsigned int deviceSet, channelIndex;

    if (MainCore::getDeviceAndChannelIndexFromId(m_settings.m_amDemod, deviceSet, channelIndex))
    {
        const int halfChannelBW = 20000/2;
        int dcOffset = halfChannelBW;

        double centerFrequency;
        int sampleRate;

        if (ChannelWebAPIUtils::getCenterFrequency(deviceSet, centerFrequency))
        {
            if (ChannelWebAPIUtils::getDevSampleRate(deviceSet, sampleRate))
            {
                sampleRate *= 0.75; // Don't use guard bands

                // Adjust device center frequency if not in range
                if (   ((targetFrequencyHz - halfChannelBW) < (centerFrequency - sampleRate / 2))
                    || ((targetFrequencyHz + halfChannelBW) >= (centerFrequency + sampleRate / 2))
                   )
                {
                    ChannelWebAPIUtils::setCenterFrequency(deviceSet, targetFrequencyHz - dcOffset);
                    ChannelWebAPIUtils::setFrequencyOffset(deviceSet, channelIndex, dcOffset);
                }
                else
                {
                    qint64 offset = targetFrequencyHz - centerFrequency;
                    // Also adjust center frequency if channel would cross DC
                    if (std::abs(offset) < halfChannelBW)
                    {
                        ChannelWebAPIUtils::setCenterFrequency(deviceSet, targetFrequencyHz - dcOffset);
                        ChannelWebAPIUtils::setFrequencyOffset(deviceSet, channelIndex, dcOffset);
                    }
                    else
                    {
                        // Just tune channel
                        ChannelWebAPIUtils::setFrequencyOffset(deviceSet, channelIndex, offset);
                    }
                }

                return true;
            }
        }
    }
    return false;
}

void ADSBDemodGUI::clearFromMap(const QString& name)
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

void ADSBDemodGUI::sendToMap(Aircraft *aircraft, QList<SWGSDRangel::SWGMapAnimation *> *animations)
{
    // Send to Map feature
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_adsbDemod, "mapitems", mapPipes);

    if (mapPipes.size() > 0)
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

        for (const auto& pipe : mapPipes)
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
            swgMapItem->setImage(new QString(QString("qrc:///map/%1").arg(aircraft->getImage())));
            // Use track, in preference to heading, as we typically receive far fewer heading frames
            if (aircraft->m_trackValid) {
                swgMapItem->setImageRotation(aircraft->m_track);
            } else {
                swgMapItem->setImageRotation(aircraft->m_heading);
            }
            swgMapItem->setText(new QString(aircraft->getText(&m_settings, true)));

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
            swgMapItem->setAnimations(animations);  // Does this need to be duplicated?

            SWGSDRangel::SWGMapAircraftState *aircraftState = new SWGSDRangel::SWGMapAircraftState();
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
            if (aircraft->m_baroValid) {
                aircraftState->setQnh(aircraft->m_baro);
            } else {
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
            QString acasCapability = aircraft->m_acasItem->text();
            if (acasCapability == "") {
                aircraftState->setTcasMode(0); // Unknown
            } else if (acasCapability == "No ACAS") {
                aircraftState->setTcasMode(1); // Off
            } else if (acasCapability == "No RA") {
                aircraftState->setTcasMode(2); // TA Only
            } else {
                aircraftState->setTcasMode(3); // TA/RA
            }

            // Windspeed/direction are rarely available, so use calculated headwind as backup
            QVariant windSpeed = aircraft->m_windSpeedItem->data(Qt::DisplayRole);
            QVariant windDirection = aircraft->m_windDirItem->data(Qt::DisplayRole);
            if (!windSpeed.isNull() && !windDirection.isNull())
            {
                aircraftState->setWindSpeed(windSpeed.toFloat());
                aircraftState->setWindDirection(windDirection.toFloat());
            }
            else
            {
                QVariant headwind = aircraft->m_headwindItem->data(Qt::DisplayRole);
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
            QVariant staticAirTemp = aircraft->m_staticAirTempItem->data(Qt::DisplayRole);
            if (!staticAirTemp.isNull())
            {
                aircraftState->setStaticAirTemperature(staticAirTemp.toFloat());
            }
            else
            {
                QVariant estAirTemp = aircraft->m_estAirTempItem->data(Qt::DisplayRole);
                if (!estAirTemp.isNull()) {
                    aircraftState->setStaticAirTemperature(estAirTemp.toFloat());
                } else {
                    aircraftState->setStaticAirTemperature(std::numeric_limits<float>::quiet_NaN());
                }
            }

            swgMapItem->setAircraftState(aircraftState);

            MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_adsbDemod, swgMapItem);
            messageQueue->push(msg);
        }
    }
}

// Find aircraft with icao, or create if it doesn't exist
Aircraft *ADSBDemodGUI::getAircraft(int icao, bool &newAircraft)
{
    Aircraft *aircraft;

    if (m_aircraft.contains(icao))
    {
        // Update existing aircraft info
        aircraft = m_aircraft.value(icao);
    }
    else
    {
        // Add new aircraft
        newAircraft = true;
        aircraft = new Aircraft(this);
        aircraft->m_icao = icao;
        //aircraft->m_icaoHex = QString::number(aircraft->m_icao, 16);
        aircraft->m_icaoHex = QString("%1").arg(aircraft->m_icao, 6, 16, QChar('0'));
        m_aircraft.insert(icao, aircraft);
        aircraft->m_icaoItem->setText(aircraft->m_icaoHex);
        ui->adsbData->setSortingEnabled(false);
        int row = ui->adsbData->rowCount();
        ui->adsbData->setRowCount(row + 1);
        ui->adsbData->setItem(row, ADSB_COL_ICAO, aircraft->m_icaoItem);
        ui->adsbData->setItem(row, ADSB_COL_ATC_CALLSIGN, aircraft->m_atcCallsignItem);
        ui->adsbData->setItem(row, ADSB_COL_CALLSIGN, aircraft->m_callsignItem);
        ui->adsbData->setItem(row, ADSB_COL_MODEL, aircraft->m_modelItem);
        ui->adsbData->setItem(row, ADSB_COL_TYPE, aircraft->m_typeItem);
        ui->adsbData->setItem(row, ADSB_COL_SIDEVIEW, aircraft->m_sideviewItem);
        ui->adsbData->setItem(row, ADSB_COL_AIRLINE, aircraft->m_airlineItem);
        ui->adsbData->setItem(row, ADSB_COL_ALTITUDE, aircraft->m_altitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_HEADING, aircraft->m_headingItem);
        ui->adsbData->setItem(row, ADSB_COL_TRACK, aircraft->m_trackItem);
        ui->adsbData->setItem(row, ADSB_COL_VERTICALRATE, aircraft->m_verticalRateItem);
        ui->adsbData->setItem(row, ADSB_COL_RANGE, aircraft->m_rangeItem);
        ui->adsbData->setItem(row, ADSB_COL_AZEL, aircraft->m_azElItem);
        ui->adsbData->setItem(row, ADSB_COL_LATITUDE, aircraft->m_latitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_LONGITUDE, aircraft->m_longitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_CATEGORY, aircraft->m_emitterCategoryItem);
        ui->adsbData->setItem(row, ADSB_COL_STATUS, aircraft->m_statusItem);
        ui->adsbData->setItem(row, ADSB_COL_SQUAWK, aircraft->m_squawkItem);
        ui->adsbData->setItem(row, ADSB_COL_IDENT, aircraft->m_identItem);
        ui->adsbData->setItem(row, ADSB_COL_REGISTRATION, aircraft->m_registrationItem);
        ui->adsbData->setItem(row, ADSB_COL_COUNTRY, aircraft->m_countryItem);
        ui->adsbData->setItem(row, ADSB_COL_REGISTERED, aircraft->m_registeredItem);
        ui->adsbData->setItem(row, ADSB_COL_MANUFACTURER, aircraft->m_manufacturerNameItem);
        ui->adsbData->setItem(row, ADSB_COL_OWNER, aircraft->m_ownerItem);
        ui->adsbData->setItem(row, ADSB_COL_OPERATOR_ICAO, aircraft->m_operatorICAOItem);
        ui->adsbData->setItem(row, ADSB_COL_IC, aircraft->m_interogatorCodeItem);
        ui->adsbData->setItem(row, ADSB_COL_TIME, aircraft->m_timeItem);
        ui->adsbData->setItem(row, ADSB_COL_FRAMECOUNT, aircraft->m_totalFrameCountItem);
        ui->adsbData->setItem(row, ADSB_COL_ADSB_FRAMECOUNT, aircraft->m_adsbFrameCountItem);
        ui->adsbData->setItem(row, ADSB_COL_MODES_FRAMECOUNT, aircraft->m_modesFrameCountItem);
        ui->adsbData->setItem(row, ADSB_COL_NON_TRANSPONDER, aircraft->m_nonTransponderItem);
        ui->adsbData->setItem(row, ADSB_COL_TIS_B_FRAMECOUNT, aircraft->m_tisBFrameCountItem);
        ui->adsbData->setItem(row, ADSB_COL_ADSR_FRAMECOUNT, aircraft->m_adsrFrameCountItem);
        ui->adsbData->setItem(row, ADSB_COL_RADIUS, aircraft->m_radiusItem);
        ui->adsbData->setItem(row, ADSB_COL_NACP, aircraft->m_nacpItem);
        ui->adsbData->setItem(row, ADSB_COL_NACV, aircraft->m_nacvItem);
        ui->adsbData->setItem(row, ADSB_COL_GVA, aircraft->m_gvaItem);
        ui->adsbData->setItem(row, ADSB_COL_NIC, aircraft->m_nicItem);
        ui->adsbData->setItem(row, ADSB_COL_NIC_BARO, aircraft->m_nicBaroItem);
        ui->adsbData->setItem(row, ADSB_COL_SIL, aircraft->m_silItem);
        ui->adsbData->setItem(row, ADSB_COL_CORRELATION, aircraft->m_correlationItem);
        ui->adsbData->setItem(row, ADSB_COL_RSSI, aircraft->m_rssiItem);
        ui->adsbData->setItem(row, ADSB_COL_FLIGHT_STATUS, aircraft->m_flightStatusItem);
        ui->adsbData->setItem(row, ADSB_COL_DEP, aircraft->m_depItem);
        ui->adsbData->setItem(row, ADSB_COL_ARR, aircraft->m_arrItem);
        ui->adsbData->setItem(row, ADSB_COL_STOPS, aircraft->m_stopsItem);
        ui->adsbData->setItem(row, ADSB_COL_STD, aircraft->m_stdItem);
        ui->adsbData->setItem(row, ADSB_COL_ETD, aircraft->m_etdItem);
        ui->adsbData->setItem(row, ADSB_COL_ATD, aircraft->m_atdItem);
        ui->adsbData->setItem(row, ADSB_COL_STA, aircraft->m_staItem);
        ui->adsbData->setItem(row, ADSB_COL_ETA, aircraft->m_etaItem);
        ui->adsbData->setItem(row, ADSB_COL_ATA, aircraft->m_ataItem);
        ui->adsbData->setItem(row, ADSB_COL_SEL_ALTITUDE, aircraft->m_selAltitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_SEL_HEADING, aircraft->m_selHeadingItem);
        ui->adsbData->setItem(row, ADSB_COL_BARO, aircraft->m_baroItem);
        ui->adsbData->setItem(row, ADSB_COL_AP, aircraft->m_apItem);
        ui->adsbData->setItem(row, ADSB_COL_V_MODE, aircraft->m_vModeItem);
        ui->adsbData->setItem(row, ADSB_COL_L_MODE, aircraft->m_lModeItem);
        ui->adsbData->setItem(row, ADSB_COL_TCAS, aircraft->m_tcasItem);
        ui->adsbData->setItem(row, ADSB_COL_ACAS, aircraft->m_acasItem);
        ui->adsbData->setItem(row, ADSB_COL_RA, aircraft->m_raItem);
        ui->adsbData->setItem(row, ADSB_COL_MAX_SPEED, aircraft->m_maxSpeedItem);
        ui->adsbData->setItem(row, ADSB_COL_VERSION, aircraft->m_versionItem);
        ui->adsbData->setItem(row, ADSB_COL_LENGTH, aircraft->m_lengthItem);
        ui->adsbData->setItem(row, ADSB_COL_WIDTH, aircraft->m_widthItem);
        ui->adsbData->setItem(row, ADSB_COL_ROLL, aircraft->m_rollItem);
        ui->adsbData->setItem(row, ADSB_COL_GROUND_SPEED, aircraft->m_groundspeedItem);
        ui->adsbData->setItem(row, ADSB_COL_TURNRATE, aircraft->m_turnRateItem);
        ui->adsbData->setItem(row, ADSB_COL_TRUE_AIRSPEED, aircraft->m_trueAirspeedItem);
        ui->adsbData->setItem(row, ADSB_COL_INDICATED_AIRSPEED, aircraft->m_indicatedAirspeedItem);
        ui->adsbData->setItem(row, ADSB_COL_MACH, aircraft->m_machItem);
        ui->adsbData->setItem(row, ADSB_COL_HEADWIND, aircraft->m_headwindItem);
        ui->adsbData->setItem(row, ADSB_COL_EST_AIR_TEMP, aircraft->m_estAirTempItem);
        ui->adsbData->setItem(row, ADSB_COL_WIND_SPEED, aircraft->m_windSpeedItem);
        ui->adsbData->setItem(row, ADSB_COL_WIND_DIR, aircraft->m_windDirItem);
        ui->adsbData->setItem(row, ADSB_COL_STATIC_PRESSURE, aircraft->m_staticPressureItem);
        ui->adsbData->setItem(row, ADSB_COL_STATIC_AIR_TEMP, aircraft->m_staticAirTempItem);
        ui->adsbData->setItem(row, ADSB_COL_HUMIDITY, aircraft->m_humidityItem);
        // Look aircraft up in database
        if (m_aircraftInfo != nullptr)
        {
            if (m_aircraftInfo->contains(icao))
            {
                aircraft->m_aircraftInfo = m_aircraftInfo->value(icao);
                aircraft->m_modelItem->setText(aircraft->m_aircraftInfo->m_model);
                aircraft->m_typeItem->setText(aircraft->m_aircraftInfo->m_type);
                aircraft->m_registrationItem->setText(aircraft->m_aircraftInfo->m_registration);
                aircraft->m_manufacturerNameItem->setText(aircraft->m_aircraftInfo->m_manufacturerName);
                aircraft->m_ownerItem->setText(aircraft->m_aircraftInfo->m_owner);
                aircraft->m_operatorICAOItem->setText(aircraft->m_aircraftInfo->m_operatorICAO);
                aircraft->m_registeredItem->setText(aircraft->m_aircraftInfo->m_registered);
                // Try loading an airline logo based on operator ICAO
                QIcon *icon = nullptr;
                if (aircraft->m_aircraftInfo->m_operatorICAO.size() > 0)
                {
                    aircraft->m_airlineIconURL = AircraftInformation::resourcePathToURL(AircraftInformation::getAirlineIconPath(aircraft->m_aircraftInfo->m_operatorICAO));
                    icon = AircraftInformation::getAirlineIcon(aircraft->m_aircraftInfo->m_operatorICAO);
                    if (icon != nullptr)
                    {
                        aircraft->m_airlineItem->setSizeHint(QSize(85, 20));
                        aircraft->m_airlineItem->setIcon(*icon);
                    }
                }
                if (icon == nullptr)
                {
                    if (aircraft->m_aircraftInfo->m_operator.size() > 0)
                        aircraft->m_airlineItem->setText(aircraft->m_aircraftInfo->m_operator);
                    else
                        aircraft->m_airlineItem->setText(aircraft->m_aircraftInfo->m_owner);
                }
                // Try loading a sideview
                icon = AircraftInformation::getSideviewIcon(aircraft->m_aircraftInfo->m_registration, aircraft->m_aircraftInfo->m_operatorICAO, aircraft->m_aircraftInfo->m_type);
                if (icon)
                {
                    aircraft->m_sideviewItem->setSizeHint(QSize(85, 20));
                    aircraft->m_sideviewItem->setIcon(*icon);
                    aircraft->m_sideviewIconURL = AircraftInformation::resourcePathToURL(AircraftInformation::getSideviewIconPath(aircraft->m_aircraftInfo->m_registration, aircraft->m_aircraftInfo->m_operatorICAO, aircraft->m_aircraftInfo->m_type));
                }
                // Try loading a flag based on registration
                if (aircraft->m_aircraftInfo->m_registration.size() > 0)
                {
                    QString flag = aircraft->m_aircraftInfo->getFlag();
                    if (flag != "")
                    {
                        aircraft->m_flagIconURL = AircraftInformation::resourcePathToURL(AircraftInformation::getFlagIconPath(flag));
                        icon = AircraftInformation::getFlagIcon(flag);
                        if (icon != nullptr)
                        {
                            aircraft->m_countryItem->setSizeHint(QSize(40, 20));
                            aircraft->m_countryItem->setIcon(*icon);
                        }
                    }
                }
                get3DModel(aircraft);
            }
        }

        if (aircraft->m_aircraft3DModel.isEmpty())
        {
            // Default to A320 until we get some more info
            aircraft->m_aircraftCat3DModel = get3DModel("A320");
            if (m_modelAltitudeOffset.contains("A320"))
            {
                aircraft->m_modelAltitudeOffset = m_modelAltitudeOffset.value("A320");
                aircraft->m_labelAltitudeOffset = m_labelAltitudeOffset.value("A320");
            }
        }

        if (!m_loadingData)
        {
            if (m_settings.m_autoResizeTableColumns)
                ui->adsbData->resizeColumnsToContents();
            ui->adsbData->setSortingEnabled(true);
        }
        // Check to see if we need to emit a notification about this new aircraft
        checkStaticNotification(aircraft);
    }

    return aircraft;
}

void ADSBDemodGUI::setCallsign(Aircraft *aircraft, const QString& callsign)
{
    aircraft->m_callsign = callsign;
    aircraft->m_callsignItem->setText(aircraft->m_callsign);
    if (m_routeInfo && m_routeInfo->contains(aircraft->m_callsign))
    {
        AircraftRouteInformation *route = m_routeInfo->value(aircraft->m_callsign);
        aircraft->m_depItem->setText(route->m_dep);
        aircraft->m_arrItem->setText(route->m_arr);
        aircraft->m_stopsItem->setText(route->m_stops);
    }
    atcCallsign(aircraft);
    callsignToFlight(aircraft);
}

void ADSBDemodGUI::atcCallsign(Aircraft *aircraft)
{
    QString icao = aircraft->m_callsign.left(3);
    const Airline *airline = Airline::getByICAO(icao);
    if (airline)
    {
        aircraft->m_atcCallsignItem->setText(airline->m_callsign);
        // Create icons using data from Airline class, if it doesn't exist in database
        if (!aircraft->m_aircraftInfo)
        {
            // Airline logo
            QIcon *icon = nullptr;
            aircraft->m_airlineIconURL = AircraftInformation::resourcePathToURL(AircraftInformation::getAirlineIconPath(airline->m_icao));
            icon = AircraftInformation::getAirlineIcon(airline->m_icao);
            if (icon != nullptr)
            {
                aircraft->m_airlineItem->setSizeHint(QSize(85, 20));
                aircraft->m_airlineItem->setIcon(*icon);
            }
            else
            {
                aircraft->m_airlineItem->setText(airline->m_name);
            }
            // Flag
            QString flag = airline->m_country.toLower().replace(" ", "_");
            aircraft->m_flagIconURL = AircraftInformation::resourcePathToURL(AircraftInformation::getFlagIconPath(flag));
            icon = AircraftInformation::getFlagIcon(flag);
            if (icon != nullptr)
            {
                aircraft->m_countryItem->setSizeHint(QSize(40, 20));
                aircraft->m_countryItem->setIcon(*icon);
            }
        }
    }
}

// Try to map callsign to flight number
void ADSBDemodGUI::callsignToFlight(Aircraft *aircraft)
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

// ADS-B and Mode-S selected altitudes have slightly different precision, so values can jump around a bit
// And we end up with altitudes such as 38008 rather than 38000
// To remove this, we round to nearest 50 feet
int ADSBDemodGUI::roundTo50Feet(int alt)
{
    return ((alt + 25) / 50) * 50;
}

// Estimate outside air temperature (static temperature) from Mach number and true airspeed, assuming dry air
bool ADSBDemodGUI::calcAirTemp(Aircraft *aircraft)
{
    if (aircraft->m_machValid && aircraft->m_trueAirspeedValid)
    {
        // Calculate speed of sound
        float c = Units::knotsToMetresPerSecond(aircraft->m_trueAirspeed) / aircraft->m_mach;

        // Calculate temperature, given the speed of sound
        float a = c / 331.3f;
        float T = (a * a - 1.0f) * 273.15f;

        aircraft->m_estAirTempItem->setData(Qt::DisplayRole, (int)std::round(T));

        return true;
    }
    else
    {
        return false;
    }
}

void ADSBDemodGUI::resetStats()
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
    for (int i = 0; i < ui->statsTable->rowCount(); i++) {
        ui->statsTable->item(i, 0)->setData(Qt::DisplayRole, 0);
    }

    ADSBDemod::MsgResetStats* message = ADSBDemod::MsgResetStats::create();
    m_adsbDemod->getInputMessageQueue()->push(message);
}

void ADSBDemodGUI::updateDFStats(int df)
{
    switch (df)
    {
    case 0:
        ui->statsTable->item(DF0, 0)->setData(Qt::DisplayRole, m_dfStats[df]);
        break;
    case 4:
        ui->statsTable->item(DF4, 0)->setData(Qt::DisplayRole, m_dfStats[df]);
        break;
    case 5:
        ui->statsTable->item(DF5, 0)->setData(Qt::DisplayRole, m_dfStats[df]);
        break;
    case 11:
        ui->statsTable->item(DF11, 0)->setData(Qt::DisplayRole, m_dfStats[df]);
        break;
    case 16:
        ui->statsTable->item(DF16, 0)->setData(Qt::DisplayRole, m_dfStats[df]);
        break;
    case 17:
        ui->statsTable->item(DF17, 0)->setData(Qt::DisplayRole, m_dfStats[df]);
        break;
    case 18:
        ui->statsTable->item(DF18, 0)->setData(Qt::DisplayRole, m_dfStats[df]);
        break;
    case 19:
        ui->statsTable->item(DF19, 0)->setData(Qt::DisplayRole, m_dfStats[df]);
        break;
    case 20:
    case 21:
        ui->statsTable->item(DF20_21, 0)->setData(Qt::DisplayRole, m_dfStats[20] + m_dfStats[21]);
        break;
    case 22:
        ui->statsTable->item(DF22, 0)->setData(Qt::DisplayRole, m_dfStats[df]);
        break;
    case 24:
    case 25:
    case 26:
    case 27:
        ui->statsTable->item(DF24, 0)->setData(Qt::DisplayRole, m_dfStats[24] + m_dfStats[25] + m_dfStats[26] + m_dfStats[27]);
        break;
    default:
        break;
    }
}

bool ADSBDemodGUI::updateTCStats(int tc, int row, int low, int high)
{
    if ((tc >= low) && (tc <= high))
    {
        qint64 sum = 0;

        for (int i = low; i <= high; i++) {
            sum += m_tcStats[i];
        }
        ui->statsTable->item(row, 0)->setData(Qt::DisplayRole, sum);
        return true;
    }
    else
    {
        return false;
    }
}

void Aircraft::setOnSurface(const QDateTime& dateTime)
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
    m_altitudeItem->setData(Qt::DisplayRole, "Surface");
    m_altitudeDateTime = dateTime;
}

void Aircraft::setAltitude(int altitudeFt, bool gnss, const QDateTime& dateTime, const ADSBDemodSettings& settings)
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
    m_altitudeItem->setData(Qt::DisplayRole, settings.m_siUnits ? Units::feetToIntegerMetres(m_altitude) : m_altitude);
    m_altitudeDateTime = dateTime;
}

void Aircraft::setVerticalRate(int verticalRate, const ADSBDemodSettings& settings)
{
    m_verticalRate = verticalRate;
    m_verticalRateValid = true;
    if (settings.m_siUnits) {
        m_verticalRateItem->setData(Qt::DisplayRole, Units::feetPerMinToIntegerMetresPerSecond(verticalRate));
    } else {
        m_verticalRateItem->setData(Qt::DisplayRole, m_verticalRate);
    }
}

void Aircraft::setGroundspeed(float groundspeed, const ADSBDemodSettings& settings)
{
    m_groundspeed = groundspeed;
    m_groundspeedValid = true;
    m_groundspeedItem->setData(Qt::DisplayRole, settings.m_siUnits ? Units::knotsToIntegerKPH(m_groundspeed) : (int)std::round(m_groundspeed));
    // FIXME: dateTime?
}

void Aircraft::setTrueAirspeed(int airspeed, const ADSBDemodSettings& settings)
{
    m_trueAirspeed = airspeed;
    m_trueAirspeedValid = true;
    m_trueAirspeedItem->setData(Qt::DisplayRole, settings.m_siUnits ? Units::knotsToIntegerKPH(m_trueAirspeed) : m_trueAirspeed);
}

void Aircraft::setIndicatedAirspeed(int airspeed, const QDateTime& dateTime, const ADSBDemodSettings& settings)
{
    m_indicatedAirspeed = airspeed;
    m_indicatedAirspeedValid = true;
    m_indicatedAirspeedItem->setData(Qt::DisplayRole, settings.m_siUnits ? Units::knotsToIntegerKPH(m_indicatedAirspeed) : m_indicatedAirspeed);
    m_indicatedAirspeedDateTime = dateTime;
}

void Aircraft::setTrack(float track, const QDateTime& dateTime)
{
    m_track = track;
    m_trackValid = true;
    m_trackDateTime = dateTime;
    m_trackItem->setData(Qt::DisplayRole, std::round(m_track));
    m_orientationDateTime = dateTime;
}

void Aircraft::setHeading(float heading, const QDateTime& dateTime)
{
    m_heading = heading;
    m_headingValid = true;
    m_headingDateTime = dateTime;
    m_headingItem->setData(Qt::DisplayRole, std::round(m_heading));
    m_orientationDateTime = dateTime;
    m_trackWhenHeadingSet = m_track;
}

void ADSBDemodGUI::handleADSB(
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

    const int df = (data[0] >> 3) & ADS_B_DF_MASK; // Downlink format
    const int ca = data[0] & 0x7; // Capability (or Control Field for TIS-B)
    unsigned icao;
    Aircraft *aircraft;

    if ((df == 11) || (df == 17) || (df == 18))
    {
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
            qDebug() << "ADSBDemodGUI::handleADSB: iaco of 0 - df" << df;
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
            // Aircraft identification - BDS 0,8
            QString emitterCategory, callsign;

            decodeID(data, emitterCategory, callsign);

            QString prevEmitterCategory = aircraft->m_emitterCategory;
            aircraft->m_emitterCategory = emitterCategory;
            aircraft->m_emitterCategoryItem->setText(aircraft->m_emitterCategory);

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
                    aircraft->m_groundspeedItem->setData(Qt::DisplayRole, "");
                }
                else if (movement == 1)
                {
                    // Aircraft stopped
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
                    else
                    {
                        if (!aircraft->m_globalPosition)
                        {
                            aircraft->m_globalPosition = true;
                            double latDiff = abs(globalLatitude - aircraft->m_latitude);
                            double lonDiff = abs(globalLongitude - aircraft->m_longitude);
                            double maxLatDiff = 50000/111000.0;
                            double maxLonDiff = cos(Units::degreesToRadians(globalLatitude)) * maxLatDiff;
                            if ((latDiff > maxLatDiff) || (lonDiff > maxLonDiff))
                            {
                                qDebug() << "Aircraft global position a long way from local - deleting track" << aircraft->m_icaoHex << globalLatitude << aircraft->m_latitude << globalLongitude << aircraft->m_longitude;
                                aircraft->clearCoordinates(&m_aircraftModel);
                            }
                        }
                    }
                }

                // Local decode using a single aircraft position + location of receiver or previous global position
                localValid = decodeLocalPosition(f, aircraft->m_cprLat[f], aircraft->m_cprLong[f], aircraft->m_onSurface, aircraft, localLatitude, localLongitude, true);

                if (aircraft->m_globalPosition && !localValid)
                {
                    qDebug() << "Aircraft local position not valid" << aircraft->m_icaoHex << localLatitude << aircraft->m_latitude << localLongitude << aircraft->m_longitude;
                    aircraft->m_globalPosition = false;
                }

                bool positionsInconsistent = false;
                if (globalValid && localValid)
                {
                    double latDiff = abs(globalLatitude - localLatitude);
                    double lonDiff = abs(globalLongitude - localLongitude);
                    double maxLatDiff = 5.1/111000.0;
                    double maxLonDiff = cos(Units::degreesToRadians(globalLatitude)) * maxLatDiff;
                    positionsInconsistent = (latDiff > maxLatDiff) || (lonDiff > maxLonDiff);
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
                        updateAircraftPosition(aircraft, globalLatitude, globalLongitude, dateTime);
                        aircraft->addCoordinate(dateTime, &m_aircraftModel);
                    }
                    else if (localValid)
                    {
                        updateAircraftPosition(aircraft, localLatitude, localLongitude, dateTime);
                        aircraft->addCoordinate(dateTime, &m_aircraftModel);
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
                int nacv = ((data[5] >> 3) & 0x3); // Navigation accuracy for velocity

                aircraft->m_nacvItem->setData(Qt::DisplayRole, m_nacvStrings[nacv]);
            }

            if ((st == 1) || (st == 2))
            {
                // Ground speed
                float v, h;

                decodeGroundspeed(data, v, h);

                clearOldHeading(aircraft, dateTime, h);
                aircraft->setTrack(h, dateTime);
                aircraft->setGroundspeed(v, m_settings);
            }
            else
            {
                // Airspeed (only likely to get this if an aircraft is unable to determine it's position)
                bool tas;
                int as;
                bool hdgValid;
                float hdg;

                decodeAirspeed(data, tas, as, hdgValid, hdg);

                if (hdgValid) {
                    aircraft->setHeading(hdg, dateTime);
                }

                if (tas) {
                    aircraft->setTrueAirspeed(as, m_settings);
                } else {
                    aircraft->setIndicatedAirspeed(as, dateTime, m_settings);
                }
            }

            int verticalRate;
            decodeVerticalRate(data, verticalRate);
            aircraft->setVerticalRate(verticalRate, m_settings);
        }
        else if (tc == 28)
        {
            // Aircraft status - BDS 6,1
            int st = data[4] & 0x7;   // Subtype
            if (st == 1)
            {
                int es = (data[5] >> 5) & 0x7; // Emergency state
                int modeA =  ((data[5] << 8) & 0x1f00) | (data[6] & 0xff); // Mode-A code (squawk)
                aircraft->m_status = m_emergencyStatus[es];
                aircraft->m_statusItem->setText(aircraft->m_status);
                aircraft->m_squawk = squawkDecode(modeA);
                if (modeA & 0x40)
                    aircraft->m_squawkItem->setText(QString("%1 IDENT").arg(aircraft->m_squawk, 4, 10, QLatin1Char('0')));
                else
                    aircraft->m_squawkItem->setText(QString("%1").arg(aircraft->m_squawk, 4, 10, QLatin1Char('0')));
            }
            else if (st == 2)
            {
                // TCAS/ACAS RA Broadcast
            }
        }
        else if (tc == 29)
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
                    aircraft->m_selAltitudeItem->setData(Qt::DisplayRole, Units::feetToIntegerMetres(aircraft->m_selAltitude));
                } else {
                    aircraft->m_selAltitudeItem->setData(Qt::DisplayRole, roundTo50Feet(aircraft->m_selAltitude));
                }
            }
            int baroFix = ((data[6] & 0xf) << 5) | ((data[7] >> 3) & 0x1f);
            if (baroFix != 0)
            {
                float baro = (baroFix - 1) * 0.8f + 800.0f; // mb
                aircraft->m_baro = baro;
                aircraft->m_baroValid = true;
                aircraft->m_baroItem->setData(Qt::DisplayRole, std::round(aircraft->m_baro));
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
                aircraft->m_selHeadingItem->setData(Qt::DisplayRole, std::round(aircraft->m_selHeading));
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
                aircraft->m_apItem->setText(autoPilot ? QChar(0x2713) : QChar(0x2717)); // Tick or cross

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

                if (aircraft->m_tcasItem->text() != "RA") {
                    aircraft->m_tcasItem->setText(tcasOperational ? QChar(0x2713) : QChar(0x2717)); // Tick or cross
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
                aircraft->m_vModeItem->setText(vMode);

                QString lMode = "";
                if (lnavMode) {
                    lMode = lMode + "LNAV ";
                }
                if (approachMode) {
                    lMode = lMode + "APP ";
                }
                lMode = lMode.trimmed();
                aircraft->m_lModeItem->setText(lMode);
            }

        }
        else if (tc == 24)
        {
            // Surface system status
            // Get quite a few from heathrow data
        }
        else if (tc == 31)
        {
            // Aircraft operational status
            int st = data[4] & 0x7;

            int adsbVersion = (data[9] >> 5) & 0x7;

            aircraft->m_adsbVersion = adsbVersion;
            aircraft->m_adsbVersionValid = true;
            aircraft->m_versionItem->setData(Qt::DisplayRole, aircraft->m_adsbVersion);

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

                aircraft->m_nacpItem->setData(Qt::DisplayRole, nacpStrings[nacp]);
                aircraft->m_silItem->setData(Qt::DisplayRole, silString);

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

                    aircraft->m_gvaItem->setData(Qt::DisplayRole, gvaStrings[gva]);
                    aircraft->m_nicBaroItem->setText(nicBaro ? QChar(0x2713) : QChar(0x2717)); // Tick or cross
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

                    aircraft->m_nacvItem->setData(Qt::DisplayRole, m_nacvStrings[nacv]);

                    switch (lengthWidthCode)
                    {
                    case 0:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, "");
                        aircraft->m_widthItem->setData(Qt::DisplayRole, "");
                        break;
                    case 1:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 15);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 23);
                        break;
                    case 2:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 25);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 28.5);
                        break;
                    case 3:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 25);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 34);
                        break;
                    case 4:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 35);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 33);
                        break;
                    case 5:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 35);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 38);
                        break;
                    case 6:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 45);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 39.5);
                        break;
                    case 7:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 45);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 45);
                        break;
                    case 8:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 55);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 45);
                        break;
                    case 9:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 55);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 52);
                        break;
                    case 10:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 65);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 59.5);
                        break;
                    case 11:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 65);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 67);
                        break;
                    case 12:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 75);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 72.5);
                        break;
                    case 13:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 75);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 80);
                        break;
                    case 14:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 85);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 80);
                        break;
                    case 15:
                        aircraft->m_lengthItem->setData(Qt::DisplayRole, 85);
                        aircraft->m_widthItem->setData(Qt::DisplayRole, 90);
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
                    aircraft->m_tcasItem->setForeground(QBrush(Qt::red));
                    aircraft->m_tcasItem->setText("RA");
                }
                else if (aircraft->m_tcasOperationalValid)
                {
                    aircraft->m_tcasItem->setForeground(QBrush());
                    aircraft->m_tcasItem->setText(aircraft->m_tcasOperational ? QChar(0x2713) : QChar(0x2717)); // Tick or cross
                }
                else
                {
                    aircraft->m_tcasItem->setText("");
                }
                aircraft->m_identItem->setText(identActive ? QChar(0x2713) : QChar(0x2717)); // Tick or cross

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
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "?");
            aircraft->m_radius = -1.0f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 0);
        }
        else if (tc == 5)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 7.5 m");
            aircraft->m_radius = 7.5f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 11);
        }
        else if (tc == 6)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 25 m");
            aircraft->m_radius = 25.0f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 10);
        }
        else if ((tc == 7) && aircraft->m_nicSupplementAValid && aircraft->m_nicSupplementA)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 75 m");
            aircraft->m_radius = 75.0f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 9);
        }
        else if (tc == 7)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 0.1 NM");
            aircraft->m_radius = 185.2f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 8);
        }
        else if (tc == 8)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, ">= 0.1 NM");
            aircraft->m_radius = 185.2f; // ?
            aircraft->m_nicItem->setData(Qt::DisplayRole, 0);
        }
        else if (tc == 9)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 7.5 m");
            aircraft->m_radius = 7.5f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 11);
        }
        else if (tc == 10)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 25 m");
            aircraft->m_radius = 25.0f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 10);
        }
        else if ((tc == 11) && aircraft->m_nicSupplementAValid && aircraft->m_nicSupplementA && aircraft->m_nicSupplementBValid && aircraft->m_nicSupplementB)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 75 m");
            aircraft->m_radius = 75.0f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 9);
        }
        else if (tc == 11)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 0.1 NM");
            aircraft->m_radius = 185.2f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 8);
        }
        else if (tc == 12)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 0.2 NM");
            aircraft->m_radius = 370.4f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 7);
        }
        else if ((tc == 13) && aircraft->m_nicSupplementAValid && !aircraft->m_nicSupplementA && aircraft->m_nicSupplementBValid && aircraft->m_nicSupplementB)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 0.3 m");
            aircraft->m_radius = 1111.2f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 6);
        }
        else if ((tc == 13) && aircraft->m_nicSupplementAValid && aircraft->m_nicSupplementA && aircraft->m_nicSupplementBValid && aircraft->m_nicSupplementB)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 0.6 m");
            aircraft->m_radius = 1111.2f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 6);
        }
        else if (tc == 13)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 0.5 NM");
            aircraft->m_radius = 926.0f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 6);
        }
        else if (tc == 14)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 1 NM");
            aircraft->m_radius = 1852.0f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 5);
        }
        else if (tc == 15)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 2 NM");
            aircraft->m_radius = 3704.0f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 4);
        }
        else if ((tc == 16) && aircraft->m_nicSupplementAValid && aircraft->m_nicSupplementA && aircraft->m_nicSupplementBValid && aircraft->m_nicSupplementB)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 4 m");
            aircraft->m_radius = 7408.0f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 3);
        }
        else if (tc == 16)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 8 NM");
            aircraft->m_radius = 14816.0f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 2);
        }
        else if (tc == 17)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 20 NM");
            aircraft->m_radius = 37040.0f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 1);
        }
        else if (tc == 18)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, ">= 20 NM");
            aircraft->m_radius = 37040.0f; // ?
            aircraft->m_nicItem->setData(Qt::DisplayRole, 0);
        }
        else if (tc == 20)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 7.5 m");
            aircraft->m_radius = 7.5f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 11);
        }
        else if (tc == 21)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, "< 25 m");
            aircraft->m_radius = 25.0f;
            aircraft->m_nicItem->setData(Qt::DisplayRole, 10);
        }
        else if (tc == 22)
        {
            aircraft->m_radiusItem->setData(Qt::DisplayRole, ">= 25 m");
            aircraft->m_radius = 25.0f; // ?
            aircraft->m_nicItem->setData(Qt::DisplayRole, 0);
        }

        // Update aircraft in map
        if (aircraft->m_positionValid)
        {
            // Check to see if we need to start any animations
            QList<SWGSDRangel::SWGMapAnimation *> *animations = animate(dateTime, aircraft);

            // Update map displayed in channel
            if (updateModel) {
                m_aircraftModel.aircraftUpdated(aircraft);
            }

            // Send to Map feature
            sendToMap(aircraft, animations);

            if (resetAnimation)
            {
                // Wait until after model has changed before resetting
                // otherwise animation might play on old model
                aircraft->m_gearDown = false;
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

            aircraft->m_interogatorCodeItem->setData(Qt::DisplayRole, ic);
            if (ic > 0) {
                m_interogators[ic - 1].update(ic, aircraft, &m_airspaceModel, ui->ic, m_settings.m_displayIC[ic - 1]);
            }
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
            aircraft->m_acasItem->setText(acasCapability);
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
            aircraft->m_maxSpeedItem->setText(maxSpeed);
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

        if (dv == 0x30)
        {
            // TODO: Merge in decodeCommB?
            unsigned activeRAs = ((data[5] & 0xff) << 6) | ((data[6] >> 2) & 0x3f);
            unsigned racs = ((data[6] & 0x3) << 2) | ((data[7] >> 6) & 0x3);
            bool raTerminated = (data[7] >> 5) & 1;
            bool multipleThreats = (data[7] >> 4) & 1;
            unsigned threatType = (data[7] >> 2) & 0x3;

            QString threats;
            bool msb = (activeRAs >> 13) & 0x1;
            if (!msb)
            {
                if (!multipleThreats) {
                    threats = "No threats";
                } else {
                    threats = "Multiple threats";
                }
            }
            else
            {
                threats = "One threat";
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

            QString threatData;
            if (threatType == 1)
            {
                unsigned threatICAO = ((data[7] & 0x3) << 22) | ((data[8] & 0xff) << 14) | ((data[9] & 0xff) << 6) | ((data[10] >> 2) & 0x3f);
                threatData = QString::number(threatICAO, 16);
            }

            if (raTerminated)
            {
                aircraft->m_raItem->setText("RA terminated");
            }
            else
            {
                QStringList s;
                s.append(threats);
                s.append(racStrings.join(""));
                s.append(threatData);
                aircraft->m_raItem->setText(s.join(" "));
            }
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
    QTime time = dateTime.time();
    aircraft->m_timeItem->setText(QString("%1:%2:%3").arg(time.hour(), 2, 10, QLatin1Char('0')).arg(time.minute(), 2, 10, QLatin1Char('0')).arg(time.second(), 2, 10, QLatin1Char('0')));
    if (df == 17)
    {
        aircraft->m_adsbFrameCount++;
        aircraft->m_adsbFrameCountItem->setData(Qt::DisplayRole, aircraft->m_adsbFrameCount);
    }
    else if (df == 18)
    {
        if (ca == 0)
        {
            aircraft->m_nonTransponderFrameCount++;
            aircraft->m_nonTransponderItem->setData(Qt::DisplayRole, aircraft->m_nonTransponderFrameCount);
        }
        else if (ca == 6)
        {
            aircraft->m_adsrFrameCount++;
            aircraft->m_adsrFrameCountItem->setData(Qt::DisplayRole, aircraft->m_adsrFrameCount);
        }
        else
        {
            aircraft->m_tisBFrameCount++;
            aircraft->m_tisBFrameCountItem->setData(Qt::DisplayRole, aircraft->m_tisBFrameCount);
        }
    }
    else
    {
        aircraft->m_modesFrameCount++;
        aircraft->m_modesFrameCountItem->setData(Qt::DisplayRole, aircraft->m_modesFrameCount);
    }
    aircraft->m_totalFrameCountItem->setData(Qt::DisplayRole, aircraft->m_adsbFrameCount + aircraft->m_tisBFrameCount + aircraft->m_modesFrameCount);

    if (correlation < aircraft->m_minCorrelation)
        aircraft->m_minCorrelation = correlation;
    if (correlation > aircraft->m_maxCorrelation)
        aircraft->m_maxCorrelation = correlation;
    m_correlationAvg(correlation);
    aircraft->m_correlationAvg(correlation);
    aircraft->m_correlation = aircraft->m_correlationAvg.instantAverage();
    aircraft->m_correlationItem->setText(QString("%1/%2/%3")
        .arg(CalcDb::dbPower(aircraft->m_minCorrelation), 3, 'f', 1)
        .arg(CalcDb::dbPower(aircraft->m_correlation), 3, 'f', 1)
        .arg(CalcDb::dbPower(aircraft->m_maxCorrelation), 3, 'f', 1));
    m_correlationOnesAvg(correlationOnes);
    aircraft->m_rssiItem->setText(QString("%1")
        .arg(CalcDb::dbPower(m_correlationOnesAvg.instantAverage()), 3, 'f', 1));

    // Check to see if we need to emit a notification about this aircraft
    checkDynamicNotification(aircraft);

    // Update text below photo if it's likely to have changed
    if ((aircraft == m_highlightAircraft) && (newAircraft || updatedCallsign)) {
        updatePhotoText(aircraft);
    }

    PROFILER_STOP("ADS-B decode");
}

void ADSBDemodGUI::Interogator::update(int ic, Aircraft *aircraft, AirspaceModel *airspaceModel, CheckList *checkList, bool display)
{
    if (aircraft->m_positionValid)
    {
        if (m_valid)
        {
            bool changed = false;

            if (aircraft->m_latitude < m_minLatitude)
            {
                m_minLatitude = aircraft->m_latitude;
                changed = true;
            }
            if (aircraft->m_latitude > m_maxLatitude)
            {
                m_maxLatitude = aircraft->m_latitude;
                changed = true;
            }
            if (aircraft->m_longitude < m_minLongitude)
            {
                m_minLongitude = aircraft->m_longitude;
                changed = true;
            }
            if (aircraft->m_longitude > m_maxLongitude)
            {
                m_maxLongitude = aircraft->m_longitude;
                changed = true;
            }
            if (changed)
            {
                calcPoly();
                if (display) {
                    airspaceModel->airspaceUpdated(&m_airspace);
                }
            }
        }
        else
        {
            const double minSize = 0.02;
            QBrush brush(QColor(colors[ic*3], colors[ic*3+1], colors[ic*3+2], 0xa0));

            m_valid = true;
            m_minLatitude = aircraft->m_latitude - minSize;
            m_maxLatitude = aircraft->m_latitude + minSize;
            m_minLongitude = aircraft->m_longitude - minSize;
            m_maxLongitude = aircraft->m_longitude + minSize;
            m_airspace.m_name = QString("IC %1").arg(ic);
            m_airspace.m_bottom.m_alt = -1;
            m_airspace.m_top.m_alt = -1;
            calcPoly();
            if (display)
            {
                airspaceModel->addAirspace(&m_airspace);
                checkList->addCheckItem(QString::number(ic), ic, Qt::Checked)->setBackground(brush);
            }
            else
            {
                checkList->addCheckItem(QString::number(ic), ic, Qt::Unchecked)->setBackground(brush);
            }
            checkList->setSortRole(Qt::UserRole + 1); // Sort via data rather than label
            checkList->model()->sort(0, Qt::AscendingOrder);
        }
    }
}

void ADSBDemodGUI::Interogator::calcPoly()
{
    double w = (m_maxLongitude - m_minLongitude) / 2.0;
    double h = (m_maxLatitude - m_minLatitude) / 2.0;
    double x = m_minLongitude + w;
    double y = m_minLatitude + h;
    m_airspace.m_center.setX(x);
    m_airspace.m_center.setY(y);
    m_airspace.m_position = m_airspace.m_center;

    const int s = 15;
    m_airspace.m_polygon.resize(360/s+1);

    for (int d = 0, i = 0; d <= 360; d += s, i++)
    {
        double a = Units::degreesToRadians((double) d);
        double r1 = cos(a) * w;
        double r2 = sin(a) * h;

        m_airspace.m_polygon[i] = QPointF(x + r1, y + r2);
    }
}

// Clear heading if much older than latest track figure or too different
void ADSBDemodGUI::clearOldHeading(Aircraft *aircraft, const QDateTime& dateTime, float newTrack)
{
    if (aircraft->m_headingValid
        && (aircraft->m_heading != newTrack)
        && (   (std::abs(newTrack - aircraft->m_trackWhenHeadingSet) >= 5)
            || (aircraft->m_headingDateTime.secsTo(dateTime) >= 10)
           )
       )
    {
        aircraft->m_headingValid = false;
        aircraft->m_headingItem->setData(Qt::DisplayRole, "");
    }
}

void ADSBDemodGUI::decodeID(const QByteArray& data, QString& emitterCategory, QString& callsign)
{
    // Aircraft identification - BDS 0,8
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

void ADSBDemodGUI::decodeGroundspeed(const QByteArray& data, float& v, float& h)
{
    int s_ew = (data[5] >> 2) & 1; // East-west velocity sign
    int v_ew = ((data[5] & 0x3) << 8) | (data[6] & 0xff); // East-west velocity
    int s_ns = (data[7] >> 7) & 1; // North-south velocity sign
    int v_ns = ((data[7] & 0x7f) << 3) | ((data[8] >> 5) & 0x7); // North-south velocity

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
    v = std::round(std::sqrt(v_we*v_we + v_sn*v_sn));
    h = std::atan2(v_we, v_sn) * 360.0/(2.0*M_PI);
    if (h < 0.0) {
        h += 360.0;
    }
}

void ADSBDemodGUI::decodeAirspeed(const QByteArray& data, bool& tas, int& as, bool& hdgValid, float& hdg)
{
    hdgValid = (data[5] >> 2) & 1; // Heading status
    int hdgFix =  ((data[5] & 0x3) << 8) | (data[6] & 0xff); // Heading
    hdg = hdgFix / 1024.0f * 360.0f;

    tas = (data[7] >> 7) & 1; // Airspeed type (true or indicated)
    as = ((data[7] & 0x7f) << 3) | ((data[8] >> 5) & 0x7); // Airspeed
}

void ADSBDemodGUI::decodeVerticalRate(const QByteArray& data, int& verticalRate)
{
    int s_vr = (data[8] >> 3) & 1; // Vertical rate sign
    int vr = ((data[8] & 0x7) << 6) | ((data[9] >> 2) & 0x3f); // Vertical rate
    verticalRate = (vr-1)*64*(s_vr?-1:1);
}

// Called when we have both lat & long
void ADSBDemodGUI::updateAircraftPosition(Aircraft *aircraft, double latitude, double longitude, const QDateTime& dateTime)
{
    // Calculate range, azimuth and elevation to aircraft from station
    m_azEl.setTarget(latitude, longitude, aircraft->m_altitudeValid ? Units::feetToMetres(aircraft->m_altitude) : 0);
    m_azEl.calculate();

    aircraft->m_latitude = latitude;
    aircraft->m_longitude = longitude;
    aircraft->m_latitudeItem->setData(Qt::DisplayRole, aircraft->m_latitude);
    aircraft->m_longitudeItem->setData(Qt::DisplayRole, aircraft->m_longitude);
    aircraft->m_positionDateTime = dateTime;

    if (!aircraft->m_positionValid)
    {
        aircraft->m_positionValid = true;
        // Now we have a position, add a plane to the map
        m_aircraftModel.addAircraft(aircraft);
    }

    aircraft->m_range = m_azEl.getDistance();
    aircraft->m_azimuth = m_azEl.getAzimuth();
    aircraft->m_elevation = m_azEl.getElevation();
    aircraft->m_rangeItem->setText(QString::number(aircraft->m_range/1000.0, 'f', 1));
    aircraft->m_azElItem->setText(QString("%1/%2").arg(std::round(aircraft->m_azimuth)).arg(std::round(aircraft->m_elevation)));
    if (aircraft == m_trackAircraft) {
        m_adsbDemod->setTarget(aircraft->targetName(), aircraft->m_azimuth, aircraft->m_elevation, aircraft->m_range);
    }
    if (m_azEl.getDistance() > m_maxRangeStat)
    {
        m_maxRangeStat = m_azEl.getDistance();
        ui->statsTable->item(MAX_RANGE, 0)->setData(Qt::DisplayRole, m_maxRangeStat / 1000.0);
    }
    if (aircraft->m_altitudeValid && (aircraft->m_altitude > m_maxAltitudeStat))
    {
        m_maxAltitudeStat = aircraft->m_altitude;
        ui->statsTable->item(MAX_ALTITUDE, 0)->setData(Qt::DisplayRole, m_maxAltitudeStat);
    }
    if (aircraft->m_altitudeValid) {
        updateCoverageMap(m_azEl.getAzimuth(), m_azEl.getElevation(), m_azEl.getDistance(), aircraft->m_altitude);
    }
}

bool ADSBDemodGUI::validateGlobalPosition(double latitude, double longitude, bool countFailure)
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
        ui->statsTable->item(RANGE_FAILS, 0)->setData(Qt::DisplayRole, m_rangeFails);
    }
    return false;
}

// Called when we have lat & long from local decode and we need to check if it is in a valid range (<180nm/333km airborne or 45nm/83km for surface)
bool ADSBDemodGUI::validateLocalPosition(double latitude, double longitude, bool surfacePosition, bool countFailure)
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
        ui->statsTable->item(RANGE_FAILS, 0)->setData(Qt::DisplayRole, m_rangeFails);
    }
    return false;
}

bool ADSBDemodGUI::decodeGlobalPosition(int f, const double cprLat[2], const double cprLong[2], const QDateTime cprTime[2], double& latitude, double& longitude, bool countFailure)
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
bool ADSBDemodGUI::decodeLocalPosition(int f, double cprLat, double cprLong, bool onSurface, const Aircraft *aircraft, double& latitude, double& longitude, bool countFailure)
{
    double refLatitude;
    double refLongitude;

    if (aircraft->m_globalPosition)
    {
        refLatitude = aircraft->m_latitude;
        refLongitude = aircraft->m_longitude;
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

    if (aircraft->m_globalPosition)
    {
        // Reasonableness spec is 2.5nm / 30 seconds - we're less stringent
        double latDiff = abs(refLatitude - latitude);
        double lonDiff = abs(refLongitude - longitude);
        double maxLatDiff = 20000/111000.0;
        double maxLonDiff = cos(Units::degreesToRadians(latitude)) * maxLatDiff;
        if ((latDiff > maxLatDiff) || (lonDiff > maxLonDiff)) {
            if (countFailure) {
                //qDebug() << "FAIL" << latDiff << maxLatDiff << lonDiff << maxLonDiff;
            }
        }
        return !((latDiff > maxLatDiff) || (lonDiff > maxLonDiff));
    }
    else
    {
        // Check position is within range of antenna
        return validateLocalPosition(latitude, longitude, onSurface, countFailure);
    }
}

void ADSBDemodGUI::decodeCpr(const QByteArray& data, int& f, double& latCpr, double& lonCpr) const
{
    f = (data[6] >> 2) & 1; // CPR odd/even frame - should alternate every 0.2s
    int latCprFix = ((data[6] & 3) << 15) | ((data[7] & 0xff) << 7) | ((data[8] >> 1) & 0x7f);
    int lonCprFix = ((data[8] & 1) << 16) | ((data[9] & 0xff) << 8) | (data[10] & 0xff);

    latCpr = latCprFix / 131072.0;
    lonCpr = lonCprFix / 131072.0;
}

// This is relative to pressure of 1013.25 - it isn't corrected according to aircraft baro setting - so we can appear underground
bool ADSBDemodGUI::decodeAltitude(const QByteArray& data, int& altFt) const
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
void ADSBDemodGUI::decodeModeSAltitude(const QByteArray& data, const QDateTime dateTime, Aircraft *aircraft)
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
                ui->statsTable->item(ALT_FAILS, 0)->setData(Qt::DisplayRole, m_altFails);
            }
        }
    }
    if (!altUnlikely) {
        aircraft->setAltitude(altitude, false, dateTime, m_settings);
    }
}

void ADSBDemodGUI::decodeModeS(const QByteArray data, const QDateTime dateTime, int df, Aircraft *aircraft)
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

        if (squawk != aircraft->m_squawk)
        {
            aircraft->m_squawk = squawk;
            if (identCode & 0x40) {
                aircraft->m_squawkItem->setText(QString("%1 IDENT").arg(aircraft->m_squawk, 4, 10, QLatin1Char('0')));
            } else {
                aircraft->m_squawkItem->setText(QString("%1").arg(aircraft->m_squawk, 4, 10, QLatin1Char('0')));
            }
        }
    }
}

static bool isSpeedAndHeadingInconsitent(float speed1, float heading1, float speed2, float heading2)
{
    return (abs(speed1 - speed2) > 50) || (abs(heading1 - heading2) > 45);
}

static bool isVerticalRateInconsistent(int verticalRate1, int verticalRate2)
{
    return abs(verticalRate1 - verticalRate2) > 1500;
}

static bool isAltitudeInconsistent(int altitude1, int altitude2)
{
    return abs(altitude1 - altitude2) > 1500; // 30s at climb rate of 3kft/m
}

static bool isPositionInconsistent(double latitude1, double longitude1, double latitude2, double longitude2)
{
    return (abs(latitude1 - latitude2) > 0.2f) || (abs(longitude1 - longitude2) > 0.3f); // 1 deg lat is ~70 miles. 500mph is ~8miles per minute
}

void ADSBDemodGUI::decodeCommB(const QByteArray data, const QDateTime dateTime, int df, Aircraft *aircraft, bool &updatedCallsign)
{
    // We only see downlink messages, so do not know the data format, so have to decode all possibilities
    // and then see which have legal values and values that are consistent with ADS-B data

    // All IFR aircraft should support ELS (Elementary Surveillance) which includes BDS 1,0 1,7 2,0 3,0
    // Large aircraft are also required to support EHS (Enhanced Surveillance) which adds BDS 4,0 5,0 6,0
    // There is also MRAR (Meteorological Routine Air Report) BDS 4,4 4,5, but only a small % of aircraft support this
    // See: https://www.icao.int/APAC/Documents/edocs/Mode%20S%20DAPs%20Implementation%20and%20Operations%20Guidance%20Document3.0.pdf

    // I've implemented a few extra BDSes, but these are rarely seen.
    // Figure 2 in "Mode S Transponder Comm-B Capabilities in Current Operational Aircraft" gives a breakdown of what capabilities are reported by BDS 1,7:
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
        bool tcInconsistent = !((tc == 0) || ((tc >= 5) && (tc <= 8)) || ((tc >= 9) && (tc << 18)) || ((tc >= 20) && (tc <= 22))); // Only position type codes

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
            if (decodeLocalPosition(f, cprLat[f], cprLon[f], aircraft->m_onSurface, aircraft, latitude_0_5, longitude_0_5, false)) {
                positionValid_0_5 = true;
            }
        }
        // Require a position having been received by other means, otherwise too high chance of it being incorrect
        bool positionInconsistent_0_5 = !positionValid_0_5 || (aircraft->m_positionValid && isPositionInconsistent(aircraft->m_latitude, aircraft->m_longitude, latitude_0_5, longitude_0_5));

        bool bds_0_5 = ((tc >= 9) && (tc <= 18)) && !positionInconsistent_0_5 && !altitudeInconsistent_0_5 && !tcInconsistent;

        // BDS 0,8 - Aircraft identification and category

        QString emitterCategory, callsign_0_8;

        decodeID(data, emitterCategory, callsign_0_8);

        bool emitterCategoryInconsistent = (emitterCategory == "Reserved") || (emitterCategory != aircraft->m_emitterCategory);
        bool callsignInconsistent_0_8 = callsign_0_8.isEmpty() || callsign_0_8.contains('#'); // Callsign can change between flights

        bool bds_0_8 = ((tc >= 1) && (tc <= 4)) && !emitterCategoryInconsistent && !callsignInconsistent_0_8;

        // BDS 0,9 - Airborne Velocity

        int st = data[4] & 0x7;   // Subtype
        bool groundspeedSubType = (st == 1) || (st == 2);
        float groundspeed_0_9, track;
        bool airspeedType_0_9;
        int airspeed;
        bool headingValid;
        float heading;
        if (groundspeedSubType)
        {
            // Ground speed
            decodeGroundspeed(data, groundspeed_0_9, track);
        }
        else
        {
            // Airspeed
            decodeAirspeed(data, airspeedType_0_9, airspeed, headingValid, heading);
        }
        int verticalRate;
        decodeVerticalRate(data, verticalRate);

        bool groundspeedInconsistent = groundspeedSubType
            && aircraft->m_groundspeedValid
            && isSpeedAndHeadingInconsitent(groundspeed_0_9, track, aircraft->m_groundspeed, aircraft->m_track);
        bool airspeedInconsistent = !groundspeedSubType
            && (airspeedType_0_9 ? aircraft->m_trueAirspeedValid : aircraft->m_indicatedAirspeedValid)
            && isSpeedAndHeadingInconsitent(airspeed, heading, airspeedType_0_9 ? aircraft->m_trueAirspeed : aircraft->m_indicatedAirspeed, aircraft->m_heading);
        bool verticalRateInconsistent =  aircraft->m_verticalRateValid && isVerticalRateInconsistent(verticalRate, aircraft->m_verticalRate);

        bool bds_0_9 = (tc == 19) && !groundspeedInconsistent && !airspeedInconsistent && !verticalRateInconsistent;

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
        if (cap_5_5) {
            caps.append("6,0");
        }

        bool bds_1_7 = cap_2_0 && (data[7] == 0x0) && (data[8] == 0x0) && (data[9] == 0x0) && (data[10] == 0x0);

        // BDS 1,8 1,9 1,A 1,B 1,C 1,D 1,E 1,F Mode S specific services

        // Apart from 1,C and 1,F these can have any bits set/clear and specify BDS code capabilities
        // Don't decode for now

        // BDS 2,0 - Aircraft identification - ELS

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

        // BDS 2,1 - Aircraft and airline registration markings

        bool aircraftRegistrationStatus = (data[4] >> 7) & 0x1;
        char aircraftRegistration[8];
        c[0] = (data[4] >> 1) & 0x3f;
        c[1] = ((data[4] & 0x1) << 5) | ((data[5] >> 3) & 0x1f);
        c[2] = ((data[5] & 0x7) << 3) | ((data[6] >> 5) & 0x7);
        c[3] = ((data[6] & 0x1f) << 1) | ((data[7] >> 7) & 0x1);
        c[4] = ((data[7] >> 1) & 0x1f);
        c[5] = ((data[7] & 0x1) >> 3) | ((data[8] >> 3) & 0x1f);
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
        bool airlineRegistrationInvalid = QString(airlineRegistrationString).contains('#') || !QChar::isLetter(c[0]) || !QChar::isLetter(c[1]);
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
        c[5] = ((data[7] & 0x1) >> 3) | ((data[8] >> 3) & 0x1f);
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
        int windDirectionFix = ((data[5] & 0x3) << 6) | ((data[6] >> 2) & 0x3f);
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

        int latitudeFix = ((data[4] & 0x3f) << 13) | ((data[5] & 0xff) << 5) | ((data[6] >> 3) & 0x1f);
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
        int arAltitudeRate = arAltitudeRateFix * 64; // Ft/min
        bool arAltitudeRateInconsistent = (abs(arAltitudeRate) > 6000) || (!arAltitudeRateStatus && (arAltitudeRateFix != 0));

        bool bds_5_3 = !arMagHeadingInconsistent && !arIndicatedAirspeedInconsistent && !arMachInconsistent && !arTrueAirspeedInconsistent && !arAltitudeRateInconsistent;

        // BDS 6,0 - Heading and speed report - EHS

        bool magHeadingStatus = (data[4] >> 7) & 1;
        int magHeadingFix = ((data[4] & 0x7f) << 4) | ((data[6] >> 4) & 0xf);
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
                        aircraft->addCoordinate(dateTime, &m_aircraftModel);
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
                    aircraft->setGroundspeed(groundspeed_0_9, m_settings);
                    aircraft->setTrack(track, dateTime);
                }
                else
                {
                    if (airspeedType_0_9) {
                        aircraft->setTrueAirspeed(airspeed, m_settings);
                    } else {
                        aircraft->setIndicatedAirspeed(airspeed, dateTime, m_settings);
                    }
                    aircraft->setHeading(heading, dateTime);
                }
                aircraft->setVerticalRate(verticalRate, m_settings);
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
                aircraft->m_bdsCapabilities[4][8] |= cap_4_8;
                aircraft->m_bdsCapabilities[5][0] |= cap_5_0;
                aircraft->m_bdsCapabilities[5][1] |= cap_5_1;
                aircraft->m_bdsCapabilities[5][2] |= cap_5_2;
                aircraft->m_bdsCapabilities[5][3] |= cap_5_4;
                aircraft->m_bdsCapabilities[5][4] |= cap_5_5;
                aircraft->m_bdsCapabilities[5][5] |= cap_5_6;
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
                    aircraft->m_registrationItem->setText(aircraftRegistrationString);
                }
            }
            if (bds_4_0)
            {
                // Could use targetAltSource here, but yet to see it set to anything other than unknown
                if (mcpSelectedAltStatus)
                {
                    aircraft->m_selAltitude = mcpSelectedAlt;
                    aircraft->m_selAltitudeValid = true;
                    if (m_settings.m_siUnits) {
                        aircraft->m_selAltitudeItem->setData(Qt::DisplayRole, Units::feetToIntegerMetres(aircraft->m_selAltitude));
                    } else {
                        aircraft->m_selAltitudeItem->setData(Qt::DisplayRole, roundTo50Feet(aircraft->m_selAltitude));
                    }
                }
                else if (fmsSelectedAltStatus)
                {
                    aircraft->m_selAltitude = fmsSelectedAlt;
                    aircraft->m_selAltitudeValid = true;
                    if (m_settings.m_siUnits) {
                        aircraft->m_selAltitudeItem->setData(Qt::DisplayRole, Units::feetToIntegerMetres(aircraft->m_selAltitude));
                    } else {
                        aircraft->m_selAltitudeItem->setData(Qt::DisplayRole, roundTo50Feet(aircraft->m_selAltitude));
                    }
                }
                else
                {
                    aircraft->m_selAltitude = 0;
                    aircraft->m_selAltitudeValid = false;
                    aircraft->m_selAltitudeItem->setText("");
                }

                if (baroSettingStatus)
                {
                    aircraft->m_baro = baroSetting;
                    aircraft->m_baroValid = true;
                    aircraft->m_baroItem->setData(Qt::DisplayRole, std::round(aircraft->m_baro));
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
                    aircraft->m_vModeItem->setText(mode);

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
                            aircraft->m_groundspeedItem->setData(Qt::DisplayRole, Units::knotsToIntegerKPH(aircraft->m_groundspeed));
                        } else {
                            aircraft->m_windSpeedItem->setData(Qt::DisplayRole, windSpeed);
                        }
                        aircraft->m_windDirItem->setData(Qt::DisplayRole, windDirection);
                        // Not clear if air temp depends on the previous status bit
                        aircraft->m_staticAirTempItem->setData(Qt::DisplayRole, (int)std::round(staticAirTemperature));
                    }
                    if (averageStaticPressureStatus) {
                        aircraft->m_staticPressureItem->setData(Qt::DisplayRole, averageStaticPressure);
                    }
                    if (humidityStatus) {
                        aircraft->m_humidityItem->setData(Qt::DisplayRole, (int)std::round(humidity));
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
                        << "(Aircraft Alt: " << aircraft->m_altitude << "ft)"
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
                    aircraft->m_rollItem->setData(Qt::DisplayRole, std::round(aircraft->m_roll));
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
                    aircraft->m_turnRateItem->setData(Qt::DisplayRole, std::round(aircraft->m_turnRate*10.0f)/10.0f);
                }
                if (trueAirspeedStatus)
                {
                    aircraft->m_trueAirspeed = trueAirspeed;
                    aircraft->m_trueAirspeedValid = true;
                    if (m_settings.m_siUnits) {
                        aircraft->m_trueAirspeedItem->setData(Qt::DisplayRole, Units::knotsToIntegerKPH(aircraft->m_trueAirspeed));
                    } else {
                        aircraft->m_trueAirspeedItem->setData(Qt::DisplayRole, aircraft->m_trueAirspeed);
                    }
                }
                if (headwindStatus)
                {
                    if (m_settings.m_siUnits) {
                        aircraft->m_headwindItem->setData(Qt::DisplayRole, Units::knotsToIntegerKPH(headwind));
                    } else {
                        aircraft->m_headwindItem->setData(Qt::DisplayRole, headwind);
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
                        aircraft->m_indicatedAirspeedItem->setData(Qt::DisplayRole, Units::knotsToIntegerKPH(aircraft->m_indicatedAirspeed));
                    } else {
                        aircraft->m_indicatedAirspeedItem->setData(Qt::DisplayRole, aircraft->m_indicatedAirspeed);
                    }
                    aircraft->m_indicatedAirspeedDateTime = dateTime;
                }
                if (machStatus)
                {
                    aircraft->m_mach = mach;
                    aircraft->m_machValid = true;
                    aircraft->m_machItem->setData(Qt::DisplayRole, aircraft->m_mach);
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

QList<SWGSDRangel::SWGMapAnimation *> * ADSBDemodGUI::animate(QDateTime dateTime, Aircraft *aircraft)
{
    QList<SWGSDRangel::SWGMapAnimation *> *animations = new QList<SWGSDRangel::SWGMapAnimation *>();

    const int gearDownSpeed = 150;
    const int gearUpAltitude = 200;
    const int gearUpVerticalRate = 1000;
    const int accelerationHeight = 1500;
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
        animations->append(gearAnimation(dateTime, false));
        animations->append(gearAngle(dateTime, true));
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
            animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.0));
            animations->append(slatsAnimation(dateTime, true));
            aircraft->m_flaps = 0.0;
        }
        else if ((aircraft->m_groundspeed >= 30) && (aircraft->m_flaps < 0.25))
        {
            // Flaps for takeoff
            if (debug) {
                qDebug() << "Takeoff flaps " << aircraft->m_icaoHex;
            }
            animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.25));
            animations->append(slatsAnimation(dateTime, false));
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
        animations->append(gearAngle(dateTime, false));
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
        animations->append(gearAnimation(dateTime.addSecs(2), true));
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
        animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.0));
        animations->append(slatsAnimation(dateTime, true));
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
            animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.25));
            //animations->append(slatsAnimation(dateTime, false));
            aircraft->m_flaps = 0.25;
            aircraft->m_pitchEst = 1.0;
        }
    }

    // Gear down for landing
    // We don't know airport elevation, so base on speed and descent rate
    if (!aircraft->m_gearDown
        && !aircraft->m_onSurface
        && !aircraft->m_runwayAltitudeValid
        && (aircraft->m_verticalRateValid && (aircraft->m_verticalRate < 100))
        && (   (aircraft->m_groundspeedValid && (aircraft->m_groundspeed < gearDownSpeed))
            || (aircraft->m_altitudeValid && (aircraft->m_altitude < 2000))
           )
       )
    {
        if (debug) {
            qDebug() << "Flaps/Gear down for landing " << aircraft->m_icaoHex;
        }
        animations->append(flapsAnimation(dateTime, aircraft->m_flaps, 0.5));
        animations->append(slatsAnimation(dateTime, false));
        animations->append(gearAnimation(dateTime.addSecs(8), false));
        animations->append(flapsAnimation(dateTime.addSecs(16), 0.5, 1.0));
        aircraft->m_gearDown = true;
        aircraft->m_flaps = 1.0;
        aircraft->m_pitchEst = 3.0;
    }

    // Engine control
    if (aircraft->m_emitterCategory == "Rotorcraft")
    {
        // Helicopter rotors
        if (!aircraft->m_rotorStarted && !aircraft->m_onSurface)
        {
            // Start rotors
            if (debug) {
                qDebug() << "Start rotors " << aircraft->m_icaoHex;
            }
            animations->append(rotorAnimation(dateTime, false));
            aircraft->m_rotorStarted = true;
        }
        else if (aircraft->m_rotorStarted && aircraft->m_onSurface)
        {
            if (debug) {
                qDebug() << "Stop rotors " << aircraft->m_icaoHex;
            }
            animations->append(rotorAnimation(dateTime, true));
            aircraft->m_rotorStarted = false;
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
            animations->append(engineAnimation(dateTime, 1, false));
            animations->append(engineAnimation(dateTime, 2, false));
            aircraft->m_engineStarted = true;
        }
        else if (aircraft->m_engineStarted && aircraft->m_groundspeedValid && (aircraft->m_groundspeed == 0))
        {
            if (debug) {
                qDebug() << "Stop engines " << aircraft->m_icaoHex;
            }
            animations->append(engineAnimation(dateTime, 1, true));
            animations->append(engineAnimation(dateTime, 2, true));
            aircraft->m_engineStarted = false;
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
    else if ((aircraft->m_flaps < 0.25) && aircraft->m_verticalRateValid)
    {
        // In climb/descent
        aircraft->m_pitchEst = std::abs(aircraft->m_verticalRate / 400.0);
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

SWGSDRangel::SWGMapAnimation *ADSBDemodGUI::gearAnimation(QDateTime startDateTime, bool up)
{
    // Gear up (0.0) / down (1.0)
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString("libxplanemp/controls/gear_ratio"));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(up);
    animation->setLoop(0);
    animation->setDuration(5);
    animation->setMultiplier(0.2f);
    return animation;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodGUI::gearAngle(QDateTime startDateTime, bool flat)
{
    // Gear deflection - on ground it should be flat (0.0) in the air at an angle (1.0)
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString("libxplanemp/gear/tire_vertical_deflection_mtr"));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(flat);
    animation->setLoop(0);
    animation->setDuration(1);
    animation->setMultiplier(1);
    return animation;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodGUI::flapsAnimation(QDateTime startDateTime, float currentFlaps, float flaps)
{
    // Extend / retract flags
    bool retract = flaps < currentFlaps;
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString("libxplanemp/controls/flap_ratio"));
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
    return animation;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodGUI::slatsAnimation(QDateTime startDateTime, bool retract)
{
    // Extend / retract slats
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString("libxplanemp/controls/slat_ratio"));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(retract);
    animation->setLoop(0);
    animation->setDuration(5);
    animation->setMultiplier(0.2f);
    return animation;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodGUI::rotorAnimation(QDateTime startDateTime, bool stop)
{
    // Turn rotors in helicopter.glb model
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString("Take 001"));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(false);
    animation->setLoop(true);
    animation->setMultiplier(1);
    animation->setStop(stop);
    return animation;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodGUI::engineAnimation(QDateTime startDateTime, int engine, bool stop)
{
    // Turn propellors
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString(QString("libxplanemp/engines/engine_rotation_angle_deg%1").arg(engine)));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(false);
    animation->setLoop(true);
    animation->setMultiplier(1);
    animation->setStop(stop);
    return animation;
}

void ADSBDemodGUI::checkStaticNotification(Aircraft *aircraft)
{
    for (int i = 0; i < m_settings.m_notificationSettings.size(); i++)
    {
        QString match;
        switch (m_settings.m_notificationSettings[i]->m_matchColumn)
        {
        case ADSB_COL_ICAO:
            match = aircraft->m_icaoItem->data(Qt::DisplayRole).toString();
            break;
        case ADSB_COL_MODEL:
            match = aircraft->m_modelItem->data(Qt::DisplayRole).toString();
            break;
        case ADSB_COL_TYPE:
            match = aircraft->m_typeItem->data(Qt::DisplayRole).toString();
            break;
        case ADSB_COL_REGISTRATION:
            match = aircraft->m_registrationItem->data(Qt::DisplayRole).toString();
            break;
        case ADSB_COL_MANUFACTURER:
            match = aircraft->m_manufacturerNameItem->data(Qt::DisplayRole).toString();
            break;
        case ADSB_COL_OWNER:
            match = aircraft->m_ownerItem->data(Qt::DisplayRole).toString();
            break;
        case ADSB_COL_OPERATOR_ICAO:
            match = aircraft->m_operatorICAOItem->data(Qt::DisplayRole).toString();
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
                    highlightAircraft(aircraft);

                    if (!m_settings.m_notificationSettings[i]->m_speech.isEmpty()) {
                        speechNotification(aircraft, m_settings.m_notificationSettings[i]->m_speech);
                    }
                    if (!m_settings.m_notificationSettings[i]->m_command.isEmpty()) {
                        commandNotification(aircraft, m_settings.m_notificationSettings[i]->m_command);
                    }
                    if (m_settings.m_notificationSettings[i]->m_autoTarget) {
                        targetAircraft(aircraft);
                    }

                    aircraft->m_notified = true;
                }
            }
        }
    }
}

void ADSBDemodGUI::checkDynamicNotification(Aircraft *aircraft)
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
               )
            {
                QString match;
                switch (m_settings.m_notificationSettings[i]->m_matchColumn)
                {
                case ADSB_COL_CALLSIGN:
                    match = aircraft->m_callsignItem->data(Qt::DisplayRole).toString();
                    break;
                case ADSB_COL_ALTITUDE:
                    match = aircraft->m_altitudeItem->data(Qt::DisplayRole).toString();
                    break;
                case ADSB_COL_GROUND_SPEED:
                    match = aircraft->m_groundspeedItem->data(Qt::DisplayRole).toString();
                    break;
                case ADSB_COL_RANGE:
                    match = aircraft->m_rangeItem->data(Qt::DisplayRole).toString();
                    break;
                case ADSB_COL_CATEGORY:
                    match = aircraft->m_emitterCategoryItem->data(Qt::DisplayRole).toString();
                    break;
                case ADSB_COL_STATUS:
                    match = aircraft->m_statusItem->data(Qt::DisplayRole).toString();
                    break;
                case ADSB_COL_SQUAWK:
                    match = aircraft->m_squawkItem->data(Qt::DisplayRole).toString();
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
                            highlightAircraft(aircraft);

                            if (!m_settings.m_notificationSettings[i]->m_speech.isEmpty()) {
                                speechNotification(aircraft, m_settings.m_notificationSettings[i]->m_speech);
                            }
                            if (!m_settings.m_notificationSettings[i]->m_command.isEmpty()) {
                                commandNotification(aircraft, m_settings.m_notificationSettings[i]->m_command);
                            }
                            if (m_settings.m_notificationSettings[i]->m_autoTarget) {
                                targetAircraft(aircraft);
                            }

                            aircraft->m_notified = true;
                        }
                    }
                }
            }
        }
    }
}

// Initialise text to speech engine
// This takes 10 seconds on some versions of Linux, so only do it, if user actually
// has speech notifications configured
void ADSBDemodGUI::enableSpeechIfNeeded()
{
#ifdef QT_TEXTTOSPEECH_FOUND
    if (m_speech) {
        return;
    }
    for (const auto& notification : m_settings.m_notificationSettings)
    {
        if (!notification->m_speech.isEmpty())
        {
            qDebug() << "ADSBDemodGUI: Enabling text to speech";
            m_speech = new QTextToSpeech(this);
            return;
        }
    }
#endif
}

void ADSBDemodGUI::speechNotification(Aircraft *aircraft, const QString &speech)
{
#ifdef QT_TEXTTOSPEECH_FOUND
    if (m_speech) {
        m_speech->say(subAircraftString(aircraft, speech));
    } else {
        qWarning() << "ADSBDemodGUI::speechNotification: Unable to say " << speech;
    }
#else
    qWarning() << "ADSBDemodGUI::speechNotification: TextToSpeech not supported. Unable to say " << speech;
#endif
}

void ADSBDemodGUI::commandNotification(Aircraft *aircraft, const QString &command)
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
    qWarning() << "ADSBDemodGUI::commandNotification: QProcess not supported. Can't run: " << command;
#endif
}

QString ADSBDemodGUI::subAircraftString(Aircraft *aircraft, const QString &string)
{
    QString s = string;
    s = s.replace("${icao}", aircraft->m_icaoItem->data(Qt::DisplayRole).toString());
    s = s.replace("${callsign}", aircraft->m_callsignItem->data(Qt::DisplayRole).toString());
    s = s.replace("${aircraft}", aircraft->m_modelItem->data(Qt::DisplayRole).toString());
    s = s.replace("${type}", aircraft->m_typeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${speed}", aircraft->m_groundspeedItem->data(Qt::DisplayRole).toString()); // For backwards compatibility
    s = s.replace("${gs}", aircraft->m_groundspeedItem->data(Qt::DisplayRole).toString());
    s = s.replace("${tas}", aircraft->m_trueAirspeedItem->data(Qt::DisplayRole).toString());
    s = s.replace("${ias}", aircraft->m_indicatedAirspeedItem->data(Qt::DisplayRole).toString());
    s = s.replace("${mach}", aircraft->m_machItem->data(Qt::DisplayRole).toString());
    s = s.replace("${selAltitude}", aircraft->m_selAltitudeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${altitude}", aircraft->m_altitudeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${verticalRate}", aircraft->m_verticalRateItem->data(Qt::DisplayRole).toString());
    s = s.replace("${selHeading}", aircraft->m_selHeadingItem->data(Qt::DisplayRole).toString());
    s = s.replace("${heading}", aircraft->m_headingItem->data(Qt::DisplayRole).toString());
    s = s.replace("${track}", aircraft->m_trackItem->data(Qt::DisplayRole).toString());
    s = s.replace("${turnRate}", aircraft->m_turnRateItem->data(Qt::DisplayRole).toString());
    s = s.replace("${roll}", aircraft->m_rollItem->data(Qt::DisplayRole).toString());
    s = s.replace("${range}", aircraft->m_rangeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${azel}", aircraft->m_azElItem->data(Qt::DisplayRole).toString());
    s = s.replace("${category}", aircraft->m_emitterCategoryItem->data(Qt::DisplayRole).toString());
    s = s.replace("${status}", aircraft->m_statusItem->data(Qt::DisplayRole).toString());
    s = s.replace("${squawk}", aircraft->m_squawkItem->data(Qt::DisplayRole).toString());
    s = s.replace("${ident}", aircraft->m_identItem->data(Qt::DisplayRole).toString());
    s = s.replace("${registration}", aircraft->m_registrationItem->data(Qt::DisplayRole).toString());
    s = s.replace("${manufacturer}", aircraft->m_manufacturerNameItem->data(Qt::DisplayRole).toString());
    s = s.replace("${owner}", aircraft->m_ownerItem->data(Qt::DisplayRole).toString());
    s = s.replace("${operator}", aircraft->m_operatorICAOItem->data(Qt::DisplayRole).toString());
    s = s.replace("${ap}", aircraft->m_apItem->data(Qt::DisplayRole).toString());
    s = s.replace("${vMode}", aircraft->m_vModeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${lMode}", aircraft->m_lModeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${tcas}", aircraft->m_tcasItem->data(Qt::DisplayRole).toString());
    s = s.replace("${acas}", aircraft->m_acasItem->data(Qt::DisplayRole).toString());
    s = s.replace("${ra}", aircraft->m_raItem->data(Qt::DisplayRole).toString());
    s = s.replace("${maxSpeed}", aircraft->m_maxSpeedItem->data(Qt::DisplayRole).toString());
    s = s.replace("${version}", aircraft->m_versionItem->data(Qt::DisplayRole).toString());
    s = s.replace("${length}", aircraft->m_lengthItem->data(Qt::DisplayRole).toString());
    s = s.replace("${width}", aircraft->m_widthItem->data(Qt::DisplayRole).toString());
    s = s.replace("${baro}", aircraft->m_baroItem->data(Qt::DisplayRole).toString());
    s = s.replace("${headwind}", aircraft->m_headwindItem->data(Qt::DisplayRole).toString());
    s = s.replace("${windSpeed}", aircraft->m_windSpeedItem->data(Qt::DisplayRole).toString());
    s = s.replace("${windDirection}", aircraft->m_windDirItem->data(Qt::DisplayRole).toString());
    s = s.replace("${staticPressure}", aircraft->m_staticPressureItem->data(Qt::DisplayRole).toString());
    s = s.replace("${staticAirTemperature}", aircraft->m_staticAirTempItem->data(Qt::DisplayRole).toString());
    s = s.replace("${humidity}", aircraft->m_humidityItem->data(Qt::DisplayRole).toString());
    s = s.replace("${latitude}", aircraft->m_latitudeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${longitude}", aircraft->m_longitudeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${ic}", aircraft->m_interogatorCodeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${rssi}", aircraft->m_rssiItem->data(Qt::DisplayRole).toString());
    s = s.replace("${flightstatus}", aircraft->m_flightStatusItem->data(Qt::DisplayRole).toString());
    s = s.replace("${departure}", aircraft->m_depItem->data(Qt::DisplayRole).toString());
    s = s.replace("${arrival}", aircraft->m_arrItem->data(Qt::DisplayRole).toString());
    s = s.replace("${stops}", aircraft->m_stopsItem->data(Qt::DisplayRole).toString());
    s = s.replace("${std}", aircraft->m_stdItem->data(Qt::DisplayRole).toString());
    s = s.replace("${etd}", aircraft->m_etdItem->data(Qt::DisplayRole).toString());
    s = s.replace("${atd}", aircraft->m_atdItem->data(Qt::DisplayRole).toString());
    s = s.replace("${sta}", aircraft->m_staItem->data(Qt::DisplayRole).toString());
    s = s.replace("${eta}", aircraft->m_etaItem->data(Qt::DisplayRole).toString());
    s = s.replace("${ata}", aircraft->m_ataItem->data(Qt::DisplayRole).toString());
    return s;
}

bool ADSBDemodGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        int sr = notif.getSampleRate();
        bool srTooLow = sr < 2000000;
        ui->warning->setVisible(srTooLow);
        if (srTooLow) {
            ui->warning->setText(QString("Sample rate must be >= 2000000. Currently %1").arg(sr));
        } else {
            ui->warning->setText("");
        }
        getRollupContents()->arrangeRollups();
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = sr;
        ui->deltaFrequency->setValueRange(false, 7, -sr/2, sr/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(sr/2));
        updateAbsoluteCenterFrequency();
        return true;
    }
    else if (ADSBDemodReport::MsgReportADSB::match(message))
    {
        ADSBDemodReport::MsgReportADSB& report = (ADSBDemodReport::MsgReportADSB&) message;
        handleADSB(
            report.getData(),
            report.getDateTime(),
            report.getPreambleCorrelation(),
            report.getCorrelationOnes(),
            report.getCRC(),
            true);
        return true;
    }
    else if (ADSBDemodReport::MsgReportDemodStats::match(message))
    {
        ADSBDemodReport::MsgReportDemodStats& report = (ADSBDemodReport::MsgReportDemodStats&) message;
        ADSBDemodStats stats = report.getDemodStats();

        ui->statsTable->item(ADSB_FRAMES, 0)->setData(Qt::DisplayRole, stats.m_adsbFrames);
        ui->statsTable->item(MODE_S_FRAMES, 0)->setData(Qt::DisplayRole, stats.m_modesFrames);
        int totalFrames = stats.m_adsbFrames + stats.m_modesFrames;
        ui->statsTable->item(TOTAL_FRAMES, 0)->setData(Qt::DisplayRole, totalFrames);
        ui->statsTable->item(CORRELATOR_MATCHES, 0)->setData(Qt::DisplayRole, stats.m_correlatorMatches);
        float percentValid = 100.0f * totalFrames / (float) (stats.m_correlatorMatches - stats.m_preambleFails);
        ui->statsTable->item(PERCENT_VALID, 0)->setData(Qt::DisplayRole, QString::number(percentValid, 'f', 2));
        ui->statsTable->item(PREAMBLE_FAILS, 0)->setData(Qt::DisplayRole, stats.m_preambleFails);
        ui->statsTable->item(CRC_FAILS, 0)->setData(Qt::DisplayRole, stats.m_crcFails);
        ui->statsTable->item(TYPE_FAILS, 0)->setData(Qt::DisplayRole, stats.m_typeFails);
        ui->statsTable->item(INVALID_FAILS, 0)->setData(Qt::DisplayRole, stats.m_invalidFails);
        ui->statsTable->item(ICAO_FAILS, 0)->setData(Qt::DisplayRole, stats.m_icaoFails);
        ui->statsTable->item(AVERAGE_CORRELATION, 0)->setData(Qt::DisplayRole, QString::number(CalcDb::dbPower(m_correlationAvg.instantAverage()), 'f', 1));
        return true;
    }
    else if (ADSBDemod::MsgConfigureADSBDemod::match(message))
    {
        qDebug("ADSBDemodGUI::handleMessage: ADSBDemod::MsgConfigureADSBDemod");
        const ADSBDemod::MsgConfigureADSBDemod& cfg = (ADSBDemod::MsgConfigureADSBDemod&) message;
        const ADSBDemodSettings settings = cfg.getSettings();
        const QStringList settingsKeys = cfg.getSettingsKeys();
        bool force = cfg.getForce();

        if (force) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(settingsKeys, settings);
        }

        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings(settingsKeys, force);
        blockApplySettings(false);
        return true;
    }

    return false;
}

void ADSBDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void ADSBDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySetting("inputFrequencyOffset");
}

void ADSBDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void ADSBDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySetting("inputFrequencyOffset");
}

void ADSBDemodGUI::on_rfBW_valueChanged(int value)
{
    Real bw = (Real)value;
    ui->rfBWText->setText(QString("%1M").arg(bw / 1000000.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySetting("rfBandwidth");
}

void ADSBDemodGUI::on_threshold_valueChanged(int value)
{
    Real thresholddB = ((Real)value)/10.0f;
    ui->thresholdText->setText(QString("%1").arg(thresholddB, 0, 'f', 1));
    m_settings.m_correlationThreshold = thresholddB;
    applySetting("correlationThreshold");
}

void ADSBDemodGUI::on_chipsThreshold_valueChanged(int value)
{
    ui->chipsThresholdText->setText(QString("%1").arg(value));
    m_settings.m_chipsThreshold = value;
    applySetting("chipsThreshold");
}

void ADSBDemodGUI::on_phaseSteps_valueChanged(int value)
{
    ui->phaseStepsText->setText(QString("%1").arg(value));
    m_settings.m_interpolatorPhaseSteps = value;
    applySetting("interpolatorPhaseSteps");
}

void ADSBDemodGUI::on_tapsPerPhase_valueChanged(int value)
{
    Real tapsPerPhase = ((Real)value)/10.0f;
    ui->tapsPerPhaseText->setText(QString("%1").arg(tapsPerPhase, 0, 'f', 1));
    m_settings.m_interpolatorTapsPerPhase = tapsPerPhase;
    applySetting("interpolatorTapsPerPhase");
}

void ADSBDemodGUI::on_feed_clicked(bool checked)
{
    m_settings.m_feedEnabled = checked;
    // Don't disable host/port - so they can be entered before connecting
    applySetting("feedEnabled");
    applyImportSettings();
}

void ADSBDemodGUI::on_notifications_clicked()
{
    ADSBDemodNotificationDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted)
    {
        enableSpeechIfNeeded();
        applySetting("notificationSettings");
    }
}

void ADSBDemodGUI::on_flightInfo_clicked()
{
    if (m_flightInformation)
    {
        // Selection mode is single, so only a single row should be returned
        QModelIndexList indexList = ui->adsbData->selectionModel()->selectedRows();
        if (!indexList.isEmpty())
        {
            int row = indexList.at(0).row();
            int icao = ui->adsbData->item(row, 0)->text().toInt(nullptr, 16);
            if (m_aircraft.contains(icao))
            {
                Aircraft *aircraft = m_aircraft.value(icao);
                if (!aircraft->m_flight.isEmpty())
                {
                    // Download flight information
                    m_flightInformation->getFlightInformation(aircraft->m_flight);
                }
                else
                {
                    qDebug() << "ADSBDemodGUI::on_flightInfo_clicked - No flight number for selected aircraft";
                }
            }
            else
            {
                qDebug() << "ADSBDemodGUI::on_flightInfo_clicked - No aircraft with icao " << icao;
            }
        }
        else
        {
            qDebug() << "ADSBDemodGUI::on_flightInfo_clicked - No aircraft selected";
        }
    }
    else
    {
        qDebug() << "ADSBDemodGUI::on_flightInfo_clicked - No flight information service - have you set an API key?";
    }
}

// Find highlighted aircraft on Map Feature
void ADSBDemodGUI::on_findOnMapFeature_clicked()
{
    QModelIndexList indexList = ui->adsbData->selectionModel()->selectedRows();
    if (!indexList.isEmpty())
    {
        int row = indexList.at(0).row();
        QString icao = ui->adsbData->item(row, 0)->text();
        FeatureWebAPIUtils::mapFind(icao);
    }
}

void ADSBDemodGUI::on_deleteAircraft_clicked()
{
    QHash<int, Aircraft *>::iterator i = m_aircraft.begin();

    while (i != m_aircraft.end()) {
        removeAircraft(i, i.value());
    }
}

// Find aircraft on channel map
void ADSBDemodGUI::findOnChannelMap(Aircraft *aircraft)
{
#ifdef QT_LOCATION_FOUND
    if (aircraft->m_positionValid)
    {
        QQuickItem *item = ui->map->rootObject();
        QObject *object = item->findChild<QObject*>("map");
        if(object != NULL)
        {
            QGeoCoordinate geocoord = object->property("center").value<QGeoCoordinate>();
            geocoord.setLatitude(aircraft->m_latitude);
            geocoord.setLongitude(aircraft->m_longitude);
            object->setProperty("center", QVariant::fromValue(geocoord));
        }
    }
#endif
}

void ADSBDemodGUI::statsTable_customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item = ui->statsTable->itemAt(pos);
    if (item)
    {
        QMenu* tableContextMenu = new QMenu(ui->statsTable);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
            });
        tableContextMenu->addAction(copyAction);
    }
}

void ADSBDemodGUI::adsbData_customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item = ui->adsbData->itemAt(pos);
    if (item)
    {
        int row = item->row();
        int icao = ui->adsbData->item(row, 0)->text().toInt(nullptr, 16);
        Aircraft *aircraft = nullptr;
        if (m_aircraft.contains(icao)) {
            aircraft = m_aircraft.value(icao);
        } else {
            return;
        }
        QString icaoHex = QString("%1").arg(icao, 6, 16, QChar('0'));

        QMenu* tableContextMenu = new QMenu(ui->adsbData);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        // Copy current cell

        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
        });
        tableContextMenu->addAction(copyAction);
        tableContextMenu->addSeparator();

        // View aircraft on various websites

        QAction* planeSpottersAction = new QAction("View aircraft on planespotters.net...", tableContextMenu);
        connect(planeSpottersAction, &QAction::triggered, this, [icao]()->void {
            QString icaoUpper = QString("%1").arg(icao, 1, 16).toUpper();
            QDesktopServices::openUrl(QUrl(QString("https://www.planespotters.net/hex/%1").arg(icaoUpper)));
        });
        tableContextMenu->addAction(planeSpottersAction);

        QAction* adsbExchangeAction = new QAction("View aircraft on adsbexchange.com...", tableContextMenu);
        connect(adsbExchangeAction, &QAction::triggered, this, [icaoHex]()->void {
            QDesktopServices::openUrl(QUrl(QString("https://globe.adsbexchange.com/?icao=%1").arg(icaoHex)));
        });
        tableContextMenu->addAction(adsbExchangeAction);

        QAction* viewOpenSkyAction = new QAction("View aircraft on opensky-network.org...", tableContextMenu);
        connect(viewOpenSkyAction, &QAction::triggered, this, [icaoHex]()->void {
            QDesktopServices::openUrl(QUrl(QString("https://old.opensky-network.org/aircraft-profile?icao24=%1").arg(icaoHex)));
        });
        tableContextMenu->addAction(viewOpenSkyAction);

        if (!aircraft->m_callsign.isEmpty())
        {
            QAction* flightRadarAction = new QAction("View flight on flightradar24.com...", tableContextMenu);
            connect(flightRadarAction, &QAction::triggered, this, [aircraft]()->void {
                QDesktopServices::openUrl(QUrl(QString("https://www.flightradar24.com/%1").arg(aircraft->m_callsign)));
            });
            tableContextMenu->addAction(flightRadarAction);
        }

        tableContextMenu->addSeparator();

        // Edit aircraft

        /*if (!aircraft->m_aircraftInfo)
        {
            QAction* addOpenSkyAction = new QAction("Add aircraft to opensky-network.org...", tableContextMenu);
            connect(addOpenSkyAction, &QAction::triggered, this, []()->void {
                QDesktopServices::openUrl(QUrl(QString("https://old.opensky-network.org/edit-aircraft-profile")));
            });
            tableContextMenu->addAction(addOpenSkyAction);
        }
        else
        {

            QAction* editOpenSkyAction = new QAction("Edit aircraft on opensky-network.org...", tableContextMenu);
            connect(editOpenSkyAction, &QAction::triggered, this, [icaoHex]()->void {
                QDesktopServices::openUrl(QUrl(QString("https://old.opensky-network.org/edit-aircraft-profile?icao24=%1").arg(icaoHex)));
            });
            tableContextMenu->addAction(editOpenSkyAction);
        }*/

        QAction* editSDMAction = new QAction("Edit aircraft on sdm.virtualradarserver.co.uk...", tableContextMenu);
        connect(editSDMAction, &QAction::triggered, this, []()->void {
            QDesktopServices::openUrl(QUrl(QString("https://sdm.virtualradarserver.co.uk/Edit/Aircraft")));
            });
        tableContextMenu->addAction(editSDMAction);

        // Find on Map
        if (aircraft->m_positionValid)
        {
            tableContextMenu->addSeparator();

            QAction* findChannelMapAction = new QAction("Find on ADS-B map", tableContextMenu);
            connect(findChannelMapAction, &QAction::triggered, this, [this, aircraft]()->void {
                findOnChannelMap(aircraft);
            });
            tableContextMenu->addAction(findChannelMapAction);

            QAction* findMapFeatureAction = new QAction("Find on feature map", tableContextMenu);
            connect(findMapFeatureAction, &QAction::triggered, this, [icaoHex]()->void {
                FeatureWebAPIUtils::mapFind(icaoHex);
            });
            tableContextMenu->addAction(findMapFeatureAction);
        }

        tableContextMenu->popup(ui->adsbData->viewport()->mapToGlobal(pos));
    }
}

void ADSBDemodGUI::on_adsbData_cellClicked(int row, int column)
{
    (void) column;
    // Get ICAO of aircraft in row clicked
    int icao = ui->adsbData->item(row, 0)->text().toInt(nullptr, 16);
    if (m_aircraft.contains(icao)) {
         highlightAircraft(m_aircraft.value(icao));
    }
}

void ADSBDemodGUI::on_adsbData_cellDoubleClicked(int row, int column)
{
    // Get ICAO of aircraft in row double clicked
    int icao = ui->adsbData->item(row, 0)->text().toInt(nullptr, 16);
    if (column == ADSB_COL_ICAO)
    {
        // Search for aircraft on planespotters.net
        QString icaoUpper = QString("%1").arg(icao, 1, 16).toUpper();
        QDesktopServices::openUrl(QUrl(QString("https://www.planespotters.net/hex/%1").arg(icaoUpper)));
    }
    else if (m_aircraft.contains(icao))
    {
        Aircraft *aircraft = m_aircraft.value(icao);

        if (column == ADSB_COL_CALLSIGN)
        {
            if (!aircraft->m_callsign.isEmpty())
            {
                // Search for flight on flightradar24
                QDesktopServices::openUrl(QUrl(QString("https://www.flightradar24.com/%1").arg(aircraft->m_callsign)));
            }
        }
        else
        {
            if (column == ADSB_COL_AZEL)
            {
                targetAircraft(aircraft);
            }
            // Center map view on aircraft if it has a valid position
            if (aircraft->m_positionValid)
            {
                findOnChannelMap(aircraft);
            }
        }
    }
}

// Columns in table reordered
void ADSBDemodGUI::adsbData_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;
    m_settings.m_columnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void ADSBDemodGUI::adsbData_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;
    m_settings.m_columnSizes[logicalIndex] = newSize;
}

// Right click in ADSB table header - show column select menu
void ADSBDemodGUI::columnSelectMenu(QPoint pos)
{
    menu->popup(ui->adsbData->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void ADSBDemodGUI::columnSelectMenuChecked(bool checked)
{
    (void) checked;
    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->adsbData->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction *ADSBDemodGUI::createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(columnSelectMenuChecked()));
    return action;
}

void ADSBDemodGUI::on_spb_currentIndexChanged(int value)
{
    m_settings.m_samplesPerBit = (value + 1) * 2;
    applySetting("samplesPerBi");
}

void ADSBDemodGUI::on_demodModeS_clicked(bool checked)
{
    m_settings.m_demodModeS = checked;
    applySetting("demodModeS");
}

void ADSBDemodGUI::on_getAircraftDB_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_osnDB.downloadAircraftInformation();
    }
}

void ADSBDemodGUI::on_getAirportDB_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_ourAirportsDB.downloadAirportInformation();
    }
}

void ADSBDemodGUI::on_getAirspacesDB_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setMaximum(OpenAIP::m_countryCodes.size());
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_openAIP.downloadAirspaces();
    }
}

void ADSBDemodGUI::on_coverage_clicked(bool checked)
{
    m_settings.m_displayCoverage = checked;
    applySetting("displayCoverage");
    updateAirspaces();
}

void ADSBDemodGUI::on_displayChart_clicked(bool checked)
{
    m_settings.m_displayChart = checked;
    applySetting("displayChart");
    ui->chart->setVisible(m_settings.m_displayChart);
}

void ADSBDemodGUI::on_stats_clicked(bool checked)
{
    m_settings.m_displayDemodStats = checked;
    ui->statsTable->setVisible(m_settings.m_displayDemodStats);
    applySetting("displayDemodStats");
}

void ADSBDemodGUI::on_ic_globalCheckStateChanged(int state)
{
    (void) state;

    for (int i = 0; i < ui->ic->count(); i++)
    {
        int ic = ui->ic->itemText(i).toInt();
        int idx = ic - 1;
        bool checked = ui->ic->isChecked(i);
        m_settings.m_displayIC[idx] = checked;
        bool visible = m_airspaceModel.contains(&m_interogators[idx].m_airspace);
        if (checked && !visible) {
            m_airspaceModel.addAirspace(&m_interogators[idx].m_airspace);
        } else if (!checked && visible) {
            m_airspaceModel.removeAirspace(&m_interogators[idx].m_airspace);
        }
    }
}

void ADSBDemodGUI::on_flightPaths_clicked(bool checked)
{
    m_settings.m_flightPaths = checked;
    m_aircraftModel.setSettings(&m_settings);
}

void ADSBDemodGUI::on_allFlightPaths_clicked(bool checked)
{
    m_settings.m_allFlightPaths = checked;
    m_aircraftModel.setSettings(&m_settings);
}

void ADSBDemodGUI::on_atcLabels_clicked(bool checked)
{
    m_settings.m_atcLabels = checked;
    m_aircraftModel.setSettings(&m_settings);
    applySetting("atcLabels");
}

void ADSBDemodGUI::on_displayOrientation_clicked(bool checked)
{
    m_settings.m_displayOrientation = checked;
    applySetting("displayOrientation");
    ui->splitter->setOrientation(m_settings.m_displayOrientation ? Qt::Horizontal : Qt::Vertical);
}

void ADSBDemodGUI::on_displayRadius_clicked(bool checked)
{
    m_settings.m_displayRadius = checked;
    setShowContainmentRadius(m_settings.m_displayRadius);
    applySetting("displayRadius");
}

QString ADSBDemodGUI::getDataDir()
{
    // Get directory to store app data in (aircraft & airport databases and user-definable icons)
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    // First dir is writable
    return locations[0];
}

void ADSBDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySetting("rollupState");
}

void ADSBDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.setDefaultTitle(m_displayedName);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_adsbDemod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitle(m_channelMarker.getTitle());
        setTitleColor(m_settings.m_rgbColor);

        QStringList settingsKeys({
           "rgbColor",
           "title",
           "useReverseAPI",
           "reverseAPIAddress",
           "reverseAPIPort",
           "reverseAPIDeviceIndex",
           "reverseAPIChannelIndex"
            });

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings(settingsKeys);
    }

    resetContextMenuType();
}

void ADSBDemodGUI::updateChannelList()
{
    std::vector<ChannelAPI*> channels = MainCore::instance()->getChannels("sdrangel.channel.amdemod");

    ui->amDemod->blockSignals(true);
    ui->amDemod->clear();

    for (const auto channel : channels) {
        ui->amDemod->addItem(QString("R%1:%2").arg(channel->getDeviceSetIndex()).arg(channel->getIndexInDeviceSet()));
    }

    // Select current setting, if exists
    // If not, make sure nothing selected, as channel may be created later on
    int idx = ui->amDemod->findText(m_settings.m_amDemod);
    if (idx >= 0) {
        ui->amDemod->setCurrentIndex(idx);
    } else {
        ui->amDemod->setCurrentIndex(-1);
    }

    ui->amDemod->blockSignals(false);

    // If no current setting, select first channel
    if (m_settings.m_amDemod.isEmpty())
    {
        ui->amDemod->setCurrentIndex(0);
        on_amDemod_currentIndexChanged(0);
    }
}

void ADSBDemodGUI::on_amDemod_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_amDemod = ui->amDemod->currentText();
        applySetting("amDemod");
    }
}

QString ADSBDemodGUI::get3DModel(const QString &aircraftType, const QString &operatorICAO) const
{
    QString aircraftTypeOperator = aircraftType + "_" + operatorICAO;
    if (m_3DModels.contains(aircraftTypeOperator)) {
        return m_3DModels.value(aircraftTypeOperator);
    }
    if (m_settings.m_verboseModelMatching) {
        qDebug() << "ADS-B: No livery for " << aircraftTypeOperator;
    }
    return "";
}

QString ADSBDemodGUI::get3DModel(const QString &aircraftType)
{
    if (m_3DModelsByType.contains(aircraftType))
    {
        // Choose a random livery
        QStringList models = m_3DModelsByType.value(aircraftType);
        int size = models.size();
        int idx = m_random.bounded(size);
        return models[idx];
    }
    if (m_settings.m_verboseModelMatching) {
        qDebug() << "ADS-B: No aircraft for " << aircraftType;
    }
    return "";
}

void ADSBDemodGUI::get3DModel(Aircraft *aircraft)
{
    QString model;
    if (aircraft->m_aircraftInfo && !aircraft->m_aircraftInfo->m_model.isEmpty())
    {
        QString aircraftType;
        for (auto mm : m_3DModelMatch)
        {
            if (mm->match(aircraft->m_aircraftInfo->m_model, aircraft->m_aircraftInfo->m_manufacturerName, aircraftType))
            {
                QString operatorICAO = aircraft->m_aircraftInfo->m_operatorICAO;

                // Look for operator specific livery
                if (!operatorICAO.isEmpty()) {
                    model = get3DModel(aircraftType, operatorICAO);
                }

                if (model.isEmpty())
                {
                    // Try similar operator (E.g. EasyJet instead of EasyJet Europe)
                    static const QHash<QString, QString> alternateOperator = {
                        {"EJU", "EZY"},
                        {"WUK", "WZZ"},
                        {"TFL", "TOM"},
                        {"NOZ", "NAX"},
                        {"NSZ", "NAX"},
                        {"BCS", "DHK"},
                    };

                    if (alternateOperator.contains(operatorICAO))
                    {
                        operatorICAO = alternateOperator.value(operatorICAO);
                        model = get3DModel(aircraftType, operatorICAO);
                    }

                    if (model.isEmpty())
                    {

                        if (m_settings.m_favourLivery && !operatorICAO.isEmpty())
                        {
                            // Try to find similar aircraft with matching livery
                            static const QHash<QString, QStringList> alternateTypes = {
                                {"B788", {"B77W", "B77L", "B772", "B773", "B763", "A332", "A333"}},
                                {"B77W", {"B77L", "B772", "B773", "B788", "B763", "A332", "A333"}},
                                {"B77L", {"B77W", "B772", "B773", "B788", "B763", "A332", "A333"}},
                                {"B772", {"B77W", "B77L", "B773", "B788", "B763", "A332", "A333"}},
                                {"B773", {"B77W", "B77L", "B772", "B788", "B763", "A332", "A333"}},
                                {"A332", {"A333", "B77W", "B77L", "B773", "B772", "B788", "B763"}},
                                {"A333", {"A332", "B77W", "B77L", "B773", "B772", "B788", "B763"}},
                                {"A342", {"A343", "A345", "A346"}},
                                {"A343", {"A342", "A345", "A346"}},
                                {"A345", {"A343", "A342", "A346"}},
                                {"A346", {"A345", "A343", "A342"}},
                                {"B744", {"B74F"}},
                                {"B74F", {"B744"}},
                                {"B733", {"B734", "B737", "B738", "B739", "B752", "A320", "A319", "A321"}},
                                {"B734", {"B733", "B737", "B738", "B739", "B752", "A320", "A319", "A321"}},
                                {"B737", {"B733", "B734", "B738", "B739", "B752", "A320", "A319", "A321"}},
                                {"B738", {"B733", "B734", "B737", "B739", "B752", "A320", "A319", "A321"}},
                                {"B739", {"B733", "B734", "B737", "B738", "B752", "A320", "A319", "A321"}},
                                {"A319", {"A320", "A321", "B733", "B734", "B737", "B738", "B739"}},
                                {"A320", {"A319", "A321", "B733", "B734", "B737", "B738", "B739"}},
                                {"A321", {"A319", "A320", "B733", "B734", "B737", "B738", "B739"}},
                                {"A306", {"A332", "A333", "B763"}},
                            };

                            if (alternateTypes.contains(aircraftType))
                            {
                                for (const auto& alternate : alternateTypes.value(aircraftType)) {
                                    model = get3DModel(alternate, operatorICAO);
                                    if (!model.isEmpty()) {
                                        break;
                                    }
                                }
                            }
                        }

                        if (model.isEmpty())
                        {
                            // Try for aircraft without specific livery
                            model = get3DModel(aircraftType);
                        }
                    }
                }
                if (!model.isEmpty())
                {
                    aircraft->m_aircraft3DModel = model;
                    if (m_modelAltitudeOffset.contains(aircraftType))
                    {
                        aircraft->m_modelAltitudeOffset = m_modelAltitudeOffset.value(aircraftType);
                        aircraft->m_labelAltitudeOffset = m_labelAltitudeOffset.value(aircraftType);
                    }
                }
                break;
            }
        }
        if (m_settings.m_verboseModelMatching)
        {
            if (model.isEmpty()) {
                qDebug() << "ADS-B: No 3D model for " << aircraft->m_aircraftInfo->m_model << " " << aircraft->m_aircraftInfo->m_operatorICAO << " for " << aircraft->m_icaoHex;
            } else {
                qDebug() << "ADS-B: Matched " << aircraft->m_aircraftInfo->m_model << " " << aircraft->m_aircraftInfo->m_operatorICAO << " to " << model << " for " << aircraft->m_icaoHex;
            }
        }
    }
}

void ADSBDemodGUI::get3DModelBasedOnCategory(Aircraft *aircraft)
{
    QString aircraftType;

    if (!aircraft->m_emitterCategory.compare("Heavy"))
    {
        static const QStringList heavy = {"B744", "B77W", "B788", "A388"};
        aircraftType = heavy[m_random.bounded(heavy.size())];
    }
    else if (!aircraft->m_emitterCategory.compare("Large"))
    {
        static const QStringList large = {"A319", "A320", "A321", "B737", "B738", "B739"};
        aircraftType = large[m_random.bounded(large.size())];
    }
    else if (!aircraft->m_emitterCategory.compare("Small"))
    {
        aircraftType = "LJ45";
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
    else if (!aircraft->m_emitterCategory.compare("Light"))
    {
        aircraftType = "C172";
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
        aircraftType = "A320";
    }

    if (!aircraftType.isEmpty())
    {
        aircraft->m_aircraftCat3DModel = "";
        if (aircraft->m_aircraftInfo) {
            aircraft->m_aircraftCat3DModel = get3DModel(aircraftType, aircraft->m_aircraftInfo->m_operatorICAO);
        }
        if (aircraft->m_aircraftCat3DModel.isEmpty()) {
            aircraft->m_aircraftCat3DModel = get3DModel(aircraftType, aircraft->m_callsign.left(3));
        }
        if (aircraft->m_aircraftCat3DModel.isEmpty()) {
            aircraft->m_aircraftCat3DModel = get3DModel(aircraftType);
        }
        if (m_modelAltitudeOffset.contains(aircraftType))
        {
            aircraft->m_modelAltitudeOffset = m_modelAltitudeOffset.value(aircraftType);
            aircraft->m_labelAltitudeOffset = m_labelAltitudeOffset.value(aircraftType);
        }
    }
}

void ADSBDemodGUI::update3DModels()
{
    // Look for all aircraft gltfs in 3d directory
    QString modelDir = getDataDir() + "/3d";
    static const QStringList subDirs = {"BB_Airbus_png", "BB_Boeing_png", "BB_Jets_png", "BB_Props_png", "BB_GA_png", "BB_Mil_png", "BB_Heli_png"};

    for (auto subDir : subDirs)
    {
        QString dirName = modelDir + "/" + subDir;
        QDir dir(dirName);
        QStringList aircrafts = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        for (auto aircraft : aircrafts)
        {
            if (m_settings.m_verboseModelMatching) {
                qDebug() << "Aircraft " << aircraft;
            }
            QDir aircraftDir(dir.filePath(aircraft));
            QStringList gltfs = aircraftDir.entryList({"*.gltf"});
            QStringList allAircraft;
            for (auto gltf : gltfs)
            {
                QStringList filenameParts = gltf.split(".")[0].split("_");
                if (filenameParts.size() == 2)
                {
                    QString livery = filenameParts[1];
                    if (m_settings.m_verboseModelMatching) {
                        qDebug() << "Aircraft " << aircraft << "Livery " << livery;
                    }
                    // Only use relative path, as Map feature will add the prefix
                    QString filename = subDir + "/" + aircraft + "/" + gltf;
                    m_3DModels.insert(aircraft + "_" + livery, filename);
                    allAircraft.append(filename);
                }
            }
            if (gltfs.size() > 0) {
                m_3DModelsByType.insert(aircraft, allAircraft);
            }
        }
    }

    // Vertical offset so undercarriage isn't underground, because 0,0,0 is in the middle of the model
    // rather than at the bottom
    m_modelAltitudeOffset.insert("A306", 4.6f);
    m_modelAltitudeOffset.insert("A310", 4.6f);
    m_modelAltitudeOffset.insert("A318", 3.7f);
    m_modelAltitudeOffset.insert("A319", 3.5f);
    m_modelAltitudeOffset.insert("A320", 3.5f);
    m_modelAltitudeOffset.insert("A321", 3.5f);
    m_modelAltitudeOffset.insert("A332", 5.52f);
    m_modelAltitudeOffset.insert("A333", 5.52f);
    m_modelAltitudeOffset.insert("A334", 5.52f);
    m_modelAltitudeOffset.insert("A343", 4.65f);
    m_modelAltitudeOffset.insert("A345", 4.65f);
    m_modelAltitudeOffset.insert("A346", 4.65f);
    m_modelAltitudeOffset.insert("A388", 5.75f);
    m_modelAltitudeOffset.insert("B717", 0.0f);
    m_modelAltitudeOffset.insert("B733", 3.1f);
    m_modelAltitudeOffset.insert("B734", 3.27f);
    m_modelAltitudeOffset.insert("B737", 3.0f);
    m_modelAltitudeOffset.insert("B738", 3.31f);
    m_modelAltitudeOffset.insert("B739", 3.32f);
    m_modelAltitudeOffset.insert("B74F", 5.3f);
    m_modelAltitudeOffset.insert("B744", 5.25f);
    m_modelAltitudeOffset.insert("B752", 3.6f);
    m_modelAltitudeOffset.insert("B763", 4.44f);
    m_modelAltitudeOffset.insert("B772", 5.57f);
    m_modelAltitudeOffset.insert("B773", 5.6f);
    m_modelAltitudeOffset.insert("B77L", 5.57f);
    m_modelAltitudeOffset.insert("B77W", 5.57f);
    m_modelAltitudeOffset.insert("B788", 4.1f);
    m_modelAltitudeOffset.insert("BE20", 1.48f);
    m_modelAltitudeOffset.insert("C150", 1.05f);
    m_modelAltitudeOffset.insert("C172", 1.16f);
    m_modelAltitudeOffset.insert("C421", 1.16f);
    m_modelAltitudeOffset.insert("H25B", 1.45f);
    m_modelAltitudeOffset.insert("LJ45", 1.27f);
    m_modelAltitudeOffset.insert("B462", 1.8f);
    m_modelAltitudeOffset.insert("B463", 1.9f);
    m_modelAltitudeOffset.insert("CRJ2", 1.3f);
    m_modelAltitudeOffset.insert("CRJ7", 1.66f);
    m_modelAltitudeOffset.insert("CRJ9", 2.27f);
    m_modelAltitudeOffset.insert("CRJX", 2.49f);
    m_modelAltitudeOffset.insert("DC10", 5.2f);
    m_modelAltitudeOffset.insert("E135", 1.88f);
    m_modelAltitudeOffset.insert("E145", 1.86f);
    m_modelAltitudeOffset.insert("E170", 2.3f);
    m_modelAltitudeOffset.insert("E190", 3.05f);
    m_modelAltitudeOffset.insert("E195", 2.97f);
    m_modelAltitudeOffset.insert("F28", 2.34f);
    m_modelAltitudeOffset.insert("F70", 2.43f);
    m_modelAltitudeOffset.insert("F100", 2.23f);
    m_modelAltitudeOffset.insert("J328", 1.01f);
    m_modelAltitudeOffset.insert("MD11", 5.22f);
    m_modelAltitudeOffset.insert("MD83", 2.71f);
    m_modelAltitudeOffset.insert("MD90", 2.62f);
    m_modelAltitudeOffset.insert("AT42", 1.75f);
    m_modelAltitudeOffset.insert("AT72", 1.83f);
    m_modelAltitudeOffset.insert("D328", 0.99f);
    m_modelAltitudeOffset.insert("DH8D", 1.65f);
    m_modelAltitudeOffset.insert("F50", 2.16f);
    m_modelAltitudeOffset.insert("JS41", 1.9f);
    m_modelAltitudeOffset.insert("L410", 1.1f);
    m_modelAltitudeOffset.insert("SB20", 2.0f);
    m_modelAltitudeOffset.insert("SF34", 1.89f);

    // Label offsets (from bottom of aircraft)
    m_labelAltitudeOffset.insert("A306", 10.0f);
    m_labelAltitudeOffset.insert("A310", 15.0f);
    m_labelAltitudeOffset.insert("A318", 10.0f);
    m_labelAltitudeOffset.insert("A319", 10.0f);
    m_labelAltitudeOffset.insert("A320", 10.0f);
    m_labelAltitudeOffset.insert("A321", 10.0f);
    m_labelAltitudeOffset.insert("A332", 14.0f);
    m_labelAltitudeOffset.insert("A333", 14.0f);
    m_labelAltitudeOffset.insert("A334", 14.0f);
    m_labelAltitudeOffset.insert("A343", 14.0f);
    m_labelAltitudeOffset.insert("A345", 14.0f);
    m_labelAltitudeOffset.insert("A346", 14.0f);
    m_labelAltitudeOffset.insert("A388", 20.0f);
    m_labelAltitudeOffset.insert("B717", 7.5f);
    m_labelAltitudeOffset.insert("B733", 10.0f);
    m_labelAltitudeOffset.insert("B734", 10.0f);
    m_labelAltitudeOffset.insert("B737", 10.0f);
    m_labelAltitudeOffset.insert("B738", 10.0f);
    m_labelAltitudeOffset.insert("B739", 10.0f);
    m_labelAltitudeOffset.insert("B74F", 15.0f);
    m_labelAltitudeOffset.insert("B744", 15.0f);
    m_labelAltitudeOffset.insert("B752", 12.0f);
    m_labelAltitudeOffset.insert("B763", 14.0f);
    m_labelAltitudeOffset.insert("B772", 14.0f);
    m_labelAltitudeOffset.insert("B773", 14.0f);
    m_labelAltitudeOffset.insert("B77L", 14.0f);
    m_labelAltitudeOffset.insert("B77W", 14.0f);
    m_labelAltitudeOffset.insert("B788", 14.0f);
    m_labelAltitudeOffset.insert("BE20", 4.0f);
    m_labelAltitudeOffset.insert("C150", 3.0f);
    m_labelAltitudeOffset.insert("C172", 3.0f);
    m_labelAltitudeOffset.insert("C421", 4.0f);
    m_labelAltitudeOffset.insert("H25B", 5.0f);
    m_labelAltitudeOffset.insert("LJ45", 5.0f);
    m_labelAltitudeOffset.insert("B462", 7.0f);
    m_labelAltitudeOffset.insert("B463", 7.0f);
    m_labelAltitudeOffset.insert("CRJ2", 5.5f);
    m_labelAltitudeOffset.insert("CRJ7", 6.0f);
    m_labelAltitudeOffset.insert("CRJ9", 6.0f);
    m_labelAltitudeOffset.insert("CRJX", 6.0f);
    m_labelAltitudeOffset.insert("DC10", 15.0f);
    m_labelAltitudeOffset.insert("E135", 5.0f);
    m_labelAltitudeOffset.insert("E145", 5.0f);
    m_labelAltitudeOffset.insert("E170", 8.0f);
    m_labelAltitudeOffset.insert("E190", 8.5f);
    m_labelAltitudeOffset.insert("E195", 8.5f);
    m_labelAltitudeOffset.insert("F28", 7.0f);
    m_labelAltitudeOffset.insert("F70", 6.5f);
    m_labelAltitudeOffset.insert("F100", 6.5f);
    m_labelAltitudeOffset.insert("J328", 5.0f);  // Check
    m_labelAltitudeOffset.insert("MD11", 15.0f);
    m_labelAltitudeOffset.insert("MD83", 7.5f);
    m_labelAltitudeOffset.insert("MD90", 7.5f);
    m_labelAltitudeOffset.insert("AT42", 7.0f);
    m_labelAltitudeOffset.insert("AT72", 7.0f);
    m_labelAltitudeOffset.insert("D328", 6.0f);
    m_labelAltitudeOffset.insert("DH8D", 6.5f);
    m_labelAltitudeOffset.insert("F50",  7.0f);
    m_labelAltitudeOffset.insert("JS41", 5.0f);
    m_labelAltitudeOffset.insert("L410", 5.0f);
    m_labelAltitudeOffset.insert("SB20", 6.5f);
    m_labelAltitudeOffset.insert("SF34", 6.0f);

    // Map from OpenSky database names to 3D model names
    m_3DModelMatch.append(new ModelMatch("A300.*", "A306")); // A300 B4 is A300-600, but use for others as closest match
    m_3DModelMatch.append(new ModelMatch("A310.*", "A310"));
    m_3DModelMatch.append(new ModelMatch("A318.*", "A318"));
    m_3DModelMatch.append(new ModelMatch("A.?319.*", "A319"));
    m_3DModelMatch.append(new ModelMatch("A.?320.*", "A320"));
    m_3DModelMatch.append(new ModelMatch("A.?321.*", "A321"));
    m_3DModelMatch.append(new ModelMatch("A330.2.*", "A332"));
    m_3DModelMatch.append(new ModelMatch("A330.3.*", "A333"));
    m_3DModelMatch.append(new ModelMatch("A330.7.*", "A333")); // BelugaXL
    m_3DModelMatch.append(new ModelMatch("A330.8.*", "A332")); // 200 Neo
    m_3DModelMatch.append(new ModelMatch("A330.9.*", "A333")); // 300 Neo
    m_3DModelMatch.append(new ModelMatch("A340.3.*", "A343"));
    m_3DModelMatch.append(new ModelMatch("A340.5.*", "A345"));
    m_3DModelMatch.append(new ModelMatch("A340.6.*", "A346"));
    m_3DModelMatch.append(new ModelMatch("A350.*", "A333"));   // No A350 model - use 330 as twin engine
    m_3DModelMatch.append(new ModelMatch("A380.*", "A388"));

    m_3DModelMatch.append(new ModelMatch("737.2.*", "B733"));  // No 200 model
    m_3DModelMatch.append(new ModelMatch("737.3.*", "B733"));
    m_3DModelMatch.append(new ModelMatch("737.4.*", "B734"));
    m_3DModelMatch.append(new ModelMatch("737.5.*", "B734"));  // No 500 model
    m_3DModelMatch.append(new ModelMatch("737.6.*", "B737"));  // No 600 model
    m_3DModelMatch.append(new ModelMatch("737NG.6.*", "B737"));
    m_3DModelMatch.append(new ModelMatch("737.7.*", "B737"));
    m_3DModelMatch.append(new ModelMatch("737NG.7.*", "B737"));
    m_3DModelMatch.append(new ModelMatch("737.8.*", "B738"));
    m_3DModelMatch.append(new ModelMatch("737NG.8.*", "B738"));  // No Max model yet
    m_3DModelMatch.append(new ModelMatch("737MAX.8.*", "B738"));
    m_3DModelMatch.append(new ModelMatch("737.9", "B739"));
    m_3DModelMatch.append(new ModelMatch("737NG.9", "B739"));
    m_3DModelMatch.append(new ModelMatch("737MAX.9", "B739"));
    m_3DModelMatch.append(new ModelMatch("B747.*F", "B74F"));
    m_3DModelMatch.append(new ModelMatch("B747.*\\(F\\)", "B74F"));
    m_3DModelMatch.append(new ModelMatch("747.*", "B744"));
    m_3DModelMatch.append(new ModelMatch("757.*", "B752"));
    m_3DModelMatch.append(new ModelMatch("767.*", "B763"));
    m_3DModelMatch.append(new ModelMatch("777.2.*LR.*", "B77L"));
    m_3DModelMatch.append(new ModelMatch("777.2.*", "B772"));
    m_3DModelMatch.append(new ModelMatch("777.3.*ER.*", "B77W"));
    m_3DModelMatch.append(new ModelMatch("777.3.*", "B773"));
    m_3DModelMatch.append(new ModelMatch("777.*", "B772"));
    m_3DModelMatch.append(new ModelMatch("787.*", "B788"));
    m_3DModelMatch.append(new ModelMatch("717.*", "B717"));
    // No 727 model

    // Jets
    m_3DModelMatch.append(new ModelMatch(".*EMB.135.*", "E135"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.135.*", "E135"));
    m_3DModelMatch.append(new ModelMatch("Embraer 135.*", "E135"));
    m_3DModelMatch.append(new ModelMatch(".*EMB.145.*", "E145"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.145.*", "E145"));
    m_3DModelMatch.append(new ModelMatch("Embraer 145.*", "E145"));
    m_3DModelMatch.append(new ModelMatch(".*EMB.170.*", "E170"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.170.*", "E170"));
    m_3DModelMatch.append(new ModelMatch("Embraer 170.*", "E170"));
    m_3DModelMatch.append(new ModelMatch(".*EMB.190.*", "E190"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.190.*", "E190"));
    m_3DModelMatch.append(new ModelMatch("Embraer 190.*", "E190"));
    m_3DModelMatch.append(new ModelMatch(".*EMB.195.*", "E195"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.195.*", "E195"));
    m_3DModelMatch.append(new ModelMatch("Embraer 195.*", "E195"));

    m_3DModelMatch.append(new ModelMatch(".*CRJ.200.*", "CRJ2"));
    m_3DModelMatch.append(new ModelMatch(".*CRJ.700.*", "CRJ7"));
    m_3DModelMatch.append(new ModelMatch(".*CRJ.900.*", "CRJ9"));
    m_3DModelMatch.append(new ModelMatch(".*CRJ.1000.*", "CRJX"));

    // PNGs missing
    //m_3DModelMatch.append(new ModelMatch("(BAE )?146.2.*", "B462"));
    //m_3DModelMatch.append(new ModelMatch("(BAE )?146.3.*", "B463"));

    m_3DModelMatch.append(new ModelMatch("DC-10.*", "DC10"));

    m_3DModelMatch.append(new ModelMatch(".*MD.11.*", "MD11"));
    m_3DModelMatch.append(new ModelMatch(".*MD.83.*", "MD83"));
    m_3DModelMatch.append(new ModelMatch(".*MD.90.*", "MD90"));

    m_3DModelMatch.append(new ModelMatch(".*F28.*", "F28"));
    m_3DModelMatch.append(new ModelMatch(".*F70.*", "F70"));
    m_3DModelMatch.append(new ModelMatch(".*F100.*", "F100"));

    // GA
    m_3DModelMatch.append(new ModelMatch(".*B200.*", "BE20"));
    m_3DModelMatch.append(new ManufacturerModelMatch(".*200.*", ".*Beech.*", "BE20"));
    m_3DModelMatch.append(new ModelMatch(".*150.*", "C150"));
    m_3DModelMatch.append(new ModelMatch(".*172.*", "C172"));
    m_3DModelMatch.append(new ModelMatch(".*421.*", "C421"));
    m_3DModelMatch.append(new ModelMatch(".*125.*", "H25B"));
    m_3DModelMatch.append(new ManufacturerModelMatch(".*400.*", "Hawker.*", "H25B"));
    m_3DModelMatch.append(new ManufacturerModelMatch(".*400.*", "Raytheon.*", "H25B"));
    m_3DModelMatch.append(new ModelMatch(".*Learjet.*", "LJ45"));

    // Props
    m_3DModelMatch.append(new ModelMatch("ATR.*42.*", "AT42"));
    m_3DModelMatch.append(new ModelMatch("ATR.*72.*", "AT72"));
    m_3DModelMatch.append(new ModelMatch("Do 328.*", "D328"));
    m_3DModelMatch.append(new ModelMatch("DHC-8.*", "DH8D"));
    m_3DModelMatch.append(new ModelMatch(".*F50.*", "F50"));
    m_3DModelMatch.append(new ModelMatch("Jetstream 41.*", "JS41"));
    m_3DModelMatch.append(new ModelMatch(".*L.410.*", "L410"));
    m_3DModelMatch.append(new ModelMatch("SAAB.2000.*", "SB20"));
    m_3DModelMatch.append(new ManufacturerModelMatch(".*340.*", "Saab.*", "SF34"));
}

void ADSBDemodGUI::updateAirports()
{
    if (!m_airportInfo) {
        return;
    }

    m_airportModel.removeAllAirports();
    QHash<int, AirportInformation *>::const_iterator i = m_airportInfo->begin();
    AzEl azEl = m_azEl;

    while (i != m_airportInfo->end())
    {
        const AirportInformation *airportInfo = i.value();

        // Calculate distance and az/el to airport from My Position
        azEl.setTarget(airportInfo->m_latitude, airportInfo->m_longitude, Units::feetToMetres(airportInfo->m_elevation));
        azEl.calculate();

        // Only display airport if in range
        if (azEl.getDistance() <= m_settings.m_airportRange*1000.0f)
        {
            // Only display the airport if it's large enough
            if (airportInfo->m_type >= (AirportInformation::AirportType)m_settings.m_airportMinimumSize)
            {
                // Only display heliports if enabled
                if (m_settings.m_displayHeliports || (airportInfo->m_type != AirportInformation::AirportType::Heliport))
                {
                    m_airportModel.addAirport(airportInfo, azEl.getAzimuth(), azEl.getElevation(), azEl.getDistance());
                }
            }
        }
        ++i;
    }
    // Save settings we've just used so we know if they've changed
    m_currentAirportMinimumSize = m_settings.m_airportMinimumSize;
    m_currentDisplayHeliports = m_settings.m_displayHeliports;
}

void ADSBDemodGUI::updateAirspaces()
{
    AzEl azEl = m_azEl;
    m_airspaceModel.removeAllAirspaces();
    for (const auto airspace: *m_airspaces)
    {
        if (m_settings.m_airspaces.contains(airspace->m_category))
        {
            // Calculate distance to airspace from My Position
            azEl.setTarget(airspace->m_center.y(), airspace->m_center.x(), 0);
            azEl.calculate();

            // Only display airport if in range
            if (azEl.getDistance() <= m_settings.m_airspaceRange*1000.0f) {
                m_airspaceModel.addAirspace(airspace);
            }
        }
    }
    if (m_settings.m_displayCoverage)
    {
        m_airspaceModel.addAirspace(&m_coverageAirspace[0]);
        m_airspaceModel.addAirspace(&m_coverageAirspace[1]);
    }
    for (int i = 0; i < ADSB_IC_MAX; i++) {
        if (m_interogators[i].m_valid && m_settings.m_displayIC[i]) {
            m_airspaceModel.addAirspace(&m_interogators[i].m_airspace);
        }
    }
}

void ADSBDemodGUI::updateNavAids()
{
    AzEl azEl = m_azEl;
    m_navAidModel.removeAllNavAids();
    if (m_settings.m_displayNavAids)
    {
        for (const auto navAid: *m_navAids)
        {
            // Calculate distance to NavAid from My Position
            azEl.setTarget(navAid->m_latitude, navAid->m_longitude, Units::feetToMetres(navAid->m_elevation));
            azEl.calculate();

            // Only display NavAid if in range
            if (azEl.getDistance() <= m_settings.m_airspaceRange*1000.0f) {
                m_navAidModel.addNavAid(navAid);
            }
        }
    }
}

// Set a static target, such as an airport
void ADSBDemodGUI::target(const QString& name, float az, float el, float range)
{
    if (m_trackAircraft)
    {
        // Restore colour of current target
        m_trackAircraft->m_isTarget = false;
        m_aircraftModel.aircraftUpdated(m_trackAircraft);
        m_trackAircraft = nullptr;
    }
    m_adsbDemod->setTarget(name, az, el, range);
}

void ADSBDemodGUI::targetAircraft(Aircraft *aircraft)
{
    if (aircraft != m_trackAircraft)
    {
        if (m_trackAircraft)
        {
            // Restore colour of current target
            m_trackAircraft->m_isTarget = false;
            m_aircraftModel.aircraftUpdated(m_trackAircraft);
        }
        // Track this aircraft
        m_trackAircraft = aircraft;
        if (aircraft->m_positionValid)
            m_adsbDemod->setTarget(aircraft->targetName(), aircraft->m_azimuth, aircraft->m_elevation, aircraft->m_range);
        // Change colour of new target
        aircraft->m_isTarget = true;
        m_aircraftModel.aircraftUpdated(aircraft);
    }
}

void ADSBDemodGUI::updatePhotoText(Aircraft *aircraft)
{
    if (m_settings.m_displayPhotos)
    {
        QString callsign = aircraft->m_callsignItem->text().trimmed();
        QString reg = aircraft->m_registrationItem->text().trimmed();
        if (!callsign.isEmpty() && !reg.isEmpty()) {
            ui->photoHeader->setText(QString("%1 - %2").arg(callsign).arg(reg));
        } else if (!callsign.isEmpty()) {
            ui->photoHeader->setText(QString("%1").arg(callsign));
        } else if (!reg.isEmpty()) {
            ui->photoHeader->setText(QString("%1").arg(reg));
        }

        QIcon icon = aircraft->m_countryItem->icon();
        QList<QSize> sizes = icon.availableSizes();
        if (sizes.size() > 0) {
            ui->photoFlag->setPixmap(icon.pixmap(sizes[0]));
        } else {
            ui->photoFlag->setPixmap(QPixmap());
        }

        updatePhotoFlightInformation(aircraft);

        QString aircraftDetails = "<table width=200>"; // Note, Qt seems to make the table bigger than this so text is cropped, not wrapped
        QString manufacturer = aircraft->m_manufacturerNameItem->text();
        if (!manufacturer.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Manufacturer:<td>%1").arg(manufacturer));
        }
        QString model = aircraft->m_modelItem->text();
        if (!model.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Aircraft:<td>%1").arg(model));
        }
        QString owner = aircraft->m_ownerItem->text();
        if (!owner.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Owner:<td>%1").arg(owner));
        }
        QString operatorICAO = aircraft->m_operatorICAOItem->text();
        if (!operatorICAO.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Operator:<td>%1").arg(operatorICAO));
        }
        QString registered = aircraft->m_registeredItem->text();
        if (!registered.isEmpty()) {
            aircraftDetails.append(QString("<tr><th align=left>Registered:<td>%1").arg(registered));
        }
        aircraftDetails.append("</table>");
        ui->aircraftDetails->setText(aircraftDetails);
    }
}

void ADSBDemodGUI::updatePhotoFlightInformation(Aircraft *aircraft)
{
    if (m_settings.m_displayPhotos)
    {
        QString dep = aircraft->m_depItem->text();
        QString arr = aircraft->m_arrItem->text();
        QString std = aircraft->m_stdItem->text();
        QString etd = aircraft->m_etdItem->text();
        QString atd = aircraft->m_atdItem->text();
        QString sta = aircraft->m_staItem->text();
        QString eta = aircraft->m_etaItem->text();
        QString ata = aircraft->m_ataItem->text();
        QString flightDetails;
        if (!dep.isEmpty() && !arr.isEmpty())
        {
            flightDetails = QString("<center><table width=200><tr><th colspan=4>%1 - %2").arg(dep).arg(arr);
            if (!std.isEmpty() && !sta.isEmpty()) {
                flightDetails.append(QString("<tr><td>STD<td>%1<td>STA<td>%2").arg(std).arg(sta));
            }
            if ((!atd.isEmpty() || !etd.isEmpty()) && (!ata.isEmpty() || !eta.isEmpty()))
            {
                if (!atd.isEmpty()) {
                    flightDetails.append(QString("<tr><td>Actual<td>%1").arg(atd));
                } else if (!etd.isEmpty()) {
                    flightDetails.append(QString("<tr><td>Estimated<td>%1").arg(etd));
                }
                if (!ata.isEmpty()) {
                    flightDetails.append(QString("<td>Actual<td>%1").arg(ata));
                } else if (!eta.isEmpty()) {
                    flightDetails.append(QString("<td>Estimated<td>%1").arg(eta));
                }
            }
            flightDetails.append("</center>");
        }
        ui->flightDetails->setText(flightDetails);
    }
}

void ADSBDemodGUI::highlightAircraft(Aircraft *aircraft)
{
    if (aircraft != m_highlightAircraft)
    {
        // Hide photo of old aircraft
        ui->photoHeader->setVisible(false);
        ui->photoFlag->setVisible(false);
        ui->photo->setVisible(false);
        ui->flightDetails->setVisible(false);
        ui->aircraftDetails->setVisible(false);
        if (m_highlightAircraft)
        {
            // Restore colour
            m_highlightAircraft->m_isHighlighted = false;
            m_aircraftModel.highlightChanged(m_highlightAircraft);
        }
        // Highlight this aircraft
        m_highlightAircraft = aircraft;
        if (aircraft)
        {
            aircraft->m_isHighlighted = true;
            m_aircraftModel.highlightChanged(aircraft);
            if (m_settings.m_displayPhotos)
            {
                // Download photo
                updatePhotoText(aircraft);
                m_planeSpotters.getAircraftPhoto(aircraft->m_icaoHex);
            }
        }
    }
    if (aircraft)
    {
        // Highlight the row in the table - always do this, as it can become
        // unselected
        ui->adsbData->selectRow(aircraft->m_icaoItem->row());
    }
    else
    {
        ui->adsbData->clearSelection();
    }
}

// Show feed dialog
void ADSBDemodGUI::feedSelect(const QPoint& p)
{
    ADSBDemodFeedDialog dialog(&m_settings);
    dialog.move(p);
    new DialogPositioner(&dialog, false);

    if (dialog.exec() == QDialog::Accepted)
    {
        applySettings({
            "exportClientEnabled",
            "exportClientHost",
            "exportClientPort",
            "exportClientFormat",
            "exportServerEnabled",
            "exportServerPort",
            "importEnabled",
            "importHost",
            "importUsername",
            "importPassword",
            "importParameters",
            "importPeriod",
            "importMinLatitude",
            "importMaxLatitude",
            "importMinLongitude",
            "importMaxLongitude"
            });
        applyImportSettings();
    }
}

// Show display settings dialog
void ADSBDemodGUI::on_displaySettings_clicked()
{
    bool oldSiUnits = m_settings.m_siUnits;
    ADSBDemodDisplayDialog dialog(&m_settings);
    new DialogPositioner(&dialog, true);
    if (dialog.exec() == QDialog::Accepted)
    {
        bool unitsChanged = m_settings.m_siUnits != oldSiUnits;
        if (unitsChanged) {
            m_aircraftModel.allAircraftUpdated();
        }
        displaySettings(dialog.getSettingsKeys(), false);
        applySettings(dialog.getSettingsKeys());
    }
}

void ADSBDemodGUI::setShowContainmentRadius(bool show)
{
    QQuickItem *item = ui->map->rootObject();
    if (!item)
    {
        qCritical("ADSBDemodGUI::setShowContainmentRadius: Map not found. Are all required Qt plugins installed?");
        return;
    }
    else
    {
        QQmlProperty::write(item, "showContainmentRadius", show);
    }
}

void ADSBDemodGUI::applyMapSettings()
{
#ifdef QT_LOCATION_FOUND
    Real stationLatitude = MainCore::instance()->getSettings().getLatitude();
    Real stationLongitude = MainCore::instance()->getSettings().getLongitude();
    Real stationAltitude = MainCore::instance()->getSettings().getAltitude();

    QQuickItem *item = ui->map->rootObject();
    if (!item)
    {
        qCritical("ADSBDemodGUI::applyMapSettings: Map not found. Are all required Qt plugins installed?");
        return;
    }

    QObject *object = item->findChild<QObject*>("map");
    QGeoCoordinate coords;
    double zoom;
    if (object != nullptr)
    {
        // Save existing position of map
        coords = object->property("center").value<QGeoCoordinate>();
        zoom = object->property("zoomLevel").value<double>();
    }
    else
    {
        // Center on my location when map is first opened
        coords.setLatitude(stationLatitude);
        coords.setLongitude(stationLongitude);
        coords.setAltitude(stationAltitude);
        zoom = 10.0;
    }

    // Check requested map provider is available - if not, try the other
    QString mapProvider = m_settings.m_mapProvider;
    QStringList mapProviders = QGeoServiceProvider::availableServiceProviders();
    if ((mapProvider == "osm") && (!mapProviders.contains(mapProvider))) {
        mapProvider = "mapboxgl";
    }
    if ((mapProvider == "mapboxgl") && (!mapProviders.contains(mapProvider))) {
        mapProvider = "osm";
    }

    // Create the map using the specified provider
    QQmlProperty::write(item, "smoothing", MainCore::instance()->getSettings().getMapSmoothing());
    QQmlProperty::write(item, "aircraftMinZoomLevel", m_settings.m_aircraftMinZoom);
    QQmlProperty::write(item, "showContainmentRadius", m_settings.m_displayRadius);
    QQmlProperty::write(item, "mapProvider", mapProvider);
    QVariantMap parameters;
    QString mapType;

    if (mapProvider == "osm")
    {
#ifdef __EMSCRIPTEN__
        // Default is http://maps-redirect.qt.io/osm/5.8/ and Emscripten needs https
        parameters["osm.mapping.providersrepository.address"] = QString("https://sdrangel.beniston.com/sdrangel/maps/");
#else
        // Use our repo, so we can append API key and redefine transmit maps
        parameters["osm.mapping.providersrepository.address"] = QString("http://127.0.0.1:%1/").arg(m_osmPort);
#endif
        // Use ADS-B specific cache, as we use different transmit maps
        QString cachePath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/QtLocation/5.8/tiles/osm/sdrangel_adsb";
        parameters["osm.mapping.cache.directory"] = cachePath;
        // On Linux, we need to create the directory
        QDir dir(cachePath);
        if (!dir.exists()) {
            dir.mkpath(cachePath);
        }
        switch (m_settings.m_mapType)
        {
        case ADSBDemodSettings::AVIATION_LIGHT:
            mapType = "Transit Map";
            break;
        case ADSBDemodSettings::AVIATION_DARK:
            mapType = "Night Transit Map";
            break;
        case ADSBDemodSettings::STREET:
            mapType = "Street Map";
            break;
        case ADSBDemodSettings::SATELLITE:
            mapType = "Satellite Map";
            break;
        }
    }
    else if (mapProvider == "mapboxgl")
    {
        switch (m_settings.m_mapType)
        {
        case ADSBDemodSettings::AVIATION_LIGHT:
            mapType = "mapbox://styles/mapbox/light-v9";
            break;
        case ADSBDemodSettings::AVIATION_DARK:
            mapType = "mapbox://styles/mapbox/dark-v9";
            break;
        case ADSBDemodSettings::STREET:
            mapType = "mapbox://styles/mapbox/streets-v10";
            break;
        case ADSBDemodSettings::SATELLITE:
            mapType = "mapbox://styles/mapbox/satellite-v9";
            break;
        }
    }

    QVariant retVal;
    if (!QMetaObject::invokeMethod(item, "createMap", Qt::DirectConnection,
                                Q_RETURN_ARG(QVariant, retVal),
                                Q_ARG(QVariant, QVariant::fromValue(parameters)),
                                Q_ARG(QVariant, mapType),
                                Q_ARG(QVariant, QVariant::fromValue(this))))
    {
        qCritical() << "ADSBDemodGUI::applyMapSettings - Failed to invoke createMap";
    }
    QObject *newMap = retVal.value<QObject *>();

    // Restore position of map
    if (newMap != nullptr)
    {
        // Move antenna icon to My Position
        QObject *stationObject = newMap->findChild<QObject*>("station");
        if(stationObject != NULL)
        {
            QGeoCoordinate coords = stationObject->property("coordinate").value<QGeoCoordinate>();
            coords.setLatitude(stationLatitude);
            coords.setLongitude(stationLongitude);
            coords.setAltitude(stationAltitude);
            stationObject->setProperty("coordinate", QVariant::fromValue(coords));
            stationObject->setProperty("stationName", QVariant::fromValue(MainCore::instance()->getSettings().getStationName()));
        }
        else
        {
            qDebug() << "ADSBDemodGUI::applyMapSettings - Couldn't find station";
        }

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        newMap = newMap->findChild<QObject*>("map");
#endif
        if (coords.isValid())
        {
            newMap->setProperty("zoomLevel", QVariant::fromValue(zoom));
            newMap->setProperty("center", QVariant::fromValue(coords));
        }
    }
    else
    {
        qDebug() << "ADSBDemodGUI::applyMapSettings - createMap returned a nullptr";
    }
#endif // QT_LOCATION_FOUND
}

// Called from QML when empty space clicked
void ADSBDemodGUI::clearHighlighted()
{
    highlightAircraft(nullptr);
}

ADSBDemodGUI::ADSBDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::ADSBDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_basicSettingsShown(false),
    m_doApplySettings(true),
    m_tickCount(0),
    m_aircraftInfo(nullptr),
    m_airportModel(this),
    m_airspaceModel(this),
    m_trackAircraft(nullptr),
    m_highlightAircraft(nullptr),
    m_chart(nullptr),
    m_adsbFrameRateSeries(nullptr),
    m_modesFrameRateSeries(nullptr),
    m_aircraftSeries(nullptr),
    m_xAxis(nullptr),
    m_fpsYAxis(nullptr),
    m_aircraftYAxis(nullptr),
    m_speech(nullptr),
    m_progressDialog(nullptr),
    m_loadingData(false)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodadsb/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    // Enable MSAA antialiasing on 2D map
    // This can be much faster than using layer.smooth in the QML, when there are many items
    // However, only seems to work when set to 16, and doesn't seem to be supported on all graphics cards
    int multisamples = MainCore::instance()->getSettings().getMapMultisampling();
    if (multisamples > 0)
    {
        QSurfaceFormat format;
        format.setSamples(multisamples);
#ifdef QT_LOCATION_FOUND
        ui->map->setFormat(format);
#endif
    }

    m_osmPort = 0; // Pick a free port
    m_templateServer = new ADSBOSMTemplateServer(maptilerAPIKey(), m_osmPort);

#ifdef QT_LOCATION_FOUND
    ui->map->setAttribute(Qt::WA_AcceptTouchEvents, true);

    ui->map->rootContext()->setContextProperty("aircraftModel", &m_aircraftModel);
    ui->map->rootContext()->setContextProperty("airportModel", &m_airportModel);
    ui->map->rootContext()->setContextProperty("airspaceModel", &m_airspaceModel);
    ui->map->rootContext()->setContextProperty("navAidModel", &m_navAidModel);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map.qml")));
#elif defined(__EMSCRIPTEN__)
    // No Qt5Compat.GraphicalEffects
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map_6_strict.qml")));
#else
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map_6_strict.qml")));
#endif
    ui->map->installEventFilter(this);
#else
    ui->map->hide();
#endif

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_adsbDemod = reinterpret_cast<ADSBDemod*>(rxChannel);
    m_adsbDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *feedRightClickEnabler = new CRightClickEnabler(ui->feed);
    connect(feedRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(feedSelect(const QPoint &)));

    CRightClickEnabler *coverageClickEnabler = new CRightClickEnabler(ui->coverage);
    connect(coverageClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(clearCoverage(const QPoint &)));

    CRightClickEnabler *statsClickEnabler = new CRightClickEnabler(ui->stats);
    connect(statsClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(clearStats(const QPoint &)));

    CRightClickEnabler *displayChartClickEnabler = new CRightClickEnabler(ui->displayChart);
    connect(displayChartClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(clearChart(const QPoint &)));

    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    ui->warning->setVisible(false);
    ui->warning->setStyleSheet("QLabel { background-color: red; }");

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::red);
    m_channelMarker.setBandwidth(5000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("ADS-B Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    ui->ic->setText("IC");

    // Set size of airline icons
    ui->adsbData->setIconSize(QSize(85, 20));
    // Resize the table using dummy data
    resizeTable();
    // Allow user to reorder columns
    ui->adsbData->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->adsbData->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    menu = new QMenu(ui->adsbData);
    for (int i = 0; i < ui->adsbData->horizontalHeader()->count(); i++)
    {
        QString text = ui->adsbData->horizontalHeaderItem(i)->text();
        menu->addAction(createCheckableItem(text, i, true));
    }
    ui->adsbData->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->adsbData->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(columnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->adsbData->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(adsbData_sectionMoved(int, int, int)));
    connect(ui->adsbData->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(adsbData_sectionResized(int, int, int)));
    // Context menu
    ui->adsbData->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->adsbData, SIGNAL(customContextMenuRequested(QPoint)), SLOT(adsbData_customContextMenuRequested(QPoint)));
    TableTapAndHold *tableTapAndHold = new TableTapAndHold(ui->adsbData);
    connect(tableTapAndHold, &TableTapAndHold::tapAndHold, this, &ADSBDemodGUI::adsbData_customContextMenuRequested);

    ui->photoHeader->setVisible(false);
    ui->photoFlag->setVisible(false);
    ui->photo->setVisible(false);
    ui->flightDetails->setVisible(false);
    ui->aircraftDetails->setVisible(false);

    ui->statsTable->horizontalHeader()->setStretchLastSection(true);
    ui->statsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->statsTable, SIGNAL(customContextMenuRequested(QPoint)), SLOT(statsTable_customContextMenuRequested(QPoint)));
    TableTapAndHold *statsTapAndHold = new TableTapAndHold(ui->statsTable);
    connect(statsTapAndHold, &TableTapAndHold::tapAndHold, this, &ADSBDemodGUI::statsTable_customContextMenuRequested);

    plotChart();

    // Read aircraft information database, if it has previously been downloaded
    AircraftInformation::init();
    connect(&m_osnDB, &OsnDB::downloadingURL, this, &ADSBDemodGUI::downloadingURL);
    connect(&m_osnDB, &OsnDB::downloadError, this, &ADSBDemodGUI::downloadError);
    connect(&m_osnDB, &OsnDB::downloadProgress, this, &ADSBDemodGUI::downloadProgress);
    connect(&m_osnDB, &OsnDB::downloadAircraftInformationFinished, this, &ADSBDemodGUI::downloadAircraftInformationFinished);
    m_aircraftInfo = OsnDB::getAircraftInformation();
    m_routeInfo = OsnDB::getAircraftRouteInformation();

    // Read airport information database, if it has previously been downloaded
    connect(&m_ourAirportsDB, &OurAirportsDB::downloadingURL, this, &ADSBDemodGUI::downloadingURL);
    connect(&m_ourAirportsDB, &OurAirportsDB::downloadError, this, &ADSBDemodGUI::downloadError);
    connect(&m_ourAirportsDB, &OurAirportsDB::downloadProgress, this, &ADSBDemodGUI::downloadProgress);
    connect(&m_ourAirportsDB, &OurAirportsDB::downloadAirportInformationFinished, this, &ADSBDemodGUI::downloadAirportInformationFinished);
    m_airportInfo = OurAirportsDB::getAirportsById();

    // Read airspaces and NAVAIDs
    connect(&m_openAIP, &OpenAIP::downloadingURL, this, &ADSBDemodGUI::downloadingURL);
    connect(&m_openAIP, &OpenAIP::downloadError, this, &ADSBDemodGUI::downloadError);
    connect(&m_openAIP, &OpenAIP::downloadAirspaceFinished, this, &ADSBDemodGUI::downloadAirspaceFinished);
    connect(&m_openAIP, &OpenAIP::downloadNavAidsFinished, this, &ADSBDemodGUI::downloadNavAidsFinished);
    m_airspaces = OpenAIP::getAirspaces();
    m_navAids = OpenAIP::getNavAids();

    // Get station position
    Real stationLatitude = MainCore::instance()->getSettings().getLatitude();
    Real stationLongitude = MainCore::instance()->getSettings().getLongitude();
    Real stationAltitude = MainCore::instance()->getSettings().getAltitude();
    m_azEl.setLocation(stationLatitude, stationLongitude, stationAltitude);

    // These are the default values in sdrbase/settings/preferences.cpp
    if ((stationLatitude == 49.012423f) && (stationLongitude == 8.418125f)) {
        ui->warning->setText("Please set your antenna location under Preferences > My Position");
    }

    // Get updated when position changes
    connect(&MainCore::instance()->getSettings(), &MainSettings::preferenceChanged, this, &ADSBDemodGUI::preferenceChanged);
    if (m_deviceUISet->m_deviceAPI->getSampleSource()) {
        connect(m_deviceUISet->m_deviceAPI->getSampleSource(), &DeviceSampleSource::positionChanged, this, &ADSBDemodGUI::devicePositionChanged);
    }

    // Get airport weather when requested
    connect(&m_airportModel, &AirportModel::requestMetar, this, &ADSBDemodGUI::requestMetar);

    resetStats();

    initCoverageMap();

    // Add airports within range of My Position
    updateAirports();
    updateAirspaces();
    updateNavAids();
    update3DModels();

    m_flightInformation = nullptr;
    m_aviationWeather = nullptr;

    connect(&m_planeSpotters, &PlaneSpotters::aircraftPhoto, this, &ADSBDemodGUI::aircraftPhoto);
    connect(ui->photo, &ClickableLabel::clicked, this, &ADSBDemodGUI::photoClicked);

    // Update demod list when channels are added or removed
    connect(MainCore::instance(), &MainCore::channelAdded, this, &ADSBDemodGUI::updateChannelList);
    connect(MainCore::instance(), &MainCore::channelRemoved, this, &ADSBDemodGUI::updateChannelList);

    updateChannelList();
    displaySettings(QStringList(), true);
    makeUIConnections();
    applyAllSettings();

    connect(&m_importTimer, &QTimer::timeout, this, &ADSBDemodGUI::import);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &ADSBDemodGUI::handleImportReply
    );
    applyImportSettings();

    connect(&m_redrawMapTimer, &QTimer::timeout, this, &ADSBDemodGUI::redrawMap);
    m_redrawMapTimer.setSingleShot(true);
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

ADSBDemodGUI::~ADSBDemodGUI()
{
    m_adsbDemod->setMessageQueueToGUI(nullptr);
    disconnect(&MainCore::instance()->getSettings(), &MainSettings::preferenceChanged, this, &ADSBDemodGUI::preferenceChanged);
    if (m_templateServer)
    {
        m_templateServer->close();
        delete m_templateServer;
    }
    disconnect(&m_openAIP, &OpenAIP::downloadingURL, this, &ADSBDemodGUI::downloadingURL);
    disconnect(&m_openAIP, &OpenAIP::downloadError, this, &ADSBDemodGUI::downloadError);
    disconnect(&m_openAIP, &OpenAIP::downloadAirspaceFinished, this, &ADSBDemodGUI::downloadAirspaceFinished);
    disconnect(&m_openAIP, &OpenAIP::downloadNavAidsFinished, this, &ADSBDemodGUI::downloadNavAidsFinished);
    disconnect(&m_planeSpotters, &PlaneSpotters::aircraftPhoto, this, &ADSBDemodGUI::aircraftPhoto);
    disconnect(&m_redrawMapTimer, &QTimer::timeout, this, &ADSBDemodGUI::redrawMap);
    disconnect(&MainCore::instance()->getMasterTimer(), &QTimer::timeout, this, &ADSBDemodGUI::tick);
    m_redrawMapTimer.stop();
    // Remove aircraft from Map feature
    QHash<int, Aircraft *>::iterator i = m_aircraft.begin();
    while (i != m_aircraft.end())
    {
        Aircraft *aircraft = i.value();
        clearFromMap(QString("%1").arg(aircraft->m_icao, 0, 16));
        ++i;
    }
    delete ui;
    qDeleteAll(m_aircraft);
    if (m_flightInformation)
    {
        disconnect(m_flightInformation, &FlightInformation::flightUpdated, this, &ADSBDemodGUI::flightInformationUpdated);
        delete m_flightInformation;
    }
    delete m_aviationWeather;
    qDeleteAll(m_3DModelMatch);
    delete m_networkManager;
}

// Free keys, so no point in stealing them :)

QString ADSBDemodGUI::maptilerAPIKey() const
{
    return m_settings.m_maptilerAPIKey.isEmpty() ? "Nzl2cSyOnewxUc9VWg4n" : m_settings.m_maptilerAPIKey;
}

void ADSBDemodGUI::applySetting(const QString& settingsKey)
{
    applySettings({settingsKey});
}

void ADSBDemodGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    m_settingsKeys.append(settingsKeys);
    if (m_doApplySettings)
    {
        qDebug() << "ADSBDemodGUI::applySettings";

        ADSBDemod::MsgConfigureADSBDemod* message = ADSBDemod::MsgConfigureADSBDemod::create(m_settings, m_settingsKeys, force);
        m_adsbDemod->getInputMessageQueue()->push(message);

        m_settingsKeys.clear();
    }
}

void ADSBDemodGUI::applyAllSettings()
{
    applySettings(QStringList(), true);
}

// All settings are valid - we just use settingsKeys here to avoid updating things that are slow to update
void ADSBDemodGUI::displaySettings(const QStringList& settingsKeys, bool force)
{
    if (settingsKeys.contains("maptilerAPIKey"))
    {
        m_templateServer->close();
        delete m_templateServer;
        m_templateServer = new ADSBOSMTemplateServer(maptilerAPIKey(), m_osmPort);
    }

    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBWText->setText(QString("%1M").arg(m_settings.m_rfBandwidth / 1000000.0, 0, 'f', 1));
    ui->rfBW->setValue((int)m_settings.m_rfBandwidth);

    ui->spb->setCurrentIndex(m_settings.m_samplesPerBit/2-1);
    ui->demodModeS->setChecked(m_settings.m_demodModeS);

    ui->thresholdText->setText(QString("%1").arg(m_settings.m_correlationThreshold, 0, 'f', 1));
    ui->threshold->setValue((int)(m_settings.m_correlationThreshold*10.0f));

    ui->chipsThresholdText->setText(QString("%1").arg(m_settings.m_chipsThreshold));
    ui->chipsThreshold->setValue((int)(m_settings.m_chipsThreshold));

    ui->phaseStepsText->setText(QString("%1").arg(m_settings.m_interpolatorPhaseSteps));
    ui->phaseSteps->setValue(m_settings.m_interpolatorPhaseSteps);
    ui->tapsPerPhaseText->setText(QString("%1").arg(m_settings.m_interpolatorTapsPerPhase, 0, 'f', 1));
    ui->tapsPerPhase->setValue((int)(m_settings.m_interpolatorTapsPerPhase*10.0f));
    // Enable these controls only for developers
    if (1)
    {
        ui->phaseStepsText->setVisible(false);
        ui->phaseSteps->setVisible(false);
        ui->tapsPerPhaseText->setVisible(false);
        ui->tapsPerPhase->setVisible(false);
    }
    ui->feed->setChecked(m_settings.m_feedEnabled);

    ui->flightPaths->setChecked(m_settings.m_flightPaths);
    ui->allFlightPaths->setChecked(m_settings.m_allFlightPaths);
    m_aircraftModel.setSettings(&m_settings);

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    ui->manualQNH->setChecked(m_settings.m_manualQNH);
    ui->qnh->setEnabled(m_settings.m_manualQNH);
    ui->qnh->setValue(m_settings.m_qnh);

    ui->atcLabels->setChecked(m_settings.m_atcLabels);
    ui->coverage->setChecked(m_settings.m_displayCoverage);
    ui->displayChart->setChecked(m_settings.m_displayChart);
    ui->chart->setVisible(m_settings.m_displayChart);
    ui->stats->setChecked(m_settings.m_displayDemodStats);
    ui->statsTable->setVisible(m_settings.m_displayDemodStats);
    ui->displayOrientation->setChecked(m_settings.m_displayOrientation);
    ui->splitter->setOrientation(m_settings.m_displayOrientation ? Qt::Horizontal : Qt::Vertical);
    ui->displayRadius->setChecked(m_settings.m_displayRadius);

    updateIndexLabel();

    if (settingsKeys.contains("tableFontName") || settingsKeys.contains("tableFontSize") || force)
    {
        QFont font(m_settings.m_tableFontName, m_settings.m_tableFontSize);
        ui->adsbData->setFont(font);
        ui->statsTable->setFont(font);
    }

    // Set units in column headers
    if (settingsKeys.contains("siUnits") || force)
    {
        if (m_settings.m_siUnits)
        {
            ui->adsbData->horizontalHeaderItem(ADSB_COL_ALTITUDE)->setText("Alt (m)");
            ui->adsbData->horizontalHeaderItem(ADSB_COL_VERTICALRATE)->setText("VR (m/s)");
            ui->adsbData->horizontalHeaderItem(ADSB_COL_SEL_ALTITUDE)->setText("Sel Alt (m)");
            ui->adsbData->horizontalHeaderItem(ADSB_COL_GROUND_SPEED)->setText("GS (kph)");
            ui->adsbData->horizontalHeaderItem(ADSB_COL_TRUE_AIRSPEED)->setText("TAS (kph)");
            ui->adsbData->horizontalHeaderItem(ADSB_COL_INDICATED_AIRSPEED)->setText("IAS (kph)");
            ui->adsbData->horizontalHeaderItem(ADSB_COL_HEADWIND)->setText("H Wnd (kph)");
            ui->adsbData->horizontalHeaderItem(ADSB_COL_WIND_SPEED)->setText("Wnd (kph)");
        }
        else
        {
            ui->adsbData->horizontalHeaderItem(ADSB_COL_ALTITUDE)->setText("Alt (ft)");
            ui->adsbData->horizontalHeaderItem(ADSB_COL_VERTICALRATE)->setText("VR (ft/m)");
            ui->adsbData->horizontalHeaderItem(ADSB_COL_SEL_ALTITUDE)->setText("Sel Alt (ft)");
            ui->adsbData->horizontalHeaderItem(ADSB_COL_GROUND_SPEED)->setText("GS (kn)");
            ui->adsbData->horizontalHeaderItem(ADSB_COL_TRUE_AIRSPEED)->setText("TAS (kn)");
            ui->adsbData->horizontalHeaderItem(ADSB_COL_INDICATED_AIRSPEED)->setText("IAS (kn)");
            ui->adsbData->horizontalHeaderItem(ADSB_COL_HEADWIND)->setText("H Wnd (kn)");
            ui->adsbData->horizontalHeaderItem(ADSB_COL_WIND_SPEED)->setText("Wnd (kn)");
        }
    }

    // Order and size columns
    if (settingsKeys.contains("columnSizes") || settingsKeys.contains("columnIndexes") || force)
    {
        QHeaderView* header = ui->adsbData->horizontalHeader();
        for (int i = 0; i < ADSBDEMOD_COLUMNS; i++)
        {
            bool hidden = m_settings.m_columnSizes[i] == 0;
            header->setSectionHidden(i, hidden);
            menu->actions().at(i)->setChecked(!hidden);
            if (m_settings.m_columnSizes[i] > 0)
                ui->adsbData->setColumnWidth(i, m_settings.m_columnSizes[i]);
            header->moveSection(header->visualIndex(i), m_settings.m_columnIndexes[i]);
        }
    }

    // Only update airports on map if settings have changed
    if ((m_airportInfo != nullptr)
        && (((settingsKeys.contains("airportRange") || force) && (m_settings.m_airportRange != m_currentAirportRange))
            || ((settingsKeys.contains("airportMinimumSize") || force) && (m_settings.m_airportMinimumSize != m_currentAirportMinimumSize))
            || ((settingsKeys.contains("displayHeliports") || force) && (m_settings.m_displayHeliports != m_currentDisplayHeliports)))) {
        updateAirports();
    }

    if (settingsKeys.contains("airspaces") || force) {
        updateAirspaces();
    }
    if (settingsKeys.contains("displayNavAids") || settingsKeys.contains("airspaceRange") || force) {
        updateNavAids();
    }

    if (!m_settings.m_displayDemodStats)
        ui->stats->setText("");

    if (settingsKeys.contains("aviationstackAPIKey") || force) {
        initFlightInformation();
    }
    if (settingsKeys.contains("checkWXAPIKey") || force) {
        initAviationWeather();
    }

    if (settingsKeys.contains("importPeriod")
        || settingsKeys.contains("feedEnabled")
        || settingsKeys.contains("importEnabled")
        || force) {
        applyImportSettings();
    }

    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
    enableSpeechIfNeeded();

    if (settingsKeys.contains("mapProvider")
        || settingsKeys.contains("aircraftMinZoom")
        || settingsKeys.contains("mapType")
        || force)
    {
#ifdef __EMSCRIPTEN__
        // FIXME: If we don't have this delay, tile server requests get deleted
        QTimer::singleShot(250, [this] {
            applyMapSettings();
            });
#else
        applyMapSettings();
#endif
    }
}

void ADSBDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void ADSBDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void ADSBDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

static double roundUpNearestHundred(double x)
{
    return 100 * (int) floor(((int) (x + 99.0)) / 100.0);
}

int ADSBDemodGUI::countActiveAircraft()
{
    int total = 0;
    QDateTime now = QDateTime::currentDateTime();
    qint64 nowSecs = now.toSecsSinceEpoch();
    QHash<int, Aircraft *>::iterator i = m_aircraft.begin();

    while (i != m_aircraft.end())
    {
        Aircraft *aircraft = i.value();
        qint64 secondsSinceLastFrame = nowSecs - aircraft->m_updateTime.toSecsSinceEpoch();

        if (secondsSinceLastFrame < 10) {
            total++;
        }

        ++i;
    }

    return total;
}

void ADSBDemodGUI::removeAircraft(QHash<int, Aircraft *>::iterator& i, Aircraft *aircraft)
{
    // Don't try to track it anymore
    if (m_trackAircraft == aircraft)
    {
        m_adsbDemod->clearTarget();
        m_trackAircraft = nullptr;
    }

    // Remove map model
    m_aircraftModel.removeAircraft(aircraft);
    // Remove row from table
    ui->adsbData->removeRow(aircraft->m_icaoItem->row());
    // Remove aircraft from hash
    i = m_aircraft.erase(i);
    // Remove from map feature
    clearFromMap(aircraft->m_icaoHex);

    // And finally free its memory
    delete aircraft;
}

void ADSBDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_adsbDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1 dB").arg(powDbAvg, 0, 'f', 1));
    }

    m_tickCount++;

    // Tick is called 20x a second
    const int ticksPerSecond = 20;

    // Check for old aircraft every 10 seconds
    if (m_tickCount % (ticksPerSecond*10) == 0)
    {
        // Remove aircraft that haven't been heard of for a user-defined time, as probably out of range
        QDateTime now = QDateTime::currentDateTime();
        qint64 nowSecs = now.toSecsSinceEpoch();
        QHash<int, Aircraft *>::iterator i = m_aircraft.begin();

        while (i != m_aircraft.end())
        {
            Aircraft *aircraft = i.value();
            qint64 secondsSinceLastFrame = nowSecs - aircraft->m_updateTime.toSecsSinceEpoch();

            if (secondsSinceLastFrame >= m_settings.m_removeTimeout) {
                removeAircraft(i, aircraft);
            } else {
                ++i;
            }
        }
    }

    // Create and send aircraft report every second for WebAPI
    if (m_tickCount % ticksPerSecond == 0) {
        sendAircraftReport();
    }

    // Calculate RX frame rate every second
    if ((m_tickCount % ticksPerSecond) == 0)
    {
        QDateTime currentDateTime = QDateTime::currentDateTime();
        qint64 ms = m_frameRateTime.msecsTo(currentDateTime);

        if (ms > 0)
        {
            double s = ms / 1000.0;
            double adsbRate = m_adsbFrameRateCount / s;
            double modesFrameRate = m_modesFrameRateCount / s;
            double totalRate = (m_adsbFrameRateCount + m_modesFrameRateCount) / s;
            double dataRate = (m_totalBytes * 8 / 1000) / s; // kpbs

            ui->statsTable->item(ADSB_RATE, 0)->setData(Qt::DisplayRole, adsbRate);
            ui->statsTable->item(MODE_S_RATE, 0)->setData(Qt::DisplayRole, modesFrameRate);
            ui->statsTable->item(TOTAL_RATE, 0)->setData(Qt::DisplayRole, totalRate);
            if (totalRate > m_maxRateState)
            {
                m_maxRateState = totalRate;
                ui->statsTable->item(MAX_RATE, 0)->setData(Qt::DisplayRole, m_maxRateState);
            }
            ui->statsTable->item(DATA_RATE, 0)->setData(Qt::DisplayRole, dataRate);

            bool xAtMax = true;
            if (m_adsbFrameRateSeries)
            {
                if (m_adsbFrameRateSeries->count() > 0) {
                    xAtMax = m_adsbFrameRateSeries->at(m_adsbFrameRateSeries->count() - 1).x() == m_xAxis->max().toMSecsSinceEpoch();
                }
                m_adsbFrameRateSeries->append(currentDateTime.toMSecsSinceEpoch(), adsbRate);
            }
            if (m_modesFrameRateSeries) {
                m_modesFrameRateSeries->append(currentDateTime.toMSecsSinceEpoch(), modesFrameRate);
            }

            if (m_xAxis && xAtMax) {
                m_xAxis->setMax(currentDateTime);
            }
            if (m_fpsYAxis)
            {
                if (m_fpsYAxis->max() < adsbRate) {
                    m_fpsYAxis->setMax(roundUpNearestHundred(adsbRate));
                }
                if (m_fpsYAxis->max() < modesFrameRate) {
                    m_fpsYAxis->setMax(roundUpNearestHundred(modesFrameRate));
                }
            }

            m_frameRateTime = currentDateTime;
            m_adsbFrameRateCount = 0;
            m_modesFrameRateCount = 0;
            m_totalBytes = 0;
        }

        if (m_aircraftSeries)
        {
            int active = countActiveAircraft();

            m_aircraftSeries->append(currentDateTime.toMSecsSinceEpoch(), active);

            if (m_aircraftYAxis->max() < active + 1) {
                m_aircraftYAxis->setMax(active + 1);
            }
        }

        // Average data 10 minutes old over 1 minute, so we don't have too many points
        const int ageMins = 10;
        const int averagePeriodMins = 1;
        if ((m_tickCount % (ticksPerSecond*60*averagePeriodMins)) == 0)
        {
            QDateTime endTime, startTime;

            if (m_averageTime.isValid())
            {
                startTime = m_averageTime;
                endTime = startTime.addSecs(averagePeriodMins*60);
            }
            else
            {
                endTime = QDateTime::currentDateTime().addSecs(-ageMins*60);
                startTime = endTime.addSecs(-averagePeriodMins*60);
            }

            if (m_aircraftSeries) {
                averageSeries(m_aircraftSeries, startTime, endTime);
            }
            if (m_modesFrameRateSeries) {
                averageSeries(m_modesFrameRateSeries, startTime, endTime);
            }
            if (m_adsbFrameRateSeries) {
                averageSeries(m_adsbFrameRateSeries, startTime, endTime);
            }

            m_averageTime = endTime;
        }
    }
}

// Replace data in series between specified times with its average
void ADSBDemodGUI::averageSeries(QLineSeries *series, const QDateTime& startTime, const QDateTime& endTime)
{
    int startIdx = 0;
    int endIdx = -1;

    for (int i = series->count() - 1; i >= 0; i--)
    {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(series->at(i).x());

        if ((endIdx == -1) && (dt <= endTime))
        {
            endIdx = i;
        }
        else if (dt < startTime)
        {
            startIdx = i + 1;
            break;
        }
    }
    int count = (endIdx - startIdx) + 1;

    if ((endIdx != -1) && (count > 1))
    {
        double sum = 0.0;
        for (int i = startIdx; i <= endIdx; i++) {
            sum += series->at(i).y();
        }
        double avg = sum / count;
        series->removePoints(startIdx, count);
        qint64 midPoint = startTime.toMSecsSinceEpoch() + (endTime.toMSecsSinceEpoch() - startTime.toMSecsSinceEpoch()) / 2;
        series->insert(startIdx, QPointF(midPoint, avg));
    }
}

void ADSBDemodGUI::sendAircraftReport()
{
    ADSBDemod::MsgAircraftReport* message = ADSBDemod::MsgAircraftReport::create();

    QList<ADSBDemod::MsgAircraftReport::AircraftReport>& report = message->getReport();
    report.reserve(m_aircraft.size());

    QHash<int, Aircraft *>::iterator i = m_aircraft.begin();
    while (i != m_aircraft.end())
    {
        Aircraft *aircraft = i.value();

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

void ADSBDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    int row = ui->adsbData->rowCount();
    ui->adsbData->setRowCount(row + 1);
    ui->adsbData->setItem(row, ADSB_COL_ICAO, new QTableWidgetItem("ICAO ID"));
    ui->adsbData->setItem(row, ADSB_COL_CALLSIGN, new QTableWidgetItem("WWW88WW"));
    ui->adsbData->setItem(row, ADSB_COL_ATC_CALLSIGN, new QTableWidgetItem("ATC Callsign-"));
    ui->adsbData->setItem(row, ADSB_COL_MODEL, new QTableWidgetItem("Aircraft12345"));
    ui->adsbData->setItem(row, ADSB_COL_TYPE, new QTableWidgetItem("WWWW"));
    ui->adsbData->setItem(row, ADSB_COL_SIDEVIEW, new QTableWidgetItem("sideview"));
    ui->adsbData->setItem(row, ADSB_COL_AIRLINE, new QTableWidgetItem("airbrigdecargo1"));
    ui->adsbData->setItem(row, ADSB_COL_COUNTRY, new QTableWidgetItem("Country"));
    ui->adsbData->setItem(row, ADSB_COL_GROUND_SPEED, new QTableWidgetItem("GS (kn)"));
    ui->adsbData->setItem(row, ADSB_COL_TRUE_AIRSPEED, new QTableWidgetItem("TAS (kn)"));
    ui->adsbData->setItem(row, ADSB_COL_INDICATED_AIRSPEED, new QTableWidgetItem("IAS (kn)"));
    ui->adsbData->setItem(row, ADSB_COL_MACH, new QTableWidgetItem("0.999"));
    ui->adsbData->setItem(row, ADSB_COL_SEL_ALTITUDE, new QTableWidgetItem("Sel Alt (ft)"));
    ui->adsbData->setItem(row, ADSB_COL_ALTITUDE, new QTableWidgetItem("Surface"));
    ui->adsbData->setItem(row, ADSB_COL_VERTICALRATE, new QTableWidgetItem("VR (ft/m)"));
    ui->adsbData->setItem(row, ADSB_COL_SEL_HEADING, new QTableWidgetItem("Sel Hd (o)"));
    ui->adsbData->setItem(row, ADSB_COL_HEADING, new QTableWidgetItem("Hd (o)"));
    ui->adsbData->setItem(row, ADSB_COL_TRACK, new QTableWidgetItem("Trk (o)"));
    ui->adsbData->setItem(row, ADSB_COL_TURNRATE, new QTableWidgetItem("TR (o/s)"));
    ui->adsbData->setItem(row, ADSB_COL_ROLL, new QTableWidgetItem("Roll (o)"));
    ui->adsbData->setItem(row, ADSB_COL_RANGE, new QTableWidgetItem("D (km)"));
    ui->adsbData->setItem(row, ADSB_COL_AZEL, new QTableWidgetItem("Az/El (o)"));
    ui->adsbData->setItem(row, ADSB_COL_CATEGORY, new QTableWidgetItem("Rotorcraft"));
    ui->adsbData->setItem(row, ADSB_COL_STATUS, new QTableWidgetItem("No emergency"));
    ui->adsbData->setItem(row, ADSB_COL_SQUAWK, new QTableWidgetItem("Squawk"));
    ui->adsbData->setItem(row, ADSB_COL_IDENT, new QTableWidgetItem("Ident"));
    ui->adsbData->setItem(row, ADSB_COL_REGISTRATION, new QTableWidgetItem("G-888888"));
    ui->adsbData->setItem(row, ADSB_COL_REGISTERED, new QTableWidgetItem("8888-88-88"));
    ui->adsbData->setItem(row, ADSB_COL_MANUFACTURER, new QTableWidgetItem("The Boeing Company"));
    ui->adsbData->setItem(row, ADSB_COL_OWNER, new QTableWidgetItem("British Airways"));
    ui->adsbData->setItem(row, ADSB_COL_OPERATOR_ICAO, new QTableWidgetItem("Operator"));
    ui->adsbData->setItem(row, ADSB_COL_AP, new QTableWidgetItem("AP"));
    ui->adsbData->setItem(row, ADSB_COL_V_MODE, new QTableWidgetItem("V Mode"));
    ui->adsbData->setItem(row, ADSB_COL_L_MODE, new QTableWidgetItem("L Mode"));
    ui->adsbData->setItem(row, ADSB_COL_TCAS, new QTableWidgetItem("TCAS"));
    ui->adsbData->setItem(row, ADSB_COL_ACAS, new QTableWidgetItem("No ACAS"));
    ui->adsbData->setItem(row, ADSB_COL_RA, new QTableWidgetItem("RA"));
    ui->adsbData->setItem(row, ADSB_COL_MAX_SPEED, new QTableWidgetItem("600-1200"));
    ui->adsbData->setItem(row, ADSB_COL_VERSION, new QTableWidgetItem("Version"));
    ui->adsbData->setItem(row, ADSB_COL_LENGTH, new QTableWidgetItem("85"));
    ui->adsbData->setItem(row, ADSB_COL_WIDTH, new QTableWidgetItem("72.5"));
    ui->adsbData->setItem(row, ADSB_COL_BARO, new QTableWidgetItem("Baro (mb)"));
    ui->adsbData->setItem(row, ADSB_COL_HEADWIND, new QTableWidgetItem("H Wnd (kn)"));
    ui->adsbData->setItem(row, ADSB_COL_EST_AIR_TEMP, new QTableWidgetItem("OAT (C)"));
    ui->adsbData->setItem(row, ADSB_COL_WIND_SPEED, new QTableWidgetItem("Wnd (kn)"));
    ui->adsbData->setItem(row, ADSB_COL_WIND_DIR, new QTableWidgetItem("Wnd (o)"));
    ui->adsbData->setItem(row, ADSB_COL_STATIC_PRESSURE, new QTableWidgetItem("P (hPa)"));
    ui->adsbData->setItem(row, ADSB_COL_STATIC_AIR_TEMP, new QTableWidgetItem("T (C)"));
    ui->adsbData->setItem(row, ADSB_COL_HUMIDITY, new QTableWidgetItem("U (%)"));
    ui->adsbData->setItem(row, ADSB_COL_LATITUDE, new QTableWidgetItem("-90.00000"));
    ui->adsbData->setItem(row, ADSB_COL_LONGITUDE, new QTableWidgetItem("-180.000000"));
    ui->adsbData->setItem(row, ADSB_COL_IC, new QTableWidgetItem("63"));
    ui->adsbData->setItem(row, ADSB_COL_TIME, new QTableWidgetItem("99:99:99"));
    ui->adsbData->setItem(row, ADSB_COL_FRAMECOUNT, new QTableWidgetItem("Frames"));
    ui->adsbData->setItem(row, ADSB_COL_ADSB_FRAMECOUNT, new QTableWidgetItem("ADS-B FC"));
    ui->adsbData->setItem(row, ADSB_COL_MODES_FRAMECOUNT, new QTableWidgetItem("Mode S FC"));
    ui->adsbData->setItem(row, ADSB_COL_NON_TRANSPONDER, new QTableWidgetItem("Non-transponder"));
    ui->adsbData->setItem(row, ADSB_COL_TIS_B_FRAMECOUNT, new QTableWidgetItem("TIS-B FC"));
    ui->adsbData->setItem(row, ADSB_COL_ADSR_FRAMECOUNT, new QTableWidgetItem("ADS-R FC"));
    ui->adsbData->setItem(row, ADSB_COL_RADIUS, new QTableWidgetItem("< 0.1 NM"));
    ui->adsbData->setItem(row, ADSB_COL_NACP, new QTableWidgetItem("< 0.05 NM"));
    ui->adsbData->setItem(row, ADSB_COL_NACV, new QTableWidgetItem("< 0.3 m/s"));
    ui->adsbData->setItem(row, ADSB_COL_GVA, new QTableWidgetItem(">= 150 m"));
    ui->adsbData->setItem(row, ADSB_COL_NIC, new QTableWidgetItem("< 0.05 NM"));
    ui->adsbData->setItem(row, ADSB_COL_NIC_BARO, new QTableWidgetItem(""));
    ui->adsbData->setItem(row, ADSB_COL_SIL, new QTableWidgetItem("<= 1e-7 ph"));
    ui->adsbData->setItem(row, ADSB_COL_CORRELATION, new QTableWidgetItem("0.001/0.001/0.001"));
    ui->adsbData->setItem(row, ADSB_COL_RSSI, new QTableWidgetItem("-100.0"));
    ui->adsbData->setItem(row, ADSB_COL_FLIGHT_STATUS, new QTableWidgetItem("scheduled"));
    ui->adsbData->setItem(row, ADSB_COL_DEP, new QTableWidgetItem("WWWW"));
    ui->adsbData->setItem(row, ADSB_COL_ARR, new QTableWidgetItem("WWWW"));
    ui->adsbData->setItem(row, ADSB_COL_STOPS, new QTableWidgetItem("WWWW"));
    ui->adsbData->setItem(row, ADSB_COL_STD, new QTableWidgetItem("12:00 -1"));
    ui->adsbData->setItem(row, ADSB_COL_ETD, new QTableWidgetItem("12:00 -1"));
    ui->adsbData->setItem(row, ADSB_COL_ATD, new QTableWidgetItem("12:00 -1"));
    ui->adsbData->setItem(row, ADSB_COL_STA, new QTableWidgetItem("12:00 +1"));
    ui->adsbData->setItem(row, ADSB_COL_ETA, new QTableWidgetItem("12:00 +1"));
    ui->adsbData->setItem(row, ADSB_COL_ATA, new QTableWidgetItem("12:00 +1"));
    ui->adsbData->resizeColumnsToContents();
    ui->adsbData->setRowCount(row);
}

Aircraft* ADSBDemodGUI::findAircraftByFlight(const QString& flight)
{
    QHash<int, Aircraft *>::iterator i = m_aircraft.begin();
    while (i != m_aircraft.end())
    {
        Aircraft *aircraft = i.value();
        if (aircraft->m_flight == flight) {
            return aircraft;
        }
        ++i;
    }
    return nullptr;
}

// Convert to hh:mm (+/-days)
QString ADSBDemodGUI::dataTimeToShortString(QDateTime dt)
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

void ADSBDemodGUI::initFlightInformation()
{
    if (m_flightInformation)
    {
        disconnect(m_flightInformation, &FlightInformation::flightUpdated, this, &ADSBDemodGUI::flightInformationUpdated);
        delete m_flightInformation;
        m_flightInformation = nullptr;
    }
    if (!m_settings.m_aviationstackAPIKey.isEmpty())
    {
        m_flightInformation = FlightInformation::create(m_settings.m_aviationstackAPIKey);
        if (m_flightInformation) {
            connect(m_flightInformation, &FlightInformation::flightUpdated, this, &ADSBDemodGUI::flightInformationUpdated);
        }
    }
}

void ADSBDemodGUI::flightInformationUpdated(const FlightInformation::Flight& flight)
{
    Aircraft* aircraft = findAircraftByFlight(flight.m_flightICAO);
    if (aircraft)
    {
        aircraft->m_flightStatusItem->setText(flight.m_flightStatus);
        aircraft->m_depItem->setText(flight.m_departureICAO);
        aircraft->m_arrItem->setText(flight.m_arrivalICAO);
        aircraft->m_stopsItem->setText("");
        aircraft->m_stdItem->setText(dataTimeToShortString(flight.m_departureScheduled));
        aircraft->m_etdItem->setText(dataTimeToShortString(flight.m_departureEstimated));
        aircraft->m_atdItem->setText(dataTimeToShortString(flight.m_departureActual));
        aircraft->m_staItem->setText(dataTimeToShortString(flight.m_arrivalScheduled));
        aircraft->m_etaItem->setText(dataTimeToShortString(flight.m_arrivalEstimated));
        aircraft->m_ataItem->setText(dataTimeToShortString(flight.m_arrivalActual));
        if (aircraft->m_positionValid) {
            m_aircraftModel.aircraftUpdated(aircraft);
        }
        updatePhotoFlightInformation(aircraft);
    }
    else
    {
        qDebug() << "ADSBDemodGUI::flightInformationUpdated - Flight not found in ADS-B table: " << flight.m_flightICAO;
    }
}

void ADSBDemodGUI::aircraftPhoto(const PlaneSpottersPhoto *photo)
{
    // Make sure the photo is for the currently highlighted aircraft, as it may
    // have taken a while to download
    if (!photo->m_pixmap.isNull() && m_highlightAircraft && (m_highlightAircraft->m_icaoItem->text() == photo->m_id))
    {
        ui->photo->setPixmap(photo->m_pixmap);
        ui->photo->setToolTip(QString("Photographer: %1").arg(photo->m_photographer)); // Required by terms of use
        ui->photoHeader->setVisible(true);
        ui->photoFlag->setVisible(true);
        ui->photo->setVisible(true);
        ui->flightDetails->setVisible(true);
        ui->aircraftDetails->setVisible(true);
        m_photoLink = photo->m_link;
    }
}

void ADSBDemodGUI::photoClicked()
{
    // Photo needs to link back to PlaneSpotters, as per terms of use
    if (m_highlightAircraft)
    {
        if (m_photoLink.isEmpty())
        {
            QString icaoUpper = QString("%1").arg(m_highlightAircraft->m_icao, 1, 16).toUpper();
            QDesktopServices::openUrl(QUrl(QString("https://www.planespotters.net/hex/%1").arg(icaoUpper)));
        }
        else
        {
            QDesktopServices::openUrl(QUrl(m_photoLink));
        }
    }
}

void ADSBDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySetting("logEnabled");
}

void ADSBDemodGUI::on_logFilename_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select file to log received frames to", "", "*.csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            m_settings.m_logFilename = fileNames[0];
            ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
            applySetting("logFilename");
        }
    }
}

// Read .csv log and process as received frames
void ADSBDemodGUI::on_logOpen_clicked()
{
    QFileDialog fileDialog(nullptr, "Select .csv log file to read", "", "*.csv");
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            QFile file(fileNames[0]);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                QDateTime startTime = QDateTime::currentDateTime();
                m_loadingData = true;
                ui->adsbData->blockSignals(true);
                QTextStream in(&file);
                QString error;
                QHash<QString, int> colIndexes = CSV::readHeader(in, {"Data", "Correlation"}, error);
                if (error.isEmpty())
                {
                    int dateCol = colIndexes.value("Date");
                    int timeCol = colIndexes.value("Time");
                    int dataCol = colIndexes.value("Data");
                    int correlationCol = colIndexes.value("Correlation");
                    int maxCol = std::max(dataCol, correlationCol);

                    QMessageBox dialog(this);
                    dialog.setText("Reading ADS-B data");
                    dialog.addButton(QMessageBox::Cancel);
                    dialog.show();
                    QApplication::processEvents();
                    int count = 0;
                    int countOtherDF = 0;
                    bool cancelled = false;
                    QStringList cols;
                    crcadsb crc;
                    while (!cancelled && CSV::readRow(in, &cols))
                    {
                        if (cols.size() > maxCol)
                        {
                            QDateTime dateTime = QDateTime(QDate::fromString(cols[dateCol]), QTime::fromString(cols[timeCol]));
                            QByteArray bytes = QByteArray::fromHex(cols[dataCol].toLatin1());
                            float correlation = cols[correlationCol].toFloat();
                            int df = (bytes[0] >> 3) & ADS_B_DF_MASK; // Downlink format
                            if (   (m_settings.m_demodModeS && ((df == 0) || (df == 4) || (df == 5) || (df == 11) || (df == 16) || (df == 19) || (df == 20) || (df == 21) || (df == 22) || (df == 24)))
                                || (df == 17) || (df == 18))
                            {
                                int crcCalc = 0;
                                if ((df == 0) || (df == 4) || (df == 5) || (df == 16) || (df == 20) || (df == 21))  // handleADSB requires calculated CRC for Mode-S frames
                                {
                                    crc.init();
                                    crc.calculate((const uint8_t *)bytes.data(), bytes.size()-3);
                                    crcCalc = crc.get();
                                }
                                handleADSB(bytes, dateTime, correlation, correlation, crcCalc, false);
                                if ((count > 0) && (count % 100000 == 0))
                                {
                                    dialog.setText(QString("Reading ADS-B data\n%1 (Skipped %2)").arg(count).arg(countOtherDF));
                                    QApplication::processEvents();
                                    if (dialog.clickedButton()) {
                                        cancelled = true;
                                    }
                                }
                                count++;
                            }
                            else
                            {
                                countOtherDF++;
                            }
                        }
                    }
                    m_aircraftModel.allAircraftUpdated();
                    dialog.close();
                }
                else
                {
                    QMessageBox::critical(this, "ADS-B", error);
                }
                ui->adsbData->blockSignals(false);
                m_loadingData = false;
                if (m_settings.m_autoResizeTableColumns)
                    ui->adsbData->resizeColumnsToContents();
                ui->adsbData->setSortingEnabled(true);
                QDateTime finishTime = QDateTime::currentDateTime();
                qDebug() << "Read CSV in " << startTime.secsTo(finishTime);
            }
            else
            {
                QMessageBox::critical(this, "ADS-B", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void ADSBDemodGUI::downloadingURL(const QString& url)
{
    if (m_progressDialog)
    {
        m_progressDialog->setLabelText(QString("Downloading %1.").arg(url));
        m_progressDialog->setValue(m_progressDialog->value() + 1);
    }
}

void ADSBDemodGUI::downloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    if (m_progressDialog)
    {
        m_progressDialog->setMaximum(totalBytes);
        m_progressDialog->setValue(bytesRead);
    }
}

void ADSBDemodGUI::downloadError(const QString& error)
{
    QMessageBox::critical(this, "ADS-B", error);
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void ADSBDemodGUI::downloadAirspaceFinished()
{
    if (m_progressDialog) {
        m_progressDialog->setLabelText("Reading airspaces.");
    }
    m_airspaces = OpenAIP::getAirspaces();
    updateAirspaces();
    m_openAIP.downloadNavAids();
}

void ADSBDemodGUI::downloadNavAidsFinished()
{
    if (m_progressDialog) {
        m_progressDialog->setLabelText("Reading NAVAIDs.");
    }
    m_navAids = OpenAIP::getNavAids();
    updateNavAids();
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void ADSBDemodGUI::downloadAircraftInformationFinished()
{
    if (m_progressDialog)
    {
        delete m_progressDialog;
        m_progressDialog = new QProgressDialog("Reading Aircraft Information.", "", 0, 1, this);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_progressDialog->setWindowModality(Qt::WindowModal);
        m_progressDialog->show();
        QApplication::processEvents();
    }
    m_aircraftInfo = OsnDB::getAircraftInformation();
    m_routeInfo = OsnDB::getAircraftRouteInformation();
    m_aircraftModel.updateAircraftInformation(m_aircraftInfo);
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void ADSBDemodGUI::downloadAirportInformationFinished()
{
    if (m_progressDialog)
    {
        delete m_progressDialog;
        m_progressDialog = new QProgressDialog("Reading Airport Information.", "", 0, 1, this);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_progressDialog->setWindowModality(Qt::WindowModal);
        m_progressDialog->show();
        QApplication::processEvents();
    }
    m_airportInfo = OurAirportsDB::getAirportsById();
    updateAirports();
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

int ADSBDemodGUI::squawkDecode(int modeA) const
{
    int a, b, c, d;
    c = ((modeA >> 12) & 1) | ((modeA >> (10-1)) & 0x2) | ((modeA >> (8-2)) & 0x4);
    a = ((modeA >> 11) & 1) | ((modeA >> (9-1)) & 0x2) | ((modeA >> (7-2)) & 0x4);
    b = ((modeA >> 5) & 1) | ((modeA >> (3-1)) & 0x2) | ((modeA << (1)) & 0x4);
    d = ((modeA >> 4) & 1) | ((modeA >> (2-1)) & 0x2) | ((modeA << (2)) & 0x4);
    return a*1000 + b*100 + c*10 + d;
}

// https://en.wikipedia.org/wiki/Gillham_code
int ADSBDemodGUI::gillhamToFeet(int n) const
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

    int n500 = grayToBinary((d2 << 7) | (d4 << 6) | (a1 << 5) | (a2 << 4) | (a4 << 3) | (b1 << 2) | (b2 << 1) | b4, 4);
    int n100 = grayToBinary((c1 << 2) | (c2 << 1) | c4, 3) - 1;

    if (n100 == 6) {
        n100 = 4;
    }
    if (n500 %2 != 0) {
        n100 = 4 - n100;
    }

    return -1200 + n500*500 + n100*100;
}

int ADSBDemodGUI::grayToBinary(int gray, int bits) const
{
    int binary = 0;
    for (int i = bits - 1; i >= 0; i--) {
        binary = binary | ((((1 << (i+1)) & binary) >> 1) ^ ((1 << i) & gray));
    }
    return binary;
}

void ADSBDemodGUI::redrawMap()
{
#ifdef QT_LOCATION_FOUND
    // An awful workaround for https://bugreports.qt.io/browse/QTBUG-100333
    // Also used in Map feature
    QQuickItem *item = ui->map->rootObject();
    if (item)
    {
        QObject *object = item->findChild<QObject*>("map");
        if (object)
        {
            double zoom = object->property("zoomLevel").value<double>();
            object->setProperty("zoomLevel", QVariant::fromValue(zoom+1));
            object->setProperty("zoomLevel", QVariant::fromValue(zoom));
        }
    }
#endif
}

void ADSBDemodGUI::showEvent(QShowEvent *event)
{
    if (!event->spontaneous())
    {
        // Workaround for https://bugreports.qt.io/browse/QTBUG-100333
        // MapQuickItems can be in wrong position when window is first displayed
        m_redrawMapTimer.start(500);
    }
    ChannelGUI::showEvent(event);
}

static void scale(qint64& start, qint64& end, qint64 min, int delta, qreal centre)
{
    qint64 diff = end - start;
    double scale = pow(0.50, abs(delta) / 120.0);
    qint64 newRange;

    if (delta < 0) {
        newRange = diff / scale;
    } else {
        newRange = diff * scale;
    }

    diff = std::max(min/2, diff);
    newRange = std::max(min, newRange);
    if (delta < 0) {
        start = start - centre * diff;
    } else {
        start = start + centre * newRange;
    }
    end = start + newRange;
}

bool ADSBDemodGUI::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->map)
    {
        if (event->type() == QEvent::Resize)
        {
            // Workaround for https://bugreports.qt.io/browse/QTBUG-100333
            // MapQuickItems can be in wrong position after vertical resize
            QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);
            QSize oldSize = resizeEvent->oldSize();
            QSize size = resizeEvent->size();
            if (oldSize.height() != size.height()) {
                redrawMap();
            }
        }
    }
    else if (obj == ui->chart)
    {
        if (event->type() == QEvent::Wheel)
        {
            // Use wheel to zoom in / out of X axis or Y axis if shift held
            QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);

            int delta = wheelEvent->angleDelta().y(); // delta is typically 120 for one click of wheel

            if (m_adsbFrameRateSeries)
            {
                QPointF point = wheelEvent->position();
                QRectF plotArea = m_chart->plotArea();

                if (wheelEvent->modifiers() & Qt::ShiftModifier)
                {
                    // Center scaling on cursor location
                    qreal y = (point.y() - plotArea.y()) / plotArea.height();
                    y = 1.0 - std::min(1.0, std::max(0.0, y));

                    qint64 min, max;

                    if (m_adsbFrameRateSeries->isVisible() || m_modesFrameRateSeries->isVisible())
                    {
                        min = (qint64) m_fpsYAxis->min();
                        max = (qint64) m_fpsYAxis->max();

                        scale(min, max, 2LL, delta / 2, y);

                        min = std::max(0LL, min);
                        max = std::min(5000LL, max);

                        m_fpsYAxis->setMin((qreal) min);
                        m_fpsYAxis->setMax((qreal) max);
                    }

                    if (m_aircraftSeries->isVisible())
                    {
                        min = (qint64) m_aircraftYAxis->min();
                        max = (qint64) m_aircraftYAxis->max();

                        scale(min, max, 2LL, delta / 2, y);

                        min = std::max(0LL, min);
                        max = std::min(5000LL, max);

                        m_aircraftYAxis->setMin((qreal) min);
                        m_aircraftYAxis->setMax((qreal) max);
                    }
                }
                else
                {
                    if (m_adsbFrameRateSeries->count() > 1)
                    {
                        // Center scaling on cursor location
                        qreal x = (point.x() - plotArea.x()) / plotArea.width();
                        x = std::min(1.0, std::max(0.0, x));

                        qint64 startMS = m_xAxis->min().toMSecsSinceEpoch();
                        qint64 endMS = m_xAxis->max().toMSecsSinceEpoch();

                        scale(startMS, endMS, 10000LL, delta, x);

                        // Don't let range exceed available data
                        startMS = std::max((qint64) m_adsbFrameRateSeries->at(0).x(), startMS);
                        endMS = std::min((qint64) m_adsbFrameRateSeries->at(m_adsbFrameRateSeries->count() - 1).x(), endMS);
                        QDateTime start = QDateTime::fromMSecsSinceEpoch(startMS);
                        QDateTime end = QDateTime::fromMSecsSinceEpoch(endMS);
                        m_xAxis->setMin(start);
                        m_xAxis->setMax(end);
                    }
                }
            }
            wheelEvent->accept();
            return true;
        }
    }
    return ChannelGUI::eventFilter(obj, event);
}

void ADSBDemodGUI::applyImportSettings()
{
    m_importTimer.setInterval(m_settings.m_importPeriod * 1000);
    if (m_settings.m_feedEnabled && m_settings.m_importEnabled) {
        m_importTimer.start();
    } else {
        m_importTimer.stop();
    }
}

// Import ADS-B data from opensky-network via an API call
void ADSBDemodGUI::import()
{
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
    if (!m_settings.m_importUsername.isEmpty() && !m_settings.m_importPassword.isEmpty())
    {
        QByteArray encoded = (m_settings.m_importUsername + ":" + m_settings.m_importPassword).toLocal8Bit().toBase64();
        request.setRawHeader("Authorization", "Basic " + encoded);
    }
    m_networkManager->get(request);
}

// Handle opensky-network API call reply
void ADSBDemodGUI::handleImportReply(QNetworkReply* reply)
{
    if (reply)
    {
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
                        Aircraft *aircraft = getAircraft(icao, newAircraft);

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
                        QTime time = aircraft->m_rxTime.time();
                        aircraft->m_timeItem->setText(QString("%1:%2:%3").arg(time.hour(), 2, 10, QLatin1Char('0')).arg(time.minute(), 2, 10, QLatin1Char('0')).arg(time.second(), 2, 10, QLatin1Char('0')));
                        aircraft->m_adsbFrameCount++;
                        aircraft->m_adsbFrameCountItem->setData(Qt::DisplayRole, aircraft->m_adsbFrameCount);

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
                                aircraft->addCoordinate(dateTime, &m_aircraftModel);
                            }
                        }
                        aircraft->m_onSurface = state[8].toBool(false);
                        aircraft->m_onSurfaceValid = true;
                        if (!state[9].isNull()) {
                            aircraft->setGroundspeed((int)state[9].toDouble(), m_settings);
                        }
                        if (!state[10].isNull()) {
                            aircraft->setTrack((float)state[10].toDouble(), aircraft->m_rxTime);
                        }
                        if (!state[11].isNull()) {
                            aircraft->setVerticalRate((int)state[10].toDouble(), m_settings);
                        }
                        if (!state[14].isNull())
                        {
                            aircraft->m_squawk = state[14].toString().toInt();
                            aircraft->m_squawkItem->setText(QString("%1").arg(aircraft->m_squawk, 4, 10, QLatin1Char('0')));
                        }

                        // Update aircraft in map
                        if (aircraft->m_positionValid)
                        {
                            // Check to see if we need to start any animations
                            QList<SWGSDRangel::SWGMapAnimation *> *animations = animate(dateTime, aircraft);

                            // Update map displayed in channel
                            m_aircraftModel.aircraftUpdated(aircraft);

                            // Send to Map feature
                            sendToMap(aircraft, animations);
                        }

                        // Check to see if we need to emit a notification about this aircraft
                        checkDynamicNotification(aircraft);
                    }
                }
                else
                {
                    qDebug() << "ADSBDemodGUI::handleImportReply: Document object does not contain time and states: " << document;
                }
            }
            else
            {
                qDebug() << "ADSBDemodGUI::handleImportReply: Document is not an object: " << document;
            }
        }
        else
        {
            qDebug() << "ADSBDemodGUI::handleImportReply: error " << reply->error();
        }
        reply->deleteLater();
    }
}

void ADSBDemodGUI::updatePosition(float latitude, float longitude, float altitude)
{
    // Use device postion in preference to My Position
    ChannelWebAPIUtils::getDevicePosition(getDeviceSetIndex(), latitude, longitude, altitude);

    QGeoCoordinate stationPosition(latitude, longitude, altitude);
    QGeoCoordinate previousPosition(m_azEl.getLocationSpherical().m_latitude, m_azEl.getLocationSpherical().m_longitude, m_azEl.getLocationSpherical().m_altitude);

    if (stationPosition != previousPosition)
    {
        m_azEl.setLocation(latitude, longitude, altitude);

        // Update distances and what is visible, but only do it if position has changed significantly
        if (!m_lastFullUpdatePosition.isValid() || (stationPosition.distanceTo(m_lastFullUpdatePosition) >= 1000))
        {
            updateAirports();
            updateAirspaces();
            updateNavAids();
            m_lastFullUpdatePosition = stationPosition;
        }

#ifdef QT_LOCATION_FOUND
        // Update icon position on Map
        QQuickItem *item = ui->map->rootObject();
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QObject *map = item->findChild<QObject*>("map");
#else
        QObject *map = item->findChild<QObject*>("mapView");
#endif
        if (map != nullptr)
        {
            QObject *stationObject = map->findChild<QObject*>("station");
            if(stationObject != NULL)
            {
                QGeoCoordinate coords = stationObject->property("coordinate").value<QGeoCoordinate>();
                coords.setLatitude(latitude);
                coords.setLongitude(longitude);
                coords.setAltitude(altitude);
                stationObject->setProperty("coordinate", QVariant::fromValue(coords));
            }
        }
#endif
    }
}

void ADSBDemodGUI::devicePositionChanged(float latitude, float longitude, float altitude)
{
    updatePosition(latitude, longitude, altitude);
}

void ADSBDemodGUI::preferenceChanged(int elementType)
{
    Preferences::ElementType pref = (Preferences::ElementType)elementType;
    if ((pref == Preferences::Latitude) || (pref == Preferences::Longitude) || (pref == Preferences::Altitude))
    {
        Real myLatitude = MainCore::instance()->getSettings().getLatitude();
        Real myLongitude = MainCore::instance()->getSettings().getLongitude();
        Real myAltitude = MainCore::instance()->getSettings().getAltitude();

        updatePosition(myLatitude, myLongitude, myAltitude);
    }
    else if (pref == Preferences::StationName)
    {
#ifdef QT_LOCATION_FOUND
        // Update icon label on Map
        QQuickItem *item = ui->map->rootObject();
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QObject *map = item->findChild<QObject*>("map");
#else
        QObject *map = item->findChild<QObject*>("mapView");
#endif
        if (map != nullptr)
        {
            QObject *stationObject = map->findChild<QObject*>("station");
            if(stationObject != NULL) {
                stationObject->setProperty("stationName", QVariant::fromValue(MainCore::instance()->getSettings().getStationName()));
            }
        }
#endif
    }
    else if (pref == Preferences::MapSmoothing)
    {
#ifdef QT_LOCATION_FOUND
        QQuickItem *item = ui->map->rootObject();
        QQmlProperty::write(item, "smoothing", MainCore::instance()->getSettings().getMapSmoothing());
#endif
    }
}

void ADSBDemodGUI::initAviationWeather()
{
    if (m_aviationWeather)
    {
        disconnect(m_aviationWeather, &AviationWeather::weatherUpdated, this, &ADSBDemodGUI::weatherUpdated);
        delete m_aviationWeather;
        m_aviationWeather = nullptr;
    }
    if (!m_settings.m_checkWXAPIKey.isEmpty())
    {
        m_aviationWeather = AviationWeather::create(m_settings.m_checkWXAPIKey);
        if (m_aviationWeather) {
            connect(m_aviationWeather, &AviationWeather::weatherUpdated, this, &ADSBDemodGUI::weatherUpdated);
        }
    }
}

void ADSBDemodGUI::requestMetar(const QString& icao)
{
    if (m_aviationWeather) {
        m_aviationWeather->getWeather(icao);
    }
}

void ADSBDemodGUI::weatherUpdated(const AviationWeather::METAR &metar)
{
    m_airportModel.updateWeather(metar.m_icao, metar.m_text, metar.decoded());
}

void ADSBDemodGUI::on_manualQNH_clicked(bool checked)
{
    m_settings.m_manualQNH = checked;
    applySetting("manualQNH");
    ui->qnh->setEnabled(m_settings.m_manualQNH);
}

void ADSBDemodGUI::on_qnh_valueChanged(int value)
{
    m_settings.m_qnh = value;
    applySetting("qnh");
}

void ADSBDemodGUI::updateQNH(const Aircraft *aircraft, float qnh)
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
            ui->qnh->setValue((int)round(m_qnhAvg.instantAverage()));
        }
    }
}

void ADSBDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &ADSBDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &ADSBDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->threshold, &QDial::valueChanged, this, &ADSBDemodGUI::on_threshold_valueChanged);
    QObject::connect(ui->chipsThreshold, &QDial::valueChanged, this, &ADSBDemodGUI::on_chipsThreshold_valueChanged);
    QObject::connect(ui->phaseSteps, &QDial::valueChanged, this, &ADSBDemodGUI::on_phaseSteps_valueChanged);
    QObject::connect(ui->tapsPerPhase, &QDial::valueChanged, this, &ADSBDemodGUI::on_tapsPerPhase_valueChanged);
    QObject::connect(ui->adsbData, &QTableWidget::cellClicked, this, &ADSBDemodGUI::on_adsbData_cellClicked);
    QObject::connect(ui->adsbData, &QTableWidget::cellDoubleClicked, this, &ADSBDemodGUI::on_adsbData_cellDoubleClicked);
    QObject::connect(ui->spb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ADSBDemodGUI::on_spb_currentIndexChanged);
    QObject::connect(ui->demodModeS, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_demodModeS_clicked);
    QObject::connect(ui->feed, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_feed_clicked);
    QObject::connect(ui->notifications, &QToolButton::clicked, this, &ADSBDemodGUI::on_notifications_clicked);
    QObject::connect(ui->flightInfo, &QToolButton::clicked, this, &ADSBDemodGUI::on_flightInfo_clicked);
    QObject::connect(ui->findOnMapFeature, &QToolButton::clicked, this, &ADSBDemodGUI::on_findOnMapFeature_clicked);
    QObject::connect(ui->deleteAircraft, &QToolButton::clicked, this, &ADSBDemodGUI::on_deleteAircraft_clicked);
    QObject::connect(ui->getAircraftDB, &QToolButton::clicked, this, &ADSBDemodGUI::on_getAircraftDB_clicked);
    QObject::connect(ui->getAirportDB, &QToolButton::clicked, this, &ADSBDemodGUI::on_getAirportDB_clicked);
    QObject::connect(ui->getAirspacesDB, &QToolButton::clicked, this, &ADSBDemodGUI::on_getAirspacesDB_clicked);
    QObject::connect(ui->flightPaths, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_flightPaths_clicked);
    QObject::connect(ui->allFlightPaths, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_allFlightPaths_clicked);
    QObject::connect(ui->atcLabels, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_atcLabels_clicked);
    QObject::connect(ui->coverage, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_coverage_clicked);
    QObject::connect(ui->displayChart, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_displayChart_clicked);
    QObject::connect(ui->displayOrientation, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_displayOrientation_clicked);
    QObject::connect(ui->displayRadius, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_displayRadius_clicked);
    QObject::connect(ui->stats, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_stats_clicked);
    QObject::connect(ui->ic, &CheckList::globalCheckStateChanged, this, &ADSBDemodGUI::on_ic_globalCheckStateChanged);
    QObject::connect(ui->amDemod, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ADSBDemodGUI::on_amDemod_currentIndexChanged);
    QObject::connect(ui->displaySettings, &QToolButton::clicked, this, &ADSBDemodGUI::on_displaySettings_clicked);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &ADSBDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &ADSBDemodGUI::on_logOpen_clicked);
    QObject::connect(ui->manualQNH, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_manualQNH_clicked);
    QObject::connect(ui->qnh, QOverload<int>::of(&QSpinBox::valueChanged), this, &ADSBDemodGUI::on_qnh_valueChanged);

}

void ADSBDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

void ADSBDemodGUI::initCoverageMap()
{
    float lat = m_azEl.getLocationSpherical().m_latitude;
    float lon = m_azEl.getLocationSpherical().m_longitude;
    for (int i = 0; i < 2; i++)
    {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        m_maxRange[i].resize(360/ADSBDemodGUI::m_maxRangeDeg, 0.0f);
#else
        for (int j = 0; j < 360/ADSBDemodGUI::m_maxRangeDeg; j++) {
            m_maxRange[i].append(0.0f);
        }
#endif
        m_coverageAirspace[i].m_polygon.resize(2 * 360/ADSBDemodGUI::m_maxRangeDeg);
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

void ADSBDemodGUI::clearCoverageMap()
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

// Lats and longs in decimal degrees. Distance in metres. Bearing in degrees.
// https://www.movable-type.co.uk/scripts/latlong.html
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

void ADSBDemodGUI::updateCoverageMap(float azimuth, float elevation, float distance, float altitude)
{
    (void) elevation;

    int i = (int) (azimuth / ADSBDemodGUI::m_maxRangeDeg);
    int k = altitude >= 10000 ? 1 : 0;
    if (distance > m_maxRange[k][i])
    {
        m_maxRange[k][i] = distance;

        float lat;
        float lon;
        float b1 = i * ADSBDemodGUI::m_maxRangeDeg;
        calcRadialEndPoint(m_azEl.getLocationSpherical().m_latitude, m_azEl.getLocationSpherical().m_longitude, distance, b1, lat, lon);
        m_coverageAirspace[k].m_polygon[i*2].setX(lon);
        m_coverageAirspace[k].m_polygon[i*2].setY(lat);

        float b2 = (i + 1) * ADSBDemodGUI::m_maxRangeDeg;
        calcRadialEndPoint(m_azEl.getLocationSpherical().m_latitude, m_azEl.getLocationSpherical().m_longitude, distance, b2, lat, lon);
        m_coverageAirspace[k].m_polygon[i*2+1].setX(lon);
        m_coverageAirspace[k].m_polygon[i*2+1].setY(lat);

        m_airspaceModel.airspaceUpdated(&m_coverageAirspace[k]);
    }
}

void ADSBDemodGUI::clearCoverage(const QPoint& p)
{
    (void) p;

    clearCoverageMap();
    for (int i = 0; i < 2; i++) {
        m_airspaceModel.airspaceUpdated(&m_coverageAirspace[i]);
    }
}

void ADSBDemodGUI::clearStats(const QPoint& p)
{
    (void) p;

    resetStats();
}

void ADSBDemodGUI::clearChart(const QPoint& p)
{
    (void) p;

    if (m_adsbFrameRateSeries) {
        m_adsbFrameRateSeries->clear();
    }
    if (m_modesFrameRateSeries) {
        m_modesFrameRateSeries->clear();
    }
    if (m_aircraftSeries) {
        m_aircraftSeries->clear();
    }
    resetChartAxes();
    m_averageTime = QDateTime();
}

void ADSBDemodGUI::resetChartAxes()
{
    m_xAxis->setMin(QDateTime::currentDateTime());
    m_xAxis->setMax(QDateTime::currentDateTime().addSecs(60*60));
    m_fpsYAxis->setMin(0);
    m_fpsYAxis->setMax(100);
    m_aircraftYAxis->setMin(0);
    m_aircraftYAxis->setMax(10);
}

void ADSBDemodGUI::plotChart()
{
    QChart *oldChart = m_chart;

    m_chart = new QChart();

    m_chart->layout()->setContentsMargins(0, 0, 0, 0);
    m_chart->setMargins(QMargins(1, 1, 1, 1));
    m_chart->setTheme(QChart::ChartThemeDark);
    m_chart->legend()->setAlignment(Qt::AlignRight);

    m_adsbFrameRateSeries = new QLineSeries();
    m_adsbFrameRateSeries->setName("ADS-B");

    m_modesFrameRateSeries = new QLineSeries();
    m_modesFrameRateSeries->setName("Mode S");

    m_aircraftSeries = new QLineSeries();
    m_aircraftSeries->setName("Aircraft");

    m_xAxis = new QDateTimeAxis();
    m_fpsYAxis = new QValueAxis();
    m_aircraftYAxis = new QValueAxis();
    resetChartAxes();

    m_chart->addAxis(m_xAxis, Qt::AlignBottom);
    m_chart->addAxis(m_fpsYAxis, Qt::AlignLeft);
    m_chart->addAxis(m_aircraftYAxis, Qt::AlignRight);

    m_fpsYAxis->setTitleText("FPS");
    m_aircraftYAxis->setTitleText("Total");

    m_chart->addSeries(m_adsbFrameRateSeries);
    m_chart->addSeries(m_modesFrameRateSeries);
    m_chart->addSeries(m_aircraftSeries);

    m_adsbFrameRateSeries->attachAxis(m_xAxis);
    m_adsbFrameRateSeries->attachAxis(m_fpsYAxis);

    m_modesFrameRateSeries->attachAxis(m_xAxis);
    m_modesFrameRateSeries->attachAxis(m_fpsYAxis);

    m_aircraftSeries->attachAxis(m_xAxis);
    m_aircraftSeries->attachAxis(m_aircraftYAxis);

    ui->chart->setChart(m_chart);
    ui->chart->installEventFilter(this);

    const auto markers = m_chart->legend()->markers();
    for (QLegendMarker *marker : markers) {
        connect(marker, &QLegendMarker::clicked, this, &ADSBDemodGUI::legendMarkerClicked);
    }

    delete oldChart;
}

void ADSBDemodGUI::legendMarkerClicked()
{
    QLegendMarker* marker = qobject_cast<QLegendMarker*>(sender());
    marker->series()->setVisible(!marker->series()->isVisible());
    marker->setVisible(true);

    // Dim the marker, if series is not visible
    qreal alpha = 1.0;

    if (!marker->series()->isVisible()) {
        alpha = 0.5;
    }

    QColor color;
    QBrush brush = marker->labelBrush();
    color = brush.color();
    color.setAlphaF(alpha);
    brush.setColor(color);
    marker->setLabelBrush(brush);

    brush = marker->brush();
    color = brush.color();
    color.setAlphaF(alpha);
    brush.setColor(color);
    marker->setBrush(brush);

    QPen pen = marker->pen();
    color = pen.color();
    color.setAlphaF(alpha);
    pen.setColor(color);
    marker->setPen(pen);
}
