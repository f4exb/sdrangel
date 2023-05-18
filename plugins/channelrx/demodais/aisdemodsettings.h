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

#ifndef INCLUDE_AISDEMODSETTINGS_H
#define INCLUDE_AISDEMODSETTINGS_H

#include <QByteArray>
#include <QString>

#include "dsp/dsptypes.h"

class Serializable;

// Number of columns in the tables
#define AISDEMOD_MESSAGE_COLUMNS 10

struct AISDemodSettings
{
    qint32 m_baud;
    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_fmDeviation;
    Real m_correlationThreshold;
    QString m_filterMMSI;
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    enum UDPFormat {
        Binary,
        NMEA
    } m_udpFormat;

    QString m_logFilename;
    bool m_logEnabled;
    bool m_showSlotMap;

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_scopeGUI;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    int m_messageColumnIndexes[AISDEMOD_MESSAGE_COLUMNS];//!< How the columns are ordered in the table
    int m_messageColumnSizes[AISDEMOD_MESSAGE_COLUMNS];  //!< Size of the columns in the table

    static const int AISDEMOD_CHANNEL_SAMPLE_RATE = 57600; //!< 6x 9600 baud rate (use even multiple so Gausian filter has odd number of taps)
    static const int m_scopeStreams = 9;

    AISDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* INCLUDE_AISDEMODSETTINGS_H */
