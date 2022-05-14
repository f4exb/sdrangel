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
#include "dabdemodsettings.h"

DABDemodSettings::DABDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void DABDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 1537000.0f;
    m_filter = "";
    m_volume = 5.0f;
    m_audioMute = false;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;

    m_rgbColor = QColor(77, 105, 25).rgb();
    m_title = "DAB Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;

    for (int i = 0; i < DABDEMOD_COLUMNS; i++)
    {
        m_columnIndexes[i] = i;
        m_columnSizes[i] = -1; // Autosize
    }
}

QByteArray DABDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_streamIndex);
    s.writeString(3, m_filter);
    s.writeFloat(4, m_rfBandwidth);
    s.writeFloat(5, m_volume);
    s.writeBool(6, m_audioMute);
    s.writeString(7, m_audioDeviceName);

    if (m_channelMarker) {
        s.writeBlob(8, m_channelMarker->serialize());
    }

    s.writeU32(9, m_rgbColor);
    s.writeString(10, m_title);
    s.writeBool(11, m_useReverseAPI);
    s.writeString(12, m_reverseAPIAddress);
    s.writeU32(13, m_reverseAPIPort);
    s.writeU32(14, m_reverseAPIDeviceIndex);
    s.writeU32(15, m_reverseAPIChannelIndex);

    if (m_rollupState) {
        s.writeBlob(16, m_rollupState->serialize());
    }

    s.writeS32(17, m_workspaceIndex);
    s.writeBlob(18, m_geometryBytes);
    s.writeBool(19, m_hidden);

    for (int i = 0; i < DABDEMOD_COLUMNS; i++) {
        s.writeS32(100 + i, m_columnIndexes[i]);
    }

    for (int i = 0; i < DABDEMOD_COLUMNS; i++) {
        s.writeS32(200 + i, m_columnSizes[i]);
    }

    return s.final();
}

bool DABDemodSettings::deserialize(const QByteArray& data)
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
        d.readString(3, &m_filter, "");
        d.readFloat(4, &m_rfBandwidth, 1537000.0f);
        d.readFloat(5, &m_volume, 5.0f);
        d.readBool(6, &m_audioMute, false);
        d.readString(7, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);

        if (m_channelMarker)
        {
            d.readBlob(8, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readU32(9, &m_rgbColor, QColor(77, 105, 25).rgb());
        d.readString(10, &m_title, "DAB Demodulator");
        d.readBool(11, &m_useReverseAPI, false);
        d.readString(12, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(13, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(14, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(15, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(16, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(17, &m_workspaceIndex, 0);
        d.readBlob(18, &m_geometryBytes);
        d.readBool(19, &m_hidden, false);

        for (int i = 0; i < DABDEMOD_COLUMNS; i++) {
            d.readS32(100 + i, &m_columnIndexes[i], i);
        }

        for (int i = 0; i < DABDEMOD_COLUMNS; i++) {
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


