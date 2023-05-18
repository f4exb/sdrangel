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

#ifndef INCLUDE_DSCDEMODSETTINGS_H
#define INCLUDE_DSCDEMODSETTINGS_H

#include <QByteArray>

class Serializable;

// Number of columns in the table
#define DSCDEMOD_COLUMNS 28

struct DSCDemodSettings
{
    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;  // Not currently in GUI as probably doesn't need to be adjusted
    bool m_filterInvalid;
    int m_filterColumn;
    QString m_filter;
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    bool m_feed;

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;

    QString m_logFilename;
    bool m_logEnabled;
    Serializable *m_scopeGUI;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    int m_columnIndexes[DSCDEMOD_COLUMNS];//!< How the columns are ordered in the table
    int m_columnSizes[DSCDEMOD_COLUMNS];  //!< Size of the columns in the table

    static const int DSCDEMOD_CHANNEL_SAMPLE_RATE = 1000; // Must be integer multiple of baud rate (x10)
    static const int DSCDEMOD_BAUD_RATE = 100;
    static const int DSCDEMOD_FREQUENCY_SHIFT = 170;
    static const int m_scopeStreams = 10;

    DSCDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* INCLUDE_DSCDEMODSETTINGS_H */
