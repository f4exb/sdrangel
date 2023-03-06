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

#ifndef INCLUDE_VORDEMODSCSETTINGS_H
#define INCLUDE_VORDEMODSCSETTINGS_H

#include <QByteArray>
#include <QHash>

class Serializable;

struct VORDemodSettings
{
    qint32 m_inputFrequencyOffset;
    int m_navId; //!< VOR unique identifier when set by VOR localizer feature
    Real m_squelch;
    Real m_volume;
    bool m_audioMute;
    bool m_identBandpassEnable;
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
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    // Highest frequency is the FM subcarrier at up to ~11kHz
    // However, old VORs can have 0.005% frequency offset, which is 6kHz
    static const int VORDEMOD_CHANNEL_BANDWIDTH = 18000;
    // Sample rate needs to be at least twice the above
    // Also need to consider impact frequency resolution of Goertzel filters
    // May as well make it a common audio rate, to possibly avoid decimation
    static const int VORDEMOD_CHANNEL_SAMPLE_RATE = 48000;

    VORDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* INCLUDE_VORDEMODSCSETTINGS_H */
