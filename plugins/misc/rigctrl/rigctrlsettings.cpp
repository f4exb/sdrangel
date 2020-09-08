///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include "rigctrlsettings.h"

#include <QtGlobal>
#include <QtDebug>
#include "util/simpleserializer.h"

#define ENABLED_DEFAULT                 false
#define API_ADDRESS_DEFAULT             "http://127.0.0.1:8091/sdrangel"
#define RIG_CTRL_PORT_DEFAULT           4532
#define MAX_FREQUENCY_OFFSET_DEFAULT    10000
#define DEVICE_INDEX_DEFAULT            0
#define CHANNEL_INDEX_DEFAULT           0

void RigCtrlSettings::resetToDefaults()
{
    m_enabled = ENABLED_DEFAULT;
    m_APIAddress = API_ADDRESS_DEFAULT;
    m_rigCtrlPort = RIG_CTRL_PORT_DEFAULT;
    m_maxFrequencyOffset = MAX_FREQUENCY_OFFSET_DEFAULT;
    m_deviceIndex = DEVICE_INDEX_DEFAULT;
    m_channelIndex = CHANNEL_INDEX_DEFAULT;
}

QByteArray RigCtrlSettings::serialize() const
{
	SimpleSerializer s(1);

    s.writeBool(1, m_enabled);
    s.writeString(2, m_APIAddress);
    s.writeS32(3, m_rigCtrlPort);
    s.writeS32(4, m_maxFrequencyOffset);
    s.writeS32(5, m_deviceIndex);
    s.writeS32(6, m_channelIndex);

	return s.final();
}

bool RigCtrlSettings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid())
	{
		resetToDefaults();
		return false;
	}

	if (d.getVersion() == 1)
	{
        d.readBool(1, &m_enabled, ENABLED_DEFAULT);
		d.readString(2, &m_APIAddress, API_ADDRESS_DEFAULT);
		d.readS32(3, &m_rigCtrlPort, RIG_CTRL_PORT_DEFAULT);
		d.readS32(4, &m_maxFrequencyOffset, MAX_FREQUENCY_OFFSET_DEFAULT);
		d.readS32(5, &m_deviceIndex, DEVICE_INDEX_DEFAULT);
		d.readS32(6, &m_channelIndex, CHANNEL_INDEX_DEFAULT);

        if (!((m_rigCtrlPort > 1023) && (m_rigCtrlPort < 65536))) {
            qDebug() << "RigCtrlSettings::deserialize invalid port number ignored";
            m_rigCtrlPort = RIG_CTRL_PORT_DEFAULT;
        }
        if (m_maxFrequencyOffset < 0) {
            qDebug() << "RigCtrlSettings::deserialize invalid max frequency offset ignored";
            m_maxFrequencyOffset = MAX_FREQUENCY_OFFSET_DEFAULT;
        }
        if (m_deviceIndex < 0) {
            qDebug() << "RigCtrlSettings::deserialize invalid device index ignored";
            m_deviceIndex = DEVICE_INDEX_DEFAULT;
        }
        if (m_channelIndex < 0) {
            qDebug() << "RigCtrlSettings::deserialize invalid channel index ignored";
            m_deviceIndex = CHANNEL_INDEX_DEFAULT;
        }

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}
