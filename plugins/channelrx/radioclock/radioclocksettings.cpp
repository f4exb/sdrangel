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
#include "radioclocksettings.h"

RadioClockSettings::RadioClockSettings() :
    m_channelMarker(0),
    m_scopeGUI(0)
{
    resetToDefaults();
}

void RadioClockSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 50.0f;
    m_threshold = 5;
    m_modulation = MSF;
    m_timezone = BROADCAST;
    m_rgbColor = QColor(102, 0, 0).rgb();
    m_title = "Radio Clock";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
}

QByteArray RadioClockSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeFloat(2, m_rfBandwidth);
    s.writeFloat(4, m_threshold);
    s.writeS32(5, (int)m_modulation);
    s.writeS32(6, (int)m_timezone);
    s.writeU32(12, m_rgbColor);
    s.writeString(13, m_title);
    if (m_channelMarker) {
        s.writeBlob(14, m_channelMarker->serialize());
    }
    s.writeS32(15, m_streamIndex);
    s.writeBool(16, m_useReverseAPI);
    s.writeString(17, m_reverseAPIAddress);
    s.writeU32(18, m_reverseAPIPort);
    s.writeU32(19, m_reverseAPIDeviceIndex);
    s.writeU32(20, m_reverseAPIChannelIndex);
    s.writeBlob(21, m_scopeGUI->serialize());

    return s.final();
}

bool RadioClockSettings::deserialize(const QByteArray& data)
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
        d.readFloat(2, &m_rfBandwidth, 50.0f);
        d.readFloat(4, &m_threshold, 30);
        d.readS32(5, (int *)&m_modulation, DCF77);
        d.readS32(6, (int *)&m_timezone, BROADCAST);
        d.readU32(12, &m_rgbColor, QColor(102, 0, 0).rgb());
        d.readString(13, &m_title, "Radio Clock");
        d.readBlob(14, &bytetmp);
        if (m_channelMarker) {
            m_channelMarker->deserialize(bytetmp);
        }
        d.readS32(15, &m_streamIndex, 0);
        d.readBool(16, &m_useReverseAPI, false);
        d.readString(17, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(18, &utmp, 0);
        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }
        d.readU32(19, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(20, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        if (m_scopeGUI)
        {
            d.readBlob(21, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}


