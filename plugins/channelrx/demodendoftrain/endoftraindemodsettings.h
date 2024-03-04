///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_ENDOFTRAINDEMODSETTINGS_H
#define INCLUDE_ENDOFTRAINDEMODSETTINGS_H

#include <QString>
#include <QByteArray>

#include "dsp/dsptypes.h"

class Serializable;

// Number of columns in the table
#define ENDOFTRAINDEMOD_COLUMNS 18

struct EndOfTrainDemodSettings
{
    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_fmDeviation;
    QString m_filterFrom;
    bool m_udpEnabled;
    QString m_udpAddress;
    uint16_t m_udpPort;
    QString m_logFilename;
    bool m_logEnabled;
    bool m_useFileTime;

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;

    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    QList<int> m_columnIndexes;     //!< How the columns are ordered in the table
    QList<int> m_columnSizes;       //!< Size of the columns in the table

    static const int CHANNEL_SAMPLE_RATE  = 48000; // Must be integer multiple of baud rate
    static const int BAUD_RATE = 1200;
    static const int m_scopeStreams = 9;

    EndOfTrainDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const EndOfTrainDemodSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force = false) const;
};

#endif /* INCLUDE_ENDOFTRAINDEMODSETTINGS_H */
