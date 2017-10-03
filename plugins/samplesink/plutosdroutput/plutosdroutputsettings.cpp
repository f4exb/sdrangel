///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
#include "plutosdroutputsettings.h"


PlutoSDROutputSettings::PlutoSDROutputSettings()
{
	resetToDefaults();
}

void PlutoSDROutputSettings::resetToDefaults()
{
	m_centerFrequency = 435000 * 1000;
	m_LOppmTenths = 0;
	m_log2Interp = 0;
	m_devSampleRate = 2500 * 1000;
	m_lpfBW = 1500000;
	m_lpfFIREnable = false;
	m_lpfFIRBW = 500000U;
	m_lpfFIRlog2Interp = 0;
	m_att = -50;
	m_antennaPath = RFPATH_A;
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
}

QByteArray PlutoSDROutputSettings::serialize() const
{
	SimpleSerializer s(1);

    s.writeS32(1, m_LOppmTenths);
    s.writeS32(2, m_lpfFIRGain);
    s.writeU32(3, m_lpfFIRlog2Interp);
    s.writeU32(4, m_log2Interp);
    s.writeU32(9, m_lpfBW);
    s.writeBool(10, m_lpfFIREnable);
    s.writeU32(11, m_lpfFIRBW);
    s.writeU64(12, m_devSampleRate);
    s.writeS32(13, m_att);
    s.writeS32(14, (int) m_antennaPath);
    s.writeBool(15, m_transverterMode);
    s.writeS64(16, m_transverterDeltaFrequency);

	return s.final();
}

bool PlutoSDROutputSettings::deserialize(const QByteArray& data)
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
		uint32_t uintval;

        d.readS32(1, &m_LOppmTenths, 0);
        d.readS32(2, &m_lpfFIRGain, 0);
        d.readU32(3, &uintval, 0);
        if (uintval > 2) {
            m_lpfFIRlog2Interp = 2;
        } else {
            m_lpfFIRlog2Interp = uintval;
        }
        d.readU32(4, &m_log2Interp, 0);
        d.readU32(9, &m_lpfBW, 1500000);
        d.readBool(10, &m_lpfFIREnable, false);
        d.readU32(11, &m_lpfFIRBW, 500000U);
        d.readU64(12, &m_devSampleRate, 1536000U);
        d.readS32(13, &m_att, -50);
        d.readS32(14, &intval, 0);
        if ((intval >= 0) && (intval < (int) RFPATH_END)) {
            m_antennaPath = (RFPath) intval;
        } else {
            m_antennaPath = RFPATH_A;
        }
        d.readBool(15, &m_transverterMode, false);
        d.readS64(16, &m_transverterDeltaFrequency, 0);

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

void PlutoSDROutputSettings::translateRFPath(RFPath path, QString& s)
{
    switch(path)
    {
    case RFPATH_A:
        s = "A";
        break;
    case RFPATH_B:
        s = "B";
        break;
    default:
        s = "A";
        break;
    }
}
