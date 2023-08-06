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
#include "inputcontrollersettings.h"

class Serializable;

struct GS232ControllerSettings
{
    struct AvailableChannelOrFeature
    {
        QString m_kind; //!< "R" for channel, "F" for feature
        int m_superIndex;
        int m_index;
        QString m_type;

        AvailableChannelOrFeature() = default;
        AvailableChannelOrFeature(const AvailableChannelOrFeature&) = default;
        AvailableChannelOrFeature& operator=(const AvailableChannelOrFeature&) = default;
        bool operator==(const AvailableChannelOrFeature& a) const {
            return (m_kind == a.m_kind) && (m_superIndex == a.m_superIndex) && (m_index == a.m_index) && (m_type == a.m_type);
        }
    };

    float m_azimuth;
    float m_elevation;
    QString m_serialPort;
    int m_baudRate;
    QString m_host;
    int m_port;
    bool m_track;
    QString m_source;           // Plugin to get az/el from. E.g: "R0:0 ADSBDemod". Use a string, so can be set via WebAPI
    float m_azimuthOffset;
    float m_elevationOffset;
    int m_azimuthMin;
    int m_azimuthMax;
    int m_elevationMin;
    int m_elevationMax;
    float m_tolerance;
    enum Protocol { GS232, SPID, ROTCTLD, DFM } m_protocol;
    enum Connection { SERIAL, TCP } m_connection;
    int m_precision;
    enum Coordinates { AZ_EL, X_Y_85, X_Y_30 } m_coordinates;
    QString m_inputController;
    InputControllerSettings m_inputControllerSettings;
    bool m_targetControlEnabled;
    bool m_offsetControlEnabled;
    bool m_highSensitivity;

    bool m_dfmTrackOn;
    bool m_dfmLubePumpsOn;
    bool m_dfmBrakesOn;
    bool m_dfmDrivesOn;

    Serializable *m_rollupState;
    QString m_title;
    quint32 m_rgbColor;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;

    GS232ControllerSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void calcTargetAzEl(float& targetAz, float& targetEl) const;
    void applySettings(const QStringList& settingsKeys, const GS232ControllerSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;

    static const QStringList m_pipeTypes;
    static const QStringList m_pipeURIs;
};

#endif // INCLUDE_FEATURE_GS232CONTROLLERSETTINGS_H_
