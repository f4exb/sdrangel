///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_GS232CONTROLLERSETTINGS_H_
#define INCLUDE_FEATURE_GS232CONTROLLERSETTINGS_H_

#include <QByteArray>
#include <QString>

#include "util/message.h"

class Serializable;

struct GS232ControllerSettings
{
    float m_azimuth;
    float m_elevation;
    QString m_serialPort;
    int m_baudRate;
    bool m_track;
    QString m_target;           // Plugin to get az/el from. E.g: "R0:0 ADSBDemod". Use a string, so can be set via WebAPI
    int m_azimuthOffset;
    int m_elevationOffset;
    int m_azimuthMin;
    int m_azimuthMax;
    int m_elevationMin;
    int m_elevationMax;
    int m_tolerance;
    enum Protocol { GS232, SPID } m_protocol;
    QString m_title;
    quint32 m_rgbColor;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;

    GS232ControllerSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    static const QStringList m_pipeTypes;
    static const QStringList m_pipeURIs;
};

#endif // INCLUDE_FEATURE_GS232CONTROLLERSETTINGS_H_
