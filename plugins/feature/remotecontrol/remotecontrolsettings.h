///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_FEATURE_REMOTECONTROLSETTINGS_H_
#define INCLUDE_FEATURE_REMOTECONTROLSETTINGS_H_

#include <QByteArray>
#include <QString>

#include "util/message.h"
#include "util/iot/device.h"

class Serializable;

struct RemoteControlControl {
    QString m_id;
    QString m_labelLeft;
    QString m_labelRight;

    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

struct RemoteControlSensor {
    QString m_id;
    QString m_labelLeft;
    QString m_labelRight;
    QString m_format;
    bool m_plot;

    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

struct RemoteControlDevice {
    QString m_protocol;                         // TPLink, HomeAssistant, VISA
    QString m_label;                            // Label to display as device name
    QList<RemoteControlControl> m_controls;     // Enabled controls
    QList<RemoteControlSensor> m_sensors;       // Enabled sensors
    bool m_verticalControls;
    bool m_verticalSensors;
    bool m_commonYAxis;                         // Whether multiple series on chart should share same axis
    DeviceDiscoverer::DeviceInfo m_info;
    RemoteControlDevice() :
        m_verticalControls(false),
        m_verticalSensors(true),
        m_commonYAxis(false)
    {
    }
    RemoteControlControl *getControl(const QString &id)
    {
        for (int i = 0; i < m_controls.size(); i++)
        {
            if (m_controls[i].m_id == id) {
                return &m_controls[i];
            }
        }
        return nullptr;
    }
    RemoteControlSensor *getSensor(const QString &id)
    {
        for (int i = 0; i < m_sensors.size(); i++)
        {
            if (m_sensors[i].m_id == id) {
                return &m_sensors[i];
            }
        }
        return nullptr;
    }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
protected:
    QByteArray serializeControlList() const;
    void deserializeControlList(const QByteArray& data);
    QByteArray serializeSensorList() const;
    void deserializeSensorList(const QByteArray& data);
};

struct RemoteControlSettings
{
    float m_updatePeriod;               //!< Period between device state updates
    QString m_tpLinkUsername;
    QString m_tpLinkPassword;
    QString m_homeAssistantToken;
    QString m_homeAssistantHost;
    QString m_visaResourceFilter;
    bool m_visaLogIO;
    bool m_chartHeightFixed;            //!< Whether chart heights should be fixed (to m_chartHeightPixels) or allowed to expand to use available space
    int m_chartHeightPixels;            //!< Chart height in pixels when fixed

    QList<RemoteControlDevice *> m_devices;

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

    RemoteControlSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }

    QByteArray serializeDeviceList(const QList<RemoteControlDevice *>& devices) const;
    void deserializeDeviceList(const QByteArray& data, QList<RemoteControlDevice *>& devices);
};

#endif // INCLUDE_FEATURE_REMOTECONTROLSETTINGS_H_
