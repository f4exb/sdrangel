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

#include <QDebug>

#include "ax25.h"

// CRC is assumed to be correct (checked in packetdemodsink)
bool AX25Packet::decode(QByteArray packet)
{
    int i, j;
    char destAddress[7];
    unsigned char destSSID;
    char sourceAddress[7];
    unsigned char sourceSSID;
    char repeaterAddress[7];
    unsigned char repeaterSSID;
    unsigned char ssid;
    unsigned char control;

    // Check for minimum size packet. Addresses, control and CRC
    if (packet.size() < 7+7+1+2)
        return false;

    // Address - ASCII shifted right one bit
    for (i = 0; i < 6; i++)
        destAddress[i] = (packet[i] >> 1) & 0x7f;
    destAddress[6] = '\0';
    destSSID = packet[6];
    for (i = 0; i < 6; i++)
        sourceAddress[i] = (packet[7+i] >> 1) & 0x7f;
    sourceAddress[6] = '\0';
    sourceSSID = packet[13];

    // From = source address
    m_from = QString(sourceAddress).trimmed();
    ssid = (sourceSSID >> 1) & 0xf;
    if (ssid != 0)
        m_from = QString("%1-%2").arg(m_from).arg(ssid);

    // To = destination address
    m_to = QString(destAddress).trimmed();
    ssid = (destSSID >> 1) & 0xf;
    if (ssid != 0)
        m_to = QString("%1-%2").arg(m_to).arg(ssid);

    // List of repeater addresses for via field
    m_via = QString("");
    i = 13;
    while ((packet[i] & 1) == 0)
    {
        i++;
        for (j = 0; j < 6; j++)
            repeaterAddress[j] = (packet[i+j] >> 1) & 0x7f;
        repeaterAddress[j] = '\0';
        i += 6;
        repeaterSSID = packet[i];
        ssid = (repeaterSSID >> 1) & 0xf;
        QString repeater = QString(repeaterAddress).trimmed();
        QString ssidString = (ssid != 0) ? QString("%2-%3").arg(repeater).arg(ssid) : QString(repeater);
        if (m_via == "")
            m_via = ssidString;
        else
            m_via = QString("%1,%2").arg(m_via).arg(ssidString);
    }
    i++;
    // Control can be 1 or 2 bytes - how to know if 2?
    //  I, U and S frames
    control = packet[i++];
    if ((control & 1) == 0)
        m_type = QString("I");
    else if ((control & 3) == 3)
    {
        // See figure 4.4 of AX.25 spec
        switch (control & 0xef)
        {
        case 0x6f:
            m_type = QString("SABME");
            break;
        case 0x2f:
            m_type = QString("SABM");
            break;
        case 0x43:
            m_type = QString("DISC");
            break;
        case 0x0f:
            m_type = QString("DM");
            break;
        case 0x63:
            m_type = QString("UA");
            break;
        case 0x87:
            m_type = QString("FR");
            break;
        case 0x03:
            m_type = QString("UI");
            break;
        case 0xaf:
            m_type = QString("XID");
            break;
        case 0xe3:
            m_type = QString("TEST");
            break;
        default:
            m_type = QString("U");
            break;
        }
    }
    else
        m_type = QString("S");
    // APRS packets use UI frames, which are a subype of U frames
    // Only I and UI frames have Layer 3 Protocol ID (PID).
    if ((m_type == "I") || (m_type == "UI"))
        m_pid = QString("%1").arg(((unsigned)packet[i++]) & 0xff, 2, 16, QLatin1Char('0'));
    else
        m_pid = QString("");
    int infoStart, infoEnd;
    infoStart = i;
    infoEnd = packet.size()-2-i;
    QByteArray info(packet.mid(infoStart, infoEnd));
    m_dataASCII = QString::fromLatin1(info);
    m_dataHex = QString(info.toHex());

    return true;
}

bool AX25Packet::ssid(QByteArray& b, int i, int len, uint8_t& ssid)
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
                // FIXME: APRS-IS seems to support 2 letter SSIDs
                // These can't be sent over RF, as not enough bits in AX.25 packet
                qDebug() << "AX25Packet::ssid: SSID greater than 15 not supported";
                ssid = 0;
                return false;
            }
            else
            {
                return true;
            }
        }
        else
        {
            qDebug() << "AX25Packet::ssid: SSID number missing";
            return false;
        }
    }
    else
        return false;
}

QByteArray AX25Packet::encodeAddress(QString address, uint8_t crrl)
{
    int len;
    int i;
    QByteArray encodedAddress;
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
                AX25Packet::ssid(b, i, len, ssid);
                hyphenSeen = true;
                encodedAddress.append(' ' << 1);
            }
            else
            {
                encodedAddress.append(b[i] << 1);
            }
        }
        else
        {
            encodedAddress.append(' ' << 1);
        }
    }
    if (b[i] == '-')
    {
        AX25Packet::ssid(b, i, len, ssid);
    }
    encodedAddress.append(crrl | (ssid << 1));

    return encodedAddress;
}
