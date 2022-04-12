///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "wfmdemodsettings.h"

const int WFMDemodSettings::m_rfBWMin = 10000;
const int WFMDemodSettings::m_rfBWMax = 300000;
const int WFMDemodSettings::m_rfBWDigits = 6;

WFMDemodSettings::WFMDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void WFMDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 80000;
    m_afBandwidth = 15000;
    m_volume = 2.0;
    m_squelch = -60.0;
    m_audioMute = false;
    m_rgbColor = QColor(0, 0, 255).rgb();
    m_title = "WFM Demodulator";
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray WFMDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_rfBandwidth);
    s.writeS32(3, m_afBandwidth/1000.0);
    s.writeS32(4, m_volume*10.0);
    s.writeS32(5, m_squelch);
    s.writeU32(7, m_rgbColor);
    s.writeString(8, m_title);
    s.writeString(9, m_audioDeviceName);

    if (m_channelMarker) {
        s.writeBlob(11, m_channelMarker->serialize());
    }

    s.writeBool(12, m_useReverseAPI);
    s.writeString(13, m_reverseAPIAddress);
    s.writeU32(14, m_reverseAPIPort);
    s.writeU32(15, m_reverseAPIDeviceIndex);
    s.writeU32(16, m_reverseAPIChannelIndex);
    s.writeS32(17, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(18, m_rollupState->serialize());
    }

    s.writeS32(19, m_workspaceIndex);
    s.writeBlob(20, m_geometryBytes);
    s.writeBool(21, m_hidden);

    return s.final();
}

bool WFMDemodSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readS32(2, &tmp, 4);
        m_rfBandwidth = tmp < m_rfBWMin ? m_rfBWMin : tmp > m_rfBWMax ? m_rfBWMax : tmp;
        d.readS32(3, &tmp, 3);
        m_afBandwidth = tmp * 1000.0;
        d.readS32(4, &tmp, 20);
        m_volume = tmp * 0.1;
        d.readS32(5, &tmp, -60);
        m_squelch = tmp;
        d.readU32(7, &m_rgbColor);
        d.readString(8, &m_title, "WFM Demodulator");
        d.readString(9, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);

        if (m_channelMarker)
        {
            d.readBlob(11, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readBool(12, &m_useReverseAPI, false);
        d.readString(13, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(14, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(15, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(16, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(17, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(18, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(19, &m_workspaceIndex, 0);
        d.readBlob(20, &m_geometryBytes);
        d.readBool(21, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
