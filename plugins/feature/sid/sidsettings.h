///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023-2024 Jon Beniston, M7RCE                                   //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_SIDSETTINGS_H_
#define INCLUDE_FEATURE_SIDSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <QDateTime>
#include <QColor>

class Serializable;

struct SIDSettings
{
    struct ChannelSettings
    {
        QString m_id;
        bool m_enabled;
        QColor m_color;
        QString m_label;

        QByteArray serialize() const;
        bool deserialize(const QByteArray& data);
    };

    QList<ChannelSettings> m_channelSettings;   // Channels to record power from
    float m_period;                             // Mesaurement period, in seconds

    bool m_autosave;
    bool m_autoload;
    QString m_filename;                 // Filename for autosave
    int m_autosavePeriod;               // In minutes

    int m_samples;                      // Number of samples in average
    bool m_autoscaleX;
    bool m_autoscaleY;
    bool m_separateCharts;
    bool m_displayLegend;
    Qt::Alignment m_legendAlignment;
    bool m_displayAxisTitles;
    bool m_displaySecondaryAxis;
    bool m_plotXRayLongPrimary;
    bool m_plotXRayLongSecondary;
    bool m_plotXRayShortPrimary;
    bool m_plotXRayShortSecondary;
    bool m_plotGRB;
    bool m_plotSTIX;
    bool m_plotProton;
    QDateTime m_startDateTime;
    QDateTime m_endDateTime;
    float m_y1Min;
    float m_y1Max;
    QList<QRgb> m_xrayShortColors;
    QList<QRgb> m_xrayLongColors;
    QList<QRgb> m_protonColors;
    QRgb m_grbColor;
    QRgb m_stixColor;

    bool m_sdoEnabled;
    bool m_sdoVideoEnabled;
    QString m_sdoData;
    bool m_sdoNow;
    QDateTime m_sdoDateTime;
    QString m_map;                      // 3D map Id to send date/time to

    QList<int> m_sdoSplitterSizes;
    QList<int> m_chartSplitterSizes;

    QString m_title;
    quint32 m_rgbColor;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;

    SIDSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void applySettings(const QStringList& settingsKeys, const SIDSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
    ChannelSettings *getChannelSettings(const QString& id);
    void getChannels(QStringList& ids, QStringList& titles);
    bool createChannelSettings();

    static const QList<QRgb> m_defaultColors;
    static const QList<QRgb> m_defaultXRayShortColors;
    static const QList<QRgb> m_defaultXRayLongColors;
    static const QList<QRgb> m_defaultProtonColors;
    static const QRgb m_defaultGRBColor;
    static const QRgb m_defaultSTIXColor;
};

#endif // INCLUDE_FEATURE_SIDSETTINGS_H_
