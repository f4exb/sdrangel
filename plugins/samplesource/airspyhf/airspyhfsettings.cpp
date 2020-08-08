///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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
    m_iqOrder = true;
    m_bandIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_useDSP = true;
    m_useAGC = false;
    m_agcHigh = false;
    m_useLNA = false;
    m_attenuatorSteps = 0;
	m_dcBlock = false;
	m_iqCorrection = false;
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
    s.writeBool(10, m_useReverseAPI);
    s.writeString(11, m_reverseAPIAddress);
    s.writeU32(12, m_reverseAPIPort);
    s.writeU32(13, m_reverseAPIDeviceIndex);
    s.writeBool(14, m_useDSP);
    s.writeBool(15, m_useAGC);
    s.writeBool(16, m_agcHigh);
    s.writeBool(17, m_useLNA);
    s.writeU32(18, m_attenuatorSteps);
	s.writeBool(19, m_dcBlock);
	s.writeBool(20, m_iqCorrection);
    s.writeBool(21, m_iqOrder);

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
        d.readBool(10, &m_useReverseAPI, false);
        d.readString(11, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(12, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(13, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;
        d.readBool(14, &m_useDSP, true);
        d.readBool(15, &m_useAGC, false);
        d.readBool(16, &m_agcHigh, false);
        d.readBool(17, &m_useLNA, false);
        d.readU32(18, &m_attenuatorSteps, 0);
		d.readBool(19, &m_dcBlock, false);
		d.readBool(20, &m_iqCorrection, false);
        d.readBool(21, &m_iqOrder, true);

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}
