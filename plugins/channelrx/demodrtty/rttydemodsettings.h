///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
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

#ifndef INCLUDE_RTTYDEMODSETTINGS_H
#define INCLUDE_RTTYDEMODSETTINGS_H

#include <QByteArray>

#include "util/baudot.h"

class Serializable;

struct RttyDemodSettings
{
    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_baudRate;
    int m_frequencyShift;
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    Baudot::CharacterSet m_characterSet;
    bool m_suppressCRLF;
    bool m_unshiftOnSpace;
    enum FilterType {
        LOWPASS,
        COSINE_B_1,
        COSINE_B_0_75,
        COSINE_B_0_5,
        COSINE_B_1_BW_0_75,
        COSINE_B_1_BW_1_25,
        MAV,
        FILTERED_MAV
    } m_filter;
    bool m_atc;
    bool m_msbFirst;    // false = LSB first, true = MSB first
    bool m_spaceHigh;   // false = mark high frequency, true = space high frequency
    int m_squelch;      // In dB

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;

    int m_scopeCh1;
    int m_scopeCh2;

    QString m_logFilename;
    bool m_logEnabled;
    Serializable *m_scopeGUI;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    static const int RTTYDEMOD_CHANNEL_SAMPLE_RATE = 1000;

    RttyDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* INCLUDE_RTTYDEMODSETTINGS_H */

