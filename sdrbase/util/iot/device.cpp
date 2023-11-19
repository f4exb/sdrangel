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

#include "util/simpleserializer.h"
#include "util/iot/device.h"
#include "util/iot/tplink.h"
#include "util/iot/homeassistant.h"
#include "util/iot/visa.h"

Device::Device(DeviceDiscoverer::DeviceInfo *info)
{
    if (info) {
        m_info = *info;
    }
}

Device* Device::create(const QHash<QString, QVariant>& settings, const QString& protocol, DeviceDiscoverer::DeviceInfo *info)
{
    if (checkSettings(settings, protocol))
    {
        if (protocol == "TPLink")
        {
            if (settings.contains("deviceId"))
            {
                return new TPLinkDevice(settings.value("username").toString(),
                                        settings.value("password").toString(),
                                        settings.value("deviceId").toString(),
                                        info);
            }
            else
            {
                qDebug() << "Device::create: A deviceId is required for: " << protocol;
            }
        }
        else if (protocol == "HomeAssistant")
        {
            if (settings.contains("deviceId"))
            {
                return new HomeAssistantDevice(settings.value("apiKey").toString(),
                                               settings.value("url").toString(),
                                               settings.value("deviceId").toString(),
                                               settings.value("controlIds").toStringList(),
                                               settings.value("sensorIds").toStringList(),
                                               info);
            }
            else
            {
                qDebug() << "Device::create: A deviceId is required for: " << protocol;
            }
        }
        else if (protocol == "VISA")
        {
            if (settings.contains("deviceId"))
            {
                return new VISADevice(settings,
                                      settings.value("deviceId").toString(),
                                      settings.value("controlIds").toStringList(),
                                      settings.value("sensorIds").toStringList(),
                                      info);
            }
            else
            {
                qDebug() << "Device::create: A deviceId is required for: " << protocol;
            }
        }
    }
    return nullptr;
}

bool Device::checkSettings(const QHash<QString, QVariant>& settings, const QString& protocol)
{
    if (protocol == "TPLink")
    {
        if (settings.contains("username") && settings.contains("password"))
        {
            return true;
        }
        else
        {
            qDebug() << "Device::checkSettings: A username and password are required for: " << protocol;
            return false;
        }
    }
    else if (protocol == "HomeAssistant")
    {
        if (settings.contains("apiKey"))
        {
            if (settings.contains("url"))
            {
                return true;
            }
            else
            {
                qDebug() << "Device::checkSettings: A host url is required for: " << protocol;
                return false;
            }
        }
        else
        {
            qDebug() << "Device::checkSettings: An apiKey is required for: " << protocol;
            return false;
        }
    }
    else if (protocol == "VISA")
    {
        return true;
    }
    else
    {
        qDebug() << "Device::checkSettings: Unsupported protocol: " << protocol;
        return false;
    }
}

void Device::setState(const QString &controlId, bool state)
{
    qDebug() << "Device::setState: " << getProtocol() << " doesn't support bool. Can't set " << controlId << " to " << state;
}

void Device::setState(const QString &controlId, int state)
{
    qDebug() << "Device::setState: " << getProtocol() << " doesn't support int. Can't set " << controlId << " to " << state;
}

void Device::setState(const QString &controlId, float state)
{
    qDebug() << "Device::setState: " << getProtocol() << " doesn't support float. Can't set " << controlId << " to " << state;
}

void Device::setState(const QString &controlId, const QString &state)
{
    qDebug() << "Device::setState: " << getProtocol() << " doesn't support QString. Can't set " << controlId << " to " << state;
}

void Device::recordGetRequest(void *ptr)
{
    m_getRequests.insert(ptr, QDateTime::currentDateTime());
}

void Device::removeGetRequest(void *ptr)
{
    m_getRequests.remove(ptr);
}

void Device::recordSetRequest(const QString &id, int guardMS)
{
    m_setRequests.insert(id, QDateTime::currentDateTime().addMSecs(guardMS));
}

bool Device::getAfterSet(void *ptr, const QString &id)
{
    if (m_getRequests.contains(ptr) && m_setRequests.contains(id))
    {
        QDateTime getTime = m_getRequests.value(ptr);
        QDateTime setTime = m_setRequests.value(id);
        return getTime > setTime;
    }
    else
    {
        return true;
    }
}

const QStringList DeviceDiscoverer::m_typeStrings = {
    "Auto",
    "Boolean",
    "Integer",
    "Float",
    "String",
    "List",
    "Button"
};

const QStringList DeviceDiscoverer::m_widgetTypeStrings = {
    "Spin box",
    "Dial",
    "Slider"
};

DeviceDiscoverer::DeviceDiscoverer()
{
}

DeviceDiscoverer *DeviceDiscoverer::getDiscoverer(const QHash<QString, QVariant>& settings, const QString& protocol)
{
    if (Device::checkSettings(settings, protocol))
    {
        if (protocol == "TPLink")
        {
            return new TPLinkDeviceDiscoverer(settings.value("username").toString(), settings.value("password").toString());
        }
        else if (protocol == "HomeAssistant")
        {
            return new HomeAssistantDeviceDiscoverer(settings.value("apiKey").toString(), settings.value("url").toString());
        }
        else if (protocol == "VISA")
        {
            return new VISADeviceDiscoverer(settings.value("resourceFilter").toString());
        }
    }
    return nullptr;
}


DeviceDiscoverer::DeviceInfo::DeviceInfo()
{
}

DeviceDiscoverer::DeviceInfo::DeviceInfo(const DeviceInfo &info)
{
    m_name = info.m_name;
    m_id = info.m_id;
    m_model = info.m_model;
    // Take deep-copy of controls and sensors
    for (auto const control : info.m_controls) {
        ControlInfo *ci = control->clone();
        m_controls.append(ci);
    }
    for (auto const sensor : info.m_sensors) {
        m_sensors.append(sensor->clone());
    }
}

DeviceDiscoverer::DeviceInfo& DeviceDiscoverer::DeviceInfo::operator=(const DeviceInfo &info)
{
    m_name = info.m_name;
    m_id = info.m_id;
    m_model = info.m_model;
    qDeleteAll(m_controls);
    m_controls.clear();
    qDeleteAll(m_sensors);
    m_sensors.clear();
    // Take deep-copy of controls and sensors
    for (auto const control : info.m_controls) {
        m_controls.append(control->clone());
    }
    for (auto const sensor : info.m_sensors) {
        m_sensors.append(sensor->clone());
    }
    return *this;
}

DeviceDiscoverer::DeviceInfo::~DeviceInfo()
{
    qDeleteAll(m_controls);
    m_controls.clear();
    qDeleteAll(m_sensors);
    m_sensors.clear();
}

DeviceDiscoverer::DeviceInfo::operator QString() const
{
    QString controls;
    QString sensors;

    for (auto control : m_controls) {
        controls.append((QString)*control);
    }
    for (auto sensor : m_sensors) {
        sensors.append((QString)*sensor);
    }

    return QString("DeviceInfo: m_name: %1 m_id: %2 m_model: %3 m_controls: %4 m_sensors: %5")
                .arg(m_name)
                .arg(m_id)
                .arg(m_model)
                .arg(controls)
                .arg(sensors);
}


DeviceDiscoverer::ControlInfo::ControlInfo() :
    m_type(AUTO),
    m_min(-1000000),
    m_max(1000000),
    m_scale(1.0f),
    m_precision(3),
    m_widgetType(SPIN_BOX)
{
}

DeviceDiscoverer::ControlInfo::operator QString() const
{
    return QString("ControlInfo: m_name: %1 m_id: %2 m_type: %3")
                .arg(m_name)
                .arg(m_id)
                .arg(DeviceDiscoverer::m_typeStrings[m_type]);
}

DeviceDiscoverer::ControlInfo *DeviceDiscoverer::ControlInfo::clone() const
{
    return new ControlInfo(*this);
}

QByteArray DeviceDiscoverer::ControlInfo::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_name);
    s.writeString(2, m_id);
    s.writeS32(3, (int)m_type);
    s.writeFloat(4, m_min);
    s.writeFloat(5, m_max);
    s.writeFloat(6, m_scale);
    s.writeS32(7, m_precision);
    s.writeList(8, m_values);
    s.writeS32(9, (int)m_widgetType);
    s.writeString(10, m_units);

    return s.final();
}

bool DeviceDiscoverer::ControlInfo::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        return false;
    }

    if (d.getVersion() == 1)
    {
        d.readString(1, &m_name);
        d.readString(2, &m_id);
        d.readS32(3, (int*)&m_type);
        d.readFloat(4, &m_min);
        d.readFloat(5, &m_max);
        d.readFloat(6, &m_scale, 1.0f);
        d.readS32(7, &m_precision, 3);
        d.readList(8, &m_values);
        d.readS32(9, (int *)&m_widgetType);
        d.readString(10, &m_units);
        return true;
    }
    else
    {
        return false;
    }
}

DeviceDiscoverer::SensorInfo::operator QString() const
{
    return QString("SensorInfo: m_name: %1 m_id: %2 m_type: %3")
                .arg(m_name)
                .arg(m_id)
                .arg(DeviceDiscoverer::m_typeStrings[m_type]);
}

DeviceDiscoverer::SensorInfo *DeviceDiscoverer::SensorInfo::clone() const
{
    return new SensorInfo(*this);
}

QByteArray DeviceDiscoverer::SensorInfo::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_name);
    s.writeString(2, m_id);
    s.writeS32(3, (int)m_type);
    s.writeString(4, m_units);

    return s.final();
}

bool DeviceDiscoverer::SensorInfo::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        return false;
    }

    if (d.getVersion() == 1)
    {
        d.readString(1, &m_name);
        d.readString(2, &m_id);
        d.readS32(3, (int*)&m_type);
        d.readString(4, &m_units);
        return true;
    }
    else
    {
        return false;
    }
}

QByteArray DeviceDiscoverer::DeviceInfo::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_name);
    s.writeString(2, m_id);
    s.writeString(3, m_model);
    s.writeList(10, m_controls);
    s.writeList(11, m_sensors);

    return s.final();
}

bool DeviceDiscoverer::DeviceInfo::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray blob;

        d.readString(1, &m_name);
        d.readString(2, &m_id);
        d.readString(3, &m_model);
        d.readList(10, &m_controls);
        d.readList(11, &m_sensors);
        return true;
    }
    else
    {
        return false;
    }
}

DeviceDiscoverer::ControlInfo *DeviceDiscoverer::DeviceInfo::getControl(const QString &id) const
{
    for (auto c : m_controls)
    {
        if (c->m_id == id) {
            return c;
        }
    }
    return nullptr;
}

DeviceDiscoverer::SensorInfo *DeviceDiscoverer::DeviceInfo::getSensor(const QString &id) const
{
    for (auto s : m_sensors)
    {
        if (s->m_id == id) {
            return s;
        }
    }
    return nullptr;
}

void DeviceDiscoverer::DeviceInfo::deleteControl(const QString &id)
{
    for (int i = 0; i < m_controls.size(); i++)
    {
        if (m_controls[i]->m_id == id)
        {
            delete m_controls.takeAt(i);
            return;
        }
    }
}

void DeviceDiscoverer::DeviceInfo::deleteSensor(const QString &id)
{
    for (int i = 0; i < m_sensors.size(); i++)
    {
        if (m_sensors[i]->m_id == id)
        {
            delete m_sensors.takeAt(i);
            return;
        }
    }
}

QDataStream& operator<<(QDataStream& out, const DeviceDiscoverer::ControlInfo* control)
{
    int typeId;
    if (dynamic_cast<const VISADevice::VISAControl *>(control)) {
        typeId = 1;
    } else {
        typeId = 0;
    }
    out << typeId;
    out << control->serialize();
    return out;
}

QDataStream& operator>>(QDataStream& in, DeviceDiscoverer::ControlInfo*& control)
{
    QByteArray data;
    int typeId;
    in >> typeId;
    if (typeId == 1) {
        control = new VISADevice::VISAControl();
    } else {
        control = new DeviceDiscoverer::ControlInfo();
    }
    in >> data;
    control->deserialize(data);
    return in;
}

QDataStream& operator<<(QDataStream& out, const DeviceDiscoverer::SensorInfo* sensor)
{
    int typeId;
    if (dynamic_cast<const VISADevice::VISASensor *>(sensor)) {
        typeId = 1;
    } else {
        typeId = 0;
    }
    out << typeId;
    out << sensor->serialize();
    return out;
}

QDataStream& operator>>(QDataStream& in, DeviceDiscoverer::SensorInfo*& sensor)
{

    QByteArray data;
    int typeId;
    in >> typeId;
    if (typeId == 1) {
        sensor = new VISADevice::VISASensor();
    } else {
        sensor = new DeviceDiscoverer::SensorInfo();
    }
    in >> data;
    sensor->deserialize(data);
    return in;
}

QDataStream& operator<<(QDataStream& out, const VISADevice::VISASensor &sensor)
{
    out << sensor.serialize();
    return out;
}

QDataStream& operator>>(QDataStream& in, VISADevice::VISASensor& sensor)
{
    QByteArray data;
    in >> data;
    sensor.deserialize(data);
    return in;
}

QDataStream& operator<<(QDataStream& out, const VISADevice::VISAControl &control)
{
    out << control.serialize();
    return out;
}

QDataStream& operator>>(QDataStream& in, VISADevice::VISAControl& control)
{
    QByteArray data;
    in >> data;
    control.deserialize(data);
    return in;
}
