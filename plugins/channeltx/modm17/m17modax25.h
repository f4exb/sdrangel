///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODM17_M17MODAX25_H_
#define PLUGINS_CHANNELTX_MODM17_M17MODAX25_H_

#include <QByteArray>
#include <cstdint>

class QString;

class M17ModAX25
{
public:
    M17ModAX25();
    ~M17ModAX25();
    void setAX25Control(int ax25Control) { m_ax25Control = ax25Control; }
    void setAX25PID(int ax25PID) { m_ax25PID = ax25PID; }
    QByteArray makePacket(const QString& callsign, const QString& to, const QString& via, const QString& data);

    static const int AX25_MAX_FLAGS = 1024;
    static const int AX25_MAX_BYTES = (2*AX25_MAX_FLAGS+1+28+2+256+2+1);
    static const int AX25_MAX_BITS = (AX25_MAX_BYTES*2);
    static const uint8_t AX25_FLAG =  0x7e;
    static const uint8_t AX25_NO_L3 = 0xf0;

private:
    int m_ax25Control;
    int m_ax25PID;

    static uint8_t *ax25_address(uint8_t *p, QString address, uint8_t crrl);
    static bool ax25_ssid(QByteArray& b, int i, int len, uint8_t& ssid);
};

#endif // PLUGINS_CHANNELTX_MODM17_M17MODAX25_H_

