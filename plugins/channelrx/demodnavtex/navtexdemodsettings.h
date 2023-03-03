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

#ifndef INCLUDE_NAVTEXDEMODSETTINGS_H
#define INCLUDE_NAVTEXDEMODSETTINGS_H

#include <QByteArray>

class Serializable;

// Number of columns in the table
#define NAVTEXDEMOD_COLUMNS 11

struct NavtexDemodSettings
{
    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    int m_navArea;
    QString m_filterStation;
    QString m_filterType;
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;

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

    int m_columnIndexes[NAVTEXDEMOD_COLUMNS];//!< How the columns are ordered in the table
    int m_columnSizes[NAVTEXDEMOD_COLUMNS];  //!< Size of the columns in the table

    static const int NAVTEXDEMOD_CHANNEL_SAMPLE_RATE = 1000; // Must be integer multiple of baud rate (x10)
    static const int NAVTEXDEMOD_BAUD_RATE = 100;
    static const int NAVTEXDEMOD_SAMPLES_PER_BIT = NAVTEXDEMOD_CHANNEL_SAMPLE_RATE / NAVTEXDEMOD_BAUD_RATE;
    static const int NAVTEXDEMOD_FREQUENCY_SHIFT = 170;

    NavtexDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* INCLUDE_NAVTEXDEMODSETTINGS_H */

