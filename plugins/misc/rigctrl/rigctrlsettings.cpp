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

const bool RigCtrlSettings::m_EnabledDefault = false;
const int RigCtrlSettings::m_RigCtrlPortDefault = 4532;
const int RigCtrlSettings::m_MaxFrequencyOffsetDefault = 10000;
const int RigCtrlSettings::m_DeviceIndexDefault = 0;
const int RigCtrlSettings::m_ChannelIndexDefault = 0;


void RigCtrlSettings::resetToDefaults()
{
    m_enabled = m_EnabledDefault;
    m_rigCtrlPort = m_RigCtrlPortDefault;
    m_maxFrequencyOffset = m_MaxFrequencyOffsetDefault;
    m_deviceIndex = m_DeviceIndexDefault;
    m_channelIndex = m_ChannelIndexDefault;
}

QByteArray RigCtrlSettings::serialize() const
{
	SimpleSerializer s(1);

    s.writeBool(1, m_enabled);
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
        d.readBool(1, &m_enabled, m_EnabledDefault);
		d.readS32(3, &m_rigCtrlPort, m_RigCtrlPortDefault);
		d.readS32(4, &m_maxFrequencyOffset, m_MaxFrequencyOffsetDefault);
		d.readS32(5, &m_deviceIndex, m_DeviceIndexDefault);
		d.readS32(6, &m_channelIndex, m_ChannelIndexDefault);

        if (!((m_rigCtrlPort > 1023) && (m_rigCtrlPort < 65536))) {
            qDebug() << "RigCtrlSettings::deserialize invalid port number ignored";
            m_rigCtrlPort = m_RigCtrlPortDefault;
        }
        if (m_maxFrequencyOffset < 0) {
            qDebug() << "RigCtrlSettings::deserialize invalid max frequency offset ignored";
            m_maxFrequencyOffset = m_MaxFrequencyOffsetDefault;
        }
        if (m_deviceIndex < 0) {
            qDebug() << "RigCtrlSettings::deserialize invalid device index ignored";
            m_deviceIndex = m_DeviceIndexDefault;
        }
        if (m_channelIndex < 0) {
            qDebug() << "RigCtrlSettings::deserialize invalid channel index ignored";
            m_deviceIndex = m_ChannelIndexDefault;
        }

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}
