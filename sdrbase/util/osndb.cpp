///////////////////////////////////////////////////////////////////////////////////
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

#include "util/osndb.h"

QHash<QString, QIcon *> AircraftInformation::m_airlineIcons;
QHash<QString, bool> AircraftInformation::m_airlineMissingIcons;
QHash<QString, QIcon *> AircraftInformation::m_flagIcons;
QHash<QString, QString> *AircraftInformation::m_prefixMap;
QHash<QString, QString> *AircraftInformation::m_militaryMap;
QMutex AircraftInformation::m_mutex;
