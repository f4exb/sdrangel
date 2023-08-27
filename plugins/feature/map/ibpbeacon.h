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

#ifndef INCLUDE_IBP_BEACON_H
#define INCLUDE_IBP_BEACON_H

#include <QString>
#include <QList>

#include "util/maidenhead.h"

// International Beacon Project beacons
// https://www.ncdxf.org/beacon/

struct IBPBeacon {

    QString m_dxEntity;
    QString m_callsign;
    QString m_location;
    QString m_locator;
    int m_offset; // Time offset in seconds
    float m_latitude;
    float m_longitude;

    IBPBeacon(const QString& dxEntity, const QString& callsign, const QString& location, const QString& locator, int offset) :
        m_dxEntity(dxEntity),
        m_callsign(callsign),
        m_location(location),
        m_locator(locator),
        m_offset(offset)
    {
        Maidenhead::fromMaidenhead(locator, m_latitude, m_longitude);
    }

    QString getText() const
    {
        QStringList list;
        list.append("IBP Beacon");
        list.append(QString("DX Entity: %1").arg(m_dxEntity));
        list.append(QString("Callsign: %1").arg(m_callsign));
        list.append(QString("Frequency: 14.1, 18.11, 21.15, 24.93, 28.2 MHz"));
        list.append(QString("Power: 100 Watts ERP"));
        list.append(QString("Polarization: V"));
        list.append(QString("Pattern: Omni"));
        list.append(QString("Key: A1"));
        list.append(QString("Locator: %1").arg(m_locator));
        return list.join("\n");
    }

    static QList<IBPBeacon> m_beacons;
    static QList<double> m_frequencies;
    static const int m_period = 10; // Beacons rotate every 10 seconds
};

#endif // INCLUDE_IBP_BEACON_H
