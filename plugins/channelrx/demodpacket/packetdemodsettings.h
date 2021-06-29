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

#ifndef INCLUDE_PACKETDEMODSETTINGS_H
#define INCLUDE_PACKETDEMODSETTINGS_H

#include <QByteArray>
#include <QHash>

class Serializable;

// Number of columns in the table
#define PACKETDEMOD_COLUMNS 7

struct PacketDemodSettings
{
    qint32 m_inputFrequencyOffset;
    qint32 m_baud;
    Real m_rfBandwidth;
    Real m_fmDeviation;
    QString m_filterFrom;
    QString m_filterTo;
    QString m_filterPID;
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

    int m_columnIndexes[PACKETDEMOD_COLUMNS];//!< How the columns are ordered in the table
    int m_columnSizes[PACKETDEMOD_COLUMNS];  //!< Size of the columns in the table

    static const int PACKETDEMOD_CHANNEL_BANDWIDTH = 9600;
    static const int PACKETDEMOD_CHANNEL_SAMPLE_RATE  = 38400; // Must be integer multiple of m_baud=1200

    PacketDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* INCLUDE_PACKETDEMODSETTINGS_H */
