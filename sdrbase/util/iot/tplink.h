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

#ifndef INCLUDE_IOT_TPLINK_H
#define INCLUDE_IOT_TPLINK_H

#include "util/iot/device.h"

class SDRBASE_API TPLinkCommon {
protected:
    TPLinkCommon(const QString& username, const QString &password);
    void login();
    void handleLoginReply(QNetworkReply* reply, QString &errorMessage);

    bool m_loggedIn;
    bool m_outstandingRequest;      // Issue getState / getDevices after logged in
    QString m_username;
    QString m_password;
    QString m_token;
    QNetworkAccessManager *m_networkManager;

    static const QString m_url;
};

// Supports TPLink's Kasa plugs - https://www.tp-link.com/uk/smarthome/
class SDRBASE_API TPLinkDevice : public Device, TPLinkCommon {
    Q_OBJECT
public:

    TPLinkDevice(const QString& username, const QString &password, const QString &deviceId, DeviceDiscoverer::DeviceInfo *info=nullptr);
    ~TPLinkDevice();
    virtual void getState() override;
    using Device::setState;
    virtual void setState(const QString &controlId, bool state) override;
    virtual QString getProtocol() const override { return "TPLink"; }
    virtual QString getDeviceId() const override { return m_deviceId; }

private:

    QString m_deviceId;

public slots:
    void handleReply(QNetworkReply* reply);
};

class SDRBASE_API TPLinkDeviceDiscoverer : public DeviceDiscoverer, TPLinkCommon {
    Q_OBJECT
public:

    TPLinkDeviceDiscoverer(const QString& username, const QString &password);
    ~TPLinkDeviceDiscoverer();
    virtual void getDevices() override;

private:
    void getState(const QString &deviceId);

    QHash<QNetworkReply *, QString> m_getStateReplies;
    QList<DeviceInfo> m_devices;

public slots:
    void handleReply(QNetworkReply* reply);
};

#endif /* INCLUDE_IOT_TPLINK_H */
