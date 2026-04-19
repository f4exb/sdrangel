///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRangel Contributors                                      //
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

#include <sstream>

#include <QColor>

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "cwmodsettings.h"

CWModSettings::CWModSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void CWModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 500;
    m_gain = 0.0f;
    m_channelMute = false;
    m_wpm = 20;
    m_text = "CQ CQ CQ DE SDRangel";
    m_repeat = false;
    m_repeatCount = -1;
    m_rgbColor = QColor(200, 150, 50).rgb();
    m_title = "CW Modulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray CWModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_rfBandwidth);
    s.writeReal(3, m_gain);
    s.writeBool(4, m_channelMute);
    s.writeS32(5, m_wpm);
    s.writeString(6, m_text);
    s.writeBool(7, m_repeat);
    s.writeS32(8, m_repeatCount);
    s.writeU32(20, m_rgbColor);
    s.writeString(21, m_title);

    if (m_channelMarker) {
        s.writeBlob(22, m_channelMarker->serialize());
    }

    s.writeS32(23, m_streamIndex);
    s.writeBool(24, m_useReverseAPI);
    s.writeString(25, m_reverseAPIAddress);
    s.writeU32(26, m_reverseAPIPort);
    s.writeU32(27, m_reverseAPIDeviceIndex);
    s.writeU32(28, m_reverseAPIChannelIndex);

    if (m_rollupState) {
        s.writeBlob(29, m_rollupState->serialize());
    }

    s.writeS32(30, m_workspaceIndex);
    s.writeBlob(31, m_geometryBytes);
    s.writeBool(32, m_hidden);

    return s.final();
}

bool CWModSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        qint32 tmp;
        uint32_t utmp;

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readS32(2, &m_rfBandwidth, 500);
        d.readReal(3, &m_gain, 0.0f);
        d.readBool(4, &m_channelMute, false);
        d.readS32(5, &m_wpm, 20);
        d.readString(6, &m_text, "CQ CQ CQ DE SDRangel");
        d.readBool(7, &m_repeat, false);
        d.readS32(8, &m_repeatCount, -1);
        d.readU32(20, &m_rgbColor, QColor(200, 150, 50).rgb());
        d.readString(21, &m_title, "CW Modulator");

        if (m_channelMarker)
        {
            d.readBlob(22, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(23, &m_streamIndex, 0);
        d.readBool(24, &m_useReverseAPI, false);
        d.readString(25, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(26, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(27, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(28, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(29, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(30, &m_workspaceIndex, 0);
        d.readBlob(31, &m_geometryBytes);
        d.readBool(32, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void CWModSettings::applySettings(const QStringList& settingsKeys, const CWModSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("gain")) {
        m_gain = settings.m_gain;
    }
    if (settingsKeys.contains("channelMute")) {
        m_channelMute = settings.m_channelMute;
    }
    if (settingsKeys.contains("wpm")) {
        m_wpm = settings.m_wpm;
    }
    if (settingsKeys.contains("text")) {
        m_text = settings.m_text;
    }
    if (settingsKeys.contains("repeat")) {
        m_repeat = settings.m_repeat;
    }
    if (settingsKeys.contains("repeatCount")) {
        m_repeatCount = settings.m_repeatCount;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("reverseAPIChannelIndex")) {
        m_reverseAPIChannelIndex = settings.m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
}

QString CWModSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (force || settingsKeys.contains("inputFrequencyOffset")) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (force || settingsKeys.contains("rfBandwidth")) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (force || settingsKeys.contains("gain")) {
        ostr << " m_gain: " << m_gain;
    }
    if (force || settingsKeys.contains("channelMute")) {
        ostr << " m_channelMute: " << m_channelMute;
    }
    if (force || settingsKeys.contains("wpm")) {
        ostr << " m_wpm: " << m_wpm;
    }
    if (force || settingsKeys.contains("text")) {
        ostr << " m_text: " << m_text.toStdString();
    }
    if (force || settingsKeys.contains("repeat")) {
        ostr << " m_repeat: " << m_repeat;
    }
    if (force || settingsKeys.contains("repeatCount")) {
        ostr << " m_repeatCount: " << m_repeatCount;
    }
    if (force || settingsKeys.contains("title")) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (force || settingsKeys.contains("streamIndex")) {
        ostr << " m_streamIndex: " << m_streamIndex;
    }

    return QString(ostr.str().c_str());
}
