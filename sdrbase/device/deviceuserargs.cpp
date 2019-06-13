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

#include "util/simpleserializer.h"
#include "deviceuserargs.h"

QDataStream &operator<<(QDataStream &ds, const DeviceUserArgs::Args &inObj)
{
	ds << inObj.m_id << inObj.m_sequence << inObj.m_args;
}

QDataStream &operator>>(QDataStream &ds, DeviceUserArgs::Args &outObj)
{
	ds >> outObj.m_id >> outObj.m_sequence >> outObj.m_args;
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

QList<DeviceUserArgs::Args>::iterator DeviceUserArgs::findDeviceArgs(const QString& id, int sequence)
{
    DeviceUserArgs::Args args;
    args.m_id = id;
    args.m_sequence = sequence;
    QList<DeviceUserArgs::Args>::iterator it = m_argsByDevice.begin();

    for (; it != m_argsByDevice.end(); ++it)
    {
        if (*it == args) {
            return it;
        }
    }
}

void DeviceUserArgs::addDeviceArgs(const QString& id, int sequence, const QString& deviceArgs)
{
    Args args;
    args.m_id = id;
    args.m_sequence = sequence;
    args.m_args = deviceArgs;

    QList<DeviceUserArgs::Args>::iterator it = m_argsByDevice.begin();

    for (; it != m_argsByDevice.end(); ++it)
    {
        if (*it == args) {
            break;
        }
    }

    if (it == m_argsByDevice.end()) {
        m_argsByDevice.push_back(args);
    }
}

void DeviceUserArgs::addOrUpdateDeviceArgs(const QString& id, int sequence, const QString& deviceArgs)
{
    Args args;
    args.m_id = id;
    args.m_sequence = sequence;
    args.m_args = deviceArgs;

    QList<DeviceUserArgs::Args>::iterator it = m_argsByDevice.begin();

    for (; it != m_argsByDevice.end(); ++it)
    {
        if (*it == args)
        {
            it->m_args = deviceArgs;
            return;
        }
    }

    if (it == m_argsByDevice.end()) {
        m_argsByDevice.push_back(args);
    }
}

void DeviceUserArgs::updateDeviceArgs(const QString& id, int sequence, const QString& deviceArgs)
{
    Args args;
    args.m_id = id;
    args.m_sequence = sequence;

    QList<DeviceUserArgs::Args>::iterator it = m_argsByDevice.begin();

    for (; it != m_argsByDevice.end(); ++it)
    {
        if (*it == args)
        {
            it->m_args = deviceArgs;
        }
    }
}

void DeviceUserArgs::deleteDeviceArgs(const QString& id, int sequence)
{
    Args args;
    args.m_id = id;
    args.m_sequence = sequence;

    QList<DeviceUserArgs::Args>::iterator it = m_argsByDevice.begin();

    for (; it != m_argsByDevice.end(); ++it)
    {
        if (*it == args)
        {
            m_argsByDevice.erase(it);
            return;
        }
    }
}
