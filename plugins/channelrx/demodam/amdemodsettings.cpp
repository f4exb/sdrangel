///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
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
#include "settings/rollupstate.h"
#include "amdemodsettings.h"

AMDemodSettings::AMDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void AMDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 5000;
    m_squelch = -40.0;
    m_volume = 2.0;
    m_audioMute = false;
    m_bandpassEnable = false;
    m_afBandwidth = 5000;
    m_rgbColor = QColor(255, 255, 0).rgb();
    m_title = "AM Demodulator";
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_pll = false;
    m_syncAMOperation = SyncAMDSB;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray AMDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_rfBandwidth/100);
    s.writeS32(3, m_streamIndex);
    s.writeS32(4, m_volume*10);
    s.writeS32(5, m_squelch);

    if (m_channelMarker) {
        s.writeBlob(6, m_channelMarker->serialize());
    }

    s.writeU32(7, m_rgbColor);
    s.writeBool(8, m_bandpassEnable);
    s.writeString(9, m_title);
    s.writeReal(10, m_afBandwidth);
    s.writeString(11, m_audioDeviceName);
    s.writeBool(12, m_pll);
    s.writeS32(13, (int) m_syncAMOperation);
    s.writeBool(14, m_useReverseAPI);
    s.writeString(15, m_reverseAPIAddress);
    s.writeU32(16, m_reverseAPIPort);
    s.writeU32(17, m_reverseAPIDeviceIndex);
    s.writeU32(18, m_reverseAPIChannelIndex);

    if (m_rollupState) {
        s.writeBlob(19, m_rollupState->serialize());
    }

    s.writeS32(20, m_workspaceIndex);
    s.writeBlob(21, m_geometryBytes);
    s.writeBool(22, m_hidden);
    s.writeBool(23, m_audioMute);

    return s.final();
}

bool AMDemodSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &tmp, 4);
        m_rfBandwidth = 100 * tmp;
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

        d.readU32(7, &m_rgbColor);
        d.readBool(8, &m_bandpassEnable, false);
        d.readString(9, &m_title, "AM Demodulator");
        d.readReal(10, &m_afBandwidth, 5000);
        d.readString(11, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readBool(12, &m_pll, false);
        d.readS32(13, &tmp, 0);
        m_syncAMOperation = tmp < 0 ? SyncAMDSB : tmp > 2 ? SyncAMDSB : (SyncAMOperation) tmp;
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

        if (m_rollupState)
        {
            d.readBlob(19, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(20, &m_workspaceIndex, 0);
        d.readBlob(21, &m_geometryBytes);
        d.readBool(22, &m_hidden, false);
        d.readBool(23, &m_audioMute);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}


