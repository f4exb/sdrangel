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
#include <QDebug>

#include "ui_adsbdemodgui.h"
#include "channel/channelwebapiutils.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/crightclickenabler.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

#include "adsbdemodreport.h"
#include "adsbdemod.h"
#include "adsbdemodgui.h"
#include "adsbdemodfeeddialog.h"
#include "adsbdemoddisplaydialog.h"
#include "adsb.h"

// ADS-B table columns
#define ADSB_COL_ICAO           0
#define ADSB_COL_FLIGHT         1
#define ADSB_COL_MODEL          2
#define ADSB_COL_AIRLINE        3
#define ADSB_COL_ALTITUDE       4
#define ADSB_COL_SPEED          5
#define ADSB_COL_HEADING        6
#define ADSB_COL_VERTICALRATE   7
#define ADSB_COL_RANGE          8
#define ADSB_COL_AZEL           9
#define ADSB_COL_LATITUDE       10
#define ADSB_COL_LONGITUDE      11
#define ADSB_COL_CATEGORY       12
#define ADSB_COL_STATUS         13
#define ADSB_COL_REGISTRATION   14
#define ADSB_COL_COUNTRY        15
#define ADSB_COL_REGISTERED     16
#define ADSB_COL_MANUFACTURER   17
#define ADSB_COL_OWNER          18
#define ADSB_COL_OPERATOR_ICAO  19
#define ADSB_COL_TIME           20
#define ADSB_COL_FRAMECOUNT     21
#define ADSB_COL_CORRELATION    22
#define ADSB_COL_RSSI           23

const char *Aircraft::m_speedTypeNames[] = {
    "GS", "TAS", "IAS"
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
    if(m_settings.deserialize(data)) {
        updateDeviceSetList();
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

// Longitude zone (returns value in range [1,59]
static int cprNL(double lat)
{
    if (lat == 0.0)
        return 59;
    else if ((lat == 87.0) || (lat == -87.0))
        return 2;
    else if ((lat > 87.0) || (lat < -87.0))
        return 1;
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
    if (nl > 1)
        return nl;
    else
        return 1;
}

static Real feetToMetres(Real feet)
{
    return feet * 0.3048f;
}

static int feetToMetresInt(Real feet)
{
    return (int)std::round(feetToMetres(feet));
}

static Real knotsToKPH(Real knots)
{
    return knots * 1.852f;
}

static int knotsToKPHInt(Real knots)
{
    return (int)std::round(knotsToKPH(knots));
}

static Real feetPerMinToMetresPerSecond(Real fpm)
{
    return fpm * 0.00508f;
}

static int feetPerMinToMetresPerSecondInt(Real fpm)
{
    return (int)std::round(feetPerMinToMetresPerSecond(fpm));
}

// Can't use std::fmod, as that works differently for negative numbers (See C.2.6.2)
static Real modulus(double x, double y)
{
    return x - y * std::floor(x/y);
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
        coords.setAltitude(feetToMetres(m_aircrafts[row]->m_altitude));
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
        QStringList list;
        if (m_aircrafts[row]->m_flight.length() > 0)
        {
            list.append(QString("Flight: %1").arg(m_aircrafts[row]->m_flight));
        }
        else
        {
            list.append(QString("ICAO: %1").arg(m_aircrafts[row]->m_icao, 1, 16));
        }
        if (m_aircrafts[row]->m_showAll || m_aircrafts[row]->m_isHighlighted)
        {
            if (m_aircrafts[row]->m_aircraftInfo != nullptr)
            {
                if (m_aircrafts[row]->m_aircraftInfo->m_model.size() > 0)
                {
                    list.append(QString("Aircraft: %1").arg(m_aircrafts[row]->m_aircraftInfo->m_model));
                }
            }
            if (m_aircrafts[row]->m_altitudeValid)
            {
                if (m_aircrafts[row]->m_gui->useSIUints())
                    list.append(QString("Altitude: %1 (m)").arg(feetToMetresInt(m_aircrafts[row]->m_altitude)));
                else
                    list.append(QString("Altitude: %1 (ft)").arg(m_aircrafts[row]->m_altitude));
            }
            if (m_aircrafts[row]->m_speedValid)
            {
                if (m_aircrafts[row]->m_gui->useSIUints())
                    list.append(QString("%1: %2 (kph)").arg(m_aircrafts[row]->m_speedTypeNames[m_aircrafts[row]->m_speedType]).arg(knotsToKPHInt(m_aircrafts[row]->m_speed)));
                else
                    list.append(QString("%1: %2 (kn)").arg(m_aircrafts[row]->m_speedTypeNames[m_aircrafts[row]->m_speedType]).arg(m_aircrafts[row]->m_speed));
            }
            if (m_aircrafts[row]->m_verticalRateValid)
            {
                QString desc;
                Real rate;
                QString units;

                if (m_aircrafts[row]->m_gui->useSIUints())
                {
                    rate = feetPerMinToMetresPerSecondInt(m_aircrafts[row]->m_verticalRate);
                    units = QString("m/s");
                }
                else
                {
                    rate = m_aircrafts[row]->m_verticalRate;
                    units = QString("ft/min");
                }
                if (m_aircrafts[row]->m_verticalRate == 0)
                    desc = "Level flight";
                else if (rate > 0)
                    desc = QString("Climbing: %1 (%2)").arg(rate).arg(units);
                else
                    desc = QString("Descending: %1 (%2)").arg(rate).arg(units);
                list.append(QString(desc));
            }
            if  ((m_aircrafts[row]->m_status.length() > 0) && m_aircrafts[row]->m_status.compare("No emergency"))
            {
                list.append(m_aircrafts[row]->m_status);
            }
        }
        QString data = list.join("\n");
        return QVariant::fromValue(data);
    }
    else if (role == AircraftModel::aircraftImageRole)
    {
        // Select an image to use for the aircraft
        if (m_aircrafts[row]->m_emitterCategory.length() > 0)
        {
            if (!m_aircrafts[row]->m_emitterCategory.compare("Heavy"))
                return QVariant::fromValue(QString("aircraft_4engine.png"));
            else if (!m_aircrafts[row]->m_emitterCategory.compare("Large"))
                return QVariant::fromValue(QString("aircraft_2engine.png"));
            else if (!m_aircrafts[row]->m_emitterCategory.compare("Small"))
                return QVariant::fromValue(QString("aircraft_2enginesmall.png"));
            else if (!m_aircrafts[row]->m_emitterCategory.compare("Rotorcraft"))
                return QVariant::fromValue(QString("aircraft_helicopter.png"));
            else if (!m_aircrafts[row]->m_emitterCategory.compare("High performance"))
                return QVariant::fromValue(QString("aircraft_fighter.png"));
            else if (!m_aircrafts[row]->m_emitterCategory.compare("Light")
                    || !m_aircrafts[row]->m_emitterCategory.compare("Ultralight")
                    || !m_aircrafts[row]->m_emitterCategory.compare("Glider/sailplane"))
                return QVariant::fromValue(QString("aircraft_light.png"));
            else if (!m_aircrafts[row]->m_emitterCategory.compare("Space vehicle"))
                return QVariant::fromValue(QString("aircraft_space.png"));
            else if (!m_aircrafts[row]->m_emitterCategory.compare("UAV"))
                return QVariant::fromValue(QString("aircraft_drone.png"));
            else if (!m_aircrafts[row]->m_emitterCategory.compare("Emergency vehicle")
                    || !m_aircrafts[row]->m_emitterCategory.compare("Service vehicle"))
                return QVariant::fromValue(QString("truck.png"));
            else
                return QVariant::fromValue(QString("aircraft_2engine.png"));
        }
        else
            return QVariant::fromValue(QString("aircraft_2engine.png"));
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
       if (m_flightPaths)
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
        coords.setAltitude(feetToMetres(m_airports[row]->m_elevation));
        return QVariant::fromValue(coords);
    }
    else if (role == AirportModel::airportDataRole)
    {
        if (m_showFreq[row])
            return QVariant::fromValue(m_airportDataFreq[row]);
        else
            return QVariant::fromValue(m_airports[row]->m_ident);
    }
    else if (role == AirportModel::airportDataRowsRole)
    {
        if (m_showFreq[row])
            return QVariant::fromValue(m_airportDataFreqRows[row]);
        else
            return 1;
    }
    else if (role == AirportModel::airportImageRole)
    {
        // Select an image to use for the airport
        if (m_airports[row]->m_type == ADSBDemodSettings::AirportType::Large)
            return QVariant::fromValue(QString("airport_large.png"));
        else if (m_airports[row]->m_type == ADSBDemodSettings::AirportType::Medium)
            return QVariant::fromValue(QString("airport_medium.png"));
        else if (m_airports[row]->m_type == ADSBDemodSettings::AirportType::Heliport)
            return QVariant::fromValue(QString("heliport.png"));
        else
            return QVariant::fromValue(QString("airport_small.png"));
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
        }
        return true;
    }
    else if (role == AirportModel::selectedFreqRole)
    {
        int idx = value.toInt();
        if ((idx >= 0) && (idx < m_airports[row]->m_frequencies.size()))
            m_gui->setFrequency(m_airports[row]->m_frequencies[idx]->m_frequency * 1000000);
        else
            qDebug() << "AirportModel::setData unexpected idx " << idx << " frequencies.size() " << m_airports[row]->m_frequencies.size();
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
    m_azEl.setTarget(aircraft->m_latitude, aircraft->m_longitude, feetToMetres(aircraft->m_altitude));
    m_azEl.calculate();
    aircraft->m_range = m_azEl.getDistance();
    aircraft->m_azimuth = m_azEl.getAzimuth();
    aircraft->m_elevation = m_azEl.getElevation();
    aircraft->m_rangeItem->setText(QString::number(aircraft->m_range/1000.0, 'f', 1));
    aircraft->m_azElItem->setText(QString("%1/%2").arg(std::round(aircraft->m_azimuth)).arg(std::round(aircraft->m_elevation)));
    if (aircraft == m_trackAircraft)
        m_adsbDemod->setTarget(aircraft->m_azimuth, aircraft->m_elevation);
}

// Try to find an airline logo based on ICAO
QIcon *ADSBDemodGUI::getAirlineIcon(const QString &operatorICAO)
{
    if (m_airlineIcons.contains(operatorICAO))
        return m_airlineIcons.value(operatorICAO);
    else
    {
        QIcon *icon = nullptr;
        QString endPath = QString("/airlinelogos/%1.bmp").arg(operatorICAO);
        // Try in user directory first, so they can customise
        QString userIconPath = getDataDir() + endPath;
        QFile file(userIconPath);
        if (file.exists())
        {
            icon = new QIcon(userIconPath);
            m_airlineIcons.insert(operatorICAO, icon);
        }
        else
        {
            // Try in resources
            QResource resourceIconPath(QString(":%1").arg(endPath));
            if (resourceIconPath.isValid())
            {
                icon = new QIcon(":" + endPath);
                m_airlineIcons.insert(operatorICAO, icon);
            }
        }
        return icon;
    }
}

// Try to find an flag logo based on a country
QIcon *ADSBDemodGUI::getFlagIcon(const QString &country)
{
    if (m_flagIcons.contains(country))
        return m_flagIcons.value(country);
    else
    {
        QIcon *icon = nullptr;
        QString endPath = QString("/flags/%1.bmp").arg(country);
        // Try in user directory first, so they can customise
        QString userIconPath = getDataDir() + endPath;
        QFile file(userIconPath);
        if (file.exists())
        {
            icon = new QIcon(userIconPath);
            m_flagIcons.insert(country, icon);
        }
        else
        {
            // Try in resources
            QResource resourceIconPath(QString(":%1").arg(endPath));
            if (resourceIconPath.isValid())
            {
                icon = new QIcon(":" + endPath);
                m_flagIcons.insert(country, icon);
            }
        }
        return icon;
    }
}

void ADSBDemodGUI::handleADSB(
    const QByteArray data,
    const QDateTime dateTime,
    float correlation,
    float correlationOnes)
{
    const char idMap[] = "?ABCDEFGHIJKLMNOPQRSTUVWXYZ????? ???????????????0123456789??????";
    const QString categorySetA[] = {
        "None",
        "Light",
        "Small",
        "Large",
        "High vortex",
        "Heavy",
        "High performance",
        "Rotorcraft"
    };
    const QString categorySetB[] = {
        "None",
        "Glider/sailplane",
        "Lighter-than-air",
        "Parachutist",
        "Ultralight",
        "Reserved",
        "UAV",
        "Space vehicle"
    };
    const QString categorySetC[] = {
        "None",
        "Emergency vehicle",
        "Service vehicle",
        "Ground obstruction",
        "Cluster obstacle",
        "Line obstacle",
        "Reserved",
        "Reserved"
    };
    const QString emergencyStatus[] = {
        "No emergency",
        "General emergency",
        "Lifeguard/Medical",
        "Minimum fuel",
        "No communications",
        "Unlawful interference",
        "Downed aircraft",
        "Reserved"
    };

    int df = (data[0] >> 3) & ADS_B_DF_MASK; // Downlink format
    int ca = data[0] & 0x7; // Capability
    unsigned icao = ((data[1] & 0xff) << 16) | ((data[2] & 0xff) << 8) | (data[3] & 0xff); // ICAO aircraft address
    int tc = (data[4] >> 3) & 0x1f; // Type code

    Aircraft *aircraft;
    if (m_aircraft.contains(icao))
    {
        // Update existing aircraft info
        aircraft = m_aircraft.value(icao);
    }
    else
    {
        // Add new aircraft
        aircraft = new Aircraft(this);
        aircraft->m_icao = icao;
        m_aircraft.insert(icao, aircraft);
        // Check for TIS-B addresses
        if ((df == 18) && !((df == 18) && ((ca == 0) || (ca == 1) || (ca == 6))))
            aircraft->m_icaoItem->setText(QString("TIS-B %1").arg(aircraft->m_icao, 1, 16));
        else
            aircraft->m_icaoItem->setText(QString("%1").arg(aircraft->m_icao, 1, 16));
        ui->adsbData->setSortingEnabled(false);
        int row = ui->adsbData->rowCount();
        ui->adsbData->setRowCount(row + 1);
        ui->adsbData->setItem(row, ADSB_COL_ICAO, aircraft->m_icaoItem);
        ui->adsbData->setItem(row, ADSB_COL_FLIGHT, aircraft->m_flightItem);
        ui->adsbData->setItem(row, ADSB_COL_MODEL, aircraft->m_modelItem);
        ui->adsbData->setItem(row, ADSB_COL_AIRLINE, aircraft->m_airlineItem);
        ui->adsbData->setItem(row, ADSB_COL_ALTITUDE, aircraft->m_altitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_SPEED, aircraft->m_speedItem);
        ui->adsbData->setItem(row, ADSB_COL_HEADING, aircraft->m_headingItem);
        ui->adsbData->setItem(row, ADSB_COL_VERTICALRATE, aircraft->m_verticalRateItem);
        ui->adsbData->setItem(row, ADSB_COL_RANGE, aircraft->m_rangeItem);
        ui->adsbData->setItem(row, ADSB_COL_AZEL, aircraft->m_azElItem);
        ui->adsbData->setItem(row, ADSB_COL_LATITUDE, aircraft->m_latitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_LONGITUDE, aircraft->m_longitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_CATEGORY, aircraft->m_emitterCategoryItem);
        ui->adsbData->setItem(row, ADSB_COL_STATUS, aircraft->m_statusItem);
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
                    icon = getAirlineIcon(aircraft->m_aircraftInfo->m_operatorICAO);
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
                if ((aircraft->m_aircraftInfo->m_registration.size() > 0) && (m_prefixMap != nullptr))
                {
                    QString flag;
                    int idx = aircraft->m_aircraftInfo->m_registration.indexOf('-');
                    if (idx >= 0)
                    {
                        QString prefix;

                        // Some countries use AA-A - try these first as first letters are common
                        prefix = aircraft->m_aircraftInfo->m_registration.left(idx + 2);
                        if (m_prefixMap->contains(prefix))
                            flag =  m_prefixMap->value(prefix);
                        else
                        {
                            // Try letters before '-'
                            prefix = aircraft->m_aircraftInfo->m_registration.left(idx);
                            if (m_prefixMap->contains(prefix))
                                flag =  m_prefixMap->value(prefix);
                        }
                    }
                    else
                    {
                        // No '-' Could be military
                        if ((m_militaryMap != nullptr) && (m_militaryMap->contains(aircraft->m_aircraftInfo->m_operator)))
                            flag = m_militaryMap->value(aircraft->m_aircraftInfo->m_operator);
                    }
                    if (flag != "")
                    {
                        icon = getFlagIcon(flag);
                        if (icon != nullptr)
                        {
                            aircraft->m_countryItem->setSizeHint(QSize(40, 20));
                            aircraft->m_countryItem->setIcon(*icon);
                        }
                    }
                }
            }
        }
        ui->adsbData->setSortingEnabled(true);
    }
    aircraft->m_time = dateTime;
    QTime time = dateTime.time();
    aircraft->m_timeItem->setText(QString("%1:%2:%3").arg(time.hour(), 2, 10, QLatin1Char('0')).arg(time.minute(), 2, 10, QLatin1Char('0')).arg(time.second(), 2, 10, QLatin1Char('0')));
    aircraft->m_adsbFrameCount++;
    aircraft->m_adsbFrameCountItem->setData(Qt::DisplayRole, aircraft->m_adsbFrameCount);

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
    if ((df == 17) || ((df == 18) && ((ca == 0) || (ca == 1) || (ca == 6))))
    {
        if ((tc >= 1) && ((tc <= 4)))
        {
            // Aircraft identification
            int ec = data[4] & 0x7;   // Emitter category
            if (tc == 4)
                aircraft->m_emitterCategory = categorySetA[ec];
            else if (tc == 3)
                aircraft->m_emitterCategory = categorySetB[ec];
            else if (tc == 2)
                aircraft->m_emitterCategory = categorySetC[ec];
            else
                aircraft->m_emitterCategory = QString("Reserved");
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
                callsign[i] = idMap[c[i]];
            callsign[8] = '\0';

            aircraft->m_flight = QString(callsign);
            aircraft->m_flightItem->setText(aircraft->m_flight);
        }
        else if ((tc >= 5) && (tc <= 8))
        {
            // Surface position
        }
        else if (((tc >= 9) && (tc <= 18)) || ((tc >= 20) && (tc <= 22)))
        {
            // Airbourne position (9-18 baro, 20-22 GNSS)
            int ss = (data[4] >> 1) & 0x3; // Surveillance status
            int nicsb = data[4] & 1; // NIC supplement-B
            int alt = ((data[5] & 0xff) << 4) | ((data[6] >> 4) & 0xf); // Altitude
            int n = ((alt >> 1) & 0x7f0) | (alt & 0xf);
            int alt_ft = n * ((alt & 0x10) ? 25 : 100) - 1000;

            aircraft->m_altitude = alt_ft;
            aircraft->m_altitudeValid = true;
            // setData rather than setText so it sorts numerically
            aircraft->m_altitudeItem->setData(Qt::DisplayRole, m_settings.m_siUnits ? feetToMetresInt(aircraft->m_altitude) : aircraft->m_altitude);

            int t = (data[6] >> 3) & 1; // Time synchronisation to UTC
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
            // We could compare global + local methods to see if the positions are sensible
            if (aircraft->m_cprValid[0] && aircraft->m_cprValid[1]
               && (std::abs(aircraft->m_cprTime[0].toSecsSinceEpoch() - aircraft->m_cprTime[1].toSecsSinceEpoch()) < 10))
            {
                // Global decode using odd and even frames (C.2.6)

                // Calculate latitude
                const double dLatEven = 360.0/60.0;
                const double dLatOdd = 360.0/59.0;
                double latEven, latOdd;
                double latitude, longitude;
                int ni, m;

                int j = std::floor(59.0f*aircraft->m_cprLat[0] - 60.0f*aircraft->m_cprLat[1] + 0.5);
                latEven = dLatEven * ((j % 60) + aircraft->m_cprLat[0]);
                if (latEven >= 270.0f)
                    latEven -= 360.0f;
                else if (latEven <= -270.0f)
                    latEven += 360.0f;
                latOdd = dLatOdd * ((j % 59) + aircraft->m_cprLat[1]);
                if (latOdd >= 270.0f)
                    latOdd -= 360.0f;
                else if (latOdd <= -270.0f)
                    latOdd += 360.0f;
                if (aircraft->m_cprTime[0] >= aircraft->m_cprTime[1])
                    latitude = latEven;
                else
                    latitude = latOdd;

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
                    aircraft->m_coordinates.push_back(QVariant::fromValue(*new QGeoCoordinate(aircraft->m_latitude, aircraft->m_longitude, aircraft->m_altitude)));
                    updatePosition(aircraft);
                }
            }
            else
            {
                // Local decode using a single aircraft position + location of receiver
                // Only valid if within 180nm (C.2.6.4)

                // Caclulate latitude
                const double dLatEven = 360.0/60.0;
                const double dLatOdd = 360.0/59.0;
                double dLat = f ? dLatOdd : dLatEven;
                int j = std::floor(m_azEl.getLocationSpherical().m_latitude/dLat) + std::floor(modulus(m_azEl.getLocationSpherical().m_latitude, dLat)/dLat - aircraft->m_cprLat[f] + 0.5);
                aircraft->m_latitude = dLat * (j + aircraft->m_cprLat[f]);
                aircraft->m_latitudeItem->setData(Qt::DisplayRole, aircraft->m_latitude);

                // Caclulate longitude
                double dLong;
                int latNL = cprNL(aircraft->m_latitude);
                if (f == 0)
                {
                    if (latNL > 0)
                        dLong = 360.0 / latNL;
                    else
                        dLong = 360.0;
                }
                else
                {
                    if ((latNL - 1) > 0)
                        dLong = 360.0 / (latNL - 1);
                    else
                        dLong = 360.0;
                }
                int m = std::floor(m_azEl.getLocationSpherical().m_longitude/dLong) + std::floor(modulus(m_azEl.getLocationSpherical().m_longitude, dLong)/dLong - aircraft->m_cprLong[f] + 0.5);
                aircraft->m_longitude = dLong * (m + aircraft->m_cprLong[f]);
                aircraft->m_longitudeItem->setData(Qt::DisplayRole, aircraft->m_longitude);
                aircraft->m_coordinates.push_back(QVariant::fromValue(*new QGeoCoordinate(aircraft->m_latitude, aircraft->m_longitude, aircraft->m_altitude)));
                updatePosition(aircraft);
            }
        }
        else if (tc == 19)
        {
            // Airbourne velocity
            int st = data[4] & 0x7;   // Subtype
            int ic = (data[5] >> 7) & 1; // Intent change flag
            int nac = (data[5] >> 3) & 0x3; // Velocity uncertainty
            if ((st == 1) || (st == 2))
            {
                // Ground speed
                int s_ew = (data[5] >> 2) & 1; // East-west velocity sign
                int v_ew = ((data[5] & 0x3) << 8) | (data[6] & 0xff); // East-west velocity
                int s_ns = (data[7] >> 7) & 1; // North-south velocity sign
                int v_ns = ((data[7] & 0x7f) << 3) | ((data[8] >> 5) & 0x7); // North-south velocity

                int v_we;
                int v_sn;
                int v;
                int h;

                if (s_ew)
                    v_we = -1 * (v_ew - 1);
                else
                    v_we = v_ew - 1;
                if (s_ns)
                    v_sn = -1 * (v_ns - 1);
                else
                    v_sn = v_ns - 1;
                v = (int)std::sqrt(v_we*v_we + v_sn*v_sn);
                h = std::atan2(v_we, v_sn) * 360.0/(2.0*M_PI);
                if (h < 0)
                    h += 360;

                aircraft->m_heading = h;
                aircraft->m_headingValid = true;
                aircraft->m_speed = v;
                aircraft->m_speedType = Aircraft::GS;
                aircraft->m_speedValid = true;
                aircraft->m_headingItem->setData(Qt::DisplayRole, aircraft->m_heading);
                aircraft->m_speedItem->setData(Qt::DisplayRole, m_settings.m_siUnits ? knotsToKPHInt(aircraft->m_speed) : aircraft->m_speed);
            }
            else
            {
                // Airspeed
                int s_hdg = (data[5] >> 2) & 1; // Heading status
                int hdg =  ((data[5] & 0x3) << 8) | (data[6] & 0xff); // Heading
                if (s_hdg)
                {
                    aircraft->m_heading = hdg/1024.0f*360.0f;
                    aircraft->m_headingValid = true;
                    aircraft->m_headingItem->setData(Qt::DisplayRole, aircraft->m_heading);
                }

                int as_t =  (data[7] >> 7) & 1; // Airspeed type
                int as = ((data[7] & 0x7f) << 3) | ((data[8] >> 5) & 0x7); // Airspeed

                aircraft->m_speed = as;
                aircraft->m_speedType = as_t ? Aircraft::IAS : Aircraft::TAS;
                aircraft->m_speedValid = true;
                aircraft->m_speedItem->setData(Qt::DisplayRole, m_settings.m_siUnits ? knotsToKPHInt(aircraft->m_speed) : aircraft->m_speed);
            }
            int vrsrc = (data[8] >> 4) & 1; // Vertical rate source
            int s_vr = (data[8] >> 3) & 1; // Vertical rate sign
            int vr = ((data[8] & 0x7) << 6) | ((data[9] >> 2) & 0x3f); // Vertical rate
            aircraft->m_verticalRate = (vr-1)*64*(s_vr?-1:1);
            aircraft->m_verticalRateValid = true;
            if (m_settings.m_siUnits)
                aircraft->m_verticalRateItem->setData(Qt::DisplayRole, feetPerMinToMetresPerSecondInt(aircraft->m_verticalRate));
            else
                aircraft->m_verticalRateItem->setData(Qt::DisplayRole, aircraft->m_verticalRate);
            int s_dif = (data[10] >> 7) & 1; // Diff from baro alt, sign
            int dif = data[10] & 0x7f; // Diff from baro alt
        }
        else if (tc == 28)
        {
            // Aircraft status
            int st = data[4] & 0x7;   // Subtype
            int es = (data[5] >> 5) & 0x7; // Emergency state
            if (st == 1)
                aircraft->m_status = emergencyStatus[es];
            else
                aircraft->m_status = QString("");
            aircraft->m_statusItem->setText(aircraft->m_status);
        }
        else if (tc == 29)
        {
            // Target state and status
        }
        else if (tc == 31)
        {
            // Aircraft operation status
        }

        // Update aircraft in map
        if (aircraft->m_positionValid)
        {
            m_aircraftModel.aircraftUpdated(aircraft);
        }
    }
    else if (df == 18)
    {
        // TIS-B
        qDebug() << "TIS B message cf=" << ca << " icao: " << icao;
    }
}

bool ADSBDemodGUI::handleMessage(const Message& message)
{
    if (ADSBDemodReport::MsgReportADSB::match(message))
    {
        ADSBDemodReport::MsgReportADSB& report = (ADSBDemodReport::MsgReportADSB&) message;
        handleADSB(
            report.getData(),
            report.getDateTime(),
            report.getPreambleCorrelation(),
            report.getCorrelationOnes());
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

void ADSBDemodGUI::on_feed_clicked(bool checked)
{
    m_settings.m_feedEnabled = checked;
    // Don't disable host/port - so they can be entered before connecting
    applySettings();
}

void ADSBDemodGUI::on_adsbData_cellClicked(int row, int column)
{
    // Get ICAO of aircraft in row clicked
    int icao = ui->adsbData->item(row, 0)->text().toInt(nullptr, 16);
    if (m_aircraft.contains(icao))
         highlightAircraft(m_aircraft.value(icao));
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

        if (column == ADSB_COL_FLIGHT)
        {
            if (aircraft->m_flight.length() > 0)
            {
                // Search for flight on flightradar24
                QDesktopServices::openUrl(QUrl(QString("https://www.flightradar24.com/%1").arg(aircraft->m_flight.trimmed())));
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
    }
}

// Columns in table reordered
void ADSBDemodGUI::adsbData_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    m_settings.m_columnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void ADSBDemodGUI::adsbData_sectionResized(int logicalIndex, int oldSize, int newSize)
{
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

void ADSBDemodGUI::on_getOSNDB_clicked(bool checked)
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        // Download Opensky network database to a file
        QUrl dbURL(QString(OSNDB_URL));
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setAttribute(Qt::WA_DeleteOnClose);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setLabelText(QString("Downloading %1.").arg(OSNDB_URL));
        QNetworkReply *reply = m_dlm.download(dbURL, getOSNDBFilename());
        connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(updateDownloadProgress(qint64,qint64)));
    }
}

void ADSBDemodGUI::on_getAirportDB_clicked(bool checked)
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        // Download Opensky network database to a file
        QUrl dbURL(QString(AIRPORTS_URL));
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setAttribute(Qt::WA_DeleteOnClose);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setLabelText(QString("Downloading %1.").arg(AIRPORTS_URL));
        QNetworkReply *reply = m_dlm.download(dbURL, getAirportDBFilename());
        connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(updateDownloadProgress(qint64,qint64)));
    }
}

void ADSBDemodGUI::on_flightPaths_clicked(bool checked)
{
    m_settings.m_flightPaths = checked;
    m_aircraftModel.setFlightPaths(checked);
}

QString ADSBDemodGUI::getDataDir()
{
    // Get directory to store app data in (aircraft & airport databases and user-definable icons)
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    // First dir is writable
    return locations[0];
}

QString ADSBDemodGUI::getAirportDBFilename()
{
    return getDataDir() + "/airportDatabase.csv";
}

QString ADSBDemodGUI::getAirportFrequenciesDBFilename()
{
    return getDataDir() + "/airportFrequenciesDatabase.csv";
}

QString ADSBDemodGUI::getOSNDBFilename()
{
    return getDataDir() + "/aircraftDatabase.csv";
}

QString ADSBDemodGUI::getFastDBFilename()
{
    return getDataDir() + "/aircraftDatabaseFast.csv";
}

bool ADSBDemodGUI::readOSNDB(const QString& filename)
{
     m_aircraftInfo = AircraftInformation::readOSNDB(filename);
     return m_aircraftInfo != nullptr;
}

bool ADSBDemodGUI::readFastDB(const QString& filename)
{
     m_aircraftInfo = AircraftInformation::readFastDB(filename);
     return m_aircraftInfo != nullptr;
}

void ADSBDemodGUI::updateDownloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    m_progressDialog->setMaximum(totalBytes);
    m_progressDialog->setValue(bytesRead);
}

void ADSBDemodGUI::downloadFinished(const QString& filename, bool success)
{
    if (success)
    {
        if (filename == getOSNDBFilename())
        {
            readOSNDB(filename);
            // Convert to condensed format for faster loading later
            m_progressDialog->setLabelText("Processing.");
            AircraftInformation::writeFastDB(getFastDBFilename(), m_aircraftInfo);
            m_progressDialog->close();
            m_progressDialog = nullptr;
        }
        else if (filename == getAirportDBFilename())
        {
            m_airportInfo = AirportInformation::readAirportsDB(filename);
            // Now download airport frequencies
            QUrl dbURL(QString(AIRPORT_FREQUENCIES_URL));
            m_progressDialog->setLabelText(QString("Downloading %1.").arg(AIRPORT_FREQUENCIES_URL));
            QNetworkReply *reply = m_dlm.download(dbURL, getAirportFrequenciesDBFilename());
            connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(updateDownloadProgress(qint64,qint64)));
        }
        else if (filename == getAirportFrequenciesDBFilename())
        {
            if (m_airportInfo != nullptr)
            {
                AirportInformation::readFrequenciesDB(filename, m_airportInfo);
                // Update airports on map
                updateAirports();
            }
            m_progressDialog->close();
            m_progressDialog = nullptr;
        }
        else
        {
            qDebug() << "ADSBDemodGUI::downloadFinished: Unexpected filename: " << filename;
            m_progressDialog->close();
            m_progressDialog = nullptr;
        }
    }
    else
    {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }
}

void ADSBDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
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
        dialog.move(p);
        dialog.exec();

        m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }
    else if ((m_contextMenuType == ContextMenuStreamSettings) && (m_deviceUISet->m_deviceMIMOEngine))
    {
        DeviceStreamSelectionDialog dialog(this);
        dialog.setNumberOfStreams(m_adsbDemod->getNumberOfDeviceStreams());
        dialog.setStreamIndex(m_settings.m_streamIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
        displayStreamIndex();
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
        DSPDeviceSinkEngine *deviceSinkEngine = (*it)->m_deviceSinkEngine;
        DSPDeviceMIMOEngine *deviceMIMOEngine = (*it)->m_deviceMIMOEngine;

        if (deviceSourceEngine) {
            ui->device->addItem(QString("R%1").arg(deviceIndex), deviceIndex);
        }
    }

    int newDeviceIndex;

    if (it != deviceUISets.begin())
    {
        if (m_settings.m_deviceIndex < 0) {
            ui->device->setCurrentIndex(0);
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

void ADSBDemodGUI::on_devicesRefresh_clicked()
{
    updateDeviceSetList();
    displaySettings();
    applySettings();
}

void ADSBDemodGUI::on_device_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_deviceIndex = ui->device->currentData().toInt();
        applySettings();
    }
}

void ADSBDemodGUI::updateAirports()
{
    m_airportModel.removeAllAirports();
    QHash<int, AirportInformation *>::iterator i = m_airportInfo->begin();
    while (i != m_airportInfo->end())
    {
        AirportInformation *airportInfo = i.value();
        // Calculate range to airport from My Position - One degree = 60 nautical miles
        float latDiff = std::fabs(airportInfo->m_latitude - m_azEl.getLocationSpherical().m_latitude) * 60.0f;
        float longDiff = std::fabs(airportInfo->m_longitude - m_azEl.getLocationSpherical().m_longitude) * 60.0f;
        float range = sqrt(latDiff*latDiff+longDiff*longDiff);
        // Only display airport if in range
        if (range <= m_settings.m_airportRange)
        {
            // Only display the airport if it's large enough
            if (airportInfo->m_type >= m_settings.m_airportMinimumSize)
            {
                // Only display heliports if enabled
                if (m_settings.m_displayHeliports || (airportInfo->m_type != ADSBDemodSettings::AirportType::Heliport))
                {
                    m_airportModel.addAirport(airportInfo);
                }
            }
        }
        ++i;
    }
    // Save settings we've just used so we know if they've changed
    m_currentAirportRange = m_currentAirportRange;
    m_currentAirportMinimumSize = m_settings.m_airportMinimumSize;
    m_currentDisplayHeliports = m_settings.m_displayHeliports;
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
            m_adsbDemod->setTarget(aircraft->m_azimuth, aircraft->m_elevation);
        // Change colour of new target
        aircraft->m_isTarget = true;
        m_aircraftModel.aircraftUpdated(aircraft);
    }
}

void ADSBDemodGUI::highlightAircraft(Aircraft *aircraft)
{
    if (aircraft != m_highlightAircraft)
    {
        if (m_highlightAircraft)
        {
            // Restore colour
            m_highlightAircraft->m_isHighlighted = false;
            m_aircraftModel.aircraftUpdated(m_highlightAircraft);
        }
        // Highlight this aircraft
        m_highlightAircraft = aircraft;
        aircraft->m_isHighlighted = true;
        m_aircraftModel.aircraftUpdated(aircraft);
    }
    // Highlight the row in the table - always do this, as it can become
    // unselected
    ui->adsbData->selectRow(aircraft->m_icaoItem->row());
}

// Show feed dialog
void ADSBDemodGUI::feedSelect()
{
    ADSBDemodFeedDialog dialog(m_settings.m_feedHost, m_settings.m_feedPort, m_settings.m_feedFormat);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_feedHost = dialog.m_feedHost;
        m_settings.m_feedPort = dialog.m_feedPort;
        m_settings.m_feedFormat = dialog.m_feedFormat;
        applySettings();
    }
}

// Show display settings dialog
void ADSBDemodGUI::on_displaySettings_clicked(bool checked)
{
    ADSBDemodDisplayDialog dialog(m_settings.m_removeTimeout, m_settings.m_airportRange, m_settings.m_airportMinimumSize,
                                m_settings.m_displayHeliports, m_settings.m_siUnits,
                                m_settings.m_tableFontName, m_settings.m_tableFontSize,
                                m_settings.m_displayDemodStats);
    if (dialog.exec() == QDialog::Accepted)
    {
        bool unitsChanged = m_settings.m_siUnits != dialog.m_siUnits;

        m_settings.m_removeTimeout = dialog.m_removeTimeout;
        m_settings.m_airportRange = dialog.m_airportRange;
        m_settings.m_airportMinimumSize = dialog.m_airportMinimumSize;
        m_settings.m_displayHeliports = dialog.m_displayHeliports;
        m_settings.m_siUnits = dialog.m_siUnits;
        m_settings.m_tableFontName = dialog.m_fontName;
        m_settings.m_tableFontSize = dialog.m_fontSize;
        m_settings.m_displayDemodStats = dialog.m_displayDemodStats;

        if (unitsChanged)
            m_aircraftModel.allAircraftUpdated();
        displaySettings();
        applySettings();
    }
}

ADSBDemodGUI::ADSBDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::ADSBDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_basicSettingsShown(false),
    m_doApplySettings(true),
    m_tickCount(0),
    m_trackAircraft(nullptr),
    m_highlightAircraft(nullptr),
    m_aircraftInfo(nullptr),
    m_airportModel(this),
    m_progressDialog(nullptr)
{
    ui->setupUi(this);

    ui->map->rootContext()->setContextProperty("aircraftModel", &m_aircraftModel);
    ui->map->rootContext()->setContextProperty("airportModel", &m_airportModel);
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map.qml")));

    setAttribute(Qt::WA_DeleteOnClose, true);

    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &ADSBDemodGUI::downloadFinished);

    m_adsbDemod = reinterpret_cast<ADSBDemod*>(rxChannel); //new ADSBDemod(m_deviceUISet->m_deviceSourceAPI);
    m_adsbDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *feedRightClickEnabler = new CRightClickEnabler(ui->feed);
    connect(feedRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(feedSelect()));

    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

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

    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

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

    // Read aircraft information database, if it has previously been downloaded
    if (!readFastDB(getFastDBFilename()))
    {
        if (readOSNDB(getOSNDBFilename()))
            AircraftInformation::writeFastDB(getFastDBFilename(), m_aircraftInfo);
    }
    // Read airport information database, if it has previously been downloaded
    m_airportInfo = AirportInformation::readAirportsDB(getAirportDBFilename());
    if (m_airportInfo != nullptr)
        AirportInformation::readFrequenciesDB(getAirportFrequenciesDBFilename(), m_airportInfo);
    // Read registration prefix to country map
    m_prefixMap = csvHash(":/flags/regprefixmap.csv");
    // Read operator air force to military map
    m_militaryMap = csvHash(":/flags/militarymap.csv");

    // Get station position
    Real stationLatitude = MainCore::instance()->getSettings().getLatitude();
    Real stationLongitude = MainCore::instance()->getSettings().getLongitude();
    Real stationAltitude = MainCore::instance()->getSettings().getAltitude();
    m_azEl.setLocation(stationLatitude, stationLongitude, stationAltitude);

    // Centre map at My Position
    QQuickItem *item = ui->map->rootObject();
    QObject *object = item->findChild<QObject*>("map");
    if(object != NULL)
    {
        QGeoCoordinate coords = object->property("center").value<QGeoCoordinate>();
        coords.setLatitude(stationLatitude);
        coords.setLongitude(stationLongitude);
        object->setProperty("center", QVariant::fromValue(coords));
    }
    // Move antenna icon to My Position
    QObject *stationObject = item->findChild<QObject*>("station");
    if(stationObject != NULL)
    {
        QGeoCoordinate coords = stationObject->property("coordinate").value<QGeoCoordinate>();
        coords.setLatitude(stationLatitude);
        coords.setLongitude(stationLongitude);
        coords.setAltitude(stationAltitude);
        stationObject->setProperty("coordinate", QVariant::fromValue(coords));
        stationObject->setProperty("stationName", QVariant::fromValue(MainCore::instance()->getSettings().getStationName()));
    }
    // Add airports within range of My Position
    if (m_airportInfo != nullptr)
        updateAirports();

    updateDeviceSetList();
    displaySettings();
    applySettings(true);
}

ADSBDemodGUI::~ADSBDemodGUI()
{
    delete ui;
    QHash<int,Aircraft *>::iterator i = m_aircraft.begin();
    while (i != m_aircraft.end())
    {
        Aircraft *a = i.value();
        delete a;
        ++i;
    }
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

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBWText->setText(QString("%1M").arg(m_settings.m_rfBandwidth / 1000000.0, 0, 'f', 1));
    ui->rfBW->setValue((int)m_settings.m_rfBandwidth);

    ui->spb->setCurrentIndex(m_settings.m_samplesPerBit/2-1);
    ui->correlateFullPreamble->setChecked(m_settings.m_correlateFullPreamble);
    ui->demodModeS->setChecked(m_settings.m_demodModeS);

    ui->thresholdText->setText(QString("%1").arg(m_settings.m_correlationThreshold, 0, 'f', 1));
    ui->threshold->setValue((int)(m_settings.m_correlationThreshold*10.0f));

    ui->feed->setChecked(m_settings.m_feedEnabled);

    ui->flightPaths->setChecked(m_settings.m_flightPaths);
    m_aircraftModel.setFlightPaths(m_settings.m_flightPaths);

    displayStreamIndex();

    QFont font(m_settings.m_tableFontName, m_settings.m_tableFontSize);
    ui->adsbData->setFont(font);

    // Set units in column headers
    if (m_settings.m_siUnits)
    {
        ui->adsbData->horizontalHeaderItem(ADSB_COL_ALTITUDE)->setText("Alt (m)");
        ui->adsbData->horizontalHeaderItem(ADSB_COL_SPEED)->setText("Spd (kph)");
        ui->adsbData->horizontalHeaderItem(ADSB_COL_VERTICALRATE)->setText("VR (m/s)");
    }
    else
    {
        ui->adsbData->horizontalHeaderItem(ADSB_COL_ALTITUDE)->setText("Alt (ft)");
        ui->adsbData->horizontalHeaderItem(ADSB_COL_SPEED)->setText("Spd (kn)");
        ui->adsbData->horizontalHeaderItem(ADSB_COL_VERTICALRATE)->setText("VR (ft/m)");
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

    if (!m_settings.m_displayDemodStats)
        ui->stats->setText("");

    blockApplySettings(false);
}

void ADSBDemodGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void ADSBDemodGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void ADSBDemodGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
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
        // Remove aircraft that haven't been heard of for a minute as probably out of range
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
                // And finally free its memory
                delete aircraft;
            }
            else
                ++i;
        }
    }
}

void ADSBDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    int row = ui->adsbData->rowCount();
    ui->adsbData->setRowCount(row + 1);
    ui->adsbData->setItem(row, ADSB_COL_ICAO, new QTableWidgetItem("ICAO ID"));
    ui->adsbData->setItem(row, ADSB_COL_FLIGHT, new QTableWidgetItem("Flight No."));
    ui->adsbData->setItem(row, ADSB_COL_MODEL, new QTableWidgetItem("Aircraft12345"));
    ui->adsbData->setItem(row, ADSB_COL_AIRLINE, new QTableWidgetItem("airbrigdecargo1"));
    ui->adsbData->setItem(row, ADSB_COL_ALTITUDE, new QTableWidgetItem("Alt (ft)"));
    ui->adsbData->setItem(row, ADSB_COL_SPEED, new QTableWidgetItem("Spd (kn)"));
    ui->adsbData->setItem(row, ADSB_COL_HEADING, new QTableWidgetItem("Hd (o)"));
    ui->adsbData->setItem(row, ADSB_COL_VERTICALRATE, new QTableWidgetItem("VR (ft/m)"));
    ui->adsbData->setItem(row, ADSB_COL_RANGE, new QTableWidgetItem("D (km)"));
    ui->adsbData->setItem(row, ADSB_COL_AZEL, new QTableWidgetItem("Az/El (o)"));
    ui->adsbData->setItem(row, ADSB_COL_LATITUDE, new QTableWidgetItem("-90.00000"));
    ui->adsbData->setItem(row, ADSB_COL_LONGITUDE, new QTableWidgetItem("-180.00000"));
    ui->adsbData->setItem(row, ADSB_COL_CATEGORY, new QTableWidgetItem("Heavy"));
    ui->adsbData->setItem(row, ADSB_COL_STATUS, new QTableWidgetItem("No emergency"));
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
    ui->adsbData->resizeColumnsToContents();
    ui->adsbData->removeCellWidget(row, ADSB_COL_ICAO);
    ui->adsbData->removeCellWidget(row, ADSB_COL_FLIGHT);
    ui->adsbData->removeCellWidget(row, ADSB_COL_MODEL);
    ui->adsbData->removeCellWidget(row, ADSB_COL_AIRLINE);
    ui->adsbData->removeCellWidget(row, ADSB_COL_ALTITUDE);
    ui->adsbData->removeCellWidget(row, ADSB_COL_SPEED);
    ui->adsbData->removeCellWidget(row, ADSB_COL_HEADING);
    ui->adsbData->removeCellWidget(row, ADSB_COL_VERTICALRATE);
    ui->adsbData->removeCellWidget(row, ADSB_COL_RANGE);
    ui->adsbData->removeCellWidget(row, ADSB_COL_AZEL);
    ui->adsbData->removeCellWidget(row, ADSB_COL_LATITUDE);
    ui->adsbData->removeCellWidget(row, ADSB_COL_LONGITUDE);
    ui->adsbData->removeCellWidget(row, ADSB_COL_CATEGORY);
    ui->adsbData->removeCellWidget(row, ADSB_COL_STATUS);
    ui->adsbData->removeCellWidget(row, ADSB_COL_REGISTRATION);
    ui->adsbData->removeCellWidget(row, ADSB_COL_COUNTRY);
    ui->adsbData->removeCellWidget(row, ADSB_COL_REGISTERED);
    ui->adsbData->removeCellWidget(row, ADSB_COL_MANUFACTURER);
    ui->adsbData->removeCellWidget(row, ADSB_COL_OWNER);
    ui->adsbData->removeCellWidget(row, ADSB_COL_OPERATOR_ICAO);
    ui->adsbData->removeCellWidget(row, ADSB_COL_TIME);
    ui->adsbData->removeCellWidget(row, ADSB_COL_FRAMECOUNT);
    ui->adsbData->removeCellWidget(row, ADSB_COL_CORRELATION);
    ui->adsbData->removeCellWidget(row, ADSB_COL_RSSI);
    ui->adsbData->setRowCount(row);
}
