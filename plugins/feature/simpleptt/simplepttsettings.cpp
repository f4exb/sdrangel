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

#include <QColor>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "simplepttsettings.h"

SimplePTTSettings::SimplePTTSettings()
{
    resetToDefaults();
}

void SimplePTTSettings::resetToDefaults()
{
    m_title = "Simple PTT";
    m_rgbColor = QColor(255, 0, 0).rgb();
    m_rxDeviceSetIndex = -1;
    m_txDeviceSetIndex = -1;
    m_rx2TxDelayMs = 0;
    m_tx2RxDelayMs = 0;
}

QByteArray SimplePTTSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_title);
    s.writeU32(2, m_rgbColor);
    s.writeS32(3, m_rxDeviceSetIndex);
    s.writeS32(4, m_txDeviceSetIndex);
    s.writeU32(5, m_rx2TxDelayMs);
    s.writeU32(6, m_tx2RxDelayMs);
    
    return s.final();
}

bool SimplePTTSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        QByteArray bytetmp;
        qint32 tmp;
        uint32_t utmp;
        QString strtmp;

        d.readString(1, &m_title, "Simple PTT");
        d.readU32(2, &m_rgbColor, QColor(255, 0, 0).rgb());
        d.readS32(3, &m_rxDeviceSetIndex, -1);
        d.readS32(4, &m_txDeviceSetIndex, -1);
        d.readU32(5, &m_rx2TxDelayMs, 0);
        d.readU32(6, &m_tx2RxDelayMs, 0);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
