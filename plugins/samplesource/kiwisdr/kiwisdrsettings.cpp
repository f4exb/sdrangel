///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Vort                                                       //
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

#include "util/simpleserializer.h"
#include "kiwisdrsettings.h"

KiwiSDRSettings::KiwiSDRSettings()
{
    resetToDefaults();
}

void KiwiSDRSettings::resetToDefaults()
{
    m_centerFrequency = 1450000;

	m_gain = 20;
	m_useAGC = true;
    m_dcBlock = false;

	m_serverAddress = "127.0.0.1:8073";

    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray KiwiSDRSettings::serialize() const
{
    SimpleSerializer s(2);

	s.writeString(2, m_serverAddress);
	s.writeU32(3, m_gain);
	s.writeBool(4, m_useAGC);

    s.writeBool(100, m_useReverseAPI);
    s.writeString(101, m_reverseAPIAddress);
    s.writeU32(102, m_reverseAPIPort);
    s.writeU32(103, m_reverseAPIDeviceIndex);

	return s.final();
}

bool KiwiSDRSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 2)
    {
		uint32_t utmp;

		d.readString(2, &m_serverAddress, "127.0.0.1:8073");
		d.readU32(3, &m_gain, 20);
		d.readBool(4, &m_useAGC, true);

		d.readBool(100, &m_useReverseAPI, false);
		d.readString(101, &m_reverseAPIAddress, "127.0.0.1");
		d.readU32(102, &utmp, 0);

		if ((utmp > 1023) && (utmp < 65535)) {
			m_reverseAPIPort = utmp;
		}
		else {
			m_reverseAPIPort = 8888;
		}

		d.readU32(103, &utmp, 0);
		m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}






