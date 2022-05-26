///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

#include "util/simpleserializer.h"

#include "limerfeusbcalib.h"

QByteArray LimeRFEUSBCalib::serialize() const
{
    SimpleSerializer s(1);
    QByteArray data;

    serializeCalibMap(data);
    s.writeBlob(1, data);

    return s.final();
}

bool LimeRFEUSBCalib::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray data;

        d.readBlob(1, &data);
        deserializeCalibMap(data);

         return true;
    }
    else
    {
        return false;
    }
}

void LimeRFEUSBCalib::serializeCalibMap(QByteArray& data) const
{
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    *stream << m_calibrations;
    delete stream;
}

void LimeRFEUSBCalib::deserializeCalibMap(QByteArray& data)
{
    QDataStream readStream(&data, QIODevice::ReadOnly);
    readStream >> m_calibrations;
}
