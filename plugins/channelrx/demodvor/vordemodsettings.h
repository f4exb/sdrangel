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

#ifndef INCLUDE_VORDEMODSETTINGS_H
#define INCLUDE_VORDEMODSETTINGS_H

#include <QByteArray>
#include <QHash>

class Serializable;

// Number of columns in the table
#define VORDEMOD_COLUMNS 11

struct VORDemodSubChannelSettings {
    int m_id;                           //!< Unique VOR identifier (from database)
    int m_frequency;                    //!< Frequency the VOR is on
    bool m_audioMute;                   //!< Mute the audio from this VOR
};

struct VORDemodMCSettings
{
    Real m_squelch;
    Real m_volume;
    bool m_audioMute;
    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    QString m_audioDeviceName;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;

    Real m_identThreshold;              //!< Linear SNR threshold for Morse demodulator
    Real m_refThresholdDB;              //!< Threshold in dB for valid VOR reference signal
    Real m_varThresholdDB;              //!< Threshold in dB for valid VOR variable signal
    bool m_magDecAdjust;                //!< Adjust for magnetic declination when drawing radials on the map
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    int m_columnIndexes[VORDEMOD_COLUMNS];//!< How the columns are ordered in the table
    int m_columnSizes[VORDEMOD_COLUMNS];  //!< Size of the coumns in the table

    QHash<int, VORDemodSubChannelSettings *> m_subChannelSettings;

    VORDemodMCSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* INCLUDE_VORDEMODSETTINGS_H */
