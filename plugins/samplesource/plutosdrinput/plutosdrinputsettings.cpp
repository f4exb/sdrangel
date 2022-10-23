///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
    m_hwBBDCBlock = true;
    m_hwRFDCBlock = true;
    m_hwIQCorrection = true;
	m_lpfBW = 1500000;
	m_lpfFIREnable = false;
	m_lpfFIRBW = 500000U;
	m_lpfFIRlog2Decim = 0;
	m_lpfFIRGain = 0;
	m_gain = 40;
	m_antennaPath = RFPATH_A_BAL;
	m_gainMode = GAIN_MANUAL;
	m_transverterMode = false;
	m_transverterDeltaFrequency = 0;
    m_iqOrder = true;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
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
    s.writeBool(18, m_useReverseAPI);
    s.writeString(19, m_reverseAPIAddress);
    s.writeU32(20, m_reverseAPIPort);
    s.writeU32(21, m_reverseAPIDeviceIndex);
    s.writeBool(22, m_hwBBDCBlock);
    s.writeBool(23, m_hwRFDCBlock);
    s.writeBool(24, m_hwIQCorrection);
    s.writeBool(25, m_iqOrder);

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
        d.readBool(18, &m_useReverseAPI, false);
        d.readString(19, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(20, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(21, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        d.readBool(22, &m_hwBBDCBlock, true);
        d.readBool(23, &m_hwRFDCBlock, true);
        d.readBool(24, &m_hwIQCorrection, true);
        d.readBool(25, &m_iqOrder, true);

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

void PlutoSDRInputSettings::applySettings(const QStringList& settingsKeys, const PlutoSDRInputSettings& settings)
{
    if (settingsKeys.contains("centerFrequency")) {
        m_centerFrequency = settings.m_centerFrequency;
    }
    if (settingsKeys.contains("fcPos")) {
        m_fcPos = settings.m_fcPos;
    }
    if (settingsKeys.contains("LOppmTenths")) {
        m_LOppmTenths = settings.m_LOppmTenths;
    }
    if (settingsKeys.contains("log2Decim")) {
        m_log2Decim = settings.m_log2Decim;
    }
    if (settingsKeys.contains("devSampleRate")) {
        m_devSampleRate = settings.m_devSampleRate;
    }
    if (settingsKeys.contains("dcBlock")) {
        m_dcBlock = settings.m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection")) {
        m_iqCorrection = settings.m_iqCorrection;
    }
    if (settingsKeys.contains("hwBBDCBlock")) {
        m_hwBBDCBlock = settings.m_hwBBDCBlock;
    }
    if (settingsKeys.contains("hwRFDCBlock")) {
        m_hwRFDCBlock = settings.m_hwRFDCBlock;
    }
    if (settingsKeys.contains("hwIQCorrection")) {
        m_hwIQCorrection = settings.m_hwIQCorrection;
    }
    if (settingsKeys.contains("lpfBW")) {
        m_lpfBW = settings.m_lpfBW;
    }
    if (settingsKeys.contains("lpfFIREnable")) {
        m_lpfFIREnable = settings.m_lpfFIREnable;
    }
    if (settingsKeys.contains("lpfFIRBW")) {
        m_lpfFIRBW = settings.m_lpfFIRBW;
    }
    if (settingsKeys.contains("lpfFIRlog2Decim")) {
        m_lpfFIRlog2Decim = settings.m_lpfFIRlog2Decim;
    }
    if (settingsKeys.contains("lpfFIRGain")) {
        m_lpfFIRGain = settings.m_lpfFIRGain;
    }
    if (settingsKeys.contains("gain")) {
        m_gain = settings.m_gain;
    }
    if (settingsKeys.contains("antennaPath")) {
        m_antennaPath = settings.m_antennaPath;
    }
    if (settingsKeys.contains("gainMode")) {
        m_gainMode = settings.m_gainMode;
    }
    if (settingsKeys.contains("transverterMode")) {
        m_transverterMode = settings.m_transverterMode;
    }
    if (settingsKeys.contains("transverterDeltaFrequency")) {
        m_transverterDeltaFrequency = settings.m_transverterDeltaFrequency;
    }
    if (settingsKeys.contains("iqOrder")) {
        m_iqOrder = settings.m_iqOrder;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    }
}

QString PlutoSDRInputSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("centerFrequency") || force) {
        ostr << " m_centerFrequency: " << m_centerFrequency;
    }
    if (settingsKeys.contains("fcPos") || force) {
        ostr << " m_fcPos: " << m_fcPos;
    }
    if (settingsKeys.contains("LOppmTenths") || force) {
        ostr << " m_LOppmTenths: " << m_LOppmTenths;
    }
    if (settingsKeys.contains("log2Decim") || force) {
        ostr << " m_log2Decim: " << m_log2Decim;
    }
    if (settingsKeys.contains("devSampleRate") || force) {
        ostr << " m_devSampleRate: " << m_devSampleRate;
    }
    if (settingsKeys.contains("dcBlock") || force) {
        ostr << " m_dcBlock: " << m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection") || force) {
        ostr << " m_iqCorrection: " << m_iqCorrection;
    }
    if (settingsKeys.contains("hwBBDCBlock") || force) {
        ostr << " m_hwBBDCBlock: " << m_hwBBDCBlock;
    }
    if (settingsKeys.contains("hwRFDCBlock") || force) {
        ostr << " m_hwRFDCBlock: " << m_hwRFDCBlock;
    }
    if (settingsKeys.contains("hwIQCorrection") || force) {
        ostr << " m_hwIQCorrection: " << m_hwIQCorrection;
    }
    if (settingsKeys.contains("lpfBW") || force) {
        ostr << " m_lpfBW: " << m_lpfBW;
    }
    if (settingsKeys.contains("lpfFIREnable") || force) {
        ostr << " m_lpfFIREnable: " << m_lpfFIREnable;
    }
    if (settingsKeys.contains("lpfFIRBW") || force) {
        ostr << " m_lpfFIRBW: " << m_lpfFIRBW;
    }
    if (settingsKeys.contains("lpfFIRlog2Decim") || force) {
        ostr << " m_lpfFIRlog2Decim: " << m_lpfFIRlog2Decim;
    }
    if (settingsKeys.contains("lpfFIRGain") || force) {
        ostr << " m_lpfFIRGain: " << m_lpfFIRGain;
    }
    if (settingsKeys.contains("gain") || force) {
        ostr << " m_gain: " << m_gain;
    }
    if (settingsKeys.contains("antennaPath") || force) {
        ostr << " m_antennaPath: " << m_antennaPath;
    }
    if (settingsKeys.contains("gainMode") || force) {
        ostr << " m_gainMode: " << m_gainMode;
    }
    if (settingsKeys.contains("transverterMode") || force) {
        ostr << " m_transverterMode: " << m_transverterMode;
    }
    if (settingsKeys.contains("transverterDeltaFrequency") || force) {
        ostr << " m_transverterDeltaFrequency: " << m_transverterDeltaFrequency;
    }
    if (settingsKeys.contains("iqOrder") || force) {
        ostr << " m_iqOrder: " << m_iqOrder;
    }
    if (settingsKeys.contains("useReverseAPI") || force) {
        ostr << " m_useReverseAPI: " << m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress") || force) {
        ostr << " m_reverseAPIAddress: " << m_reverseAPIAddress.toStdString();
    }
    if (settingsKeys.contains("reverseAPIPort") || force) {
        ostr << " m_reverseAPIPort: " << m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex") || force) {
        ostr << " m_reverseAPIDeviceIndex: " << m_reverseAPIDeviceIndex;
    }

    return QString(ostr.str().c_str());
}
