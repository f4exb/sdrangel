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
#include "ilsdemodsettings.h"

ILSDemodSettings::ILSDemodSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_scopeGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void ILSDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 15000.0f; // 15k to support offset carrier
    m_mode = LOC;
    m_frequencyIndex = 0;
    m_squelch = -60.0;
    m_volume = 2.0;
    m_audioMute = false;
    m_average = false;
    m_ddmUnits = FULL_SCALE;
    m_identThreshold = 4.0f;
    m_ident = "";
    m_runway = "";
    m_trueBearing = 0.0f;
    m_slope = 0.0f;
    m_latitude = "";
    m_longitude = "";
    m_elevation = 0;
    m_glidePath = 3.0f;
    m_refHeight = 15.25;
    m_courseWidth = 4.0f;
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_logFilename = "ils_log.csv";
    m_logEnabled = false;
    m_scopeCh1 = 0;
    m_scopeCh2 = 1;

    m_rgbColor = QColor(0, 205, 200).rgb();
    m_title = "ILS Demodulator";
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

QByteArray ILSDemodSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeFloat(2, m_rfBandwidth);
    s.writeS32(3, (int) m_mode);
    s.writeS32(4, m_frequencyIndex);
    s.writeS32(5, m_squelch);
    s.writeFloat(6, m_volume);
    s.writeBool(7, m_audioMute);
    s.writeBool(8, m_average);
    s.writeS32(9, (int) m_ddmUnits);
    s.writeFloat(10, m_identThreshold);
    s.writeString(11, m_ident);
    s.writeString(12, m_runway);
    s.writeFloat(13, m_trueBearing);
    s.writeFloat(14, m_slope);
    s.writeString(15, m_latitude);
    s.writeString(16, m_longitude);
    s.writeS32(17, m_elevation);
    s.writeFloat(18, m_glidePath);
    s.writeFloat(19, m_refHeight);
    s.writeFloat(20, m_courseWidth);
    s.writeBool(21, m_udpEnabled);
    s.writeString(22, m_udpAddress);
    s.writeU32(23, m_udpPort);
    s.writeString(24, m_logFilename);
    s.writeBool(25, m_logEnabled);
    s.writeS32(26, m_scopeCh1);
    s.writeS32(27, m_scopeCh2);

    s.writeU32(40, m_rgbColor);
    s.writeString(41, m_title);
    if (m_channelMarker) {
        s.writeBlob(42, m_channelMarker->serialize());
    }
    s.writeString(43, m_audioDeviceName);
    s.writeS32(44, m_streamIndex);
    s.writeBool(45, m_useReverseAPI);
    s.writeString(46, m_reverseAPIAddress);
    s.writeU32(47, m_reverseAPIPort);
    s.writeU32(48, m_reverseAPIDeviceIndex);
    s.writeU32(49, m_reverseAPIChannelIndex);
    if (m_spectrumGUI) {
        s.writeBlob(50, m_spectrumGUI->serialize());
    }
    if (m_scopeGUI) {
        s.writeBlob(51, m_scopeGUI->serialize());
    }
    if (m_rollupState) {
        s.writeBlob(52, m_rollupState->serialize());
    }
    s.writeS32(53, m_workspaceIndex);
    s.writeBlob(54, m_geometryBytes);
    s.writeBool(55, m_hidden);

    return s.final();
}

bool ILSDemodSettings::deserialize(const QByteArray& data)
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
        d.readFloat(2, &m_rfBandwidth, 15000.0f);
        d.readS32(3, (int *) &m_mode, (int) LOC);
        d.readS32(4, &m_frequencyIndex, 0);
        d.readS32(5, &m_squelch, -40);
        d.readFloat(6, &m_volume, 2.0f);
        d.readBool(7, &m_audioMute, false);
        d.readBool(8, &m_average, false);
        d.readS32(9, (int *) &m_ddmUnits, (int) FULL_SCALE);
        d.readFloat(10, &m_identThreshold, 4.0f);
        d.readString(11, &m_ident, "");
        d.readString(12, &m_runway, "");
        d.readFloat(13, &m_trueBearing, 0.0f);
        d.readFloat(14, &m_slope, 0.0f);
        d.readString(15, &m_latitude, "");
        d.readString(16, &m_longitude, "");
        d.readS32(17, &m_elevation, 0);
        d.readFloat(18, &m_glidePath, 30.f);
        d.readFloat(19, &m_refHeight, 15.25f);
        d.readFloat(20, &m_courseWidth, 4.0f);

        d.readBool(21, &m_udpEnabled);
        d.readString(22, &m_udpAddress);
        d.readU32(23, &utmp);
        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }
        d.readString(24, &m_logFilename, "ils_log.csv");
        d.readBool(25, &m_logEnabled, false);
        d.readS32(26, &m_scopeCh1, 0);
        d.readS32(27, &m_scopeCh2, 0);

        d.readU32(40, &m_rgbColor, QColor(0, 205, 200).rgb());
        d.readString(41, &m_title, "ILS Demodulator");
        if (m_channelMarker)
        {
            d.readBlob(42, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }
        d.readString(43, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readS32(44, &m_streamIndex, 0);
        d.readBool(45, &m_useReverseAPI, false);
        d.readString(46, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(47, &utmp, 0);
        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }
        d.readU32(48, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(49, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        if (m_spectrumGUI)
        {
            d.readBlob(50, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }
        if (m_scopeGUI)
        {
            d.readBlob(51, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }
        if (m_rollupState)
        {
            d.readBlob(52, &bytetmp);
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

