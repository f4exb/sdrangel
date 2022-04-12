///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
#include <QDebug>

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "aismodsettings.h"

AISModSettings::AISModSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void AISModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_baud = 9600; // nominal value
    m_rfBandwidth = 25000.0f; // 12.5k for narrow, 25k for wide (narrow is obsolete)
    m_fmDeviation = 4800.0f; // To give modulation index of 0.5 for 9600 baud
    m_gain = -1.0f; // To avoid overflow, which results in out-of-band RF
    m_channelMute = false;
    m_repeat = false;
    m_repeatDelay = 1.0f;
    m_repeatCount = infinitePackets;
    m_rampUpBits = 0;
    m_rampDownBits = 0;
    m_rampRange = 60;
    m_rfNoise = false;
    m_writeToFile = false;
    m_msgType = MsgTypeScheduledPositionReport;
    m_mmsi = "0000000000";
    m_status = StatusUnderWayUsingEngine;
    m_latitude = 0.0f;
    m_longitude = 0.0f;
    m_course = 0.0f;
    m_speed = 0.0f;
    m_heading = 0;
    m_data = "";
    m_rgbColor = QColor(102, 0, 0).rgb();
    m_title = "AIS Modulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_bt = 0.4f; // 0.3 for narrow, 0.4 for wide
    m_symbolSpan = 3;
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9998;
    m_workspaceIndex = 0;
    m_hidden = false;
}

bool AISModSettings::setMode(QString mode)
{
    if (mode.endsWith("Narrow"))
    {
        m_rfBandwidth = 12500.0f;
        m_fmDeviation = m_baud * 0.25;
        m_bt = 0.3f;
        return true;
    }
    else if (mode.endsWith("Wide"))
    {
        m_rfBandwidth = 25000.0f;
        m_fmDeviation = m_baud * 0.5;
        m_bt = 0.4f;
        return true;
    }
    else
    {
        return false;
    }
}

Real AISModSettings::getRfBandwidth(int modeIndex)
{
    if (modeIndex == 0) { // Narrow
        return 12500.0f;
    } else { // Wide or other
        return 25000.0f;
    }
}

Real AISModSettings::getFMDeviation(int modeIndex)
{
    if (modeIndex == 0) { // Narrow
        return m_baud * 0.25;
    } else { // Wide or other
        return m_baud * 0.5;
    }
}

float AISModSettings::getBT(int modeIndex)
{
    if (modeIndex == 0) { // Narrow
        return 0.3f;
    } else { // Wide or other
        return 0.4f;
    }
}

QString AISModSettings::getMode() const
{
    return QString("%1 %2 %3").arg(m_rfBandwidth).arg(m_fmDeviation).arg(m_bt);
}

int AISModSettings::getMsgId() const
{
    return ((int) m_msgType) + 1;
}

QByteArray AISModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_baud);
    s.writeReal(3, m_rfBandwidth);
    s.writeReal(4, m_fmDeviation);
    s.writeReal(5, m_gain);
    s.writeBool(6, m_channelMute);
    s.writeBool(7, m_repeat);
    s.writeReal(8, m_repeatDelay);
    s.writeS32(9, m_repeatCount);
    s.writeS32(10, m_rampUpBits);
    s.writeS32(11, m_rampDownBits);
    s.writeS32(12, m_rampRange);
    s.writeBool(14, m_rfNoise);
    s.writeBool(15, m_writeToFile);
    s.writeS32(17, (int) m_msgType);
    s.writeString(18, m_mmsi);
    s.writeS32(19, (int) m_status);
    s.writeFloat(20, m_latitude);
    s.writeFloat(21, m_longitude);
    s.writeFloat(22, m_course);
    s.writeFloat(23, m_speed);
    s.writeS32(24, m_heading);
    s.writeString(25, m_data);
    s.writeReal(26, m_bt);
    s.writeS32(27, m_symbolSpan);
    s.writeU32(28, m_rgbColor);
    s.writeString(29, m_title);

    if (m_channelMarker) {
        s.writeBlob(30, m_channelMarker->serialize());
    }

    s.writeS32(31, m_streamIndex);
    s.writeBool(32, m_useReverseAPI);
    s.writeString(33, m_reverseAPIAddress);
    s.writeU32(34, m_reverseAPIPort);
    s.writeU32(35, m_reverseAPIDeviceIndex);
    s.writeU32(36, m_reverseAPIChannelIndex);
    s.writeBool(37, m_udpEnabled);
    s.writeString(38, m_udpAddress);
    s.writeU32(39, m_udpPort);

    if (m_rollupState) {
        s.writeBlob(40, m_rollupState->serialize());
    }

    s.writeS32(41, m_workspaceIndex);
    s.writeBlob(42, m_geometryBytes);
    s.writeBool(43, m_hidden);

    return s.final();
}

bool AISModSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readS32(2, &m_baud, 9600);
        d.readReal(3, &m_rfBandwidth, 25000.0f);
        d.readReal(4, &m_fmDeviation, 4800.0f);
        d.readReal(5, &m_gain, -1.0f);
        d.readBool(6, &m_channelMute, false);
        d.readBool(7, &m_repeat, false);
        d.readReal(8, &m_repeatDelay, 1.0f);
        d.readS32(9, &m_repeatCount, -1);
        d.readS32(10, &m_rampUpBits, 8);
        d.readS32(11, &m_rampDownBits, 8);
        d.readS32(12, &m_rampRange, 8);
        d.readBool(14, &m_rfNoise, false);
        d.readBool(15, &m_writeToFile, false);
        d.readS32(17, &tmp, 0);
        m_msgType = (MsgType) tmp;
        d.readString(18, &m_mmsi, "0000000000");
        d.readS32(19, &tmp, 0);
        m_status = (Status) tmp;
        d.readFloat(20, &m_latitude, 0.0f);
        d.readFloat(21, &m_longitude, 0.0f);
        d.readFloat(22, &m_course, 0.0f);
        d.readFloat(23, &m_speed, 0.0f);
        d.readS32(24, &m_heading, 0);
        d.readString(25, &m_data, "");
        d.readReal(26, &m_bt, 0.3f);
        d.readS32(27, &m_symbolSpan, 3);
        d.readU32(28, &m_rgbColor, QColor(102, 0, 0).rgb());
        d.readString(29, &m_title, "AIS Modulator");

        if (m_channelMarker)
        {
            d.readBlob(30, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(31, &m_streamIndex, 0);
        d.readBool(32, &m_useReverseAPI, false);
        d.readString(33, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(34, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(35, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(36, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readBool(37, &m_udpEnabled);
        d.readString(38, &m_udpAddress, "127.0.0.1");
        d.readU32(39, &utmp);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9998;
        }

        if (m_rollupState)
        {
            d.readBlob(40, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(41, &m_workspaceIndex, 0);
        d.readBlob(42, &m_geometryBytes);
        d.readBool(43, &m_hidden, false);

        return true;
    }
    else
    {
        qDebug() << "AISModSettings::deserialize: ERROR";
        resetToDefaults();
        return false;
    }
}
