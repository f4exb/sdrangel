///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
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

#include <QColor>

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "vordemodmcsettings.h"

VORDemodMCSettings::VORDemodMCSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void VORDemodMCSettings::resetToDefaults()
{
    m_squelch = -60.0;
    m_volume = 2.0;
    m_audioMute = false;
    m_rgbColor = QColor(255, 255, 102).rgb();
    m_title = "VOR Demodulator";
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;

    m_identThreshold = 2.0;
    m_refThresholdDB = -45.0;
    m_varThresholdDB = -90.0;
    m_magDecAdjust = true;

    for (int i = 0; i < VORDEMOD_COLUMNS; i++)
    {
        m_columnIndexes[i] = i;
        m_columnSizes[i] = -1; // Autosize
    }
}

QByteArray VORDemodMCSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(3, m_streamIndex);
    s.writeS32(4, m_volume*10);
    s.writeS32(5, m_squelch);

    if (m_channelMarker) {
        s.writeBlob(6, m_channelMarker->serialize());
    }

    s.writeU32(7, m_rgbColor);
    s.writeString(9, m_title);
    s.writeString(11, m_audioDeviceName);
    s.writeBool(14, m_useReverseAPI);
    s.writeString(15, m_reverseAPIAddress);
    s.writeU32(16, m_reverseAPIPort);
    s.writeU32(17, m_reverseAPIDeviceIndex);
    s.writeU32(18, m_reverseAPIChannelIndex);
    s.writeReal(20, m_identThreshold);
    s.writeReal(21, m_refThresholdDB);
    s.writeReal(22, m_varThresholdDB);
    s.writeBool(23, m_magDecAdjust);

    if (m_rollupState) {
        s.writeBlob(24, m_rollupState->serialize());
    }

    s.writeS32(25, m_workspaceIndex);
    s.writeBlob(26, m_geometryBytes);
    s.writeBool(27, m_hidden);

    for (int i = 0; i < VORDEMOD_COLUMNS; i++) {
        s.writeS32(100 + i, m_columnIndexes[i]);
    }

    for (int i = 0; i < VORDEMOD_COLUMNS; i++) {
        s.writeS32(200 + i, m_columnSizes[i]);
    }

    return s.final();
}

bool VORDemodMCSettings::deserialize(const QByteArray& data)
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
        qint32 tmp;
        uint32_t utmp;
        QString strtmp;

        d.readS32(3, &m_streamIndex, 0);
        d.readS32(4, &tmp, 20);
        m_volume = tmp * 0.1;
        d.readS32(5, &tmp, -40);
        m_squelch = tmp;

        if (m_channelMarker)
        {
            d.readBlob(6, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readU32(7, &m_rgbColor, QColor(255, 255, 102).rgb());
        d.readString(9, &m_title, "VOR Demodulator");
        d.readString(11, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
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
        d.readReal(20, &m_identThreshold, 2.0);
        d.readReal(21, &m_refThresholdDB, -45.0);
        d.readReal(22, &m_varThresholdDB, -90.0);
        d.readBool(23, &m_magDecAdjust, true);

        if (m_rollupState)
        {
            d.readBlob(24, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(25, &m_workspaceIndex, 0);
        d.readBlob(26, &m_geometryBytes);
        d.readBool(27, &m_hidden, false);

        for (int i = 0; i < VORDEMOD_COLUMNS; i++) {
            d.readS32(100 + i, &m_columnIndexes[i], i);
        }

        for (int i = 0; i < VORDEMOD_COLUMNS; i++) {
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


