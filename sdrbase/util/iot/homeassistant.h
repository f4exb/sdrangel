///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_IOT_HOMEASSISTANT_H
#define INCLUDE_IOT_HOMEASSISTANT_H

#include "util/iot/device.h"

// Supports Home Assistant devices - https://www.home-assistant.io/
class SDRBASE_API HomeAssistantDevice : public Device {
    Q_OBJECT
public:

    HomeAssistantDevice(const QString& apiKey, const QString& url, const QString &deviceId,
                        const QStringList &controls, const QStringList &sensors,
                        DeviceDiscoverer::DeviceInfo *info=nullptr);
    ~HomeAssistantDevice();
    virtual void getState() override;
    using Device::setState;
    virtual void setState(const QString &controlId, bool state) override;
    virtual QString getProtocol() const override { return "HomeAssistant"; }
    virtual QString getDeviceId() const override { return m_deviceId; }

private:

    QString m_deviceId;
    QStringList m_entities; // List of entities that are part of the device, to get state for (controls and sensors)
    QString m_apiKey;       // Bearer token
    QString m_url;          // Typically http://homeassistant.local:8123
    QNetworkAccessManager *m_networkManager;

public slots:
    void handleReply(QNetworkReply* reply);

};

class SDRBASE_API HomeAssistantDeviceDiscoverer : public DeviceDiscoverer {
    Q_OBJECT
public:

    HomeAssistantDeviceDiscoverer(const QString& apiKey, const QString& url);
    ~HomeAssistantDeviceDiscoverer();
    virtual void getDevices() override;

private:

    QString m_deviceId;
    QString m_apiKey;       // Bearer token
    QString m_url;          // Typically http://homeassistant.local:8123
    QNetworkAccessManager *m_networkManager;

public slots:
    void handleReply(QNetworkReply* reply);

};

#endif /* INCLUDE_IOT_HOMEASSISTANT_H */
