///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021-2026 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_INMARSATDEMODSETTINGS_H
#define INCLUDE_INMARSATDEMODSETTINGS_H

#include <QByteArray>

#include "dsp/dsptypes.h"

class Serializable;

// Number of columns in the table
#define INMARSATDEMOD_MESSAGES_COLUMNS  8
#define INMARSATDEMOD_PACKETS_COLUMNS   16

struct InmarsatDemodSettings
{
    qint32 m_inputFrequencyOffset;
    float m_rfBandwidth;
    enum Equalizer {
        NONE,
        CMA,                            //!< Constant Modulus
        LMS                             //!< Least Mean Square
    } m_equalizer;
    float m_rrcRolloff;
    float m_pllBW;                      //!< Costas loop bandwidth in rad/sample
    float m_ssBW;                       //!< Symbol sync bandwidth normalised to symbol rate (E.g. 0.05 for 5%)
    QString m_filterType;
    QString m_filterMessage;
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    QString m_logFilename;
    bool m_logEnabled;
    bool m_useFileTime;
    QList<int> m_packetsColumnIndexes;//!< How the columns are ordered in the table
    QList<int> m_packetsColumnSizes;  //!< Size of the columns in the table
    QList<int> m_messagesColumnIndexes;
    QList<int> m_messagesColumnSizes;

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

    static const int CHANNEL_SAMPLE_RATE = 48000;
    static const int BAUD_RATE = 1200;
    static const int m_scopeStreams = 13;

    InmarsatDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void setScopeGUI(Serializable *scopeGUI) { m_scopeGUI = scopeGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const InmarsatDemodSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force = false) const;
    void appendDefaultColumnSettings();
    void appendDefaultColumnIndexes(QList<int>& list, int size);
    void appendDefaultColumnSizes(QList<int>& list, int size);
};

#endif /* INCLUDE_INMARSATDEMODSETTINGS_H */
