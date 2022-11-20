///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_ANTENNATOOLSSETTINGS_H_
#define INCLUDE_FEATURE_ANTENNATOOLSSETTINGS_H_

#include <QByteArray>
#include <QString>

#include "util/message.h"

class Serializable;

struct AntennaToolsSettings
{
    enum LengthUnits { CM, M, FEET };

    double m_dipoleFrequencyMHz;
    int m_dipoleFrequencySelect;
    double m_dipoleEndEffectFactor;
    LengthUnits m_dipoleLengthUnits;

    double m_dishFrequencyMHz;
    int m_dishFrequencySelect;
    double m_dishDiameter;
    double m_dishDepth;
    int m_dishEfficiency;
    LengthUnits m_dishLengthUnits;
    double m_dishSurfaceError;

    QString m_title;
    quint32 m_rgbColor;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;

    AntennaToolsSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void applySettings(const QStringList& settingsKeys, const AntennaToolsSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif // INCLUDE_FEATURE_ANTENNATOOLSSETTINGS_H_
