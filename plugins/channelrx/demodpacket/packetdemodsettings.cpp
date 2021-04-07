///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
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

#include <QColor>

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "packetdemodsettings.h"

PacketDemodSettings::PacketDemodSettings() :
    m_channelMarker(0)
{
    resetToDefaults();
}

void PacketDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_baud = 1200;
    m_rfBandwidth = 12500.0f;
    m_fmDeviation = 2500.0f;
    m_filterFrom = "";
    m_filterTo = "";
    m_filterPID = "";
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;

    m_rgbColor = QColor(0, 105, 2).rgb();
    m_title = "Packet Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;

    for (int i = 0; i < PACKETDEMOD_COLUMNS; i++)
    {
        m_columnIndexes[i] = i;
        m_columnSizes[i] = -1; // Autosize
    }
}

QByteArray PacketDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_streamIndex);
    s.writeString(3, m_filterFrom);
    s.writeString(4, m_filterTo);
    s.writeString(5, m_filterPID);

    if (m_channelMarker) {
        s.writeBlob(6, m_channelMarker->serialize());
    }

    s.writeU32(7, m_rgbColor);
    s.writeString(9, m_title);
    s.writeBool(14, m_useReverseAPI);
    s.writeString(15, m_reverseAPIAddress);
    s.writeU32(16, m_reverseAPIPort);
    s.writeU32(17, m_reverseAPIDeviceIndex);
    s.writeU32(18, m_reverseAPIChannelIndex);

    s.writeFloat(20, m_rfBandwidth);
    s.writeFloat(21, m_fmDeviation);
    s.writeBool(22, m_udpEnabled);
    s.writeString(23, m_udpAddress);
    s.writeU32(24, m_udpPort);

    for (int i = 0; i < PACKETDEMOD_COLUMNS; i++)
        s.writeS32(100 + i, m_columnIndexes[i]);
    for (int i = 0; i < PACKETDEMOD_COLUMNS; i++)
        s.writeS32(200 + i, m_columnSizes[i]);

    return s.final();
}

bool PacketDemodSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t utmp;
        QString strtmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &m_streamIndex, 0);
        d.readString(3, &m_filterFrom, "");
        d.readString(4, &m_filterTo, "");
        d.readString(5, &m_filterPID, "");
        d.readBlob(6, &bytetmp);

        if (m_channelMarker) {
            m_channelMarker->deserialize(bytetmp);
        }

        d.readU32(7, &m_rgbColor, QColor(0, 105, 2).rgb());
        d.readString(9, &m_title, "Packet Demodulator");
        d.readBool(14, &m_useReverseAPI, false);
        d.readString(15, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(16, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(17, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(18, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        d.readFloat(20, &m_rfBandwidth, 12500.0f);
        d.readFloat(21, &m_fmDeviation, 2500.0f);

        d.readBool(22, &m_udpEnabled);
        d.readString(23, &m_udpAddress);
        d.readU32(24, &utmp);
        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }

        for (int i = 0; i < PACKETDEMOD_COLUMNS; i++)
            d.readS32(100 + i, &m_columnIndexes[i], i);
        for (int i = 0; i < PACKETDEMOD_COLUMNS; i++)
            d.readS32(200 + i, &m_columnSizes[i], -1);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}


