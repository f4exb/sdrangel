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


#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include "util/iot/tplink.h"
#include "util/simpleserializer.h"

const QString TPLinkCommon::m_url = "https://wap.tplinkcloud.com";

TPLinkCommon::TPLinkCommon(const QString& username, const QString &password) :
    m_loggedIn(false),
    m_outstandingRequest(false),
    m_username(username),
    m_password(password),
    m_networkManager(nullptr)
{
}

void TPLinkCommon::login()
{
    QUrl url(m_url);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject params {
        {"appType", "Kasa_Android"},
        {"cloudUserName", m_username},
        {"cloudPassword", m_password},
        {"terminalUUID", "9cc4653e-338f-48e4-b8ca-6ed3f67631e4"}
    };
    QJsonObject object {
        {"method", "login"},
        {"params", params}
    };
    QJsonDocument document;
    document.setObject(object);

    m_networkManager->post(request, document.toJson());
}

void TPLinkCommon::handleLoginReply(QNetworkReply* reply, QString &errorMessage)
{
    if (reply)
    {
        if (!reply->error())
        {
            QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
            if (document.isObject())
            {
                //qDebug() << "Received " << document;
                if (!m_loggedIn)
                {
                    QJsonObject obj = document.object();
                    if (obj.contains(QStringLiteral("error_code")))
                    {
                        int errorCode = obj.value(QStringLiteral("error_code")).toInt();
                        if (!errorCode)
                        {
                            if (obj.contains(QStringLiteral("result")))
                            {
                                QJsonObject result = obj.value(QStringLiteral("result")).toObject();
                                if (result.contains(QStringLiteral("token")))
                                {
                                    m_loggedIn = true;
                                    m_token = result.value(QStringLiteral("token")).toString();
                                }
                                else
                                {
                                    qDebug() << "TPLinkDevice::handleReply: Object doesn't contain a token: " << result;
                                }
                            }
                            else
                            {
                                qDebug() << "TPLinkDevice::handleReply: Object doesn't contain a result object: " << obj;
                            }
                        }
                        else
                        {
                            qDebug() << "TPLinkDevice::handleReply: Non-zero error_code while logging in: " << errorCode;
                            if (obj.contains(QStringLiteral("msg")))
                            {
                                QString msg = obj.value(QStringLiteral("msg")).toString();
                                qDebug() << "TPLinkDevice::handleReply: Error message: " << msg;
                                // Typical msg is "Incorrect email or password"
                                errorMessage = QString("TP-Link: Failed to log in. %1").arg(msg);
                            }
                            else
                            {
                                errorMessage = QString("TP-Link: Failed to log in. Error code: %1").arg(errorCode);
                            }
                        }
                    }
                    else
                    {
                        qDebug() << "TPLinkDevice::handleReply: Object doesn't contain an error_code: " << obj;
                    }
                }
            }
            else
            {
                qDebug() << "TPLinkDevice::handleReply: Document is not an object: " << document;
            }
        }
        else
        {
            qDebug() << "TPLinkDevice::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "TPLinkDevice::handleReply: reply is null";
    }

    if (!m_loggedIn && errorMessage.isEmpty()) {
        errorMessage = "TP-Link: Failed to log in.";
    }
}

TPLinkDevice::TPLinkDevice(const QString& username, const QString &password, const QString &deviceId, DeviceDiscoverer::DeviceInfo *info) :
    Device(info),
    TPLinkCommon(username, password),
    m_deviceId(deviceId)
{
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &TPLinkDevice::handleReply
    );
    login();
}

TPLinkDevice::~TPLinkDevice()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &TPLinkDevice::handleReply
    );
    delete m_networkManager;
}

void TPLinkDevice::getState()
{
    if (!m_loggedIn)
    {
        m_outstandingRequest = true;
        return;
    }
    QUrl url(m_url);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject system;
    system.insert("get_sysinfo", QJsonValue());
    QJsonObject emeter;
    emeter.insert("get_realtime", QJsonValue());
    QJsonObject requestData {
        {"system", system},
        {"emeter", emeter}
    };
    QJsonObject params {
        {"deviceId", m_deviceId},
        {"requestData", requestData},
        {"token", m_token}
    };
    QJsonObject object {
        {"method", "passthrough"},
        {"params", params}
    };
    QJsonDocument document;
    document.setObject(object);

    QNetworkReply *reply = m_networkManager->post(request, document.toJson());

    recordGetRequest(reply);
}

void TPLinkDevice::setState(const QString &controlId, bool state)
{
    if (!m_loggedIn)
    {
        // Should we queue these and apply after logged in?
        qDebug() << "TPLinkDevice::setState: Unable to set state for " << controlId << " to " << state << " as not yet logged in";
        return;
    }
    QUrl url(m_url);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject stateObj {
        {"state", (int)state}
    };
    QJsonObject system {
        {"set_relay_state", stateObj}
    };
    QJsonObject requestData {
        {"system", system}
    };
    if (controlId != "switch") {
        QJsonArray childIds {
            controlId
        };
        QJsonObject context {
            {"child_ids", childIds}
        };
        requestData.insert("context", QJsonValue(context));
    }
    QJsonObject params {
        {"deviceId", m_deviceId},
        {"requestData", requestData},
        {"token", m_token}
    };
    QJsonObject object {
        {"method", "passthrough"},
        {"params", params}
    };
    QJsonDocument document;
    document.setObject(object);

    m_networkManager->post(request, document.toJson());

    recordSetRequest(controlId);
}

void TPLinkDevice::handleReply(QNetworkReply* reply)
{
    if (!m_loggedIn)
    {
        QString errorMessage;
        TPLinkCommon::handleLoginReply(reply, errorMessage);
        if (!errorMessage.isEmpty())
        {
            emit error(errorMessage);
        }
        else if (m_outstandingRequest)
        {
            m_outstandingRequest = true;
            getState();
        }
    }
    else if (reply)
    {
        if (!reply->error())
        {
            QByteArray data = reply->readAll();
            QJsonParseError error;
            QJsonDocument document = QJsonDocument::fromJson(data, &error);
            if (!document.isNull())
            {
                if (document.isObject())
                {
                    //qDebug() << "Received " << document;
                    QJsonObject obj = document.object();
                    if (obj.contains(QStringLiteral("result")))
                    {
                        QJsonObject resultObj = obj.value(QStringLiteral("result")).toObject();
                        QHash<QString, QVariant> status;

                        if (resultObj.contains(QStringLiteral("responseData")))
                        {
                            QJsonObject responseDataObj = resultObj.value(QStringLiteral("responseData")).toObject();
                            if (responseDataObj.contains(QStringLiteral("system")))
                            {
                                QJsonObject systemObj = responseDataObj.value(QStringLiteral("system")).toObject();
                                if (systemObj.contains(QStringLiteral("get_sysinfo")))
                                {
                                    QJsonObject sysInfoObj = systemObj.value(QStringLiteral("get_sysinfo")).toObject();
                                    if (sysInfoObj.contains(QStringLiteral("child_num")))
                                    {
                                        QJsonArray children = sysInfoObj.value(QStringLiteral("children")).toArray();
                                        for (auto childRef : children)
                                        {
                                            QJsonObject childObj = childRef.toObject();
                                            if (childObj.contains(QStringLiteral("state")) && childObj.contains(QStringLiteral("id")))
                                            {
                                                QString id = childObj.value(QStringLiteral("id")).toString();
                                                if (getAfterSet(reply, id))
                                                {
                                                    int state = childObj.value(QStringLiteral("state")).toInt();
                                                    status.insert(id, state); // key should match id in discoverer
                                                }
                                            }
                                        }
                                    }
                                    else if (sysInfoObj.contains(QStringLiteral("relay_state")))
                                    {
                                        if (getAfterSet(reply, "switch"))
                                        {
                                            int state = sysInfoObj.value(QStringLiteral("relay_state")).toInt();
                                            status.insert("switch", state); // key should match id in discoverer
                                        }
                                    }
                                }
                            }
                            // KP115 has emeter, but KP105 doesn't
                            if (responseDataObj.contains(QStringLiteral("emeter")))
                            {
                                QJsonObject emeterObj = responseDataObj.value(QStringLiteral("emeter")).toObject();
                                if (emeterObj.contains(QStringLiteral("get_realtime")))
                                {
                                    QJsonObject realtimeObj = emeterObj.value(QStringLiteral("get_realtime")).toObject();
                                    if (realtimeObj.contains(QStringLiteral("current_ma")))
                                    {
                                        double current = realtimeObj.value(QStringLiteral("current_ma")).toDouble();
                                        status.insert("current", current / 1000.0);
                                    }
                                    if (realtimeObj.contains(QStringLiteral("voltage_mv")))
                                    {
                                        double voltage = realtimeObj.value(QStringLiteral("voltage_mv")).toDouble();
                                        status.insert("voltage", voltage / 1000.0);
                                    }
                                    if (realtimeObj.contains(QStringLiteral("power_mw")))
                                    {
                                        double power = realtimeObj.value(QStringLiteral("power_mw")).toDouble();
                                        status.insert("power", power / 1000.0);
                                    }
                                }
                            }
                        }

                        emit deviceUpdated(status);
                    }
                    else if (obj.contains(QStringLiteral("error_code")))
                    {
                        // If a device isn't available, we can get:
                        //    {"error_code":-20002,"msg":"Request timeout"}
                        //    {"error_code":-20571,"msg":"Device is offline"}
                        int errorCode = obj.value(QStringLiteral("error_code")).toInt();
                        QString msg = obj.value(QStringLiteral("msg")).toString();
                        qDebug() << "TPLinkDevice::handleReply: Error code: " << errorCode << " " << msg;

                        emit deviceUnavailable();
                    }
                    else
                    {
                        qDebug() << "TPLinkDevice::handleReply: Object doesn't contain a result or error_code: " << obj;
                    }
                }
                else
                {
                    qDebug() << "TPLinkDevice::handleReply: Document is not an object: " << document;
                }
            }
            else
            {
                qDebug() << "TPLinkDevice::handleReply: Error parsing JSON: " << error.errorString() << " at offset " << error.offset;
            }
        }
        else
        {
            qDebug() << "TPLinkDevice::handleReply: error: " << reply->error();
        }
        removeGetRequest(reply);
        reply->deleteLater();
    }
    else
    {
        qDebug() << "TPLinkDevice::handleReply: reply is null";
    }
}

TPLinkDeviceDiscoverer::TPLinkDeviceDiscoverer(const QString& username, const QString &password) :
    TPLinkCommon(username, password)
{
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &TPLinkDeviceDiscoverer::handleReply
    );
    login();
}

TPLinkDeviceDiscoverer::~TPLinkDeviceDiscoverer()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &TPLinkDeviceDiscoverer::handleReply
    );
    delete m_networkManager;
}

void TPLinkDeviceDiscoverer::getDevices()
{
    if (!m_loggedIn)
    {
        m_outstandingRequest = true;
        return;
    }
    QUrl url(m_url);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject params {
        {"token", m_token}
    };
    QJsonObject object {
        {"method", "getDeviceList"},
        {"params", params}
    };
    QJsonDocument document;
    document.setObject(object);

    m_networkManager->post(request, document.toJson());
}

void TPLinkDeviceDiscoverer::getState(const QString &deviceId)
{
    QUrl url(m_url);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject system;
    system.insert("get_sysinfo", QJsonValue());
    QJsonObject emeter;
    emeter.insert("get_realtime", QJsonValue());
    QJsonObject requestData {
        {"system", system},
        {"emeter", emeter}
    };
    QJsonObject params {
        {"deviceId", deviceId},
        {"requestData", requestData},
        {"token", m_token}
    };
    QJsonObject object {
        {"method", "passthrough"},
        {"params", params}
    };
    QJsonDocument document;
    document.setObject(object);

    m_getStateReplies.insert(m_networkManager->post(request, document.toJson()), deviceId);
}

void TPLinkDeviceDiscoverer::handleReply(QNetworkReply* reply)
{
    if (!m_loggedIn)
    {
        QString errorMessage;
        TPLinkCommon::handleLoginReply(reply, errorMessage);
        if (!errorMessage.isEmpty())
        {
            emit error(errorMessage);
        }
        else if (m_outstandingRequest)
        {
            m_outstandingRequest = false;
            getDevices();
        }
    }
    else if (reply)
    {
        if (!reply->error())
        {
            QByteArray data = reply->readAll();
            QJsonParseError error;
            QJsonDocument document = QJsonDocument::fromJson(data, &error);
            if (!document.isNull())
            {
                if (document.isObject())
                {
                    //qDebug() << "Received " << document;
                    QJsonObject obj = document.object();

                    if (m_getStateReplies.contains(reply))
                    {
                        // Reply for getState
                        m_getStateReplies.remove(reply);
                        QJsonObject resultObj = obj.value(QStringLiteral("result")).toObject();
                        if (resultObj.contains(QStringLiteral("responseData")))
                        {
                            QJsonObject responseDataObj = resultObj.value(QStringLiteral("responseData")).toObject();
                            if (responseDataObj.contains(QStringLiteral("system")))
                            {
                                DeviceInfo info;
                                QJsonObject systemObj = responseDataObj.value(QStringLiteral("system")).toObject();
                                if (systemObj.contains(QStringLiteral("get_sysinfo")))
                                {
                                    QJsonObject sysInfoObj = systemObj.value(QStringLiteral("get_sysinfo")).toObject();
                                    if (sysInfoObj.contains(QStringLiteral("alias"))) {
                                        info.m_name = sysInfoObj.value(QStringLiteral("alias")).toString();
                                    }
                                    if (sysInfoObj.contains(QStringLiteral("model"))) {
                                        info.m_model = sysInfoObj.value(QStringLiteral("model")).toString();
                                    }
                                    if (sysInfoObj.contains(QStringLiteral("deviceId"))) {
                                        info.m_id = sysInfoObj.value(QStringLiteral("deviceId")).toString();
                                    }
                                    if (sysInfoObj.contains(QStringLiteral("child_num")))
                                    {
                                        QJsonArray children = sysInfoObj.value(QStringLiteral("children")).toArray();
                                        int child = 1;
                                        for (auto childRef : children)
                                        {
                                            QJsonObject childObj = childRef.toObject();
                                            ControlInfo *controlInfo = new ControlInfo();
                                            controlInfo->m_id = childObj.value(QStringLiteral("id")).toString();
                                            if (childObj.contains(QStringLiteral("alias"))) {
                                                controlInfo->m_name = childObj.value(QStringLiteral("alias")).toString();
                                            }
                                            controlInfo->m_type = DeviceDiscoverer::BOOL;
                                            info.m_controls.append(controlInfo);
                                            child++;
                                        }
                                    }
                                    else if (sysInfoObj.contains(QStringLiteral("relay_state")))
                                    {
                                        ControlInfo *controlInfo = new ControlInfo();
                                        controlInfo->m_id = "switch";
                                        if (sysInfoObj.contains(QStringLiteral("alias"))) {
                                            controlInfo->m_name = sysInfoObj.value(QStringLiteral("alias")).toString();
                                        }
                                        controlInfo->m_type = DeviceDiscoverer::BOOL;
                                        info.m_controls.append(controlInfo);
                                    }
                                }
                                else
                                {
                                    qDebug() << "TPLinkDeviceDiscoverer::handleReply: get_sysinfo missing";
                                }
                                // KP115 has energy meter, but KP105 doesn't. KP105 will have emeter object, but without get_realtime sub-object
                                if (responseDataObj.contains(QStringLiteral("emeter")))
                                {
                                    QJsonObject emeterObj = responseDataObj.value(QStringLiteral("emeter")).toObject();
                                    if (emeterObj.contains(QStringLiteral("get_realtime")))
                                    {
                                        QJsonObject realtimeObj = emeterObj.value(QStringLiteral("get_realtime")).toObject();
                                        if (realtimeObj.contains(QStringLiteral("current_ma")))
                                        {
                                            SensorInfo *currentSensorInfo = new SensorInfo();
                                            currentSensorInfo->m_name = "Current";
                                            currentSensorInfo->m_id = "current";
                                            currentSensorInfo->m_type = DeviceDiscoverer::FLOAT;
                                            currentSensorInfo->m_units = "A";
                                            info.m_sensors.append(currentSensorInfo);
                                        }
                                        if (realtimeObj.contains(QStringLiteral("voltage_mv")))
                                        {
                                            SensorInfo *voltageSensorInfo = new SensorInfo();
                                            voltageSensorInfo->m_name = "Voltage";
                                            voltageSensorInfo->m_id = "voltage";
                                            voltageSensorInfo->m_type = DeviceDiscoverer::FLOAT;
                                            voltageSensorInfo->m_units = "V";
                                            info.m_sensors.append(voltageSensorInfo);
                                        }
                                        if (realtimeObj.contains(QStringLiteral("power_mw")))
                                        {
                                            SensorInfo *powerSensorInfo = new SensorInfo();
                                            powerSensorInfo->m_name = "Power";
                                            powerSensorInfo->m_id = "power";
                                            powerSensorInfo->m_type = DeviceDiscoverer::FLOAT;
                                            powerSensorInfo->m_units = "W";
                                            info.m_sensors.append(powerSensorInfo);
                                        }
                                    }
                                }
                                if (info.m_controls.size() > 0) {
                                    m_devices.append(info);
                                } else {
                                    qDebug() << "TPLinkDeviceDiscoverer::handleReply: No controls in info";
                                }

                            }
                        }
                        else
                        {
                            qDebug() << "TPLinkDeviceDiscoverer::handleReply: No responseData";
                        }

                        if (m_getStateReplies.size() == 0)
                        {
                            emit deviceList(m_devices);
                            m_devices.clear();
                        }

                    }
                    else
                    {
                        // Reply for getDevice
                        if (obj.contains(QStringLiteral("result")))
                        {
                            QJsonObject resultObj = obj.value(QStringLiteral("result")).toObject();
                            if (resultObj.contains(QStringLiteral("deviceList")))
                            {
                                QJsonArray deviceArray = resultObj.value(QStringLiteral("deviceList")).toArray();
                                for (auto deviceRef : deviceArray)
                                {
                                    QJsonObject deviceObj = deviceRef.toObject();
                                    if (deviceObj.contains(QStringLiteral("deviceId")) && deviceObj.contains(QStringLiteral("deviceType")))
                                    {
                                        // In order to discover what controls and sensors a device has, we need to get sysinfo
                                        getState(deviceObj.value(QStringLiteral("deviceId")).toString());
                                    }
                                    else
                                    {
                                        qDebug() << "TPLinkDeviceDiscoverer::handleReply: deviceList element doesn't contain a deviceId: " << deviceObj;
                                    }
                                }
                            }
                            else
                            {
                                qDebug() << "TPLinkDeviceDiscoverer::handleReply: result doesn't contain a deviceList: " << resultObj;
                            }
                        }
                        else
                        {
                            qDebug() << "TPLinkDeviceDiscoverer::handleReply: Object doesn't contain a result: " << obj;
                        }
                    }
                }
                else
                {
                    qDebug() << "TPLinkDeviceDiscoverer::handleReply: Document is not an object: " << document;
                }
            }
            else
            {
                qDebug() << "TPLinkDeviceDiscoverer::handleReply: Error parsing JSON: " << error.errorString() << " at offset " << error.offset;
            }
        }
        else
        {
            qDebug() << "TPLinkDeviceDiscoverer::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "TPLinkDeviceDiscoverer::handleReply: reply is null";
    }
}
