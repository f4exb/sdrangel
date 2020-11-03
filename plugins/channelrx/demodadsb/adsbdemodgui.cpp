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
#include "adsb.h"

// ADS-B table columns
#define ADSB_COL_ICAO           0
#define ADSB_COL_FLIGHT         1
#define ADSB_COL_LATITUDE       2
#define ADSB_COL_LONGITUDE      3
#define ADSB_COL_ALTITUDE       4
#define ADSB_COL_SPEED          5
#define ADSB_COL_HEADING        6
#define ADSB_COL_VERTICALRATE   7
#define ADSB_COL_CATEGORY       8
#define ADSB_COL_STATUS         9
#define ADSB_COL_RANGE          10
#define ADSB_COL_AZEL           11
#define ADSB_COL_TIME           12
#define ADSB_COL_FRAMECOUNT     13
#define ADSB_COL_CORRELATION    14

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
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

// Longitude zone (returns value in range [1,59]
static int cprNL(Real lat)
{
    if (lat == 0.0f)
        return 59;
    else if ((lat == 87.0f) || (lat == -87.0f))
        return 2;
    else if ((lat > 87.0f) || (lat < -87.0f))
        return 1;
    else
    {
        double nz = 15.0;
        double n = 1 - std::cos(M_PI / (2.0 * nz));
        double d = std::cos(std::fabs(lat) * M_PI/180.0);
        return std::floor((M_PI * 2.0) / std::acos(1.0 - (n/(d*d))));
    }
}

static int cprN(Real lat, int odd)
{
    int nl = cprNL(lat) - odd;
    if (nl > 1)
        return nl;
    else
        return 1;
}

static Real feetToMetres(Real feet)
{
    return feet * 0.3048;
}

// Can't use std::fmod, as that works differently for negative numbers
static Real modulus(Real x, Real y)
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
        coords.setAltitude(m_aircrafts[row]->m_altitude * 0.3048); // Convert feet to metres
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
        if (m_aircrafts[row]->m_altitudeValid)
        {
            list.append(QString("Altitude: %1 (ft)").arg(m_aircrafts[row]->m_altitude));
        }
        if (m_aircrafts[row]->m_speedValid)
        {
            list.append(QString("%1: %2 (kn)").arg(m_aircrafts[row]->m_speedTypeNames[m_aircrafts[row]->m_speedType]).arg(m_aircrafts[row]->m_speed));
        }
        if (m_aircrafts[row]->m_verticalRateValid)
        {
            QString desc;
            if (m_aircrafts[row]->m_verticalRate == 0)
                desc = "Level flight";
            else if (m_aircrafts[row]->m_verticalRate > 0)
                desc = QString("Climbing: %1 (ft/min)").arg(m_aircrafts[row]->m_verticalRate);
            else
                desc = QString("Descending: %1 (ft/min)").arg(m_aircrafts[row]->m_verticalRate);
            list.append(QString(desc));
        }
        if  ((m_aircrafts[row]->m_status.length() > 0) && m_aircrafts[row]->m_status.compare("No emergency"))
        {
            list.append(m_aircrafts[row]->m_status);
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
                return QVariant::fromValue(QString("map_truck.png"));
            else
                return QVariant::fromValue(QString("aircraft_2engine.png"));
        }
        else
            return QVariant::fromValue(QString("aircraft_2engine.png"));
    }
    else if (role == AircraftModel::bubbleColourRole)
    {
        // Select a background colour for the text bubble next to the aircraft
        if (m_aircrafts[row]->m_isBeingTracked)
            return  QVariant::fromValue(QColor("lightgreen"));
        else if ((m_aircrafts[row]->m_status.length() > 0) && m_aircrafts[row]->m_status.compare("No emergency"))
            return  QVariant::fromValue(QColor("lightred"));
        else
            return  QVariant::fromValue(QColor("lightblue"));
    }
    return QVariant();
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
    aircraft->m_rangeItem->setText(QString("%1").arg(aircraft->m_range/1000.0, 0, 'f', 1));
    aircraft->m_azElItem->setText(QString("%1/%2").arg(std::round(aircraft->m_azimuth)).arg(std::round(aircraft->m_elevation)));
    if (aircraft == m_trackAircraft)
        m_adsbDemod->setTarget(aircraft->m_azimuth, aircraft->m_elevation);
}

void ADSBDemodGUI::handleADSB(const QByteArray data, const QDateTime dateTime, float correlationOnes, float correlationZeros)
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
        aircraft = new Aircraft;
        aircraft->m_icao = icao;
        m_aircraft.insert(icao, aircraft);
        aircraft->m_icaoItem->setText(QString("%1").arg(aircraft->m_icao, 1, 16));
        int row = ui->adsbData->rowCount();
        ui->adsbData->setRowCount(row + 1);
        ui->adsbData->setItem(row, ADSB_COL_ICAO, aircraft->m_icaoItem);
        ui->adsbData->setItem(row, ADSB_COL_FLIGHT, aircraft->m_flightItem);
        ui->adsbData->setItem(row, ADSB_COL_LATITUDE, aircraft->m_latitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_LONGITUDE, aircraft->m_longitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_ALTITUDE, aircraft->m_altitudeItem);
        ui->adsbData->setItem(row, ADSB_COL_SPEED, aircraft->m_speedItem);
        ui->adsbData->setItem(row, ADSB_COL_HEADING, aircraft->m_headingItem);
        ui->adsbData->setItem(row, ADSB_COL_VERTICALRATE, aircraft->m_verticalRateItem);
        ui->adsbData->setItem(row, ADSB_COL_CATEGORY, aircraft->m_emitterCategoryItem);
        ui->adsbData->setItem(row, ADSB_COL_STATUS, aircraft->m_statusItem);
        ui->adsbData->setItem(row, ADSB_COL_RANGE, aircraft->m_rangeItem);
        ui->adsbData->setItem(row, ADSB_COL_AZEL, aircraft->m_azElItem);
        ui->adsbData->setItem(row, ADSB_COL_TIME, aircraft->m_timeItem);
        ui->adsbData->setItem(row, ADSB_COL_FRAMECOUNT, aircraft->m_adsbFrameCountItem);
        ui->adsbData->setItem(row, ADSB_COL_CORRELATION, aircraft->m_correlationItem);
    }
    aircraft->m_time = dateTime;
    QTime time = dateTime.time();
    aircraft->m_timeItem->setText(QString("%1:%2:%3").arg(time.hour(), 2, 10, QLatin1Char('0')).arg(time.minute(), 2, 10, QLatin1Char('0')).arg(time.second(), 2, 10, QLatin1Char('0')));
    aircraft->m_adsbFrameCount++;
    aircraft->m_adsbFrameCountItem->setText(QString("%1").arg(aircraft->m_adsbFrameCount));

    m_correlationZerosAvg(correlationZeros);
    aircraft->m_minCorrelation = m_correlationZerosAvg.instantAverage();
    if (correlationOnes > aircraft->m_maxCorrelation)
        aircraft->m_maxCorrelation = correlationOnes;
    m_correlationOnesAvg(correlationOnes);
    aircraft->m_correlation = m_correlationOnesAvg.instantAverage();
    aircraft->m_correlationItem->setText(QString("%1/%2/%3")
        .arg(CalcDb::dbPower(aircraft->m_minCorrelation), 3, 'f', 1)
        .arg(CalcDb::dbPower(aircraft->m_correlation), 3, 'f', 1)
        .arg(CalcDb::dbPower(aircraft->m_maxCorrelation), 3, 'f', 1));


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
        aircraft->m_altitudeItem->setText(QString("%1").arg(aircraft->m_altitude));

        int t = (data[6] >> 3) & 1; // Time
        int f = (data[6] >> 2) & 1; // CPR odd/even frame
        int lat_cpr = ((data[6] & 3) << 15) | ((data[7] & 0xff) << 7) | ((data[8] >> 1) & 0x7f);
        int lon_cpr = ((data[8] & 1) << 16) | ((data[9] & 0xff) << 8) | (data[10] & 0xff);

        aircraft->m_cprValid[f] = true;
        aircraft->m_cprLat[f] = lat_cpr/131072.0f;
        aircraft->m_cprLong[f] = lon_cpr/131072.0f;

        // Check if both odd and even frames are available
        // See: https://mode-s.org/decode/adsb/airborne-position.html
        // We could compare global + local methods to see if the positions are sensible
        if (aircraft->m_cprValid[0] && aircraft->m_cprValid[1])
        {
            // Global decode using odd and even frames

            // Calculate latitude
            const Real dLatEven = 360.0f/60.0f;
            const Real dLatOdd = 360.0f/59.0f;
            Real latEven, latOdd;

            int j = std::floor(59.0f*aircraft->m_cprLat[0] - 60.0f*aircraft->m_cprLat[1] + 0.5);
            latEven = dLatEven * ((j % 60) + aircraft->m_cprLat[0]);
            if (latEven >= 270.0f)
                latEven -= 360.0f;
            latOdd = dLatOdd * ((j % 59) + aircraft->m_cprLat[1]);
            if (latOdd >= 270.0f)
                latOdd -= 360.0f;
            if (!f)
                aircraft->m_latitude = latEven;
            else
                aircraft->m_latitude = latOdd;
            aircraft->m_latitudeItem->setText(QString("%1").arg(aircraft->m_latitude));

            // Check if both frames in same latitude zone
            int latEvenNL = cprNL(latEven);
            int latOddNL = cprNL(latOdd);
            if (latEvenNL == latOddNL)
            {
                // Calculate longitude
                if (!f)
                {
                    int ni = cprN(latEven, 0);
                    int m = std::floor(aircraft->m_cprLong[0] * (latEvenNL - 1) - aircraft->m_cprLong[1] * latEvenNL + 0.5f);
                    aircraft->m_longitude = (360.0f/ni) * ((m % ni) + aircraft->m_cprLong[0]);
                }
                else
                {
                    int ni = cprN(latOdd, 1);
                    int m = std::floor(aircraft->m_cprLong[0] * (latOddNL - 1) - aircraft->m_cprLong[1] * latOddNL + 0.5f);
                    aircraft->m_longitude = (360.0f/ni) * ((m % ni) + aircraft->m_cprLong[1]);
                }
                if (aircraft->m_longitude > 180.0f)
                    aircraft->m_longitude -= 360.0f;
                aircraft->m_longitudeItem->setText(QString("%1").arg(aircraft->m_longitude));
                updatePosition(aircraft);
            }
        }
        else
        {
            // Local decode using a single aircraft position + location of receiver
            // Only valid if within 180nm

            // Caclulate latitude
            const Real dLatEven = 360.0f/60.0f;
            const Real dLatOdd = 360.0f/59.0f;
            Real dLat = f ? dLatOdd : dLatEven;
            int j = std::floor(m_azEl.getLocationSpherical().m_latitude/dLat) + std::floor(modulus(m_azEl.getLocationSpherical().m_latitude, dLat)/dLat - aircraft->m_cprLat[f] + 0.5);
            aircraft->m_latitude = dLat * (j + aircraft->m_cprLat[f]);
            // Add suffix of L to indicate local decode
            aircraft->m_latitudeItem->setText(QString("%1 L").arg(aircraft->m_latitude));

            // Caclulate longitude
            Real dLong;
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
            // Add suffix of L to indicate local decode
            aircraft->m_longitudeItem->setText(QString("%1 L").arg(aircraft->m_longitude));
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
            aircraft->m_headingItem->setText(QString("%1").arg(aircraft->m_heading));
            aircraft->m_speedItem->setText(QString("%1").arg(aircraft->m_speed));
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
                aircraft->m_headingItem->setText(QString("%1").arg(aircraft->m_heading));
            }

            int as_t =  (data[7] >> 7) & 1; // Airspeed type
            int as = ((data[7] & 0x7f) << 3) | ((data[8] >> 5) & 0x7); // Airspeed

            aircraft->m_speed = as;
            aircraft->m_speedType = as_t ? Aircraft::IAS : Aircraft::TAS;
            aircraft->m_speedValid = true;
            aircraft->m_speedItem->setText(QString("%1").arg(aircraft->m_speed));
        }
        int vrsrc = (data[8] >> 4) & 1; // Vertical rate source
        int s_vr = (data[8] >> 3) & 1; // Vertical rate sign
        int vr = ((data[8] & 0x7) << 6) | ((data[9] >> 2) & 0x3f); // Vertical rate
        aircraft->m_verticalRate = (vr-1)*64*(s_vr?-1:1);
        aircraft->m_verticalRateValid = true;
        aircraft->m_verticalRateItem->setText(QString("%1").arg(aircraft->m_verticalRate));

        int s_dif = (data[10] >> 7) & 1; // Diff from baro alt, sign
        int dif = data[10] & 0x7f; // Diff from baro alt
    }
    else if (tc == 28)
    {
        // Aircraft status
        int st = data[4] & 0x7;   // Subtype
        int es = (data[5] >> 5) & 0x7; // Emergeny state
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

bool ADSBDemodGUI::handleMessage(const Message& message)
{
    if (ADSBDemodReport::MsgReportADSB::match(message))
    {
        ADSBDemodReport::MsgReportADSB& report = (ADSBDemodReport::MsgReportADSB&) message;
        handleADSB(
            report.getData(), report.getDateTime(),
            report.getPreambleCorrelationOnes(),
            report.getPreambleCorrelationZeros());
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

void ADSBDemodGUI::on_beastEnabled_stateChanged(int state)
{
    m_settings.m_beastEnabled = state == Qt::Checked;
    // Don't disable host/port - so they can be entered before connecting
    applySettings();
}

void ADSBDemodGUI::on_host_editingFinished(QString value)
{
    m_settings.m_beastHost = value;
    applySettings();
}

void ADSBDemodGUI::on_port_valueChanged(int value)
{
    m_settings.m_beastPort = value;
    applySettings();
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
                if (m_trackAircraft)
                {
                    // Restore colour of current target
                    m_trackAircraft->m_isBeingTracked = false;
                    m_aircraftModel.aircraftUpdated(m_trackAircraft);
                }
                // Track this aircraft
                m_trackAircraft = aircraft;
                if (aircraft->m_positionValid)
                    m_adsbDemod->setTarget(aircraft->m_azimuth, aircraft->m_elevation);
                // Change colour of new target
                aircraft->m_isBeingTracked = true;
                m_aircraftModel.aircraftUpdated(aircraft);
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

void ADSBDemodGUI::on_spb_currentIndexChanged(int value)
{
    m_settings.m_samplesPerBit = (value + 1) * 2;
    applySettings();
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


ADSBDemodGUI::ADSBDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::ADSBDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_basicSettingsShown(false),
    m_doApplySettings(true),
    m_tickCount(0),
    m_trackAircraft(nullptr)
{
    ui->setupUi(this);
    ui->map->rootContext()->setContextProperty("aircraftModel", &m_aircraftModel);

    setAttribute(Qt::WA_DeleteOnClose, true);

    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_adsbDemod = reinterpret_cast<ADSBDemod*>(rxChannel); //new ADSBDemod(m_deviceUISet->m_deviceSourceAPI);
    m_adsbDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

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

    resizeTable();

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

        ADSBDemod::MsgConfigureADSBDemod* message = ADSBDemod::MsgConfigureADSBDemod::create( m_settings, force);
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

    ui->thresholdText->setText(QString("%1").arg(m_settings.m_correlationThreshold, 0, 'f', 1));
    ui->threshold->setValue((int)(m_settings.m_correlationThreshold*10));

    ui->beastEnabled->setChecked(m_settings.m_beastEnabled);
    ui->host->setText(m_settings.m_beastHost);
    ui->port->setValue(m_settings.m_beastPort);

    displayStreamIndex();

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
    int row = ui->adsbData->rowCount();
    ui->adsbData->setRowCount(row + 1);
    ui->adsbData->setItem(row, ADSB_COL_ICAO, new QTableWidgetItem("ICAO ID"));
    ui->adsbData->setItem(row, ADSB_COL_FLIGHT, new QTableWidgetItem("Flight No"));
    ui->adsbData->setItem(row, ADSB_COL_LATITUDE, new QTableWidgetItem("-90.00000 L"));
    ui->adsbData->setItem(row, ADSB_COL_LONGITUDE, new QTableWidgetItem("-180.00000 L"));
    ui->adsbData->setItem(row, ADSB_COL_ALTITUDE, new QTableWidgetItem("Alt (ft)"));
    ui->adsbData->setItem(row, ADSB_COL_SPEED, new QTableWidgetItem("Sp (kn)"));
    ui->adsbData->setItem(row, ADSB_COL_HEADING, new QTableWidgetItem("Hd (o)"));
    ui->adsbData->setItem(row, ADSB_COL_VERTICALRATE, new QTableWidgetItem("Climb"));
    ui->adsbData->setItem(row, ADSB_COL_CATEGORY, new QTableWidgetItem("Category"));
    ui->adsbData->setItem(row, ADSB_COL_STATUS, new QTableWidgetItem("No emergency"));
    ui->adsbData->setItem(row, ADSB_COL_RANGE, new QTableWidgetItem("D (km)"));
    ui->adsbData->setItem(row, ADSB_COL_AZEL, new QTableWidgetItem("Az/El (o)"));
    ui->adsbData->setItem(row, ADSB_COL_TIME, new QTableWidgetItem("99:99:99"));
    ui->adsbData->setItem(row, ADSB_COL_FRAMECOUNT, new QTableWidgetItem("Frames"));
    ui->adsbData->setItem(row, ADSB_COL_CORRELATION, new QTableWidgetItem("-99.9/-99.9/=99.9"));
    ui->adsbData->resizeColumnsToContents();
    ui->adsbData->removeCellWidget(row, ADSB_COL_ICAO);
    ui->adsbData->removeCellWidget(row, ADSB_COL_FLIGHT);
    ui->adsbData->removeCellWidget(row, ADSB_COL_LATITUDE);
    ui->adsbData->removeCellWidget(row, ADSB_COL_LONGITUDE);
    ui->adsbData->removeCellWidget(row, ADSB_COL_ALTITUDE);
    ui->adsbData->removeCellWidget(row, ADSB_COL_SPEED);
    ui->adsbData->removeCellWidget(row, ADSB_COL_HEADING);
    ui->adsbData->removeCellWidget(row, ADSB_COL_VERTICALRATE);
    ui->adsbData->removeCellWidget(row, ADSB_COL_CATEGORY);
    ui->adsbData->removeCellWidget(row, ADSB_COL_STATUS);
    ui->adsbData->removeCellWidget(row, ADSB_COL_RANGE);
    ui->adsbData->removeCellWidget(row, ADSB_COL_AZEL);
    ui->adsbData->removeCellWidget(row, ADSB_COL_TIME);
    ui->adsbData->removeCellWidget(row, ADSB_COL_FRAMECOUNT);
    ui->adsbData->removeCellWidget(row, ADSB_COL_CORRELATION);
    ui->adsbData->setRowCount(row);
}
