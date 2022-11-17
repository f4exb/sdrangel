///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include <QDataStream>
#include <QIODevice>

#include "util/simpleserializer.h"
#include "deviceuserargs.h"

QDataStream &operator<<(QDataStream &ds, const DeviceUserArgs::Args &inObj)
{
	ds << inObj.m_id << inObj.m_sequence << inObj.m_args << inObj.m_nonDiscoverable;
    return ds;
}

QDataStream &operator>>(QDataStream &ds, DeviceUserArgs::Args &outObj)
{
	ds >> outObj.m_id >> outObj.m_sequence >> outObj.m_args >> outObj.m_nonDiscoverable;
    return ds;
}

QByteArray DeviceUserArgs::serialize() const
{
    SimpleSerializer s(1);
    QByteArray data;
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    *stream << m_argsByDevice;
    s.writeBlob(1, data);
    return s.final();
}

bool DeviceUserArgs::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        return false;
    }

    if(d.getVersion() == 1)
    {
        QByteArray data;

        d.readBlob(1, &data);
        QDataStream readStream(&data, QIODevice::ReadOnly);
        readStream >> m_argsByDevice;

        return true;
    }
    else
    {
        return false;
    }
}

QString DeviceUserArgs::findUserArgs(const QString& id, int sequence)
{
    for (int i = 0; i < m_argsByDevice.size(); i++)
    {
        if ((m_argsByDevice.at(i).m_id == id) && (m_argsByDevice.at(i).m_sequence == sequence)) {
            return m_argsByDevice.at(i).m_args;
        }
    }

    return "";
}

void DeviceUserArgs::addDeviceArgs(const QString& id, int sequence, const QString& deviceArgs, bool nonDiscoverable)
{
    int i = 0;

    for (; i < m_argsByDevice.size(); i++)
    {
        if ((m_argsByDevice.at(i).m_id == id) && (m_argsByDevice.at(i).m_sequence == sequence)) {
            break;
        }
    }

    if (i == m_argsByDevice.size()) {
        m_argsByDevice.push_back(Args(id, sequence, deviceArgs, nonDiscoverable));
    }
}

void DeviceUserArgs::addOrUpdateDeviceArgs(const QString& id, int sequence, const QString& deviceArgs, bool nonDiscoverable)
{
    int i = 0;

    for (; i < m_argsByDevice.size(); i++)
    {
        if ((m_argsByDevice.at(i).m_id == id) && (m_argsByDevice.at(i).m_sequence == sequence)) {
            m_argsByDevice[i].m_args = deviceArgs;
        }
    }

    if (i == m_argsByDevice.size()) {
        m_argsByDevice.push_back(Args(id, sequence, deviceArgs, nonDiscoverable));
    }
}

void DeviceUserArgs::updateDeviceArgs(const QString& id, int sequence, const QString& deviceArgs, bool nonDiscoverable)
{
    int i = 0;

    for (; i < m_argsByDevice.size(); i++)
    {
        if ((m_argsByDevice.at(i).m_id == id) && (m_argsByDevice.at(i).m_sequence == sequence))
        {
            m_argsByDevice[i].m_args = deviceArgs;
            m_argsByDevice[i].m_nonDiscoverable = nonDiscoverable;
        }
    }
}

void DeviceUserArgs::deleteDeviceArgs(const QString& id, int sequence)
{
    int i = 0;

    for (; i < m_argsByDevice.size(); i++)
    {
        if ((m_argsByDevice.at(i).m_id == id) && (m_argsByDevice.at(i).m_sequence == sequence))
        {
            m_argsByDevice.takeAt(i);
            return;
        }
    }
}
