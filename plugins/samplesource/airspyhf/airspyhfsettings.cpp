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

#include <QtGlobal>
#include "util/simpleserializer.h"
#include "airspyhfsettings.h"

AirspyHFSettings::AirspyHFSettings()
{
	resetToDefaults();
}

void AirspyHFSettings::resetToDefaults()
{
	m_centerFrequency = 7150*1000;
	m_LOppmTenths = 0;
	m_devSampleRateIndex = 0;
	m_log2Decim = 0;
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_bandIndex = 0;
}

QByteArray AirspyHFSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeU32(1, m_devSampleRateIndex);
	s.writeS32(2, m_LOppmTenths);
	s.writeU32(3, m_log2Decim);
    s.writeBool(7, m_transverterMode);
    s.writeS64(8, m_transverterDeltaFrequency);
    s.writeU32(9, m_bandIndex);

	return s.final();
}

bool AirspyHFSettings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid())
	{
		resetToDefaults();
		return false;
	}

	if (d.getVersion() == 1)
	{
		int intval;
		quint32 uintval;

		d.readU32(1, &m_devSampleRateIndex, 0);
        d.readS32(2, &m_LOppmTenths, 0);
		d.readU32(3, &m_log2Decim, 0);
		d.readS32(4, &intval, 0);
        d.readBool(7, &m_transverterMode, false);
        d.readS64(8, &m_transverterDeltaFrequency, 0);
        d.readU32(9, &uintval, 0);
        m_bandIndex = uintval > 1 ? 1 : uintval;

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}
