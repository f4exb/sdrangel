///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
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

#ifndef INCLUDE_VORLOCALIZERSETTINGS_H
#define INCLUDE_VORLOCALIZERSETTINGS_H

#include <QByteArray>
#include <QHash>

class Serializable;
class ChannelAPI;

// Number of columns in the table

struct VORLocalizerSubChannelSettings {
    int m_id;                           //!< Unique VOR identifier (from database)
    int m_frequency;                    //!< Frequency the VOR is on
    bool m_audioMute;                   //!< Mute the audio from this VOR
};

struct VORLocalizerSettings
{
    struct VORChannel
    {
        int m_subChannelId; //!< Unique VOR identifier (from database)
        int m_frequency;    //!< Frequency the VOR is on
        bool m_audioMute;   //!< Mute the audio from this VOR

        VORChannel() = default;
        VORChannel(const VORChannel&) = default;
        VORChannel& operator=(const VORChannel&) = default;

        bool operator<(const VORChannel& other) const;
    };

    struct AvailableChannel
    {
        int m_deviceSetIndex;
        int m_channelIndex;
        ChannelAPI *m_channelAPI;
        quint64 m_deviceCenterFrequency;
        int m_basebandSampleRate;
        int m_navId;

        AvailableChannel() = default;
        AvailableChannel(const AvailableChannel&) = default;
        AvailableChannel& operator=(const AvailableChannel&) = default;
    };

    quint32 m_rgbColor;
    QString m_title;
    bool m_magDecAdjust;                //!< Adjust for magnetic declination when drawing radials on the map
    int m_rrTime;                       //!< Round robin turn time in seconds
    bool m_forceRRAveraging;            //!< Force radial and signal magnitude averaging over RR turn
    int m_centerShift;                  //!< Center frequency shift to apply to move away from DC
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    QString m_mapProvider;

    static const int VORDEMOD_COLUMNS  = 10;
    static const int VOR_COL_NAME      =  0;
    static const int VOR_COL_FREQUENCY =  1;
    static const int VOR_COL_IDENT     =  2;
    static const int VOR_COL_MORSE     =  3;
    static const int VOR_COL_RX_IDENT  =  4;
    static const int VOR_COL_RX_MORSE  =  5;
    static const int VOR_COL_RADIAL    =  6;
    static const int VOR_COL_REF_MAG   =  7;
    static const int VOR_COL_VAR_MAG   =  8;
    static const int VOR_COL_MUTE      =  9;

    int m_columnIndexes[VORDEMOD_COLUMNS];//!< How the columns are ordered in the table
    int m_columnSizes[VORDEMOD_COLUMNS];  //!< Size of the coumns in the table

    QHash<int, VORLocalizerSubChannelSettings> m_subChannelSettings;

    VORLocalizerSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void applySettings(const QStringList& settingsKeys, const VORLocalizerSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* INCLUDE_VORLOCALIZERSETTINGS_H */
