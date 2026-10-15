///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_DRMDEMODSETTINGS_H
#define INCLUDE_DRMDEMODSETTINGS_H

#include <QByteArray>
#include <QString>
#include <QStringList>

#include "dsp/dsptypes.h"

class Serializable;

struct DRMDemodSettings
{
    static constexpr int ChannelSampleRate = 192000;

    qint32 m_inputFrequencyOffset;
    int m_acquisitionSearchRangeHz;
    int m_forcedMode;
    int m_forcedOccupancy;
    qint64 m_selectedServiceIdentifier;
    Real m_volume;
    bool m_audioMute;
    QString m_audioDeviceName;

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    DRMDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void sanitize();
    // The keys of the values sanitize() can change that differ from before:
    // with the requested keys, they carry a dependent change (a forced
    // occupancy the newly forced mode does not define becomes Auto).
    QStringList sanitizedKeys(const DRMDemodSettings& before) const;
    void applySettings(const QStringList& settingsKeys, const DRMDemodSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force = false) const;

    static int sanitizeSearchRange(int searchRangeHz);
    static int sanitizeForcedMode(int mode);
    static int sanitizeForcedOccupancy(int occupancy);
    static qint64 sanitizeServiceIdentifier(qint64 serviceIdentifier);
};

#endif // INCLUDE_DRMDEMODSETTINGS_H
