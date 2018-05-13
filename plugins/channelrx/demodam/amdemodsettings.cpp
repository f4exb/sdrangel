///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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
#include "amdemodsettings.h"

AMDemodSettings::AMDemodSettings() :
    m_channelMarker(0)
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
    m_rgbColor = QColor(255, 255, 0).rgb();
    m_title = "AM Demodulator";
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_pll = false;
    m_syncAMOperation = SyncAMDSB;
}

QByteArray AMDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_rfBandwidth/100);
    s.writeS32(4, m_volume*10);
    s.writeS32(5, m_squelch);

    if (m_channelMarker) {
        s.writeBlob(6, m_channelMarker->serialize());
    }

    s.writeU32(7, m_rgbColor);
    s.writeBool(8, m_bandpassEnable);
    s.writeString(9, m_title);
    s.writeString(11, m_audioDeviceName);
    s.writeBool(12, m_pll);
    s.writeS32(13, (int) m_syncAMOperation);

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
        QString strtmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &tmp, 4);
        m_rfBandwidth = 100 * tmp;
        d.readS32(4, &tmp, 20);
        m_volume = tmp * 0.1;
        d.readS32(5, &tmp, -40);
        m_squelch = tmp;
        d.readBlob(6, &bytetmp);

        if (m_channelMarker) {
            m_channelMarker->deserialize(bytetmp);
        }

        d.readU32(7, &m_rgbColor);
        d.readBool(8, &m_bandpassEnable, false);
        d.readString(9, &m_title, "AM Demodulator");
        d.readString(11, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readBool(12, &m_pll, false);
        d.readS32(13, &tmp, 0);
        m_syncAMOperation = tmp < 0 ? SyncAMDSB : tmp > 2 ? SyncAMDSB : (SyncAMOperation) tmp;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}


