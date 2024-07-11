///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_VLFTRANSMITTERS_H
#define INCLUDE_VLFTRANSMITTERS_H

#include <QString>
#include <QList>
#include <QHash>

#include "export.h"

// List of VLF transmitters
// Built-in list can be overridden by user supplied vlftransmitters.csv file, that is read at startup, from the app data dir
class SDRBASE_API VLFTransmitters
{

public:

    struct Transmitter {
        QString m_callsign;
        qint64 m_frequency;         // In Hz
        float m_latitude;
        float m_longitude;
        int m_power;                // In kW
    };

    static QList<Transmitter> m_transmitters;

    static QHash<QString, const Transmitter*> m_callsignHash;

private:

    friend struct Init;
    struct Init {
        Init();
    };
    static Init m_init;

};

#endif /* VLFTransmitters */
