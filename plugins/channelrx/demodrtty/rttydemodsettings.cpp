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
#include "rttydemodsettings.h"

RttyDemodSettings::RttyDemodSettings() :
    m_channelMarker(nullptr),
    m_scopeGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void RttyDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 400.0f; // OBW for 2FSK = 2 * deviation + data rate. Then add a bit for carrier frequency offset
    m_baudRate = 45.45;
    m_frequencyShift = 170;
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_characterSet = Baudot::ITA2;
    m_suppressCRLF = false;
    m_unshiftOnSpace = false;
    m_filter = LOWPASS;
    m_atc = true;
    m_msbFirst = false;
    m_spaceHigh = false;
    m_squelch = -70;
    m_logFilename = "rtty_log.csv";
    m_logEnabled = false;
    m_scopeCh1 = 0;
    m_scopeCh2 = 1;

    m_rgbColor = QColor(180, 205, 130).rgb();
    m_title = "RTTY Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray RttyDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_streamIndex);

    s.writeFloat(3, m_rfBandwidth);
    s.writeFloat(4, m_baudRate);
    s.writeS32(5, m_frequencyShift);
    s.writeS32(6, (int)m_characterSet);
    s.writeBool(7, m_suppressCRLF);
    s.writeBool(8, m_unshiftOnSpace);
    s.writeS32(9, (int)m_filter);
    s.writeBool(10, m_atc);
    s.writeBool(34, m_msbFirst);
    s.writeBool(35, m_spaceHigh);
    s.writeS32(36, m_squelch);

    if (m_channelMarker) {
        s.writeBlob(11, m_channelMarker->serialize());
    }

    s.writeU32(12, m_rgbColor);
    s.writeString(13, m_title);
    s.writeBool(14, m_useReverseAPI);
    s.writeString(15, m_reverseAPIAddress);
    s.writeU32(16, m_reverseAPIPort);
    s.writeU32(17, m_reverseAPIDeviceIndex);
    s.writeU32(18, m_reverseAPIChannelIndex);

    s.writeBool(22, m_udpEnabled);
    s.writeString(23, m_udpAddress);
    s.writeU32(24, m_udpPort);

    s.writeS32(31, m_scopeCh1);
    s.writeS32(32, m_scopeCh2);
    s.writeBlob(33, m_scopeGUI->serialize());

    s.writeString(25, m_logFilename);
    s.writeBool(26, m_logEnabled);

    if (m_rollupState) {
        s.writeBlob(27, m_rollupState->serialize());
    }

    s.writeS32(28, m_workspaceIndex);
    s.writeBlob(29, m_geometryBytes);
    s.writeBool(30, m_hidden);

    return s.final();
}

bool RttyDemodSettings::deserialize(const QByteArray& data)
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

        d.readFloat(3, &m_rfBandwidth, 450.0f);
        d.readFloat(4, &m_baudRate, 45.45f);
        d.readS32(5, &m_frequencyShift, 170);
        d.readS32(6, (int *)&m_characterSet, (int)Baudot::ITA2);
        d.readBool(7, &m_suppressCRLF, false);
        d.readBool(8, &m_unshiftOnSpace, false);
        d.readS32(9, (int *)&m_filter, (int) LOWPASS);
        d.readBool(10, &m_atc, true);
        d.readBool(34, &m_msbFirst, false);
        d.readBool(35, &m_spaceHigh, false);
        d.readS32(36, &m_squelch, -70);

        if (m_channelMarker)
        {
            d.readBlob(11, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readU32(12, &m_rgbColor, QColor(180, 205, 130).rgb());
        d.readString(13, &m_title, "RTTY Demodulator");
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


        d.readBool(22, &m_udpEnabled);
        d.readString(23, &m_udpAddress);
        d.readU32(24, &utmp);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }

        d.readS32(31, &m_scopeCh1, 0);
        d.readS32(32, &m_scopeCh2, 0);
        if (m_scopeGUI)
        {
            d.readBlob(33, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        d.readString(25, &m_logFilename, "rtty_log.csv");
        d.readBool(26, &m_logEnabled, false);

        if (m_rollupState)
        {
            d.readBlob(27, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(28, &m_workspaceIndex, 0);
        d.readBlob(29, &m_geometryBytes);
        d.readBool(30, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

