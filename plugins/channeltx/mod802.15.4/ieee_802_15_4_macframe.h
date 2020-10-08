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

#ifndef INCLUDE_IEEE_802_15_4_MACFRAME_H
#define INCLUDE_IEEE_802_15_4_MACFRAME_H

#include <stdint.h>
#include <stdio.h>

// Frame control field values
#define IEEE_802_15_4_MAC_FRAME_TYPE_MASK       0x0003
#define IEEE_802_15_4_MAC_BEACON                0x0000
#define IEEE_802_15_4_MAC_DATA                  0x0001
#define IEEE_802_15_4_MAC_ACK                   0x0002
#define IEEE_802_15_4_MAC_COMMAND               0x0003
#define IEEE_802_15_4_MAC_SECURITY_ENABLED      0x0008
#define IEEE_802_15_4_MAC_FRAME_PENDING         0x0010
#define IEEE_802_15_4_MAC_ACK_REQUIRED          0x0020
#define IEEE_802_15_4_MAC_PAN_COMPRESSION       0x0040
#define IEEE_802_15_4_MAC_NO_SEQ_NUMBER         0x0100
#define IEEE_802_15_4_MAC_DEST_ADDRESS_MASK     0x0c00
#define IEEE_802_15_4_MAC_DEST_ADDRESS_NONE     0x0000
#define IEEE_802_15_4_MAC_DEST_ADDRESS_SHORT    0x0400
#define IEEE_802_15_4_MAC_DEST_ADDRESS_EXT      0x0c00
#define IEEE_802_15_4_MAC_SOURCE_ADDRESS_MASK   0xc000
#define IEEE_802_15_4_MAC_SOURCE_ADDRESS_NONE   0x0000
#define IEEE_802_15_4_MAC_SOURCE_ADDRESS_SHORT  0x4000
#define IEEE_802_15_4_MAC_SOURCE_ADDRESS_EXT    0xc000
#define IEEE_802_15_4_MAC_PAYLOAD_MAX_LENGTH    124
#define IEEE_802_15_5_MAX_EXT_ADDRESS_LENGTH    8

typedef uint8_t ieee_802_15_4_address[IEEE_802_15_5_MAX_EXT_ADDRESS_LENGTH];

struct IEEE_802_15_4_MacFrame
{
    uint16_t m_frameControl;
    uint8_t m_sequenceNumber;
    uint16_t m_destPANID;
    uint16_t m_destShortAddress;
    ieee_802_15_4_address m_destAddress;
    uint16_t m_sourcePANID;
    uint16_t m_sourceShortAddress;
    ieee_802_15_4_address m_sourceAddress;
    uint8_t m_payload[IEEE_802_15_4_MAC_PAYLOAD_MAX_LENGTH];
    uint8_t m_payloadLength;

    IEEE_802_15_4_MacFrame()
    {
        if (false)
        {
            // Example ACK frame
            m_frameControl = IEEE_802_15_4_MAC_ACK;
            m_sequenceNumber = 0;
            m_payloadLength = 0;
        }
        else
        {
            ieee_802_15_4_address dst = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
            ieee_802_15_4_address src = {0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};

            // Example data frame
            m_frameControl = IEEE_802_15_4_MAC_DATA | IEEE_802_15_4_MAC_DEST_ADDRESS_EXT | IEEE_802_15_4_MAC_SOURCE_ADDRESS_EXT;
            m_sequenceNumber = 0;
            m_destPANID = 0xbabe;
            memcpy(m_destAddress, dst, sizeof(m_destAddress));
            m_sourcePANID = 0xbabe;
            memcpy(m_sourceAddress, src, sizeof(m_sourceAddress));
            strcpy((char *)m_payload, "SDR Angel does 15.4");
            m_payloadLength = strlen((char *)m_payload);
        }
    }

    char *bytesToHex(char *buf, uint8_t *data, int len)
    {
        for (int i = 0; i < len; i++)
            buf += sprintf(buf, "%02x ", data[i]);
        return buf;
    }

    void toHexCharArray(char *buf)
    {
        buf += sprintf(buf, "%02x %02x %02x ", m_frameControl & 0xff, (m_frameControl >> 8) & 0xff, m_sequenceNumber);

        if ((m_frameControl & IEEE_802_15_4_MAC_FRAME_TYPE_MASK) != IEEE_802_15_4_MAC_ACK)
            buf += sprintf(buf, "%02x %02x ", m_destPANID & 0xff, (m_destPANID >> 8) & 0xff);
        if ((m_frameControl & IEEE_802_15_4_MAC_DEST_ADDRESS_MASK) == IEEE_802_15_4_MAC_DEST_ADDRESS_EXT)
            buf = bytesToHex(buf, m_destAddress, sizeof(m_destAddress));
        else if ((m_frameControl & IEEE_802_15_4_MAC_DEST_ADDRESS_MASK) == IEEE_802_15_4_MAC_DEST_ADDRESS_SHORT)
            buf += sprintf(buf, "%02x %02x ", m_destShortAddress & 0xff, (m_destShortAddress >> 8) & 0xff);

        if (((m_frameControl & IEEE_802_15_4_MAC_FRAME_TYPE_MASK) != IEEE_802_15_4_MAC_ACK)
          && (!(m_frameControl & IEEE_802_15_4_MAC_PAN_COMPRESSION) || (m_destPANID != m_sourcePANID)))
            buf += sprintf(buf, "%02x %02x ", m_sourcePANID & 0xff, (m_sourcePANID >> 8) & 0xff);
        if ((m_frameControl & IEEE_802_15_4_MAC_SOURCE_ADDRESS_MASK) == IEEE_802_15_4_MAC_SOURCE_ADDRESS_EXT)
            buf = bytesToHex(buf, m_sourceAddress, sizeof(m_sourceAddress));
        else if ((m_frameControl & IEEE_802_15_4_MAC_SOURCE_ADDRESS_MASK) == IEEE_802_15_4_MAC_SOURCE_ADDRESS_SHORT)
            buf += sprintf(buf, "%02x %02x ", m_sourceShortAddress & 0xff, (m_sourceShortAddress >> 8) & 0xff);

        buf = bytesToHex(buf, m_payload, m_payloadLength);
    }

};

#endif /* INCLUDE_IEEE_802_15_4_MACFRAME_H */
