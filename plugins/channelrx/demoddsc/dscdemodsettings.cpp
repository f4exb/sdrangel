///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
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

#include <QColor>

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "dscdemodsettings.h"

DSCDemodSettings::DSCDemodSettings() :
    m_channelMarker(nullptr),
    m_scopeGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void DSCDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 450.0f; // OBW for 2FSK = 2 * deviation + data rate. Then add a bit for carrier frequency offset
    m_filterInvalid = true;
    m_filterColumn = 4;
    m_filter = "";
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_logFilename = "dsc_log.csv";
    m_logEnabled = false;
    m_feed = true;

    m_rgbColor = QColor(181, 230, 29).rgb();
    m_title = "DSC Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;

    for (int i = 0; i < DSCDEMOD_COLUMNS; i++)
    {
        m_columnIndexes[i] = i;
        m_columnSizes[i] = -1; // Autosize
    }
}

QByteArray DSCDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_streamIndex);
    s.writeBool(3, m_filterInvalid);
    s.writeS32(4, m_filterColumn);
    s.writeString(5, m_filter);

    if (m_channelMarker) {
        s.writeBlob(6, m_channelMarker->serialize());
    }
    s.writeFloat(7, m_rfBandwidth);

    s.writeBool(9, m_udpEnabled);
    s.writeString(10, m_udpAddress);
    s.writeU32(11, m_udpPort);
    s.writeString(12, m_logFilename);
    s.writeBool(13, m_logEnabled);
    s.writeBool(14, m_feed);

    s.writeU32(20, m_rgbColor);
    s.writeString(21, m_title);
    s.writeBool(22, m_useReverseAPI);
    s.writeString(23, m_reverseAPIAddress);
    s.writeU32(24, m_reverseAPIPort);
    s.writeU32(25, m_reverseAPIDeviceIndex);
    s.writeU32(26, m_reverseAPIChannelIndex);

    if (m_rollupState) {
        s.writeBlob(27, m_rollupState->serialize());
    }

    s.writeS32(28, m_workspaceIndex);
    s.writeBlob(29, m_geometryBytes);
    s.writeBool(30, m_hidden);
    s.writeBlob(31, m_scopeGUI->serialize());

    for (int i = 0; i < DSCDEMOD_COLUMNS; i++) {
        s.writeS32(100 + i, m_columnIndexes[i]);
    }

    for (int i = 0; i < DSCDEMOD_COLUMNS; i++) {
        s.writeS32(200 + i, m_columnSizes[i]);
    }

    return s.final();
}

bool DSCDemodSettings::deserialize(const QByteArray& data)
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
        d.readBool(3, &m_filterInvalid, true);
        d.readS32(4, &m_filterColumn, 5);
        d.readString(5, &m_filter, "");

        if (m_channelMarker)
        {
            d.readBlob(6, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }
        d.readFloat(7, &m_rfBandwidth, 450.0f);

        d.readBool(9, &m_udpEnabled);
        d.readString(10, &m_udpAddress);
        d.readU32(11, &utmp);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }
        d.readString(12, &m_logFilename, "dsc_log.csv");
        d.readBool(13, &m_logEnabled, false);
        d.readBool(14, &m_feed, true);

        d.readU32(20, &m_rgbColor, QColor(181, 230, 29).rgb());
        d.readString(21, &m_title, "DSC Demodulator");
        d.readBool(22, &m_useReverseAPI, false);
        d.readString(23, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(24, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(25, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(26, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(27, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(28, &m_workspaceIndex, 0);
        d.readBlob(29, &m_geometryBytes);
        d.readBool(30, &m_hidden, false);

        if (m_scopeGUI)
        {
            d.readBlob(31, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        for (int i = 0; i < DSCDEMOD_COLUMNS; i++) {
            d.readS32(100 + i, &m_columnIndexes[i], i);
        }

        for (int i = 0; i < DSCDEMOD_COLUMNS; i++) {
            d.readS32(200 + i, &m_columnSizes[i], -1);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

