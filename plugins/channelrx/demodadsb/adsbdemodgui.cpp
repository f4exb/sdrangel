///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include "device/deviceuiset.h"
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

#include "ui_adsbdemodgui.h"
#include "device/deviceapi.h"
#include "channel/channelwebapiutils.h"
#include "feature/featurewebapiutils.h"
#include "plugin/pluginapi.h"
#include "util/crc.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "util/units.h"
#include "util/morse.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/crightclickenabler.h"
#include "gui/clickablelabel.h"
#include "gui/tabletapandhold.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "mainwindow.h"

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
    QStringLiteral("Airbourne"),
    QStringLiteral("On-ground"),
    QStringLiteral("Alert, airboune"),
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
    displaySettings();
    applySettings();
}

QByteArray ADSBDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool ADSBDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        updateDeviceSetList();
        displaySettings();
        applySettings(true);
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
    if (m_emitterCategory.length() > 0)
    {
        if (!m_emitterCategory.compare("Heavy")) {
            return QString("aircraft_4engine.png"); // Can also be 777, 787
        } else if (!m_emitterCategory.compare("Large")) {
            return QString("aircraft_2engine.png");
        } else if (!m_emitterCategory.compare("Small")) {
            return QString("aircraft_2enginesmall.png");
        } else if (!m_emitterCategory.compare("Rotorcraft")) {
            return QString("aircraft_helicopter.png");
        } else if (!m_emitterCategory.compare("High performance")) {
            return QString("aircraft_fighter.png");
        } else if (!m_emitterCategory.compare("Light")
                || !m_emitterCategory.compare("Ultralight")
                || !m_emitterCategory.compare("Glider/sailplane")) {
            return QString("aircraft_light.png");
        } else if (!m_emitterCategory.compare("Space vehicle")) {
            return QString("aircraft_space.png");
        } else if (!m_emitterCategory.compare("UAV")) {
            return QString("aircraft_drone.png");
        } else if (!m_emitterCategory.compare("Emergency vehicle")
                || !m_emitterCategory.compare("Service vehicle")) {
            return QString("truck.png");
        } else {
            return QString("aircraft_2engine.png");
        }
    }
    else
    {
        return QString("aircraft_2engine.png");
    }
}

QString Aircraft::getText(bool all) const
{
    QStringList list;
    if (m_showAll || all)
    {
        if (!m_flagIconURL.isEmpty() && !m_airlineIconURL.isEmpty())
        {
            list.append(QString("<table width=100%><tr><td><img src=%1><td><img src=%2 align=right></table>").arg(m_airlineIconURL).arg(m_flagIconURL));
        }
        else
        {
            if (!m_flagIconURL.isEmpty()) {
                list.append(QString("<img src=%1>").arg(m_flagIconURL));
            } else if (!m_airlineIconURL.isEmpty()) {
                list.append(QString("<img src=%1>").arg(m_airlineIconURL));
            }
        }
        list.append(QString("ICAO: %1").arg(m_icaoHex));
        if (!m_callsign.isEmpty()) {
            list.append(QString("Callsign: %1").arg(m_callsign));
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
    else if (!m_callsign.isEmpty())
    {
        list.append(m_callsign);
    }
    else
    {
        list.append(m_icaoHex);
    }
    return list.join("<br>");
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
        return QVariant::fromValue(m_aircrafts[row]->m_heading);
    }
    else if (role == AircraftModel::adsbDataRole)
    {
        // Create the text to go in the bubble next to the aircraft
        return QVariant::fromValue(m_aircrafts[row]->getText());
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
    else if (role == AircraftModel::aircraftPathRole)
    {
       if ((m_flightPaths && m_aircrafts[row]->m_isHighlighted) || m_allFlightPaths)
           return m_aircrafts[row]->m_coordinates;
       else
           return QVariantList();
    }
    else if (role == AircraftModel::showAllRole)
        return QVariant::fromValue(m_aircrafts[row]->m_showAll);
    else if (role == AircraftModel::highlightedRole)
        return QVariant::fromValue(m_aircrafts[row]->m_isHighlighted);
    else if (role == AircraftModel::targetRole)
        return QVariant::fromValue(m_aircrafts[row]->m_isTarget);
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
            m_gui->setFrequency(m_airports[row]->m_frequencies[idx]->m_frequency * 1000000);
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
        details.append(QString("\n%1 - %2")
                    .arg(m_airspaces[row]->getAlt(&m_airspaces[row]->m_bottom))
                    .arg(m_airspaces[row]->getAlt(&m_airspaces[row]->m_top)));
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
        if ((m_airspaces[row]->m_category == "D")
            || (m_airspaces[row]->m_category == "G")
            || (m_airspaces[row]->m_category == "CTR")) {
            return QVariant::fromValue(QColor(0x00, 0x00, 0xff, 0x00));
        } else {
            return QVariant::fromValue(QColor(0xff, 0x00, 0x00, 0x00));
        }
    }
    else if (role == AirspaceModel::airspaceFillColorRole)
    {
        if ((m_airspaces[row]->m_category == "D")
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

// Set selected device to the given centre frequency (used to tune to ATC selected from airports on map)
bool ADSBDemodGUI::setFrequency(float targetFrequencyHz)
{
    return ChannelWebAPIUtils::setCenterFrequency(m_settings.m_deviceIndex, targetFrequencyHz);
}

// Called when we have both lat & long
void ADSBDemodGUI::updatePosition(Aircraft *aircraft)
{
    if (!aircraft->m_positionValid)
    {
        aircraft->m_positionValid = true;
        // Now we have a position, add a plane to the map
        QGeoCoordinate coords;
        coords.setLatitude(aircraft->m_latitude);
        coords.setLongitude(aircraft->m_longitude);
        m_aircraftModel.addAircraft(aircraft);
    }
    // Calculate range, azimuth and elevation to aircraft from station
    m_azEl.setTarget(aircraft->m_latitude, aircraft->m_longitude, Units::feetToMetres(aircraft->m_altitude));
    m_azEl.calculate();
    aircraft->m_range = m_azEl.getDistance();
    aircraft->m_azimuth = m_azEl.getAzimuth();
    aircraft->m_elevation = m_azEl.getElevation();
    aircraft->m_rangeItem->setText(QString::number(aircraft->m_range/1000.0, 'f', 1));
    aircraft->m_azElItem->setText(QString("%1/%2").arg(std::round(aircraft->m_azimuth)).arg(std::round(aircraft->m_elevation)));
    if (aircraft == m_trackAircraft) {
        m_adsbDemod->setTarget(aircraft->targetName(), aircraft->m_azimuth, aircraft->m_elevation, aircraft->m_range);
    }
}

// Called when we have lat & long from local decode and we need to check if it is in a valid range (<180nm/333km)
// or 1/4 of that for surface positions
bool ADSBDemodGUI::updateLocalPosition(Aircraft *aircraft, double latitude, double longitude, bool surfacePosition)
{
    // Calculate range to aircraft from station
    m_azEl.setTarget(latitude, longitude, Units::feetToMetres(aircraft->m_altitude));
    m_azEl.calculate();

    // Don't use the full 333km, as there may be some error in station position
    if (m_azEl.getDistance() < (surfacePosition ? 80000 : 320000))
    {
        aircraft->m_latitude = latitude;
        aircraft->m_longitude = longitude;
        updatePosition(aircraft);
        return true;
    }
    else
    {
        //qDebug() << "Local position out of range - calculated distance: " << m_azEl.getDistance();
        return false;
    }
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
        // Adjust altitude by airfield barometric elevation, so aircraft appears to
        // take-off/land at correct point on runway
        int altitudeFt = aircraft->m_altitude;

        if (!aircraft->m_onSurface && !aircraft->m_altitudeGNSS) {
            altitudeFt -= m_settings.m_airfieldElevation;
        }

        float altitudeM = Units::feetToMetres(altitudeFt);

        for (const auto& pipe : mapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
            swgMapItem->setName(new QString(aircraft->m_icaoHex));
            swgMapItem->setLatitude(aircraft->m_latitude);
            swgMapItem->setLongitude(aircraft->m_longitude);
            swgMapItem->setAltitude(altitudeM);
            swgMapItem->setPositionDateTime(new QString(aircraft->m_positionDateTime.toString(Qt::ISODateWithMs)));
            swgMapItem->setFixedPosition(false);
            swgMapItem->setAvailableUntil(new QString(aircraft->m_positionDateTime.addSecs(60).toString(Qt::ISODateWithMs)));
            swgMapItem->setImage(new QString(QString("qrc:///map/%1").arg(aircraft->getImage())));
            swgMapItem->setImageRotation(aircraft->m_heading);
            swgMapItem->setText(new QString(aircraft->getText(true)));

            if (!aircraft->m_aircraft3DModel.isEmpty()) {
                swgMapItem->setModel(new QString(aircraft->m_aircraft3DModel));
            } else {
                swgMapItem->setModel(new QString(aircraft->m_aircraftCat3DModel));
            }

            swgMapItem->setLabel(new QString(aircraft->m_callsign));

            if (aircraft->m_headingValid)
            {
                swgMapItem->setOrientation(1);
                swgMapItem->setHeading(aircraft->m_heading);
                swgMapItem->setPitch(aircraft->m_pitchEst);
                swgMapItem->setRoll(aircraft->m_rollEst);
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
        aircraft->m_icaoHex = QString::number(aircraft->m_icao, 16);
        m_aircraft.insert(icao, aircraft);
        aircraft->m_icaoItem->setText(aircraft->m_icaoHex);
        ui->adsbData->setSortingEnabled(false);
        int row = ui->adsbData->rowCount();
        ui->adsbData->setRowCount(row + 1);
        ui->adsbData->setItem(row, ADSB_COL_ICAO, aircraft->m_icaoItem);
        ui->adsbData->setItem(row, ADSB_COL_CALLSIGN, aircraft->m_callsignItem);
        ui->adsbData->setItem(row, ADSB_COL_MODEL, aircraft->m_modelItem);
        ui->adsbData->setItem(row, ADSB_COL_AIRLINE, aircraft->m_airlineItem);
        ui->adsbData->setItem(row, ADSB_COL_ALTITUDE, aircraft->m_altitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_HEADING, aircraft->m_headingItem);
        ui->adsbData->setItem(row, ADSB_COL_VERTICALRATE, aircraft->m_verticalRateItem);
        ui->adsbData->setItem(row, ADSB_COL_RANGE, aircraft->m_rangeItem);
        ui->adsbData->setItem(row, ADSB_COL_AZEL, aircraft->m_azElItem);
        ui->adsbData->setItem(row, ADSB_COL_LATITUDE, aircraft->m_latitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_LONGITUDE, aircraft->m_longitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_CATEGORY, aircraft->m_emitterCategoryItem);
        ui->adsbData->setItem(row, ADSB_COL_STATUS, aircraft->m_statusItem);
        ui->adsbData->setItem(row, ADSB_COL_SQUAWK, aircraft->m_squawkItem);
        ui->adsbData->setItem(row, ADSB_COL_REGISTRATION, aircraft->m_registrationItem);
        ui->adsbData->setItem(row, ADSB_COL_COUNTRY, aircraft->m_countryItem);
        ui->adsbData->setItem(row, ADSB_COL_REGISTERED, aircraft->m_registeredItem);
        ui->adsbData->setItem(row, ADSB_COL_MANUFACTURER, aircraft->m_manufacturerNameItem);
        ui->adsbData->setItem(row, ADSB_COL_OWNER, aircraft->m_ownerItem);
        ui->adsbData->setItem(row, ADSB_COL_OPERATOR_ICAO, aircraft->m_operatorICAOItem);
        ui->adsbData->setItem(row, ADSB_COL_TIME, aircraft->m_timeItem);
        ui->adsbData->setItem(row, ADSB_COL_FRAMECOUNT, aircraft->m_adsbFrameCountItem);
        ui->adsbData->setItem(row, ADSB_COL_CORRELATION, aircraft->m_correlationItem);
        ui->adsbData->setItem(row, ADSB_COL_RSSI, aircraft->m_rssiItem);
        ui->adsbData->setItem(row, ADSB_COL_FLIGHT_STATUS, aircraft->m_flightStatusItem);
        ui->adsbData->setItem(row, ADSB_COL_DEP, aircraft->m_depItem);
        ui->adsbData->setItem(row, ADSB_COL_ARR, aircraft->m_arrItem);
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
        ui->adsbData->setItem(row, ADSB_COL_TIS_B, aircraft->m_tisBItem);
        // Look aircraft up in database
        if (m_aircraftInfo != nullptr)
        {
            if (m_aircraftInfo->contains(icao))
            {
                aircraft->m_aircraftInfo = m_aircraftInfo->value(icao);
                aircraft->m_modelItem->setText(aircraft->m_aircraftInfo->m_model);
                aircraft->m_registrationItem->setText(aircraft->m_aircraftInfo->m_registration);
                aircraft->m_manufacturerNameItem->setText(aircraft->m_aircraftInfo->m_manufacturerName);
                aircraft->m_ownerItem->setText(aircraft->m_aircraftInfo->m_owner);
                aircraft->m_operatorICAOItem->setText(aircraft->m_aircraftInfo->m_operatorICAO);
                aircraft->m_registeredItem->setText(aircraft->m_aircraftInfo->m_registered);
                // Try loading an airline logo based on operator ICAO
                QIcon *icon = nullptr;
                if (aircraft->m_aircraftInfo->m_operatorICAO.size() > 0)
                {
                    aircraft->m_airlineIconURL = AircraftInformation::getAirlineIconPath(aircraft->m_aircraftInfo->m_operatorICAO);
                    if (aircraft->m_airlineIconURL.startsWith(':')) {
                        aircraft->m_airlineIconURL = "qrc://" + aircraft->m_airlineIconURL.mid(1);
                    }
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
                // Try loading a flag based on registration
                if (aircraft->m_aircraftInfo->m_registration.size() > 0)
                {
                    QString flag = aircraft->m_aircraftInfo->getFlag();
                    if (flag != "")
                    {
                        aircraft->m_flagIconURL = AircraftInformation::getFlagIconPath(flag);
                        if (aircraft->m_flagIconURL.startsWith(':')) {
                            aircraft->m_flagIconURL = "qrc://" + aircraft->m_flagIconURL.mid(1);
                        }
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

// Try to map callsign to flight number
void ADSBDemodGUI::callsignToFlight(Aircraft *aircraft)
{
    if (!aircraft->m_callsign.isEmpty())
    {
        QRegularExpression flightNoExp("^[A-Z]{2,3}[0-9]{1,4}$");
        // Airlines line BA add a single charater suffix that can be stripped
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

void ADSBDemodGUI::handleADSB(
    const QByteArray data,
    const QDateTime dateTime,
    float correlation,
    float correlationOnes,
    unsigned crc,
    bool updateModel)
{
    bool newAircraft = false;
    bool updatedCallsign = false;
    bool resetAnimation = false;

    int df = (data[0] >> 3) & ADS_B_DF_MASK; // Downlink format
    int ca = data[0] & 0x7; // Capability
    unsigned icao;
    Aircraft *aircraft;

    if ((df == 4) || (df == 5) || (df == 20) || (df == 21))
    {
        // Extract ICAO from parity
        int bytes = data.length();
        unsigned parity = (data[bytes-3] << 16) | (data[bytes-2] << 8) | data[bytes-1];
        icao = (parity ^ crc) & 0xffffff;
        if (m_aircraft.contains(icao))
        {
            //qDebug() << "Mode-S from known aircraft - DF " << df << " ICAO " << Qt::hex << icao;
            aircraft = getAircraft(icao, newAircraft);
        }
        else
        {
            // Ignore if not from a known aircraft, as its likely not to be a valid packet
            //qDebug() << "Skiping Mode-S from unknown aircraft - DF " << df << " ICAO " << Qt::hex << icao;
            return;
        }
    }
    else
    {
        icao = ((data[1] & 0xff) << 16) | ((data[2] & 0xff) << 8) | (data[3] & 0xff); // ICAO aircraft address
        aircraft = getAircraft(icao, newAircraft);
    }
    int tc = (data[4] >> 3) & 0x1f; // Type code

    aircraft->m_time = dateTime;
    QTime time = dateTime.time();
    aircraft->m_timeItem->setText(QString("%1:%2:%3").arg(time.hour(), 2, 10, QLatin1Char('0')).arg(time.minute(), 2, 10, QLatin1Char('0')).arg(time.second(), 2, 10, QLatin1Char('0')));
    aircraft->m_adsbFrameCount++;
    aircraft->m_adsbFrameCountItem->setData(Qt::DisplayRole, aircraft->m_adsbFrameCount);
    if (df == 18)
    {
        aircraft->m_tisBFrameCount++;
        aircraft->m_tisBItem->setData(Qt::DisplayRole, aircraft->m_tisBFrameCount);
    }

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

    // ADS-B, non-transponder ADS-B or TIS-B rebroadcast of ADS-B (ADS-R)
    if ((df == 17) || ((df == 18) && (ca != 4)))
    {

        if ((tc >= 1) && ((tc <= 4)))
        {
            // Aircraft identification - BDS 0,8
            int ec = data[4] & 0x7;   // Emitter category

            QString prevEmitterCategory = aircraft->m_emitterCategory;
            if (tc == 4) {
                aircraft->m_emitterCategory = m_categorySetA[ec];
            } else if (tc == 3) {
                aircraft->m_emitterCategory = m_categorySetB[ec];
            } else if (tc == 2) {
                aircraft->m_emitterCategory = m_categorySetC[ec];
            } else {
                aircraft->m_emitterCategory = QStringLiteral("Reserved");
            }
            aircraft->m_emitterCategoryItem->setText(aircraft->m_emitterCategory);

            // Flight/callsign - Extract 8 6-bit characters from 6 8-bit bytes, MSB first
            unsigned char c[8];
            char callsign[9];
            c[0] = (data[5] >> 2) & 0x3f; // 6
            c[1] = ((data[5] & 0x3) << 4) | ((data[6] & 0xf0) >> 4);  // 2+4
            c[2] = ((data[6] & 0xf) << 2) | ((data[7] & 0xc0) >> 6);  // 4+2
            c[3] = (data[7] & 0x3f); // 6
            c[4] = (data[8] >> 2) & 0x3f;
            c[5] = ((data[8] & 0x3) << 4) | ((data[9] & 0xf0) >> 4);
            c[6] = ((data[9] & 0xf) << 2) | ((data[10] & 0xc0) >> 6);
            c[7] = (data[10] & 0x3f);
            // Map to ASCII
            for (int i = 0; i < 8; i++)
                callsign[i] = m_idMap[c[i]];
            callsign[8] = '\0';
            QString callsignTrimmed = QString(callsign).trimmed();

            updatedCallsign = aircraft->m_callsign != callsignTrimmed;
            if (updatedCallsign)
            {
                aircraft->m_callsign = callsignTrimmed;
                aircraft->m_callsignItem->setText(aircraft->m_callsign);
                callsignToFlight(aircraft);
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
        else if (((tc >= 5) && (tc <= 18)) || ((tc >= 20) && (tc <= 22)))
        {
            bool wasOnSurface = aircraft->m_onSurface;
            aircraft->m_onSurface = (tc >= 5) && (tc <= 8);

            if (wasOnSurface != aircraft->m_onSurface)
            {
                // Can't mix CPR values used on surface and those that are airbourne
                aircraft->m_cprValid[0] = false;
                aircraft->m_cprValid[1] = false;
            }

            if (aircraft->m_onSurface)
            {
                // Surface position - BDS 0,6

                // There are a few airports that are below 0 MSL
                // https://en.wikipedia.org/wiki/List_of_lowest_airports
                // So we set altitude to a negative value here, which should
                // then get clipped to actual terrain elevation in 3D map
                aircraft->m_altitudeValid = true;
                aircraft->m_altitude = -200;
                aircraft->m_altitudeItem->setData(Qt::DisplayRole, "Surface");

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
                    aircraft->m_groundspeedValid = true;
                    aircraft->m_groundspeedItem->setData(Qt::DisplayRole, 0);
                    aircraft->m_groundspeed = 0.0;
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
                    aircraft->m_groundspeed = base + (movement - adjust) * step;
                    aircraft->m_groundspeedValid = true;
                    aircraft->m_groundspeedItem->setData(Qt::DisplayRole, m_settings.m_siUnits ? Units::knotsToIntegerKPH(aircraft->m_groundspeed) : (int)std::round(aircraft->m_groundspeed));
                }
                else if (movement == 124)
                {
                    aircraft->m_groundspeedValid = true;
                    aircraft->m_groundspeedItem->setData(Qt::DisplayRole, m_settings.m_siUnits ? 324 : 175); // Actually greater than this
                }

                int groundTrackStatus = (data[5] >> 3) & 1;
                int groundTrackValue = ((data[5] & 0x7) << 4) | ((data[6] >> 4) & 0xf);
                if (groundTrackStatus)
                {
                    aircraft->m_heading = groundTrackValue * 360.0/128.0;
                    aircraft->m_headingValid = true;
                    aircraft->m_headingItem->setData(Qt::DisplayRole, std::round(aircraft->m_heading));
                }
            }
            else if (((tc >= 9) && (tc <= 18)) || ((tc >= 20) && (tc <= 22)))
            {
                // Airbourne position (9-18 baro, 20-22 GNSS)
                int alt = ((data[5] & 0xff) << 4) | ((data[6] >> 4) & 0xf); // Altitude
                int q = (alt & 0x10) != 0;
                int n = ((alt >> 1) & 0x7f0) | (alt & 0xf);  // Remove Q-bit
                int alt_ft;
                if (q == 1)
                {
                    alt_ft = n * ((alt & 0x10) ? 25 : 100) - 1000;
                }
                else
                {
                    alt_ft = gillhamToFeet(n);
                }

                aircraft->m_altitude = alt_ft;
                aircraft->m_altitudeValid = alt != 0;
                aircraft->m_altitudeGNSS = ((tc >= 20) && (tc <= 22));
                // setData rather than setText so it sorts numerically
                aircraft->m_altitudeItem->setData(Qt::DisplayRole, m_settings.m_siUnits ? Units::feetToIntegerMetres(aircraft->m_altitude) : aircraft->m_altitude);

                // Assume runway elevation is at first reported airboune altitude
                if (wasOnSurface)
                {
                    aircraft->m_runwayAltitude = aircraft->m_altitude;
                    aircraft->m_runwayAltitudeValid = true;
                }
            }

            int f = (data[6] >> 2) & 1; // CPR odd/even frame - should alternate every 0.2s
            int lat_cpr = ((data[6] & 3) << 15) | ((data[7] & 0xff) << 7) | ((data[8] >> 1) & 0x7f);
            int lon_cpr = ((data[8] & 1) << 16) | ((data[9] & 0xff) << 8) | (data[10] & 0xff);

            aircraft->m_cprValid[f] = true;
            aircraft->m_cprLat[f] = lat_cpr/131072.0f;
            aircraft->m_cprLong[f] = lon_cpr/131072.0f;
            aircraft->m_cprTime[f] = dateTime;

            // CPR decoding
            // Refer to Technical Provisions  for Mode S Services and Extended Squitter - Appendix C2.6
            // See also: https://mode-s.org/decode/adsb/airborne-position.html
            // For global decoding, we need both odd and even frames
            // We also need to check that both frames aren't greater than 10s apart in time (C.2.6.7), otherwise position may be out by ~10deg
            // I've reduced this to 8.5s, as problems have been seen where times are just 9s apart. This may be because
            // our timestamps aren't accurate, as the times are generated when packets are decoded on buffered data.
            // We could compare global + local methods to see if the positions are sensible
            if (aircraft->m_cprValid[0] && aircraft->m_cprValid[1]
               && (std::abs(aircraft->m_cprTime[0].toMSecsSinceEpoch() - aircraft->m_cprTime[1].toMSecsSinceEpoch()) <= 8500)
               && !aircraft->m_onSurface)
            {
                // Global decode using odd and even frames (C.2.6)

                // Calculate latitude
                const double dLatEven = 360.0/60.0;
                const double dLatOdd = 360.0/59.0;
                double latEven, latOdd;
                double latitude, longitude;
                int ni, m;

                int j = std::floor(59.0f*aircraft->m_cprLat[0] - 60.0f*aircraft->m_cprLat[1] + 0.5);
                latEven = dLatEven * (modulus(j, 60) + aircraft->m_cprLat[0]);
                // Southern hemisphere is in range 270-360, so adjust to -90-0
                if (latEven >= 270.0f)
                    latEven -= 360.0f;
                latOdd = dLatOdd * (modulus(j, 59) + aircraft->m_cprLat[1]);
                if (latOdd >= 270.0f)
                    latOdd -= 360.0f;
                if (aircraft->m_cprTime[0] >= aircraft->m_cprTime[1])
                    latitude = latEven;
                else
                    latitude = latOdd;
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
                            m = std::floor(aircraft->m_cprLong[0] * (latEvenNL - 1) - aircraft->m_cprLong[1] * latEvenNL + 0.5f);
                            longitude = (360.0f/ni) * (modulus(m, ni) + aircraft->m_cprLong[0]);
                        }
                        else
                        {
                            ni = cprN(latOdd, 1);
                            m = std::floor(aircraft->m_cprLong[0] * (latOddNL - 1) - aircraft->m_cprLong[1] * latOddNL + 0.5f);
                            longitude = (360.0f/ni) * (modulus(m, ni) + aircraft->m_cprLong[1]);
                        }
                        if (longitude > 180.0f)
                            longitude -= 360.0f;
                        aircraft->m_latitude = latitude;
                        aircraft->m_latitudeItem->setData(Qt::DisplayRole, aircraft->m_latitude);
                        aircraft->m_longitude = longitude;
                        aircraft->m_longitudeItem->setData(Qt::DisplayRole, aircraft->m_longitude);
                        aircraft->m_positionDateTime = dateTime;
                        QGeoCoordinate coord(aircraft->m_latitude, aircraft->m_longitude, aircraft->m_altitude);
                        aircraft->m_coordinates.push_back(QVariant::fromValue(coord));
                        updatePosition(aircraft);
                    }
                }
                else
                {
                    qDebug() << "ADSBDemodGUI::handleADSB: Invalid latitude " << latitude << " for " << QString("%1").arg(aircraft->m_icaoHex)
                        << " m_cprLat[0] " << aircraft->m_cprLat[0]
                        << " m_cprLat[1] " << aircraft->m_cprLat[1];
                    aircraft->m_cprValid[0] = false;
                    aircraft->m_cprValid[1] = false;
                }
            }
            else
            {
                // Local decode using a single aircraft position + location of receiver
                // Only valid if airbourne within 180nm/333km (C.2.6.4) or 45nm for surface

                // Caclulate latitude
                const double maxDeg = aircraft->m_onSurface ? 90.0 : 360.0;
                const double dLatEven = maxDeg/60.0;
                const double dLatOdd = maxDeg/59.0;
                double dLat = f ? dLatOdd : dLatEven;
                double latitude, longitude;

                int j = std::floor(m_azEl.getLocationSpherical().m_latitude/dLat) + std::floor(modulus(m_azEl.getLocationSpherical().m_latitude, dLat)/dLat - aircraft->m_cprLat[f] + 0.5);
                latitude = dLat * (j + aircraft->m_cprLat[f]);

                // Caclulate longitude
                double dLong;
                int latNL = cprNL(latitude);
                if (f == 0)
                {
                    if (latNL > 0)
                        dLong = maxDeg / latNL;
                    else
                        dLong = maxDeg;
                }
                else
                {
                    if ((latNL - 1) > 0)
                        dLong = maxDeg / (latNL - 1);
                    else
                        dLong = maxDeg;
                }
                int m = std::floor(m_azEl.getLocationSpherical().m_longitude/dLong) + std::floor(modulus(m_azEl.getLocationSpherical().m_longitude, dLong)/dLong - aircraft->m_cprLong[f] + 0.5);
                longitude =  dLong * (m + aircraft->m_cprLong[f]);

                if (updateLocalPosition(aircraft, latitude, longitude, aircraft->m_onSurface))
                {
                    aircraft->m_latitude = latitude;
                    aircraft->m_latitudeItem->setData(Qt::DisplayRole, aircraft->m_latitude);
                    aircraft->m_longitude = longitude;
                    aircraft->m_longitudeItem->setData(Qt::DisplayRole, aircraft->m_longitude);
                    aircraft->m_positionDateTime = dateTime;
                    QGeoCoordinate coord(aircraft->m_latitude, aircraft->m_longitude, aircraft->m_altitude);
                    aircraft->m_coordinates.push_back(QVariant::fromValue(coord));
                }
            }
        }
        else if (tc == 19)
        {
            // Airbourne velocity - BDS 0,9
            int st = data[4] & 0x7;   // Subtype
            if ((st == 1) || (st == 2))
            {
                // Ground speed
                int s_ew = (data[5] >> 2) & 1; // East-west velocity sign
                int v_ew = ((data[5] & 0x3) << 8) | (data[6] & 0xff); // East-west velocity
                int s_ns = (data[7] >> 7) & 1; // North-south velocity sign
                int v_ns = ((data[7] & 0x7f) << 3) | ((data[8] >> 5) & 0x7); // North-south velocity

                int v_we;
                int v_sn;
                float v;
                float h;

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

                aircraft->m_heading = h; // This is actually track, rather than heading
                aircraft->m_headingValid = true;
                aircraft->m_headingDateTime = dateTime;
                aircraft->m_groundspeed = v;
                aircraft->m_groundspeedValid = true;
                aircraft->m_headingItem->setData(Qt::DisplayRole, std::round(aircraft->m_heading));
                aircraft->m_groundspeedItem->setData(Qt::DisplayRole,m_settings.m_siUnits ? Units::knotsToIntegerKPH(aircraft->m_groundspeed) : aircraft->m_groundspeed);
                aircraft->m_orientationDateTime = dateTime;
            }
            else
            {
                // Airspeed (only likely to get this if an aircraft is unable to determine it's position)
                int s_hdg = (data[5] >> 2) & 1; // Heading status
                int hdg =  ((data[5] & 0x3) << 8) | (data[6] & 0xff); // Heading
                if (s_hdg)
                {
                    aircraft->m_heading = hdg/1024.0f*360.0f;
                    aircraft->m_headingValid = true;
                    aircraft->m_headingDateTime = dateTime;
                    aircraft->m_headingItem->setData(Qt::DisplayRole, std::round(aircraft->m_heading));
                    aircraft->m_orientationDateTime = dateTime;
                }

                int as_t = (data[7] >> 7) & 1; // Airspeed type
                int as = ((data[7] & 0x7f) << 3) | ((data[8] >> 5) & 0x7); // Airspeed

                if (as_t == 1)
                {
                    aircraft->m_trueAirspeed = as;
                    aircraft->m_trueAirspeedValid = true;
                    aircraft->m_trueAirspeedItem->setData(Qt::DisplayRole, m_settings.m_siUnits ? Units::knotsToIntegerKPH(aircraft->m_trueAirspeed) : aircraft->m_trueAirspeed);
                }
                else
                {
                    aircraft->m_indicatedAirspeed = as;
                    aircraft->m_indicatedAirspeedValid = true;
                    aircraft->m_indicatedAirspeedItem->setData(Qt::DisplayRole, m_settings.m_siUnits ? Units::knotsToIntegerKPH(aircraft->m_indicatedAirspeed) : aircraft->m_indicatedAirspeed);
                }
            }
            //int vr_source = (data[8] >> 4) & 1; // Source of vertical rate GNSS=0 Baro=1
            int s_vr = (data[8] >> 3) & 1; // Vertical rate sign
            int vr = ((data[8] & 0x7) << 6) | ((data[9] >> 2) & 0x3f); // Vertical rate
            aircraft->m_verticalRate = (vr-1)*64*(s_vr?-1:1);
            aircraft->m_verticalRateValid = true;
            if (m_settings.m_siUnits)
                aircraft->m_verticalRateItem->setData(Qt::DisplayRole, Units::feetPerMinToIntegerMetresPerSecond(aircraft->m_verticalRate));
            else
                aircraft->m_verticalRateItem->setData(Qt::DisplayRole, aircraft->m_verticalRate);
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
                bool lnavMode = (data[10] >> 2) & 0x1;

                aircraft->m_apItem->setText(autoPilot ? QChar(0x2713) : QChar(0x2717)); // Tick or cross

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
        else if (tc == 31)
        {
            // Aircraft operation status
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
                // Wait until after model has changed before reseting
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
        // TIS-B that doesn't match ADS-B formats, such as TIS-B management
        qDebug() << "TIS B message cf=" << ca << " icao: " << QString::number(icao, 16);
    }
    else if ((df == 4) || (df == 5))
    {
        decodeModeS(data, df, aircraft);
    }
    else if ((df == 20) || (df == 21))
    {
        decodeModeS(data, df, aircraft);
        decodeCommB(data, dateTime, df, aircraft, updatedCallsign);
    }

    // Check to see if we need to emit a notification about this aircraft
    checkDynamicNotification(aircraft);

    // Update text below photo if it's likely to have changed
    if ((aircraft == m_highlightAircraft) && (newAircraft || updatedCallsign)) {
        updatePhotoText(aircraft);
    }
}

void ADSBDemodGUI::decodeModeS(const QByteArray data, int df, Aircraft *aircraft)
{
    bool wasOnSurface = aircraft->m_onSurface;
    bool takenOff = false;

    int flightStatus = data[0] & 0x7;
    if ((flightStatus == 0) || (flightStatus == 2))
    {
        takenOff = wasOnSurface;
        aircraft->m_onSurface = false;
    }
    else if ((flightStatus == 1) || (flightStatus == 3))
    {
        aircraft->m_onSurface = true;
    }
    if (wasOnSurface != aircraft->m_onSurface)
    {
        // Can't mix CPR values used on surface and those that are airbourne
        aircraft->m_cprValid[0] = false;
        aircraft->m_cprValid[1] = false;
    }
    //qDebug() << "Flight Status " << m_flightStatuses[flightStatus];

    int altitude = 0;   // Altitude in feet

    if ((df == 4) || (df == 20))
    {
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

        aircraft->m_altitude = altitude;
        aircraft->m_altitudeValid = true;
        aircraft->m_altitudeGNSS = false;
        aircraft->m_altitudeItem->setData(Qt::DisplayRole, m_settings.m_siUnits ? Units::feetToIntegerMetres(aircraft->m_altitude) : aircraft->m_altitude);

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

void ADSBDemodGUI::decodeCommB(const QByteArray data, const QDateTime dateTime, int df, Aircraft *aircraft, bool &updatedCallsign)
{
    // We only see downlink messages, so do not know the data format, so have to decode all posibilities
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

        // BDS 1,0 - ELS

        bool bds_1_0 = (data[4] == 0x10) && ((data[5] & 0x7c) == 0x00);

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
        char callsign[9];
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
            callsign[i] = m_idMap[c[i]];
        }
        callsign[8] = '\0';
        QString callsignTrimmed = QString(callsign).trimmed();
        bool invalidCallsign = QString(callsign).contains('#');
        bool bds_2_0 = (data[4] == 0x20) && !invalidCallsign;

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
        bool bds_3_0 = (data[4] == 0x30) && (acas < 48) && (threatType != 3);

        // BDS 4,0 - Selected vertical information - EHS

        bool mcpSelectedAltStatus = (data[4] >> 7) & 0x1;
        int mcpSelectedAltFix = ((data[4] & 0x7f) << 5) | ((data[5] >> 3) & 0x1f);
        int mcpSelectedAlt = mcpSelectedAltFix * 16; // ft
        bool mcpSelectedAltInconsistent = (mcpSelectedAlt > maxAlt) || (!mcpSelectedAltStatus && (mcpSelectedAltFix != 0));

        bool fmsSelectedAltStatus = (data[5] >> 2) & 0x1;
        int fmsSelectedAltFix = ((data[5] & 0x3) << 10) | ((data[6] & 0xff) << 2) || (data[7] >> 6) & 0x3;
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
        int windDirection = windDirectionFix * 180.0f / 256.0f; // Degreees
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
        bool noMetData = ((data[4] == 0x20) || (data[4] == 0x08)) && !(data[5] || data[6] || data[7] || data[8] || data[9] || data[10]);

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
        bool trueTrackAngleInconsistent =  (aircraft->m_headingValid && (abs(trueTrackAngle - aircraft->m_heading) > maxHeadingDiff))
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
        float latitude = latitudeFix * (360.0f / 1048576.0f);

        int longitudeFix = ((data[6] & 0x7) << 17) | ((data[7] & 0xff) << 9) | ((data[8] & 0xff) << 1) | ((data[9] >> 7) & 0x1);
        longitudeFix = (longitudeFix << 12) >> 12;
        float longitude = longitudeFix * (360.0f / 1048576.0f);

        bool positionInconsistent = !aircraft->m_positionValid
                                || (positionValid && aircraft->m_positionValid && ((abs(latitude - aircraft->m_latitude > 2.0f)) || (abs(longitude - aircraft->m_longitude) > 2.0f)))
                                || (!positionValid && ((latitudeFix != 0) || (longitudeFix != 0)));

        int pressureAltFix = ((data[9] & 0x7f) << 8) | (data[10] & 0xff);
        pressureAltFix = (pressureAltFix << 17) >> 17;
        int pressureAlt = pressureAltFix * 8;
        bool pressureAltInconsistent = (pressureAlt > 50000) || (pressureAlt < -1000) || (positionValid && aircraft->m_altitudeValid && (abs(pressureAlt - aircraft->m_altitude) > 2000))
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

        bool bds_6_0 = !magHeadingInconsistent & !indicatedAirspeedInconsistent & !machInconsistent && !baroAltRateInconsistent && !verticalVelInconsistent;

        int possibleMatches = bds_1_0 + bds_1_7 + bds_2_0 + bds_2_1 + bds_3_0 + bds_4_0 + bds_4_1 + bds_4_4 + bds_4_5 + bds_5_0 + bds_5_1 + bds_5_3 + bds_6_0;

        if (possibleMatches == 1)
        {
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
                if (updatedCallsign)
                {
                    aircraft->m_callsign = callsignTrimmed;
                    aircraft->m_callsignItem->setText(aircraft->m_callsign);
                    callsignToFlight(aircraft);
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
                    aircraft->m_heading = trueTrackAngle;
                    aircraft->m_headingValid = true;
                    aircraft->m_headingDateTime = dateTime;
                    aircraft->m_headingItem->setData(Qt::DisplayRole, std::round(aircraft->m_heading));
                    aircraft->m_orientationDateTime = dateTime;
                }
                if (groundSpeedStatus)
                {
                    aircraft->m_groundspeed = groundSpeed;
                    aircraft->m_groundspeedValid = true;
                    if (m_settings.m_siUnits) {
                        aircraft->m_groundspeedItem->setData(Qt::DisplayRole, Units::knotsToIntegerKPH(aircraft->m_groundspeed));
                    } else {
                        aircraft->m_groundspeedItem->setData(Qt::DisplayRole, aircraft->m_groundspeed);
                    }
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
                // Position is specified as "coarse" - is it worth using?
                qDebug() << "BDS 5,1 - "
                        << "ICAO:" << aircraft->m_icaoHex
                        << "latitude:" << latitude << "(ADS-B:" << aircraft->m_latitude << ") "
                        << "longitude:" << longitude  << "(ADS-B:" << aircraft->m_longitude << ") "
                        << "pressureAlt:" << pressureAlt  << "(ADS-B:" << aircraft->m_altitude << ")"
                        << "m_bdsCapabilities[5][1]: " << aircraft->m_bdsCapabilities[5][1]
                        << "m_bdsCapabilitiesValid: " << aircraft->m_bdsCapabilitiesValid;
                        ;
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
                if (indicatedAirspeedStatus)
                {
                    aircraft->m_indicatedAirspeed = indicatedAirspeed;
                    aircraft->m_indicatedAirspeedValid = true;
                    if (m_settings.m_siUnits) {
                        aircraft->m_indicatedAirspeedItem->setData(Qt::DisplayRole, Units::knotsToIntegerKPH(aircraft->m_indicatedAirspeed));
                    } else {
                        aircraft->m_indicatedAirspeedItem->setData(Qt::DisplayRole, aircraft->m_indicatedAirspeed);
                    }
                }
                if (machStatus)
                {
                    aircraft->m_mach = mach;
                    aircraft->m_machValid = true;
                    aircraft->m_machItem->setData(Qt::DisplayRole, aircraft->m_mach);
                }
                if (verticalVelStatus)
                {
                    aircraft->m_verticalRate = verticalVel;
                    aircraft->m_verticalRateValid = true;
                    if (m_settings.m_siUnits) {
                        aircraft->m_verticalRateItem->setData(Qt::DisplayRole, Units::feetPerMinToIntegerMetresPerSecond(aircraft->m_verticalRate));
                    } else {
                        aircraft->m_verticalRateItem->setData(Qt::DisplayRole, aircraft->m_verticalRate);
                    }
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

            qDebug() << (bds_1_0 ? "+" : "-")
                     << "BDS 1,0 - ";
            qDebug() << (bds_1_7 ? "+" : "-")
                     << "BDS 1,7 - "
                     << "ICAO:" << aircraft->m_icaoHex
                     << "BDS:" << caps.join(" ");
            qDebug() << (bds_2_0 ? "+" : "-")
                     << "BDS 2,0 - "
                     << "ICAO:" << aircraft->m_icaoHex
                     << "Callsign:" << callsignTrimmed;
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
                     << "latitude:" << latitude << "(ADS-B:" << aircraft->m_latitude << ") "
                     << "longitude:" << longitude  << "(ADS-B:" << aircraft->m_longitude << ") "
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
    // Check speed in case we get a mixture of surface and airbourne positions
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
    // We don't know airport elevation, so just base on speed and descent rate
    if (!aircraft->m_gearDown
        && !aircraft->m_onSurface
        && !aircraft->m_runwayAltitudeValid
        && (aircraft->m_verticalRateValid && (aircraft->m_verticalRate < 0))
        && (aircraft->m_groundspeedValid && (aircraft->m_groundspeed < gearDownSpeed))
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
        // Propellors
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
    else if (aircraft->m_headingValid)
    {
        // Really need to use more data points for this - or better yet, get it from Mode-S frames
        if (aircraft->m_prevHeadingDateTime.isValid())
        {
             qint64 msecs = aircraft->m_prevHeadingDateTime.msecsTo(aircraft->m_headingDateTime);
             if (msecs > 0)
             {
                 float headingDiff = fmod(aircraft->m_heading - aircraft->m_prevHeading + 540.0, 360.0) - 180.0;
                 float roll = headingDiff / (msecs / 1000.0);
                 //qDebug() << "Heading Diff " << headingDiff << " msecs " << msecs << " roll " << roll;
                 roll = std::min(roll, 15.0f);
                 roll = std::max(roll, -15.0f);
                 aircraft->m_rollEst = roll;
             }
        }
        aircraft->m_prevHeadingDateTime = aircraft->m_headingDateTime;
        aircraft->m_prevHeading = aircraft->m_heading;
    }

    return animations;
}

SWGSDRangel::SWGMapAnimation *ADSBDemodGUI::gearAnimation(QDateTime startDateTime, bool up)
{
    // Gear up/down
    SWGSDRangel::SWGMapAnimation *animation = new SWGSDRangel::SWGMapAnimation();
    animation->setName(new QString("libxplanemp/controls/gear_ratio"));
    animation->setStartDateTime(new QString(startDateTime.toString(Qt::ISODateWithMs)));
    animation->setReverse(up);
    animation->setLoop(0);
    animation->setDuration(5);
    animation->setMultiplier(0.2);
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
    animation->setMultiplier(0.2);
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
    animation->setMultiplier(0.2);
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
}

void ADSBDemodGUI::speechNotification(Aircraft *aircraft, const QString &speech)
{
    if (m_speech) {
        m_speech->say(subAircraftString(aircraft, speech));
    } else {
        qDebug() << "ADSBDemodGUI::speechNotification: Unable to say " << speech;
    }
}

void ADSBDemodGUI::commandNotification(Aircraft *aircraft, const QString &command)
{
    QString commandLine = subAircraftString(aircraft, command);
    QStringList allArgs = QProcess::splitCommand(commandLine);

    if (allArgs.size() > 0)
    {
        QString program = allArgs[0];
        allArgs.pop_front();
        QProcess::startDetached(program, allArgs);
    }
}

QString ADSBDemodGUI::subAircraftString(Aircraft *aircraft, const QString &string)
{
    QString s = string;
    s = s.replace("${icao}", aircraft->m_icaoItem->data(Qt::DisplayRole).toString());
    s = s.replace("${callsign}", aircraft->m_callsignItem->data(Qt::DisplayRole).toString());
    s = s.replace("${aircraft}", aircraft->m_modelItem->data(Qt::DisplayRole).toString());
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
    s = s.replace("${turnRate}", aircraft->m_turnRateItem->data(Qt::DisplayRole).toString());
    s = s.replace("${roll}", aircraft->m_rollItem->data(Qt::DisplayRole).toString());
    s = s.replace("${range}", aircraft->m_rangeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${azel}", aircraft->m_azElItem->data(Qt::DisplayRole).toString());
    s = s.replace("${category}", aircraft->m_emitterCategoryItem->data(Qt::DisplayRole).toString());
    s = s.replace("${status}", aircraft->m_statusItem->data(Qt::DisplayRole).toString());
    s = s.replace("${squawk}", aircraft->m_squawkItem->data(Qt::DisplayRole).toString());
    s = s.replace("${registration}", aircraft->m_registrationItem->data(Qt::DisplayRole).toString());
    s = s.replace("${manufacturer}", aircraft->m_manufacturerNameItem->data(Qt::DisplayRole).toString());
    s = s.replace("${owner}", aircraft->m_ownerItem->data(Qt::DisplayRole).toString());
    s = s.replace("${operator}", aircraft->m_operatorICAOItem->data(Qt::DisplayRole).toString());
    s = s.replace("${ap}", aircraft->m_apItem->data(Qt::DisplayRole).toString());
    s = s.replace("${vMode}", aircraft->m_vModeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${lMode}", aircraft->m_lModeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${baro}", aircraft->m_baroItem->data(Qt::DisplayRole).toString());
    s = s.replace("${headwind}", aircraft->m_headwindItem->data(Qt::DisplayRole).toString());
    s = s.replace("${windSpeed}", aircraft->m_windSpeedItem->data(Qt::DisplayRole).toString());
    s = s.replace("${windDirection}", aircraft->m_windDirItem->data(Qt::DisplayRole).toString());
    s = s.replace("${staticPressure}", aircraft->m_staticPressureItem->data(Qt::DisplayRole).toString());
    s = s.replace("${staticAirTemperature}", aircraft->m_staticAirTempItem->data(Qt::DisplayRole).toString());
    s = s.replace("${humidity}", aircraft->m_humidityItem->data(Qt::DisplayRole).toString());
    s = s.replace("${latitude}", aircraft->m_latitudeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${longitude}", aircraft->m_longitudeItem->data(Qt::DisplayRole).toString());
    s = s.replace("${rssi}", aircraft->m_rssiItem->data(Qt::DisplayRole).toString());
    s = s.replace("${flightstatus}", aircraft->m_flightStatusItem->data(Qt::DisplayRole).toString());
    s = s.replace("${departure}", aircraft->m_depItem->data(Qt::DisplayRole).toString());
    s = s.replace("${arrival}", aircraft->m_arrItem->data(Qt::DisplayRole).toString());
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
        if (m_settings.m_displayDemodStats)
        {
            ADSBDemodStats stats = report.getDemodStats();
            ui->stats->setText(QString("ADS-B: %1 Mode-S: %2 Matches: %3 CRC: %4 Type: %5 Avg Corr: %6 Demod Time: %7 Feed Time: %8").arg(stats.m_adsbFrames).arg(stats.m_modesFrames).arg(stats.m_correlatorMatches).arg(stats.m_crcFails).arg(stats.m_typeFails).arg(CalcDb::dbPower(m_correlationAvg.instantAverage()), 1, 'f', 1).arg(stats.m_demodTime, 1, 'f', 3).arg(stats.m_feedTime, 1, 'f', 3));
        }
        return true;
    }
    else if (ADSBDemod::MsgConfigureADSBDemod::match(message))
    {
        qDebug("ADSBDemodGUI::handleMessage: ADSBDemod::MsgConfigureADSBDemod");
        const ADSBDemod::MsgConfigureADSBDemod& cfg = (ADSBDemod::MsgConfigureADSBDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
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
    applySettings();
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
    applySettings();
}

void ADSBDemodGUI::on_rfBW_valueChanged(int value)
{
    Real bw = (Real)value;
    ui->rfBWText->setText(QString("%1M").arg(bw / 1000000.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void ADSBDemodGUI::on_threshold_valueChanged(int value)
{
    Real thresholddB = ((Real)value)/10.0f;
    ui->thresholdText->setText(QString("%1").arg(thresholddB, 0, 'f', 1));
    m_settings.m_correlationThreshold = thresholddB;
    applySettings();
}

void ADSBDemodGUI::on_phaseSteps_valueChanged(int value)
{
    ui->phaseStepsText->setText(QString("%1").arg(value));
    m_settings.m_interpolatorPhaseSteps = value;
    applySettings();
}

void ADSBDemodGUI::on_tapsPerPhase_valueChanged(int value)
{
    Real tapsPerPhase = ((Real)value)/10.0f;
    ui->tapsPerPhaseText->setText(QString("%1").arg(tapsPerPhase, 0, 'f', 1));
    m_settings.m_interpolatorTapsPerPhase = tapsPerPhase;
    applySettings();
}

void ADSBDemodGUI::on_feed_clicked(bool checked)
{
    m_settings.m_feedEnabled = checked;
    // Don't disable host/port - so they can be entered before connecting
    applySettings();
    applyImportSettings();
}

void ADSBDemodGUI::on_notifications_clicked()
{
    ADSBDemodNotificationDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted)
    {
        enableSpeechIfNeeded();
        applySettings();
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

// Find highlighed aircraft on Map Feature
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

// Find aircraft on channel map
void ADSBDemodGUI::findOnChannelMap(Aircraft *aircraft)
{
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
        }
        QString icaoHex = QString("%1").arg(icao, 1, 16);

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

        QAction* adsbExchangeAction = new QAction("View aircraft on adsbexchange.net...", tableContextMenu);
        connect(adsbExchangeAction, &QAction::triggered, this, [icaoHex]()->void {
            QDesktopServices::openUrl(QUrl(QString("https://globe.adsbexchange.com/?icao=%1").arg(icaoHex)));
        });
        tableContextMenu->addAction(adsbExchangeAction);

        QAction* viewOpenSkyAction = new QAction("View aircraft on opensky-network.org...", tableContextMenu);
        connect(viewOpenSkyAction, &QAction::triggered, this, [icaoHex]()->void {
            QDesktopServices::openUrl(QUrl(QString("https://opensky-network.org/aircraft-profile?icao24=%1").arg(icaoHex)));
        });
        tableContextMenu->addAction(viewOpenSkyAction);

        if (!aircraft->m_callsign.isEmpty())
        {
            QAction* flightRadarAction = new QAction("View flight on flightradar24.net...", tableContextMenu);
            connect(flightRadarAction, &QAction::triggered, this, [aircraft]()->void {
                QDesktopServices::openUrl(QUrl(QString("https://www.flightradar24.com/%1").arg(aircraft->m_callsign)));
            });
            tableContextMenu->addAction(flightRadarAction);
        }

        tableContextMenu->addSeparator();

        // Edit aircraft

        if (!aircraft->m_aircraftInfo)
        {
            QAction* addOpenSkyAction = new QAction("Add aircraft to opensky-network.org...", tableContextMenu);
            connect(addOpenSkyAction, &QAction::triggered, this, []()->void {
                QDesktopServices::openUrl(QUrl(QString("https://opensky-network.org/edit-aircraft-profile")));
            });
            tableContextMenu->addAction(addOpenSkyAction);
        }
        else
        {

            QAction* editOpenSkyAction = new QAction("Edit aircraft on opensky-network.org...", tableContextMenu);
            connect(editOpenSkyAction, &QAction::triggered, this, [icaoHex]()->void {
                QDesktopServices::openUrl(QUrl(QString("https://opensky-network.org/edit-aircraft-profile?icao24=%1").arg(icaoHex)));
            });
            tableContextMenu->addAction(editOpenSkyAction);
        }

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
    applySettings();
}

void ADSBDemodGUI::on_correlateFullPreamble_clicked(bool checked)
{
    m_settings.m_correlateFullPreamble = checked;
    applySettings();
}

void ADSBDemodGUI::on_demodModeS_clicked(bool checked)
{
    m_settings.m_demodModeS = checked;
    applySettings();
}

void ADSBDemodGUI::on_getOSNDB_clicked()
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

void ADSBDemodGUI::on_flightPaths_clicked(bool checked)
{
    m_settings.m_flightPaths = checked;
    m_aircraftModel.setFlightPaths(checked);
}

void ADSBDemodGUI::on_allFlightPaths_clicked(bool checked)
{
    m_settings.m_allFlightPaths = checked;
    m_aircraftModel.setAllFlightPaths(checked);
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
    applySettings();
}

void ADSBDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
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

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings();
    }

    resetContextMenuType();
}

void ADSBDemodGUI::updateDeviceSetList()
{
    MainWindow *mainWindow = MainWindow::getInstance();
    std::vector<DeviceUISet*>& deviceUISets = mainWindow->getDeviceUISets();
    std::vector<DeviceUISet*>::const_iterator it = deviceUISets.begin();

    ui->device->blockSignals(true);

    ui->device->clear();
    unsigned int deviceIndex = 0;

    for (; it != deviceUISets.end(); ++it, deviceIndex++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  (*it)->m_deviceSourceEngine;

        if (deviceSourceEngine) {
            ui->device->addItem(QString("R%1").arg(deviceIndex), deviceIndex);
        }
    }

    int newDeviceIndex;

    if (it != deviceUISets.begin())
    {
        if (m_settings.m_deviceIndex < 0) {
            ui->device->setCurrentIndex(0);
        } else if (m_settings.m_deviceIndex >= (int) deviceUISets.size()) {
            ui->device->setCurrentIndex(deviceUISets.size() - 1);
        } else {
            ui->device->setCurrentIndex(m_settings.m_deviceIndex);
        }

        newDeviceIndex = ui->device->currentData().toInt();
    }
    else
    {
        newDeviceIndex = -1;
    }

    if (newDeviceIndex != m_settings.m_deviceIndex)
    {
        qDebug("ADSBDemodGUI::updateDeviceSetLists: device index changed: %d", newDeviceIndex);
        m_settings.m_deviceIndex = newDeviceIndex;
    }

    ui->device->blockSignals(false);
}

void ADSBDemodGUI::on_device_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_deviceIndex = ui->device->currentData().toInt();
        applySettings();
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
                // Look for operator specific livery
                if (!aircraft->m_aircraftInfo->m_operatorICAO.isEmpty()) {
                    model = get3DModel(aircraftType, aircraft->m_aircraftInfo->m_operatorICAO);
                }
                if (model.isEmpty()) {
                    // Try for aircraft with out specific livery
                    model = get3DModel(aircraftType);
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

    // Map from database names to 3D model names
    m_3DModelMatch.append(new ModelMatch("A300.*", "A306")); // A300 B4 is A300-600, but use for others as closest match
    m_3DModelMatch.append(new ModelMatch("A310.*", "A310"));
    m_3DModelMatch.append(new ModelMatch("A318.*", "A318"));
    m_3DModelMatch.append(new ModelMatch("A.?319.*", "A319"));
    m_3DModelMatch.append(new ModelMatch("A.?320.*", "A320"));
    m_3DModelMatch.append(new ModelMatch("A.?321.*", "A321"));
    m_3DModelMatch.append(new ModelMatch("A330.2.*", "A332"));
    m_3DModelMatch.append(new ModelMatch("A330.3.*", "A333"));
    m_3DModelMatch.append(new ModelMatch("A330.4.*", "A342"));
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
            m_aircraftModel.aircraftUpdated(m_highlightAircraft);
        }
        // Highlight this aircraft
        m_highlightAircraft = aircraft;
        if (aircraft)
        {
            aircraft->m_isHighlighted = true;
            m_aircraftModel.aircraftUpdated(aircraft);
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

    if (dialog.exec() == QDialog::Accepted)
    {
        applySettings();
        applyImportSettings();
    }
}

// Show display settings dialog
void ADSBDemodGUI::on_displaySettings_clicked()
{
    bool oldSiUnits = m_settings.m_siUnits;
    ADSBDemodDisplayDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted)
    {
        bool unitsChanged = m_settings.m_siUnits != oldSiUnits;
        if (unitsChanged) {
            m_aircraftModel.allAircraftUpdated();
        }
        displaySettings();
        applySettings();
    }
}

void ADSBDemodGUI::applyMapSettings()
{
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

    // Create the map using the specified provider
    QQmlProperty::write(item, "smoothing", MainCore::instance()->getSettings().getMapSmoothing());
    QQmlProperty::write(item, "aircraftMinZoomLevel", m_settings.m_aircraftMinZoom);
    QQmlProperty::write(item, "mapProvider", m_settings.m_mapProvider);
    QVariantMap parameters;
    QString mapType;

    if (m_settings.m_mapProvider == "osm")
    {
        // Use our repo, so we can append API key and redefine transmit maps
        parameters["osm.mapping.providersrepository.address"] = QString("http://127.0.0.1:%1/").arg(m_osmPort);
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
    else if (m_settings.m_mapProvider == "mapboxgl")
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
        ui->map->setFormat(format);
    }

    m_osmPort = 0; // Pick a free port
    m_templateServer = new ADSBOSMTemplateServer("q2RVNAe3eFKCH4XsrE3r", m_osmPort);

    ui->map->setAttribute(Qt::WA_AcceptTouchEvents, true);

    ui->map->rootContext()->setContextProperty("aircraftModel", &m_aircraftModel);
    ui->map->rootContext()->setContextProperty("airportModel", &m_airportModel);
    ui->map->rootContext()->setContextProperty("airspaceModel", &m_airspaceModel);
    ui->map->rootContext()->setContextProperty("navAidModel", &m_navAidModel);
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map.qml")));

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_adsbDemod = reinterpret_cast<ADSBDemod*>(rxChannel); //new ADSBDemod(m_deviceUISet->m_deviceSourceAPI);
    m_adsbDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *feedRightClickEnabler = new CRightClickEnabler(ui->feed);
    connect(feedRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(feedSelect(const QPoint &)));

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

    // Read aircraft information database, if it has previously been downloaded
    AircraftInformation::init();
    connect(&m_osnDB, &OsnDB::downloadingURL, this, &ADSBDemodGUI::downloadingURL);
    connect(&m_osnDB, &OsnDB::downloadError, this, &ADSBDemodGUI::downloadError);
    connect(&m_osnDB, &OsnDB::downloadProgress, this, &ADSBDemodGUI::downloadProgress);
    connect(&m_osnDB, &OsnDB::downloadAircraftInformationFinished, this, &ADSBDemodGUI::downloadAircraftInformationFinished);
    m_aircraftInfo = OsnDB::getAircraftInformation();

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
    if ((stationLatitude == 49.012423) && (stationLongitude == 8.418125)) {
        ui->warning->setText("Please set your antenna location under Preferences > My Position");
    }

    // Get updated when position changes
    connect(&MainCore::instance()->getSettings(), &MainSettings::preferenceChanged, this, &ADSBDemodGUI::preferenceChanged);

    // Get airport weather when requested
    connect(&m_airportModel, &AirportModel::requestMetar, this, &ADSBDemodGUI::requestMetar);

    // Add airports within range of My Position
    updateAirports();
    updateAirspaces();
    updateNavAids();
    update3DModels();

    m_flightInformation = nullptr;
    m_aviationWeather = nullptr;

    connect(&m_planeSpotters, &PlaneSpotters::aircraftPhoto, this, &ADSBDemodGUI::aircraftPhoto);
    connect(ui->photo, &ClickableLabel::clicked, this, &ADSBDemodGUI::photoClicked);

    // Update device list when devices are added or removed
    connect(MainCore::instance(), &MainCore::deviceSetAdded, this, &ADSBDemodGUI::updateDeviceSetList);
    connect(MainCore::instance(), &MainCore::deviceSetRemoved, this, &ADSBDemodGUI::updateDeviceSetList);

    updateDeviceSetList();
    displaySettings();
    makeUIConnections();
    applySettings(true);

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
    ui->map->installEventFilter(this);
    DialPopup::addPopupsToChildDials(this);
}

ADSBDemodGUI::~ADSBDemodGUI()
{
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

void ADSBDemodGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        qDebug() << "ADSBDemodGUI::applySettings";

        ADSBDemod::MsgConfigureADSBDemod* message = ADSBDemod::MsgConfigureADSBDemod::create(m_settings, force);
        m_adsbDemod->getInputMessageQueue()->push(message);
    }
}

void ADSBDemodGUI::displaySettings()
{
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
    ui->correlateFullPreamble->setChecked(m_settings.m_correlateFullPreamble);
    ui->demodModeS->setChecked(m_settings.m_demodModeS);

    ui->thresholdText->setText(QString("%1").arg(m_settings.m_correlationThreshold, 0, 'f', 1));
    ui->threshold->setValue((int)(m_settings.m_correlationThreshold*10.0f));

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
    m_aircraftModel.setFlightPaths(m_settings.m_flightPaths);
    ui->allFlightPaths->setChecked(m_settings.m_allFlightPaths);
    m_aircraftModel.setAllFlightPaths(m_settings.m_allFlightPaths);

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    updateIndexLabel();

    QFont font(m_settings.m_tableFontName, m_settings.m_tableFontSize);
    ui->adsbData->setFont(font);

    // Set units in column headers
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

    // Order and size columns
    QHeaderView *header = ui->adsbData->horizontalHeader();
    for (int i = 0; i < ADSBDEMOD_COLUMNS; i++)
    {
        bool hidden = m_settings.m_columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        menu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_columnSizes[i] > 0)
            ui->adsbData->setColumnWidth(i, m_settings.m_columnSizes[i]);
        header->moveSection(header->visualIndex(i), m_settings.m_columnIndexes[i]);
    }

    // Only update airports on map if settings have changed
    if ((m_airportInfo != nullptr)
        && ((m_settings.m_airportRange != m_currentAirportRange)
            || (m_settings.m_airportMinimumSize != m_currentAirportMinimumSize)
            || (m_settings.m_displayHeliports != m_currentDisplayHeliports)))
        updateAirports();

    updateAirspaces();
    updateNavAids();

    if (!m_settings.m_displayDemodStats)
        ui->stats->setText("");

    initFlightInformation();
    initAviationWeather();

    applyMapSettings();
    applyImportSettings();

    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
    enableSpeechIfNeeded();
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

    // Tick is called 20x a second - lets check this every 10 seconds
    if (m_tickCount % (20*10) == 0)
    {
        // Remove aircraft that haven't been heard of for a user-defined time, as probably out of range
        QDateTime now = QDateTime::currentDateTime();
        qint64 nowSecs = now.toSecsSinceEpoch();
        QHash<int, Aircraft *>::iterator i = m_aircraft.begin();

        while (i != m_aircraft.end())
        {
            Aircraft *aircraft = i.value();
            qint64 secondsSinceLastFrame = nowSecs - aircraft->m_time.toSecsSinceEpoch();

            if (secondsSinceLastFrame >= m_settings.m_removeTimeout)
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
                clearFromMap(QString("%1").arg(aircraft->m_icao, 0, 16));

                // And finally free its memory
                delete aircraft;
            }
            else
            {
                ++i;
            }
        }
    }
}

void ADSBDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    int row = ui->adsbData->rowCount();
    ui->adsbData->setRowCount(row + 1);
    ui->adsbData->setItem(row, ADSB_COL_ICAO, new QTableWidgetItem("ICAO ID"));
    ui->adsbData->setItem(row, ADSB_COL_CALLSIGN, new QTableWidgetItem("Callsign-"));
    ui->adsbData->setItem(row, ADSB_COL_MODEL, new QTableWidgetItem("Aircraft12345"));
    ui->adsbData->setItem(row, ADSB_COL_AIRLINE, new QTableWidgetItem("airbrigdecargo1"));
    ui->adsbData->setItem(row, ADSB_COL_ALTITUDE, new QTableWidgetItem("Alt (ft)"));
    ui->adsbData->setItem(row, ADSB_COL_HEADING, new QTableWidgetItem("Hd (o)"));
    ui->adsbData->setItem(row, ADSB_COL_VERTICALRATE, new QTableWidgetItem("VR (ft/m)"));
    ui->adsbData->setItem(row, ADSB_COL_RANGE, new QTableWidgetItem("D (km)"));
    ui->adsbData->setItem(row, ADSB_COL_AZEL, new QTableWidgetItem("Az/El (o)"));
    ui->adsbData->setItem(row, ADSB_COL_LATITUDE, new QTableWidgetItem("-90.00000"));
    ui->adsbData->setItem(row, ADSB_COL_LONGITUDE, new QTableWidgetItem("-180.000000"));
    ui->adsbData->setItem(row, ADSB_COL_CATEGORY, new QTableWidgetItem("Heavy"));
    ui->adsbData->setItem(row, ADSB_COL_STATUS, new QTableWidgetItem("No emergency"));
    ui->adsbData->setItem(row, ADSB_COL_SQUAWK, new QTableWidgetItem("Squawk"));
    ui->adsbData->setItem(row, ADSB_COL_REGISTRATION, new QTableWidgetItem("G-12345"));
    ui->adsbData->setItem(row, ADSB_COL_COUNTRY, new QTableWidgetItem("Country"));
    ui->adsbData->setItem(row, ADSB_COL_REGISTERED, new QTableWidgetItem("Registered"));
    ui->adsbData->setItem(row, ADSB_COL_MANUFACTURER, new QTableWidgetItem("The Boeing Company"));
    ui->adsbData->setItem(row, ADSB_COL_OWNER, new QTableWidgetItem("British Airways"));
    ui->adsbData->setItem(row, ADSB_COL_OPERATOR_ICAO, new QTableWidgetItem("Operator"));
    ui->adsbData->setItem(row, ADSB_COL_TIME, new QTableWidgetItem("99:99:99"));
    ui->adsbData->setItem(row, ADSB_COL_FRAMECOUNT, new QTableWidgetItem("Frames"));
    ui->adsbData->setItem(row, ADSB_COL_CORRELATION, new QTableWidgetItem("0.001/0.001/0.001"));
    ui->adsbData->setItem(row, ADSB_COL_RSSI, new QTableWidgetItem("-100.0"));
    ui->adsbData->setItem(row, ADSB_COL_FLIGHT_STATUS, new QTableWidgetItem("scheduled"));
    ui->adsbData->setItem(row, ADSB_COL_DEP, new QTableWidgetItem("WWWW"));
    ui->adsbData->setItem(row, ADSB_COL_ARR, new QTableWidgetItem("WWWW"));
    ui->adsbData->setItem(row, ADSB_COL_STD, new QTableWidgetItem("12:00 -1"));
    ui->adsbData->setItem(row, ADSB_COL_ETD, new QTableWidgetItem("12:00 -1"));
    ui->adsbData->setItem(row, ADSB_COL_ATD, new QTableWidgetItem("12:00 -1"));
    ui->adsbData->setItem(row, ADSB_COL_STA, new QTableWidgetItem("12:00 +1"));
    ui->adsbData->setItem(row, ADSB_COL_ETA, new QTableWidgetItem("12:00 +1"));
    ui->adsbData->setItem(row, ADSB_COL_ATA, new QTableWidgetItem("12:00 +1"));
    ui->adsbData->setItem(row, ADSB_COL_SEL_ALTITUDE, new QTableWidgetItem("Sel Alt (ft)"));
    ui->adsbData->setItem(row, ADSB_COL_SEL_HEADING, new QTableWidgetItem("Sel Hd (o)"));
    ui->adsbData->setItem(row, ADSB_COL_BARO, new QTableWidgetItem("Baro (mb)"));
    ui->adsbData->setItem(row, ADSB_COL_AP, new QTableWidgetItem("AP"));
    ui->adsbData->setItem(row, ADSB_COL_V_MODE, new QTableWidgetItem("V Mode"));
    ui->adsbData->setItem(row, ADSB_COL_L_MODE, new QTableWidgetItem("L Mode"));
    ui->adsbData->setItem(row, ADSB_COL_TRUE_AIRSPEED, new QTableWidgetItem("TAS (kn)"));
    ui->adsbData->setItem(row, ADSB_COL_INDICATED_AIRSPEED, new QTableWidgetItem("IAS (kn)"));
    ui->adsbData->setItem(row, ADSB_COL_MACH, new QTableWidgetItem("0.999"));
    ui->adsbData->setItem(row, ADSB_COL_HEADWIND, new QTableWidgetItem("H Wnd (kn)"));
    ui->adsbData->setItem(row, ADSB_COL_EST_AIR_TEMP, new QTableWidgetItem("OAT (C)"));
    ui->adsbData->setItem(row, ADSB_COL_WIND_SPEED, new QTableWidgetItem("Wnd (kn)"));
    ui->adsbData->setItem(row, ADSB_COL_WIND_DIR, new QTableWidgetItem("Wnd (o)"));
    ui->adsbData->setItem(row, ADSB_COL_STATIC_PRESSURE, new QTableWidgetItem("P (hPa)"));
    ui->adsbData->setItem(row, ADSB_COL_STATIC_AIR_TEMP, new QTableWidgetItem("T (C)"));
    ui->adsbData->setItem(row, ADSB_COL_HUMIDITY, new QTableWidgetItem("U (%)"));
    ui->adsbData->setItem(row, ADSB_COL_TIS_B, new QTableWidgetItem("TIS-B"));
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
    applySettings();
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
            applySettings();
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
                            QDateTime dateTime = QDateTime::currentDateTime(); // So they aren't removed immediately as too old
                            QByteArray bytes = QByteArray::fromHex(cols[dataCol].toLatin1());
                            float correlation = cols[correlationCol].toFloat();
                            int df = (bytes[0] >> 3) & ADS_B_DF_MASK; // Downlink format
                            if ((df == 4) || (df == 5) || (df == 17) || (df == 18) || (df == 20) || (df == 21))
                            {
                                int crcCalc = 0;
                                if ((df == 4) || (df == 5) || (df == 20) || (df == 21))  // handleADSB requires calculated CRC for Mode-S frames
                                {
                                    crc.init();
                                    crc.calculate((const uint8_t *)bytes.data(), bytes.size()-3);
                                    crcCalc = crc.get();
                                }
                                //qDebug() << "bytes.szie " << bytes.size() << " crc " << Qt::hex <<  crcCalc;
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
    if (!m_settings.m_importUsername.isEmpty() && !m_settings.m_importPassword.isEmpty()) {
        urlString = urlString + m_settings.m_importUsername + ":" + m_settings.m_importPassword + "@";
    }
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
    m_networkManager->get(QNetworkRequest(QUrl(urlString)));
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
                        if (!callsign.isEmpty())
                        {
                            aircraft->m_callsign = callsign;
                            aircraft->m_callsignItem->setText(aircraft->m_callsign);
                        }

                        QDateTime timePosition = dateTime;
                        if (state[3].isNull()) {
                            timePosition = dateTime.addSecs(-15); // At least 15 seconds old
                        } else {
                            timePosition = QDateTime::fromSecsSinceEpoch(state[3].toInt());
                        }
                        aircraft->m_time = QDateTime::fromSecsSinceEpoch(state[4].toInt());
                        QTime time = aircraft->m_time.time();
                        aircraft->m_timeItem->setText(QString("%1:%2:%3").arg(time.hour(), 2, 10, QLatin1Char('0')).arg(time.minute(), 2, 10, QLatin1Char('0')).arg(time.second(), 2, 10, QLatin1Char('0')));
                        aircraft->m_adsbFrameCount++;
                        aircraft->m_adsbFrameCountItem->setData(Qt::DisplayRole, aircraft->m_adsbFrameCount);

                        if (timePosition > aircraft->m_positionDateTime)
                        {
                            if (!state[5].isNull() && !state[6].isNull())
                            {
                                aircraft->m_longitude = state[5].toDouble();
                                aircraft->m_latitude = state[6].toDouble();
                                aircraft->m_longitudeItem->setData(Qt::DisplayRole, aircraft->m_longitude);
                                aircraft->m_latitudeItem->setData(Qt::DisplayRole, aircraft->m_latitude);
                                updatePosition(aircraft);
                                aircraft->m_cprValid[0] = false;
                                aircraft->m_cprValid[1] = false;
                            }
                            if (!state[7].isNull())
                            {
                                aircraft->m_altitude = (int)Units::metresToFeet(state[7].toDouble());
                                aircraft->m_altitudeValid = true;
                                aircraft->m_altitudeGNSS = false;
                                aircraft->m_altitudeItem->setData(Qt::DisplayRole, aircraft->m_altitude);
                            }
                            if (!state[5].isNull() && !state[6].isNull() && !state[7].isNull())
                            {
                                QGeoCoordinate coord(aircraft->m_latitude, aircraft->m_longitude, aircraft->m_altitude);
                                aircraft->m_coordinates.push_back(QVariant::fromValue(coord));
                            }
                            aircraft->m_positionDateTime = timePosition;
                        }
                        aircraft->m_onSurface = state[8].toBool(false);
                        if (!state[9].isNull())
                        {
                            aircraft->m_groundspeed = (int)state[9].toDouble();
                            aircraft->m_groundspeedItem->setData(Qt::DisplayRole, aircraft->m_groundspeed);
                            aircraft->m_groundspeedValid = true;
                        }
                        if (!state[10].isNull())
                        {
                            aircraft->m_heading = (float)state[10].toDouble();
                            aircraft->m_headingItem->setData(Qt::DisplayRole, std::round(aircraft->m_heading));
                            aircraft->m_headingValid = true;
                            aircraft->m_headingDateTime = aircraft->m_time;
                        }
                        if (!state[11].isNull())
                        {
                            aircraft->m_verticalRate = (int)state[10].toDouble();
                            aircraft->m_verticalRateItem->setData(Qt::DisplayRole, aircraft->m_verticalRate);
                            aircraft->m_verticalRateValid = true;
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

void ADSBDemodGUI::preferenceChanged(int elementType)
{
    Preferences::ElementType pref = (Preferences::ElementType)elementType;
    if ((pref == Preferences::Latitude) || (pref == Preferences::Longitude) || (pref == Preferences::Altitude))
    {
        Real stationLatitude = MainCore::instance()->getSettings().getLatitude();
        Real stationLongitude = MainCore::instance()->getSettings().getLongitude();
        Real stationAltitude = MainCore::instance()->getSettings().getAltitude();

        QGeoCoordinate stationPosition(stationLatitude, stationLongitude, stationAltitude);
        QGeoCoordinate previousPosition(m_azEl.getLocationSpherical().m_latitude, m_azEl.getLocationSpherical().m_longitude, m_azEl.getLocationSpherical().m_altitude);

        if (stationPosition != previousPosition)
        {
            m_azEl.setLocation(stationLatitude, stationLongitude, stationAltitude);

            // Update distances and what is visible, but only do it if position has changed significantly
            if (!m_lastFullUpdatePosition.isValid() || (stationPosition.distanceTo(m_lastFullUpdatePosition) >= 1000))
            {
                updateAirports();
                updateAirspaces();
                updateNavAids();
                m_lastFullUpdatePosition = stationPosition;
            }

            // Update icon position on Map
            QQuickItem *item = ui->map->rootObject();
            QObject *map = item->findChild<QObject*>("map");
            if (map != nullptr)
            {
                QObject *stationObject = map->findChild<QObject*>("station");
                if(stationObject != NULL)
                {
                    QGeoCoordinate coords = stationObject->property("coordinate").value<QGeoCoordinate>();
                    coords.setLatitude(stationLatitude);
                    coords.setLongitude(stationLongitude);
                    coords.setAltitude(stationAltitude);
                    stationObject->setProperty("coordinate", QVariant::fromValue(coords));
                }
            }
        }
    }
    else if (pref == Preferences::StationName)
    {
        // Update icon label on Map
        QQuickItem *item = ui->map->rootObject();
        QObject *map = item->findChild<QObject*>("map");
        if (map != nullptr)
        {
            QObject *stationObject = map->findChild<QObject*>("station");
            if(stationObject != NULL) {
                stationObject->setProperty("stationName", QVariant::fromValue(MainCore::instance()->getSettings().getStationName()));
            }
        }
    }
    else if (pref == Preferences::MapSmoothing)
    {
        QQuickItem *item = ui->map->rootObject();
        QQmlProperty::write(item, "smoothing", MainCore::instance()->getSettings().getMapSmoothing());
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

void ADSBDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &ADSBDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &ADSBDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->threshold, &QDial::valueChanged, this, &ADSBDemodGUI::on_threshold_valueChanged);
    QObject::connect(ui->phaseSteps, &QDial::valueChanged, this, &ADSBDemodGUI::on_phaseSteps_valueChanged);
    QObject::connect(ui->tapsPerPhase, &QDial::valueChanged, this, &ADSBDemodGUI::on_tapsPerPhase_valueChanged);
    QObject::connect(ui->adsbData, &QTableWidget::cellClicked, this, &ADSBDemodGUI::on_adsbData_cellClicked);
    QObject::connect(ui->adsbData, &QTableWidget::cellDoubleClicked, this, &ADSBDemodGUI::on_adsbData_cellDoubleClicked);
    QObject::connect(ui->spb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ADSBDemodGUI::on_spb_currentIndexChanged);
    QObject::connect(ui->correlateFullPreamble, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_correlateFullPreamble_clicked);
    QObject::connect(ui->demodModeS, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_demodModeS_clicked);
    QObject::connect(ui->feed, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_feed_clicked);
    QObject::connect(ui->notifications, &QToolButton::clicked, this, &ADSBDemodGUI::on_notifications_clicked);
    QObject::connect(ui->flightInfo, &QToolButton::clicked, this, &ADSBDemodGUI::on_flightInfo_clicked);
    QObject::connect(ui->findOnMapFeature, &QToolButton::clicked, this, &ADSBDemodGUI::on_findOnMapFeature_clicked);
    QObject::connect(ui->getOSNDB, &QToolButton::clicked, this, &ADSBDemodGUI::on_getOSNDB_clicked);
    QObject::connect(ui->getAirportDB, &QToolButton::clicked, this, &ADSBDemodGUI::on_getAirportDB_clicked);
    QObject::connect(ui->getAirspacesDB, &QToolButton::clicked, this, &ADSBDemodGUI::on_getAirspacesDB_clicked);
    QObject::connect(ui->flightPaths, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_flightPaths_clicked);
    QObject::connect(ui->allFlightPaths, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_allFlightPaths_clicked);
    QObject::connect(ui->device, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ADSBDemodGUI::on_device_currentIndexChanged);
    QObject::connect(ui->displaySettings, &QToolButton::clicked, this, &ADSBDemodGUI::on_displaySettings_clicked);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &ADSBDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &ADSBDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &ADSBDemodGUI::on_logOpen_clicked);
}

void ADSBDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

