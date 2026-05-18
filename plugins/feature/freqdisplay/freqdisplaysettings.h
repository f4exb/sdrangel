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

#ifndef INCLUDE_FEATURE_FREQDISPLAYSETTINGS_H_
#define INCLUDE_FEATURE_FREQDISPLAYSETTINGS_H_

#include <QByteArray>
#include <QColor>
#include <QString>
#include <QStringList>

class Serializable;

struct FreqDisplaySettings
{
    enum DisplayMode {
        Frequency = 0, //!< Show channel centre frequency
        Power     = 1, //!< Show channel power in dB
        Both      = 2  //!< Show frequency and power on two lines
    };

    enum FrequencyUnits {
        Hz  = 0,
        kHz = 1,
        MHz = 2,
        GHz = 3
    };

    QString m_title;
    QString m_selectedChannel;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    QString m_fontName;
    bool m_transparentBackground;
    DisplayMode m_displayMode;
    bool m_speechEnabled;
    FrequencyUnits m_frequencyUnits; //!< Units to use when displaying frequency
    bool m_showUnits;                //!< Whether to append unit labels to displayed values
    int m_freqDecimalPlaces;         //!< Decimal places for frequency value (max depends on units: kHz→3, MHz→6, GHz→9)
    int m_powerDecimalPlaces;        //!< Decimal places for power value (0–3)
    QColor m_textColor;              //!< Color of the frequency / power text
    bool m_dropShadowEnabled;        //!< Whether a drop shadow is applied to the frequency / power text
    QColor m_dropShadowColor;        //!< Color of the drop shadow
    Serializable *m_rollupState;
    quint32 m_rgbColor;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    bool m_activeOnly;              //!< Only display frequency/power for channels that are active (unmuted and squelch open)

    FreqDisplaySettings();
    ~FreqDisplaySettings() = default;

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void applySettings(const QStringList& settingsKeys, const FreqDisplaySettings& settings);
};

#endif // INCLUDE_FEATURE_FREQDISPLAYSETTINGS_H_
