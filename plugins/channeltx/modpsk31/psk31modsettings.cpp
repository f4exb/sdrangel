///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
#include <QDebug>

#include "dsp/dspengine.h"
#include "util/baudot.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "psk31modsettings.h"

PSK31Settings::PSK31Settings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void PSK31Settings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_baud = 31.25;
    m_rfBandwidth = 100;
    m_gain = 0.0f;
    m_channelMute = false;
    m_repeat = false;
    m_repeatCount = 10;
    m_lpfTaps = 301;
    m_rfNoise = false;
    m_text = "CQ CQ CQ DE SDRangel CQ";
    m_prefixCRLF = true;
    m_postfixCRLF = true;
    m_predefinedTexts = QStringList({
        "CQ CQ CQ DE ${callsign} ${callsign} CQ",
        "DE ${callsign} ${callsign} ${callsign}",
        "UR 599 QTH IS ${location}",
        "TU DE ${callsign} CQ"
    });
    m_rgbColor = QColor(25, 180, 200).rgb();
    m_title = "PSK31 Modulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_pulseShaping = true;
    m_beta = 1.0f;
    m_symbolSpan = 2;
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9998;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray PSK31Settings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_baud);
    s.writeS32(3, m_rfBandwidth);
    s.writeReal(5, m_gain);
    s.writeBool(6, m_channelMute);
    s.writeBool(7, m_repeat);
    s.writeS32(9, m_repeatCount);
    s.writeS32(23, m_lpfTaps);
    s.writeBool(25, m_rfNoise);
    s.writeString(30, m_text);

    s.writeBool(64, m_prefixCRLF);
    s.writeBool(65, m_postfixCRLF);
    s.writeList(66, m_predefinedTexts);

    s.writeU32(31, m_rgbColor);
    s.writeString(32, m_title);

    if (m_channelMarker) {
        s.writeBlob(33, m_channelMarker->serialize());
    }

    s.writeS32(34, m_streamIndex);
    s.writeBool(35, m_useReverseAPI);
    s.writeString(36, m_reverseAPIAddress);
    s.writeU32(37, m_reverseAPIPort);
    s.writeU32(38, m_reverseAPIDeviceIndex);
    s.writeU32(39, m_reverseAPIChannelIndex);
    s.writeBool(46, m_pulseShaping);
    s.writeReal(47, m_beta);
    s.writeS32(48, m_symbolSpan);
    s.writeBool(51, m_udpEnabled);
    s.writeString(52, m_udpAddress);
    s.writeU32(53, m_udpPort);

    if (m_rollupState) {
        s.writeBlob(54, m_rollupState->serialize());
    }

    s.writeS32(55, m_workspaceIndex);
    s.writeBlob(56, m_geometryBytes);
    s.writeBool(57, m_hidden);

    return s.final();
}

bool PSK31Settings::deserialize(const QByteArray& data)
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
        d.readReal(2, &m_baud, 31.25f);
        d.readS32(3, &m_rfBandwidth, 100);
        d.readReal(5, &m_gain, 0.0f);
        d.readBool(6, &m_channelMute, false);
        d.readBool(7, &m_repeat, false);
        d.readS32(9, &m_repeatCount, -1);
        d.readS32(23, &m_lpfTaps, 301);
        d.readBool(25, &m_rfNoise, false);
        d.readString(30, &m_text, "CQ CQ CQ anyone using SDRangel");

        d.readBool(64, &m_prefixCRLF, true);
        d.readBool(65, &m_postfixCRLF, true);
        d.readList(66, &m_predefinedTexts);

        d.readU32(31, &m_rgbColor);
        d.readString(32, &m_title, "PSK31 Modulator");

        if (m_channelMarker)
        {
            d.readBlob(33, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(34, &m_streamIndex, 0);
        d.readBool(35, &m_useReverseAPI, false);
        d.readString(36, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(37, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(38, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(39, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readBool(46, &m_pulseShaping, true);
        d.readReal(47, &m_beta, 1.0f);
        d.readS32(48, &m_symbolSpan, 2);
        d.readBool(51, &m_udpEnabled);
        d.readString(52, &m_udpAddress, "127.0.0.1");
        d.readU32(53, &utmp);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9998;
        }

        if (m_rollupState)
        {
            d.readBlob(54, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(55, &m_workspaceIndex, 0);
        d.readBlob(56, &m_geometryBytes);
        d.readBool(57, &m_hidden, false);

        return true;
    }
    else
    {
        qDebug() << "PSK31Settings::deserialize: ERROR";
        resetToDefaults();
        return false;
    }
}
