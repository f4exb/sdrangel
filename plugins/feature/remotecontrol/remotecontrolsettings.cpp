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

#include <QColor>
#include <QDebug>
#include <QDataStream>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "remotecontrolsettings.h"

RemoteControlSettings::RemoteControlSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void RemoteControlSettings::resetToDefaults()
{
    m_updatePeriod = 1.0f;
    m_tpLinkUsername = "";
    m_tpLinkPassword = "";
    m_homeAssistantToken = "";
    m_homeAssistantHost = "http://homeassistant.local:8123";
    m_visaResourceFilter = "";
    m_visaLogIO = false;
    m_chartHeightFixed = false;
    m_chartHeightPixels = 130;

    m_title = "Remote Control";
    m_rgbColor = QColor(225, 25, 99).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;
}

QByteArray RemoteControlSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeFloat(1, m_updatePeriod);
    s.writeString(2, m_tpLinkUsername);
    s.writeString(3, m_tpLinkPassword);
    s.writeString(4, m_homeAssistantToken);
    s.writeString(5, m_homeAssistantHost);
    s.writeString(6, m_visaResourceFilter);
    s.writeBool(7, m_visaLogIO);
    s.writeBool(10, m_chartHeightFixed);
    s.writeS32(11, m_chartHeightPixels);

    s.writeBlob(19, serializeDeviceList(m_devices));

    s.writeString(20, m_title);
    s.writeU32(21, m_rgbColor);
    s.writeBool(22, m_useReverseAPI);
    s.writeString(23, m_reverseAPIAddress);
    s.writeU32(24, m_reverseAPIPort);
    s.writeU32(25, m_reverseAPIFeatureSetIndex);
    s.writeU32(26, m_reverseAPIFeatureIndex);

    if (m_rollupState) {
        s.writeBlob(27, m_rollupState->serialize());
    }

    s.writeS32(28, m_workspaceIndex);

    return s.final();
}

bool RemoteControlSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t utmp;
        QString strtmp;
        QByteArray blob;

        d.readFloat(1, &m_updatePeriod, 1.0f);
        d.readString(2, &m_tpLinkUsername, "");
        d.readString(3, &m_tpLinkPassword, "");
        d.readString(4, &m_homeAssistantToken, "");
        d.readString(5, &m_homeAssistantHost, "http://homeassistant.local:8123");
        d.readString(6, &m_visaResourceFilter, "");
        d.readBool(7, &m_visaLogIO, false);

        d.readBool(10, &m_chartHeightFixed, false);
        d.readS32(11, &m_chartHeightPixels, 130);

        d.readBlob(19, &blob);
        deserializeDeviceList(blob, m_devices);

        d.readString(20, &m_title, "Remote Control");
        d.readU32(21, &m_rgbColor, QColor(225, 25, 99).rgb());
        d.readBool(22, &m_useReverseAPI, false);
        d.readString(23, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(24, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(25, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(26, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(27, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(28, &m_workspaceIndex, 0);
        d.readBlob(29, &m_geometryBytes);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

QByteArray RemoteControlControl::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_id);
    s.writeString(2, m_labelLeft);
    s.writeString(3, m_labelRight);

    return s.final();
}

bool RemoteControlControl::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        return false;
    }

    if (d.getVersion() == 1)
    {
        d.readString(1, &m_id);
        d.readString(2, &m_labelLeft);
        d.readString(3, &m_labelRight);
        return true;
    }
    else
    {
        return false;
    }
}

QByteArray RemoteControlSensor::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_id);
    s.writeString(2, m_labelLeft);
    s.writeString(3, m_labelRight);
    s.writeString(4, m_format);
    s.writeBool(5, m_plot);

    return s.final();
}

bool RemoteControlSensor::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        return false;
    }

    if (d.getVersion() == 1)
    {
        d.readString(1, &m_id);
        d.readString(2, &m_labelLeft);
        d.readString(3, &m_labelRight);
        d.readString(4, &m_format);
        d.readBool(5, &m_plot);
        return true;
    }
    else
    {
        return false;
    }
}

QByteArray RemoteControlDevice::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_protocol);
    s.writeString(2, m_label);
    s.writeBlob(3, serializeControlList());
    s.writeBlob(4, serializeSensorList());
    s.writeBool(5, m_verticalControls);
    s.writeBool(6, m_verticalSensors);
    s.writeBool(7, m_commonYAxis);
    s.writeBlob(8, m_info.serialize());

    return s.final();
}

bool RemoteControlDevice::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray blob;

        d.readString(1, &m_protocol);
        d.readString(2, &m_label);
        d.readBlob(3, &blob);
        deserializeControlList(blob);
        d.readBlob(4, &blob);
        deserializeSensorList(blob);
        d.readBool(5, &m_verticalControls, false);
        d.readBool(6, &m_verticalSensors, true);
        d.readBool(7, &m_commonYAxis);
        d.readBlob(8, &blob);
        m_info.deserialize(blob);

        return true;
    }
    else
    {
        return false;
    }
}

QDataStream& operator<<(QDataStream& out, const RemoteControlControl& control)
{
    out << control.serialize();
    return out;
}

QDataStream& operator>>(QDataStream& in, RemoteControlControl& control)
{
    QByteArray data;
    in >> data;
    control.deserialize(data);
    return in;
}

QDataStream& operator<<(QDataStream& out, const RemoteControlSensor& sensor)
{
    out << sensor.serialize();
    return out;
}

QDataStream& operator>>(QDataStream& in, RemoteControlSensor& sensor)
{
    QByteArray data;
    in >> data;
    sensor.deserialize(data);
    return in;
}

QDataStream& operator<<(QDataStream& out, const RemoteControlDevice* device)
{
    out << device->serialize();
    return out;
}

QDataStream& operator>>(QDataStream& in, RemoteControlDevice*& device)
{
    device = new RemoteControlDevice();
    QByteArray data;
    in >> data;
    device->deserialize(data);
    return in;
}

QByteArray RemoteControlDevice::serializeControlList() const
{
    QByteArray data;
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    (*stream) << m_controls;
    delete stream;
    return data;
}

void RemoteControlDevice::deserializeControlList(const QByteArray& data)
{
    QDataStream *stream = new QDataStream(data);
    (*stream) >> m_controls;
    delete stream;
}

QByteArray RemoteControlDevice::serializeSensorList() const
{
    QByteArray data;
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    (*stream) << m_sensors;
    delete stream;
    return data;
}

void RemoteControlDevice::deserializeSensorList(const QByteArray& data)
{
    QDataStream *stream = new QDataStream(data);
    (*stream) >> m_sensors;
    delete stream;
}

QByteArray RemoteControlSettings::serializeDeviceList(const QList<RemoteControlDevice *>& devices) const
{
    QByteArray data;
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    (*stream) << devices;
    delete stream;
    return data;
}

void RemoteControlSettings::deserializeDeviceList(const QByteArray& data, QList<RemoteControlDevice *>& devices)
{
    QDataStream *stream = new QDataStream(data);
    (*stream) >> devices;
    delete stream;
}
