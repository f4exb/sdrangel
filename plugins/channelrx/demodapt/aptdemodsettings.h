///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
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

#ifndef INCLUDE_APTDEMODSETTINGS_H
#define INCLUDE_APTDEMODSETTINGS_H

#include <QByteArray>
#include <QHash>
#include <QDateTime>

class Serializable;

struct APTDemodSettings
{
    qint32 m_inputFrequencyOffset;
    float m_rfBandwidth;
    float m_fmDeviation;
    bool m_cropNoise;
    bool m_denoise;
    bool m_linearEqualise;
    bool m_histogramEqualise;
    bool m_precipitationOverlay;
    bool m_flip;
    enum ChannelSelection {BOTH_CHANNELS, CHANNEL_A, CHANNEL_B, TEMPERATURE, PALETTE} m_channels;
    bool m_decodeEnabled;
    bool m_satelliteTrackerControl;             //! Whether Sat Tracker can set direction of pass
    QString m_satelliteName;                    //!< All, NOAA 15, NOAA 18 or NOAA 19
    bool m_autoSave;
    QString m_autoSavePath;
    int m_autoSaveMinScanLines;
    bool m_saveCombined;
    bool m_saveSeparate;
    bool m_saveProjection;
    int m_scanlinesPerImageUpdate;
    int m_transparencyThreshold;
    int m_opacityThreshold;
    QStringList m_palettes;                     // List of 256x256 images to use a colour palette
    int m_palette;                              // Index in to m_palettes - only if m_channels==PALETTE
    int m_horizontalPixelsPerDegree;            // Resolution for projected image
    int m_verticalPixelsPerDegree;
    float m_satTimeOffset;
    float m_satYaw;

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    // The following are really working state, rather than settings
    QString m_tle;                              // Satelite two-line elements, from satellite tracker
    QDateTime m_aosDateTime;                    // When decoder was started (may not be current time, if replaying old file)
    bool m_northToSouth;                        // Separate from flip, in case user changes it

    APTDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* INCLUDE_APTDEMODSETTINGS_H */
