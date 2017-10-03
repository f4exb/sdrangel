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

#include "plutosdrinputsettings.h"

#include <QtGlobal>
#include "util/simpleserializer.h"


PlutoSDRInputSettings::PlutoSDRInputSettings()
{
	resetToDefaults();
}

void PlutoSDRInputSettings::resetToDefaults()
{
	m_centerFrequency = 435000 * 1000;
	m_fcPos = FC_POS_CENTER;
	m_LOppmTenths = 0;
	m_log2Decim = 0;
	m_devSampleRate = 2500 * 1000;
	m_dcBlock = false;
	m_iqCorrection = false;
	m_lpfBW = 1500000;
	m_lpfFIREnable = false;
	m_lpfFIRBW = 500000U;
	m_lpfFIRlog2Decim = 0;
	m_gain = 40;
	m_antennaPath = RFPATH_A_BAL;
	m_gainMode = GAIN_MANUAL;
	m_transverterMode = false;
	m_transverterDeltaFrequency = 0;
}

QByteArray PlutoSDRInputSettings::serialize() const
{
	SimpleSerializer s(1);

    s.writeS32(1, m_LOppmTenths);
    s.writeS32(2, m_lpfFIRGain);
    s.writeU32(3, m_lpfFIRlog2Decim);
    s.writeU32(4, m_log2Decim);
	s.writeS32(5, m_fcPos);
	s.writeBool(7, m_dcBlock);
    s.writeBool(8, m_iqCorrection);
    s.writeU32(9, m_lpfBW);
    s.writeBool(10, m_lpfFIREnable);
    s.writeU32(11, m_lpfFIRBW);
    s.writeU64(12, m_devSampleRate);
    s.writeU32(13, m_gain);
    s.writeS32(14, (int) m_antennaPath);
    s.writeS32(15, (int) m_gainMode);
    s.writeBool(16, m_transverterMode);
    s.writeS64(17, m_transverterDeltaFrequency);

	return s.final();
}

bool PlutoSDRInputSettings::deserialize(const QByteArray& data)
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
            m_lpfFIRlog2Decim = 2;
        } else {
            m_lpfFIRlog2Decim = uintval;
        }
        d.readU32(4, &m_log2Decim, 0);
		d.readS32(5, &intval, 0);
		if ((intval < 0) || (intval > 2)) {
		    m_fcPos = FC_POS_INFRA;
		} else {
	        m_fcPos = (fcPos_t) intval;
		}
        d.readBool(7, &m_dcBlock, false);
        d.readBool(8, &m_iqCorrection, false);
        d.readU32(9, &m_lpfBW, 1500000);
        d.readBool(10, &m_lpfFIREnable, false);
        d.readU32(11, &m_lpfFIRBW, 500000U);
        d.readU64(12, &m_devSampleRate, 1536000U);
        d.readU32(13, &m_gain, 40);
        d.readS32(14, &intval, 0);
        if ((intval >= 0) && (intval < (int) RFPATH_END)) {
            m_antennaPath = (RFPath) intval;
        } else {
            m_antennaPath = RFPATH_A_BAL;
        }
        d.readS32(15, &intval, 0);
        if ((intval >= 0) && (intval < (int) GAIN_END)) {
            m_gainMode = (GainMode) intval;
        } else {
            m_gainMode = GAIN_MANUAL;
        }
        d.readBool(16, &m_transverterMode, false);
        d.readS64(17, &m_transverterDeltaFrequency, 0);

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

void PlutoSDRInputSettings::translateRFPath(RFPath path, QString& s)
{
    switch(path)
    {
    case RFPATH_A_BAL:
        s = "A_BALANCED";
        break;
    case RFPATH_B_BAL:
        s = "B_BALANCED";
        break;
    case RFPATH_C_BAL:
        s = "C_BALANCED";
        break;
    case RFPATH_A_NEG:
        s = "A_N";
        break;
    case RFPATH_A_POS:
        s = "A_P";
        break;
    case RFPATH_B_NEG:
        s = "B_N";
        break;
    case RFPATH_B_POS:
        s = "B_P";
        break;
    case RFPATH_C_NEG:
        s = "C_N";
        break;
    case RFPATH_C_POS:
        s = "C_P";
        break;
    case RFPATH_TX1MON:
        s = "TX_MONITOR1";
        break;
    case RFPATH_TX2MON:
        s = "TX_MONITOR2";
        break;
    case RFPATH_TX3MON:
        s = "TX_MONITOR3";
        break;
    default:
        s = "A_BALANCED";
        break;
    }
}

void PlutoSDRInputSettings::translateGainMode(GainMode mode, QString& s)
{
    switch(mode)
    {
    case GAIN_MANUAL:
        s = "manual";
        break;
    case GAIN_AGC_SLOW:
        s = "slow_attack";
        break;
    case GAIN_AGC_FAST:
        s = "fast_attack";
        break;
    case GAIN_HYBRID:
        s = "hybrid";
        break;
    default:
        s = "manual";
        break;
    }
}
