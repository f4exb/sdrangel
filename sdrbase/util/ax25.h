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

#ifndef INCLUDE_AX25_H
#define INCLUDE_AX25_H

#include <QString>
#include <QByteArray>

#include "export.h"

struct SDRBASE_API AX25Packet {
    QString m_from;
    QString m_to;
    QString m_via;
    QString m_type;
    QString m_pid;
    QString m_dataASCII;
    QString m_dataHex;

    bool decode(QByteArray packet);

    static bool ssid(QByteArray& b, int i, int len, uint8_t& ssid);
    static QByteArray encodeAddress(QString address, uint8_t crrl=0);

};

#endif // INCLUDE_AX25_H
