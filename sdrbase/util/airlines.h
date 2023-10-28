///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_UTIL_AIRLINES_H
#define INCLUDE_UTIL_AIRLINES_H

#include <QString>
#include <QList>
#include <QHash>

#include "export.h"

class SDRBASE_API Airline {

public:

    QString m_icao;
    QString m_name;
    QString m_callsign;
    QString m_country;

    static const Airline *getByICAO(const QString& icao);
    static const Airline *getByCallsign(const QString& callsign);

private:

    Airline(const QString& icao, const QString& name, const QString& callsign, const QString& country) :
        m_icao(icao),
        m_name(name),
        m_callsign(callsign),
        m_country(country)
    {
    }

    static QHash<QString, const Airline*> m_icaoHash;
    static QHash<QString, const Airline*> m_callsignHash;

    friend struct Init;
    struct Init {
        Init();
        static const char *m_airlines[];
    };
    static Init m_init;

};

#endif
