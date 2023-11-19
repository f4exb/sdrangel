///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon <jon@beniston.com>                                     //
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

#ifndef INCLUDE_IOT_VISA_H
#define INCLUDE_IOT_VISA_H

#include "util/iot/device.h"
#include "util/visa.h"

class SDRBASE_API VISADevice : public Device {
    Q_OBJECT
public:

    struct SDRBASE_API VISAControl : public DeviceDiscoverer::ControlInfo {
        QString m_getState;
        QString m_setState;

        virtual DeviceDiscoverer::ControlInfo *clone() const override;
        virtual QByteArray serialize() const override;
        virtual bool deserialize(const QByteArray& data) override;
    };

    struct SDRBASE_API VISASensor : public DeviceDiscoverer::SensorInfo {
        QString m_getState;

        virtual DeviceDiscoverer::SensorInfo *clone() const override;
        virtual QByteArray serialize() const override;
        virtual bool deserialize(const QByteArray& data) override;
    };

    VISADevice(const QHash<QString, QVariant> settings, const QString &deviceId,
               const QStringList &controls, const QStringList &sensors,
               DeviceDiscoverer::DeviceInfo *info=nullptr);
    ~VISADevice();
    virtual void getState() override;
    using Device::setState;
    virtual void setState(const QString &controlId, bool state) override;
    virtual void setState(const QString &controlId, int state) override;
    virtual void setState(const QString &controlId, float state) override;
    virtual void setState(const QString &controlId, const QString &state) override;
    virtual QString getProtocol() const override { return "VISA"; }
    virtual QString getDeviceId() const override { return m_deviceId; }

private:
    QString m_deviceId;             // VISA resource (E.g. USB0::0xcafe...)
    VISA m_visa;
    ViSession m_session;
    QStringList m_controls;         // Control IDs for getState
    QStringList m_sensors;          // Sensor IDs for getState
    bool m_debugIO;

    bool open();
    bool convertToBool(const QString &string, bool &ok);
    void convert(QHash<QString, QVariant> &status, const QString &id, DeviceDiscoverer::Type type, const QString &state);
};

class SDRBASE_API VISADeviceDiscoverer : public DeviceDiscoverer {
    Q_OBJECT
public:

    VISADeviceDiscoverer(const QString &resourceFilter = "");
    ~VISADeviceDiscoverer();
    virtual void getDevices() override;

private:
    VISA m_visa;
    ViSession m_session;
    QString m_resourceFilter;
};

#endif /* INCLUDE_IOT_VISA_H */
