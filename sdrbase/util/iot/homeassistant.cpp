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

#include "util/iot/homeassistant.h"
#include "util/simpleserializer.h"

HomeAssistantDevice::HomeAssistantDevice(const QString& apiKey, const QString& url, const QString &deviceId,
                                        const QStringList &controls, const QStringList &sensors,
                                        DeviceDiscoverer::DeviceInfo *info) :
    Device(info),
    m_deviceId(deviceId),
    m_apiKey(apiKey),
    m_url(url)
{
    m_entities = controls;
    m_entities.append(sensors);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &HomeAssistantDevice::handleReply
    );
}

HomeAssistantDevice::~HomeAssistantDevice()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &HomeAssistantDevice::handleReply
    );
    delete m_networkManager;
}

void HomeAssistantDevice::getState()
{
    // Get state for all entities of the device
    for (auto entity : m_entities)
    {
        QUrl url(m_url + "/api/states/" + entity);
        QNetworkRequest request(url);
        request.setRawHeader("Authorization", "Bearer " + m_apiKey.toLocal8Bit());
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QNetworkReply *reply = m_networkManager->get(request);
        recordGetRequest(reply);
    }
}

void HomeAssistantDevice::setState(const QString &controlId, bool state)
{
    QString domain = controlId.left(controlId.indexOf("."));
    QUrl url(m_url + "/api/services/" + domain + "/turn_" + (state ? "on" : "off"));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_apiKey.toLocal8Bit());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject object {
        {"entity_id", controlId}
    };
    QJsonDocument document;
    document.setObject(object);

    m_networkManager->post(request, document.toJson());

    // 750ms guard, to try to avoid toggling of widget, while state updates on server
    // Perhaps should be a setting
    recordSetRequest(controlId, 750);
}

void HomeAssistantDevice::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QByteArray data = reply->readAll();
            QJsonParseError error;
            QJsonDocument document = QJsonDocument::fromJson(data, &error);
            if (!document.isNull())
            {
                //qDebug() << "Received " << document;
                // POSTs to /api/services return an array, GETs from /api/states return an object
                if (document.isObject())
                {
                    QJsonObject obj = document.object();

                    if (obj.contains(QStringLiteral("entity_id")) && obj.contains(QStringLiteral("state")))
                    {
                        QString entityId = obj.value(QStringLiteral("entity_id")).toString();
                        if (getAfterSet(reply, entityId))
                        {
                            QHash<QString, QVariant> status;
                            QString state = obj.value(QStringLiteral("state")).toString();
                            bool dOk;
                            bool iOk;
                            int i = state.toInt(&iOk);
                            double d = state.toDouble(&dOk);
                            if ((state == "on") || (state == "playing")) {
                                status.insert(entityId, 1);
                            } else if ((state == "off") || (state == "paused")) {
                                status.insert(entityId, 0);
                            } else if (iOk) {
                                status.insert(entityId, i);
                            } else if (dOk) {
                                status.insert(entityId, d);
                            } else {
                                status.insert(entityId, state);
                            }
                            emit deviceUpdated(status);
                        }
                    }
                }
            }
            else
            {
                qDebug() << "HomeAssistantDevice::handleReply: Error parsing JSON: " << error.errorString() << " at offset " << error.offset;
            }
        }
        else
        {
            qDebug() << "HomeAssistantDevice::handleReply: error: " << reply->error();
        }
        removeGetRequest(reply);
        reply->deleteLater();
    }
    else
    {
        qDebug() << "HomeAssistantDevice::handleReply: reply is null";
    }
}

HomeAssistantDeviceDiscoverer::HomeAssistantDeviceDiscoverer(const QString& apiKey, const QString& url) :
    m_apiKey(apiKey),
    m_url(url)
{
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &HomeAssistantDeviceDiscoverer::handleReply
    );
}

HomeAssistantDeviceDiscoverer::~HomeAssistantDeviceDiscoverer()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &HomeAssistantDeviceDiscoverer::handleReply
    );
    delete m_networkManager;
}

void HomeAssistantDeviceDiscoverer::getDevices()
{
    QUrl url(m_url+ "/api/template");

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_apiKey.toLocal8Bit());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Use templates to get a list of devices and associated entities
    QString tpl =
    "{% set devices = states | map(attribute='entity_id') | map('device_id') | unique | reject('eq',None)| list %}\n"
    "{%- set ns = namespace(devices = []) %}\n"
    "{%- for device in devices %}\n"
    "  {%- set entities = device_entities(device) | list %}\n"
    "  {%- if entities %}\n"
    "    {%- set ens = namespace(entityobjs = []) %}\n"
    "    {%- for entity in entities %}\n"
    "      {%- set entityobj = {'entity_id': entity, 'name': state_attr(entity,'friendly_name'), 'unit_of_measurement': state_attr(entity,'unit_of_measurement')} %}\n"
    "      {%- set ens.entityobjs = ens.entityobjs + [ entityobj ] %}\n"
    "    {%- endfor %}\n"
    "    {%- set obj = {'device_id': device, 'name': device_attr(device,'name'), 'name_by_user': device_attr(device,'name_by_user'), 'model': device_attr(device,'model'), 'entities': ens.entityobjs } %}\n"
    "    {%- set ns.devices = ns.devices + [ obj ] %}\n"
    "  {%- endif %}\n"
    "{%- endfor %}\n"
    "{{ ns.devices | tojson }}";

    QJsonObject object {
        {"template", tpl}
    };
    QJsonDocument document;
    document.setObject(object);

    m_networkManager->post(request, document.toJson());
}

void HomeAssistantDeviceDiscoverer::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QList<DeviceInfo> devices;
            QByteArray data = reply->readAll();
            //qDebug() << "Received " << data;
            QJsonParseError error;
            QJsonDocument document = QJsonDocument::fromJson(data, &error);
            if (!document.isNull())
            {
                if (document.isArray())
                {
                    for (auto deviceRef : document.array())
                    {
                        QJsonObject deviceObj = deviceRef.toObject();
                        if (deviceObj.contains(QStringLiteral("device_id")) && deviceObj.contains(QStringLiteral("entities")))
                        {
                            QJsonArray entitiesArray = deviceObj.value(QStringLiteral("entities")).toArray();
                            if (entitiesArray.size() > 0)
                            {
                                DeviceInfo info;
                                info.m_id = deviceObj.value(QStringLiteral("device_id")).toString();

                                if (deviceObj.contains(QStringLiteral("name_by_user"))) {
                                    info.m_name = deviceObj.value(QStringLiteral("name_by_user")).toString();
                                }
                                if (info.m_name.isEmpty() && deviceObj.contains(QStringLiteral("name"))) {
                                    info.m_name = deviceObj.value(QStringLiteral("name")).toString();
                                }
                                if (deviceObj.contains(QStringLiteral("model"))) {
                                    info.m_model = deviceObj.value(QStringLiteral("model")).toString();
                                }

                                for (auto entityRef : entitiesArray)
                                {
                                    QJsonObject entityObj = entityRef.toObject();
                                    QString entity = entityObj.value(QStringLiteral("entity_id")).toString();
                                    QString name = entityObj.value(QStringLiteral("name")).toString();
                                    QString domain = entity.left(entity.indexOf('.'));
                                    if (domain == "binary_sensor")
                                    {
                                        SensorInfo *sensorInfo = new SensorInfo();
                                        sensorInfo->m_name = name;
                                        sensorInfo->m_id = entity;
                                        sensorInfo->m_type = DeviceDiscoverer::BOOL;
                                        sensorInfo->m_units = entityObj.value(QStringLiteral("unit_of_measurement")).toString();
                                        info.m_sensors.append(sensorInfo);
                                    }
                                    else if (domain == "sensor")
                                    {
                                        SensorInfo *sensorInfo = new SensorInfo();
                                        sensorInfo->m_name = name;
                                        sensorInfo->m_id = entity;
                                        sensorInfo->m_type = DeviceDiscoverer::FLOAT; // FIXME: Auto?
                                        sensorInfo->m_units = entityObj.value(QStringLiteral("unit_of_measurement")).toString();
                                        info.m_sensors.append(sensorInfo);
                                    }
                                    else if ((domain == "switch") || (domain == "light") || (domain == "media_player")) // Entities that support turn_on/turn_off
                                    {
                                        ControlInfo *controlInfo = new ControlInfo();
                                        controlInfo->m_name = name;
                                        controlInfo->m_id = entity;
                                        controlInfo->m_type = DeviceDiscoverer::BOOL;
                                        info.m_controls.append(controlInfo);
                                    }
                                    else
                                    {
                                        qDebug() << "HomeAssistantDeviceDiscoverer::handleReply: Unsupported domain: " << domain;
                                    }
                                }

                                if ((info.m_controls.size() > 0) || (info.m_sensors.size() > 0))
                                {
                                    devices.append(info);
                                }
                            }
                            else
                            {
                                //qDebug() << "HomeAssistantDeviceDiscoverer::handleReply: No entities " << deviceObj.value(QStringLiteral("device_id")).toString();
                            }
                        }
                        else
                        {
                            //qDebug() << "HomeAssistantDeviceDiscoverer::handleReply: device_id or entities missing";
                        }
                    }
                }
                else
                {
                    qDebug() << "HomeAssistantDeviceDiscoverer::handleReply: Document is not an array: " << document;
                }
            }
            else
            {
                qDebug() << "HomeAssistantDeviceDiscoverer::handleReply: Error parsing JSON: " << error.errorString() << " at offset " << error.offset;
            }
            emit deviceList(devices);
        }
        else
        {
            qDebug() << "HomeAssistantDeviceDiscoverer::handleReply: error: " << reply->error() << ":" << reply->errorString();
            // Get QNetworkReply::AuthenticationRequiredError if token is invalid
            if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
                emit error("Home Assistant: Authentication failed. Check access token is valid.");
            } else {
                emit error(QString("Home Assistant: Network error. %1").arg(reply->errorString()));
            }
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "HomeAssistantDeviceDiscoverer::handleReply: reply is null";
    }
}
