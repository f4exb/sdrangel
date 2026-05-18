///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by Copilot                                                          //
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

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "freqdisplaysettings.h"

FreqDisplaySettings::FreqDisplaySettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void FreqDisplaySettings::resetToDefaults()
{
    m_title = "Frequency Display";
    m_selectedChannel.clear();
    m_workspaceIndex = -1;
    m_geometryBytes.clear();
    m_fontName.clear();
    m_transparentBackground = false;
    m_displayMode = Frequency;
    m_speechEnabled = false;
    m_frequencyUnits = Hz;
    m_showUnits = true;
    m_freqDecimalPlaces = 3;
    m_powerDecimalPlaces = 1;
    m_textColor = Qt::white;
    m_dropShadowEnabled = false;
    m_dropShadowColor = Qt::black;
    m_rgbColor = QColor(126, 132, 247).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_activeOnly = false;
}

QByteArray FreqDisplaySettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_title);
    s.writeString(2, m_selectedChannel);
    s.writeS32(3, m_workspaceIndex);
    s.writeBlob(4, m_geometryBytes);
    s.writeString(5, m_fontName);
    s.writeBool(6, m_transparentBackground);
    s.writeS32(7, static_cast<int>(m_displayMode));
    s.writeBool(8, m_speechEnabled);
    s.writeS32(9, static_cast<int>(m_frequencyUnits));
    s.writeBool(10, m_showUnits);
    s.writeS32(11, m_freqDecimalPlaces);
    s.writeS32(12, m_powerDecimalPlaces);
    s.writeU32(13, m_textColor.rgba());
    s.writeBool(15, m_dropShadowEnabled);
    s.writeU32(16, m_dropShadowColor.rgba());
    if (m_rollupState) {
        s.writeBlob(14, m_rollupState->serialize());
    }
    s.writeU32(17, m_rgbColor);
    s.writeBool(18, m_useReverseAPI);
    s.writeString(19, m_reverseAPIAddress);
    s.writeU32(20, m_reverseAPIPort);
    s.writeU32(21, m_reverseAPIFeatureSetIndex);
    s.writeU32(22, m_reverseAPIFeatureIndex);
    s.writeBool(23, m_activeOnly);

    return s.final();
}

bool FreqDisplaySettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);
    QByteArray bytetmp;
    uint32_t utmp;

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        d.readString(1, &m_title, "Frequency Display");
        d.readString(2, &m_selectedChannel, "");
        d.readS32(3, &m_workspaceIndex, -1);
        d.readBlob(4, &m_geometryBytes);
        d.readString(5, &m_fontName, "");
        d.readBool(6, &m_transparentBackground, false);
        int displayMode = 0;
        d.readS32(7, &displayMode, 0);
        m_displayMode = static_cast<DisplayMode>(displayMode);
        d.readBool(8, &m_speechEnabled, false);
        int frequencyUnits = 0;
        d.readS32(9, &frequencyUnits, 0);
        m_frequencyUnits = static_cast<FrequencyUnits>(frequencyUnits);
        d.readBool(10, &m_showUnits, true);
        d.readS32(11, &m_freqDecimalPlaces, 3);
        d.readS32(12, &m_powerDecimalPlaces, 1);
        quint32 rgba = QColor(Qt::white).rgba();
        d.readU32(13, &rgba, QColor(Qt::white).rgba());
        m_textColor = QColor::fromRgba(rgba);
        d.readBool(15, &m_dropShadowEnabled, false);
        quint32 shadowRgba = QColor(Qt::black).rgba();
        d.readU32(16, &shadowRgba, QColor(Qt::black).rgba());
        m_dropShadowColor = QColor::fromRgba(shadowRgba);
        if (m_rollupState)
        {
            d.readBlob(14, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }
        d.readU32(17, &m_rgbColor, QColor(126, 132, 247).rgb());
        d.readBool(18, &m_useReverseAPI, false);
        d.readString(19, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(20, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(21, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(22, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;

        d.readBool(23, &m_activeOnly);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void FreqDisplaySettings::applySettings(const QStringList& settingsKeys, const FreqDisplaySettings& settings)
{
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("selectedChannel")) {
        m_selectedChannel = settings.m_selectedChannel;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("geometryBytes")) {
        m_geometryBytes = settings.m_geometryBytes;
    }
    if (settingsKeys.contains("fontName")) {
        m_fontName = settings.m_fontName;
    }
    if (settingsKeys.contains("transparentBackground")) {
        m_transparentBackground = settings.m_transparentBackground;
    }
    if (settingsKeys.contains("displayMode")) {
        m_displayMode = settings.m_displayMode;
    }
    if (settingsKeys.contains("speechEnabled")) {
        m_speechEnabled = settings.m_speechEnabled;
    }
    if (settingsKeys.contains("frequencyUnits")) {
        m_frequencyUnits = settings.m_frequencyUnits;
    }
    if (settingsKeys.contains("showUnits")) {
        m_showUnits = settings.m_showUnits;
    }
    if (settingsKeys.contains("freqDecimalPlaces")) {
        m_freqDecimalPlaces = settings.m_freqDecimalPlaces;
    }
    if (settingsKeys.contains("powerDecimalPlaces")) {
        m_powerDecimalPlaces = settings.m_powerDecimalPlaces;
    }
    if (settingsKeys.contains("textColor")) {
        m_textColor = settings.m_textColor;
    }
    if (settingsKeys.contains("dropShadowEnabled")) {
        m_dropShadowEnabled = settings.m_dropShadowEnabled;
    }
    if (settingsKeys.contains("dropShadowColor")) {
        m_dropShadowColor = settings.m_dropShadowColor;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
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
    if (settingsKeys.contains("reverseAPIFeatureSetIndex")) {
        m_reverseAPIFeatureSetIndex = settings.m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex")) {
        m_reverseAPIFeatureIndex = settings.m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("activeOnly")) {
        m_activeOnly = settings.m_activeOnly;
    }
}
