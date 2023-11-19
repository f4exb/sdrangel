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

#include "util/iot/visa.h"
#include "util/visa.h"
#include "util/simpleserializer.h"

VISADevice::VISADevice(const QHash<QString, QVariant> settings, const QString &deviceId,
                       const QStringList &controls, const QStringList &sensors,
                       DeviceDiscoverer::DeviceInfo *info) :
    Device(info),
    m_deviceId(deviceId),
    m_session(0),
    m_controls(controls),
    m_sensors(sensors)
{
    m_visa.openDefault();

    QHashIterator<QString, QVariant> itr(settings);
    while (itr.hasNext())
    {
        itr.next();
        QString key = itr.key();
        QVariant value = itr.value();

        if ((key == "deviceId") || (key == "controlIds") || (key == "sensorIds"))
        {
            // Nothing to do here
        }
        else if (key == "logIO")
        {
            m_visa.setDebugIO(value.toBool());
        }
        else
        {
            qDebug() << "VISADevice::VISADevice: Unsupported setting key: " << key << " value: " << value;
        }
    }

    open();
}

VISADevice::~VISADevice()
{
    m_visa.close(m_session);
    m_visa.closeDefault();
}

bool VISADevice::open()
{
    if (!m_session) {
        m_session = m_visa.open(m_deviceId);
    }
    if (!m_session) {
        emit deviceUnavailable();
    }
    return m_session != 0;
}

bool VISADevice::convertToBool(const QString &string, bool &ok)
{
    QString lower = string.trimmed().toLower();
    if ((lower == "0") || (lower == "false") || (lower == "off"))
    {
        ok = true;
        return false;
    }
    else if ((lower == "1") || (lower == "true") || (lower == "on"))
    {
        ok = true;
        return true;
    }
    else
    {
        ok = false;
        return false;
    }
}

void VISADevice::convert(QHash<QString, QVariant> &status, const QString &id, DeviceDiscoverer::Type type, const QString &state)
{
    if (type == DeviceDiscoverer::BOOL)
    {
        bool ok;
        bool value = convertToBool(state, ok);
        if (ok) {
            status.insert(id, value);
        } else {
            status.insert(id, "error");
        }
    }
    else if (type == DeviceDiscoverer::INT)
    {
        bool ok;
        int value = state.toInt(&ok);
        if (ok) {
            status.insert(id, value);
        } else {
            status.insert(id, "error");
        }
    }
    else if (type == DeviceDiscoverer::FLOAT)
    {
        bool ok;
        float value = state.toFloat(&ok);
        if (ok) {
            status.insert(id, value);
        } else {
            status.insert(id, "error");
        }
    }
    else
    {
        status.insert(id, state);
    }
}

void VISADevice::getState()
{
    if (open())
    {
        QHash<QString, QVariant> status;

        for (auto c : m_info.m_controls)
        {
            if (m_controls.contains(c->m_id))
            {
                VISAControl *control = reinterpret_cast<VISAControl *>(c);
                QString cmds = control->m_getState.trimmed();
                if (!cmds.isEmpty())
                {
                    bool error;
                    QStringList results = m_visa.processCommands(m_session, cmds, &error);
                    if (!error && (results.size() > 0))
                    {
                        // Take last returned value as the state
                        QString state = results[results.size()-1].trimmed();
                        convert(status, control->m_id, control->m_type, state);
                    }
                    else
                    {
                        status.insert(control->m_id, "error");
                    }
                }
            }
        }
        for (auto s : m_info.m_sensors)
        {
            if (m_sensors.contains(s->m_id))
            {
                VISASensor *sensor = reinterpret_cast<VISASensor *>(s);
                QString cmds = sensor->m_getState.trimmed();
                if (!cmds.isEmpty())
                {
                    bool error;
                    QStringList results = m_visa.processCommands(m_session, cmds, &error);
                    if (!error && (results.size() > 0))
                    {
                        // Take last returned value as the state
                        QString state = results[results.size()-1].trimmed();
                        convert(status, sensor->m_id, sensor->m_type, state);
                    }
                    else
                    {
                        status.insert(sensor->m_id, "error");
                    }
                }
            }
        }

        emit deviceUpdated(status);
    }
}

void VISADevice::setState(const QString &controlId, bool state)
{
    if (open())
    {
        for (auto c : m_info.m_controls)
        {
            VISAControl *control = reinterpret_cast<VISAControl *>(c);
            if (control->m_id == controlId)
            {
                QString commands = QString::asprintf(control->m_setState.toUtf8(), (int)state);
                bool error;
                m_visa.processCommands(m_session, commands, &error);
                if (error) {
                    qDebug() << "VISADevice::setState: Failed to set state of " << controlId;
                }
            }
        }
    }
}

void VISADevice::setState(const QString &controlId, int state)
{
    if (open())
    {
        for (auto c : m_info.m_controls)
        {
            VISAControl *control = reinterpret_cast<VISAControl *>(c);
            if (control->m_id == controlId)
            {
                QString commands = QString::asprintf(control->m_setState.toUtf8(), state);
                bool error;
                m_visa.processCommands(m_session, commands, &error);
                if (error) {
                    qDebug() << "VISADevice::setState: Failed to set state of " << controlId;
                }
            }
        }
    }
}

void VISADevice::setState(const QString &controlId, float state)
{
    if (open())
    {
        for (auto c : m_info.m_controls)
        {
            VISAControl *control = reinterpret_cast<VISAControl *>(c);
            if (control->m_id == controlId)
            {
                QString commands = QString::asprintf(control->m_setState.toUtf8(), state);
                bool error;
                m_visa.processCommands(m_session, commands, &error);
                if (error) {
                    qDebug() << "VISADevice::setState: Failed to set state of " << controlId;
                }
            }
        }
    }
}

void VISADevice::setState(const QString &controlId, const QString &state)
{
    if (open())
    {
        for (auto c : m_info.m_controls)
        {
            VISAControl *control = reinterpret_cast<VISAControl *>(c);
            if (control->m_id == controlId)
            {
                QString commands = QString::asprintf(control->m_setState.toUtf8(), state.toUtf8().data());
                bool error;
                m_visa.processCommands(m_session, commands, &error);
                if (error) {
                    qDebug() << "VISADevice::setState: Failed to set state of " << controlId;
                }
            }
        }
    }
}

VISADeviceDiscoverer::VISADeviceDiscoverer(const QString& resourceFilter) :
    m_resourceFilter(resourceFilter)
{
    m_session = m_visa.openDefault();
}

VISADeviceDiscoverer::~VISADeviceDiscoverer()
{
    m_visa.closeDefault();
}

void VISADeviceDiscoverer::getDevices()
{
    QRegularExpression *filterP = nullptr;
    QRegularExpression filter(m_resourceFilter);
    if (!m_resourceFilter.trimmed().isEmpty()) {
        filterP = &filter;
    }

    // Get list of VISA instruments
    QList<VISA::Instrument> instruments = m_visa.instruments(filterP);

    // Convert to list of devices
    QList<DeviceInfo> devices;
    for (auto const &instrument : instruments)
    {
        DeviceInfo info;
        info.m_name = instrument.m_model;
        info.m_id = instrument.m_resource;
        info.m_model = instrument.m_model;

        if ((info.m_name == "DP832") || (info.m_name == "DP832A"))
        {
            for (int i = 1; i <= 3; i++)
            {
                VISADevice::VISAControl *output = new VISADevice::VISAControl();
                output->m_name = QString("CH%1").arg(i);
                output->m_id = QString("control.ch%1").arg(i);
                output->m_type = BOOL;
                output->m_getState = QString(":OUTPUT? CH%1").arg(i);
                output->m_setState = QString(":OUTPUT CH%1,%d").arg(i);
                info.m_controls.append(output);

                VISADevice::VISAControl *setVoltage = new VISADevice::VISAControl();
                setVoltage->m_name = QString("V%1").arg(i);
                setVoltage->m_id = QString("control.voltage%1").arg(i);
                setVoltage->m_type = FLOAT;
                setVoltage->m_min = 0.0f;
                setVoltage->m_max = i == 3 ? 5.0f : 30.0f;
                setVoltage->m_scale = 1.0f;
                setVoltage->m_precision = 3;
                setVoltage->m_widgetType = SPIN_BOX;
                setVoltage->m_units = "V";
                setVoltage->m_getState = QString(":SOURCE%1:VOLTage?").arg(i);
                setVoltage->m_setState = QString(":SOURCE%1:VOLTage %f").arg(i);
                info.m_controls.append(setVoltage);

                VISADevice::VISAControl *setCurrent = new VISADevice::VISAControl();
                setCurrent->m_name = QString("i%1").arg(i);
                setCurrent->m_id = QString("control.current%1").arg(i);
                setCurrent->m_type = FLOAT;
                setCurrent->m_min = 0.0f;
                setCurrent->m_max = 3.0f;
                setCurrent->m_scale = 1.0f;
                setCurrent->m_precision = 3;
                setCurrent->m_widgetType = SPIN_BOX;
                setCurrent->m_units = "A";
                setCurrent->m_getState = QString(":SOURCE%1:CURRent?").arg(i);
                setCurrent->m_setState = QString(":SOURCE%1:CURRent %f").arg(i);
                info.m_controls.append(setCurrent);

                VISADevice::VISASensor *voltage = new VISADevice::VISASensor();
                voltage->m_name = QString("V%1").arg(i);
                voltage->m_id = QString("sensor.voltage%1").arg(i);
                voltage->m_type = FLOAT;
                voltage->m_units = "V";
                voltage->m_getState = QString(":MEASure:VOLTage? CH%1").arg(i);
                info.m_sensors.append(voltage);

                VISADevice::VISASensor *current = new VISADevice::VISASensor();
                current->m_name = QString("i%1").arg(i);
                current->m_id = QString("sensor.current%1").arg(i);
                current->m_type = FLOAT;
                current->m_units = "A";
                current->m_getState = QString(":MEASure:CURRent? CH%1").arg(i);
                info.m_sensors.append(current);

                VISADevice::VISASensor *power = new VISADevice::VISASensor();
                power->m_name = QString("P%1").arg(i);
                power->m_id = QString("sensor.power%1").arg(i);
                power->m_type = FLOAT;
                power->m_units = "W";
                power->m_getState = QString(":MEASure:POWEr? CH%1").arg(i);
                info.m_sensors.append(power);
            }
        }
        else if (info.m_name == "SSA3032X")
        {
            VISADevice::VISAControl *frequency = new VISADevice::VISAControl();
            frequency->m_name = "Frequency";
            frequency->m_id = "control.frequency";
            frequency->m_type = FLOAT;
            frequency->m_min = 0.0f;
            frequency->m_max = 3.2e3f;
            frequency->m_scale = 1e6f;
            frequency->m_precision = 6;
            frequency->m_widgetType = SPIN_BOX;
            frequency->m_units = "MHz";
            frequency->m_getState = ":FREQuency:CENTer?";
            frequency->m_setState = ":FREQuency:CENTer %f";
            info.m_controls.append(frequency);

            VISADevice::VISAControl *span = new VISADevice::VISAControl();
            span->m_name = "Span";
            span->m_id = "control.span";
            span->m_type = FLOAT;
            span->m_min = 0.0f;
            span->m_max = 3.2e3f;
            span->m_scale = 1e6;
            span->m_precision = 3;
            span->m_widgetType = SPIN_BOX;
            span->m_units = "MHz";
            span->m_getState = ":FREQuency:SPAN?";
            span->m_setState = ":FREQuency:SPAN %f";
            info.m_controls.append(span);

            VISADevice::VISAControl *markerX = new VISADevice::VISAControl();
            markerX->m_name = "Marker X";
            markerX->m_id = "control.markerx";
            markerX->m_type = FLOAT;
            markerX->m_min = 0.0f;
            markerX->m_max = 3.2e3f;
            markerX->m_scale = 1e6;
            markerX->m_precision = 6;
            markerX->m_widgetType = SPIN_BOX;
            markerX->m_units = "MHz";
            markerX->m_getState = ":CALCulate:MARKer1:X?";
            markerX->m_setState = ":CALCulate:MARKer1:X %f";
            info.m_controls.append(markerX);

            VISADevice::VISASensor *markerY = new VISADevice::VISASensor();
            markerY->m_name = "Marker Y";
            markerY->m_id = "sensor.markery";
            markerY->m_type = FLOAT;
            markerY->m_units = "dBm";
            markerY->m_getState = ":CALCulate:MARKer1:Y?";
            info.m_sensors.append(markerY);
        }

        devices.append(info);
    }
    emit deviceList(devices);
}

DeviceDiscoverer::ControlInfo *VISADevice::VISAControl::clone() const
{
    return new VISAControl(*this);
}

QByteArray VISADevice::VISAControl::serialize() const
{
    SimpleSerializer s(1);

    s.writeBlob(1, ControlInfo::serialize());
    s.writeString(2, m_getState);
    s.writeString(3, m_setState);

    return s.final();
}

bool VISADevice::VISAControl::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray blob;

        d.readBlob(1, &blob);
        ControlInfo::deserialize(blob);
        d.readString(2, &m_getState);
        d.readString(3, &m_setState);
        return true;
    }
    else
    {
        return false;
    }
}

DeviceDiscoverer::SensorInfo *VISADevice::VISASensor::clone() const
{
    return new VISASensor(*this);
}

QByteArray VISADevice::VISASensor::serialize() const
{
    SimpleSerializer s(1);

    s.writeBlob(1, SensorInfo::serialize());
    s.writeString(2, m_getState);
    return s.final();
}

bool VISADevice::VISASensor::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray blob;

        d.readBlob(1, &blob);
        SensorInfo::deserialize(blob);
        d.readString(2, &m_getState);
        return true;
    }
    else
    {
        return false;
    }
}
