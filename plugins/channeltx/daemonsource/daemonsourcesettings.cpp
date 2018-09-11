///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "daemonsourcesettings.h"

DaemonSourceSettings::DaemonSourceSettings()
{
    resetToDefaults();
}

void DaemonSourceSettings::resetToDefaults()
{
    m_dataAddress = "127.0.0.1";
    m_dataPort = 9090;
    m_rgbColor = QColor(140, 4, 4).rgb();
    m_title = "Daemon source";
}

QByteArray DaemonSourceSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeString(1, m_dataAddress);
    s.writeU32(2, m_dataPort);
    s.writeU32(3, m_rgbColor);
    s.writeString(4, m_title);

    return s.final();
}

bool DaemonSourceSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        uint32_t tmp;
        QString strtmp;

        d.readString(1, &m_dataAddress, "127.0.0.1");
        d.readU32(2, &tmp, 0);

        if ((tmp > 1023) && (tmp < 65535)) {
            m_dataPort = tmp;
        } else {
            m_dataPort = 9090;
        }

        d.readU32(3, &m_rgbColor, QColor(0, 255, 255).rgb());
        d.readString(4, &m_title, "Daemon source");

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
