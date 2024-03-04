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

#include "endoftrainpacket.h"

#include "util/crc.h"

// Reverse order of bits
static quint32 reverse(quint32 x)
{
    x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
    x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
    x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
    x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
    return((x >> 16) | (x << 16));
}

// Reverse order of bits, for specified number of bits
static quint32 reverseBits(quint32 x, int bits)
{
    return reverse(x) >> (32-bits);
}

// Decode end-of-train packet
// Defined in:
//  AAR MSRP (Manual of Standards and Recommended Practices) - Section K Part II Locomotive Electronics and Train Consist System Architecture - S-9152 End of Train Communication
// but not readily available, so see:
// https://patents.google.com/patent/US5374015A/en
// https://rdso.indianrailways.gov.in/uploads/files/TC0156_15_02_2022.pdf

bool EndOfTrainPacket::decode(const QByteArray& packet)
{
    //qDebug() << packet.toHex();

    if (packet.size() != 8) {
        return false;
    }

    m_chainingBits = packet[0] & 0x3;

    m_batteryCondition = (packet[0] >> 2) & 0x3;

    m_type = (packet[0] >> 4) & 0x7;

    m_address = ((packet[2] & 0xff) << 9) | ((packet[1] & 0xff) << 1) | ((packet[0] >> 7) & 0x1);

    m_pressure = packet[3] & 0x7f;

    m_discretionary = (packet[3] >> 7) & 1;

    m_batteryCharge = packet[4] & 0x7f;

    m_valveCircuitStatus = (packet[4] >> 7) & 1;

    m_confirmation = packet[5] & 1;
    m_turbine = (packet[5] >> 1) & 1;
    m_motion = (packet[5] >> 2) & 1;
    m_markerLightBatteryCondition = (packet[5] >> 3) & 1;
    m_markerLightStatus = (packet[5] >> 4) & 1;

    // Calculate and compare CRC

    crc crc18(18, 0x39A0F, true, 0, 0x2b770);

    // 45-bits are protected
    crc18.calculate(packet[5] & 0x1f, 5);
    crc18.calculate(packet[4] & 0xff, 8);
    crc18.calculate(packet[3] & 0xff, 8);
    crc18.calculate(packet[2] & 0xff, 8);
    crc18.calculate(packet[1] & 0xff, 8);
    crc18.calculate(packet[0] & 0xff, 8);

    m_crc = ((packet[7] & 0xff) << 11) | ((packet[6] & 0xff) << 3) | ((packet[5] >> 5) & 0x7);
    m_crcCalculated = reverseBits(crc18.get(), 18);
    m_crcValid = m_crc == m_crcCalculated;

    if (crc18.get() != m_crc) {
        qDebug() << "CRC Mismatch: " << QString::number(crc18.get(), 16) << QString::number(m_crc, 16);
    }

    m_dataHex = QString(packet.toHex());

    return m_crcValid;
}
