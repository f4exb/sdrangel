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

#include <QString>
#include <array>
#include "util/crc.h"
#include "m17modax25.h"

M17ModAX25::M17ModAX25() :
    m_ax25Control(3),
    m_ax25PID(AX25_NO_L3)
{}

M17ModAX25::~M17ModAX25()
{}

QByteArray M17ModAX25::makePacket(const QString& callsign, const QString& to, const QString& via, const QString& data)
{
    std::array<uint8_t, AX25_MAX_BYTES> packet;
    uint8_t *crc_start;
    uint8_t *p;
    crc16x25 crc;
    uint16_t crcValue;
    int len;
    int packet_length;

    // Create AX.25 packet
    p = packet.data();
    // Unique preamble flag
    // *p++ = AX25_FLAG;
    crc_start = p;
    // Dest
    p = ax25_address(p, to, 0xe0);
    // From
    p = ax25_address(p, callsign, 0x60);
    // Via
    p = ax25_address(p, via, 0x61);
    // Control
    *p++ = m_ax25Control;
    // PID
    *p++ = m_ax25PID;
    // Data
    len = data.length();
    memcpy(p, data.toUtf8(), len);
    p += len;
    // CRC (do not include flags)
    crc.calculate(crc_start, p-crc_start);
    crcValue = crc.get();
    *p++ = crcValue & 0xff;
    *p++ = (crcValue >> 8);
    // Unique postamble flag
    // *p++ = AX25_FLAG;

    packet_length = p-&packet[0];

    return QByteArray(reinterpret_cast<const char*>(packet.data()), packet_length);
}

uint8_t *M17ModAX25::ax25_address(uint8_t *p, QString address, uint8_t crrl)
{
    int len;
    int i;
    QByteArray b;
    uint8_t ssid = 0;
    bool hyphenSeen = false;

    len = address.length();
    b = address.toUtf8();
    ssid = 0;

    for (i = 0; i < 6; i++)
    {
        if ((i < len) && !hyphenSeen)
        {
            if (b[i] == '-')
            {
                ax25_ssid(b, i, len, ssid);
                hyphenSeen = true;
                *p++ = ' ' << 1;
            }
            else
            {
                *p++ = b[i] << 1;
            }
        }
        else
        {
            *p++ = ' ' << 1;
        }
    }

    if (b[i] == '-') {
        ax25_ssid(b, i, len, ssid);
    }

    *p++ = crrl | (ssid << 1);

    return p;
}

bool M17ModAX25::ax25_ssid(QByteArray& b, int i, int len, uint8_t& ssid)
{
    if (b[i] == '-')
    {
        if (len > i + 1)
        {
            ssid = b[i+1] - '0';

            if ((len > i + 2) && isdigit(b[i+2])) {
                ssid = (ssid*10) + (b[i+2] - '0');
            }

            if (ssid >= 16)
            {
                qDebug("M17ModAX25::ax25_ssid: ax25_address: SSID greater than 15 not supported");
                ssid = ssid & 0xf;
                return false;
            }
            else
            {
                return true;
            }
        }
        else
        {
            qDebug("M17ModAX25::ax25_ssid: ax25_address: SSID number missing");
            return false;
        }
    }
    else
    {
        return false;
    }
}
