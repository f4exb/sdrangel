///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Copyright (C) 2018 Eric Reuter                                                //
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

#ifndef INCLUDE_ENDOFTRAINPACKET_H
#define INCLUDE_ENDOFTRAINPACKET_H

#include <QtCore>
#include <QString>
#include <QByteArray>

struct EndOfTrainPacket {

    int m_chainingBits;
    int m_batteryCondition;             // Device battery status
    int m_type;                         // Message type identifier
    int m_address;                      // EOT unit address
    int m_pressure;                     // Rear brake pressure PSIG
    int m_batteryCharge;                // Battery charge
    bool m_discretionary;               // Used differently by different vendors
    bool m_valveCircuitStatus;
    bool m_confirmation;
    bool m_turbine;
    bool m_motion;                      // Whether train is in motion
    bool m_markerLightBatteryCondition; // Condition of marker light battery
    bool m_markerLightStatus;           // Whether light is on or off
    quint32 m_crc;
    quint32 m_crcCalculated;
    bool m_crcValid;

    QString m_dataHex;

    bool decode(const QByteArray& packet);

    QString getMessageType() const
    {
        return QString::number(m_type);
    }

    QString getBatteryCondition() const
    {
        const QStringList batteryConditions = {"N/A", "Very Low", "Low", "OK"};
        return batteryConditions[m_batteryCondition];
    }

    QString getValveCircuitStatus() const
    {
        return m_valveCircuitStatus ? "OK" : "Fail";
    }

    QString getMarkerLightBatteryCondition() const
    {
        return m_markerLightBatteryCondition ? "Low" : "Ok";
    }

    QString getMarkerLightStatus() const
    {
        return m_markerLightStatus ? "On" : "Off";
    }

    float getBatteryChargePercent() const
    {
        return 100.0f * m_batteryCharge / 127.0f;
    }

    QString getArmStatus() const
    {
        if (m_type == 7)
        {
            if (m_confirmation == 0) {
                return "Arming";
            } else {
                return "Armed";
            }
        }
        else
        {
            return "Normal";
        }
    }

};

#endif // INCLUDE_ENDOFTRAINPACKET_H
