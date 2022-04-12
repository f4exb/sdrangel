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

#ifndef INCLUDE_PAGERDEMODSETTINGS_H
#define INCLUDE_PAGERDEMODSETTINGS_H

#include <QByteArray>
#include <QString>

#include "dsp/dsptypes.h"

class Serializable;

// Number of columns in the tables
#define PAGERDEMOD_MESSAGE_COLUMNS 9

struct PagerDemodSettings
{
    qint32 m_baud;                      //!< 512, 1200 or 2400
    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_fmDeviation;                 //<! 4.5k for POCSAG
    enum Decode {
        Standard,
        Inverted,
        Numeric,
        Alphanumeric,
        Heuristic
    } m_decode;                         //!< Whether to decode as numeric or alphanumeric
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    QString m_filterAddress;            //!< Filter messages by address in GUI
    int m_scopeCh1;                     //!< What signals the scope shows
    int m_scopeCh2;

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

    bool m_reverse;                 //!< Whether characters should be reversed, for right-to-left reading order
    QList<qint32> m_sevenbit;
    QList<qint32> m_unicode;

    QString m_logFilename;
    bool m_logEnabled;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    int m_messageColumnIndexes[PAGERDEMOD_MESSAGE_COLUMNS];//!< How the columns are ordered in the table
    int m_messageColumnSizes[PAGERDEMOD_MESSAGE_COLUMNS];  //!< Size of the columns in the table

    static const int m_channelSampleRate = 38400; //!< lcm(512,2400) baud rates

    PagerDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    QByteArray serializeIntList(const QList<qint32>& ints) const;
    void deserializeIntList(const QByteArray& data, QList<qint32>& ints);
};

#endif /* INCLUDE_PAGERDEMODSETTINGS_H */
