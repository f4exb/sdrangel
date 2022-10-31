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
	m_lpfFIRGain = 0;
	m_att = -50;
	m_antennaPath = RFPATH_A;
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
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
    s.writeBool(17, m_useReverseAPI);
    s.writeString(18, m_reverseAPIAddress);
    s.writeU32(19, m_reverseAPIPort);
    s.writeU32(20, m_reverseAPIDeviceIndex);

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
        d.readBool(17, &m_useReverseAPI, false);
        d.readString(18, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(19, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(20, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

void PlutoSDROutputSettings::applySettings(const QStringList& settingsKeys, const PlutoSDROutputSettings& settings)
{
    if (settingsKeys.contains("centerFrequency")) {
        m_centerFrequency = settings.m_centerFrequency;
    }
    if (settingsKeys.contains("LOppmTenths")) {
        m_LOppmTenths = settings.m_LOppmTenths;
    }
    if (settingsKeys.contains("log2Interp")) {
        m_log2Interp = settings.m_log2Interp;
    }
    if (settingsKeys.contains("devSampleRate")) {
        m_devSampleRate = settings.m_devSampleRate;
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
    if (settingsKeys.contains("lpfFIRlog2Interp")) {
        m_lpfFIRlog2Interp = settings.m_lpfFIRlog2Interp;
    }
    if (settingsKeys.contains("lpfFIRGain")) {
        m_lpfFIRGain = settings.m_lpfFIRGain;
    }
    if (settingsKeys.contains("att")) {
        m_att = settings.m_att;
    }
    if (settingsKeys.contains("antennaPath")) {
        m_antennaPath = settings.m_antennaPath;
    }
    if (settingsKeys.contains("transverterMode")) {
        m_transverterMode = settings.m_transverterMode;
    }
    if (settingsKeys.contains("transverterDeltaFrequency")) {
        m_transverterDeltaFrequency = settings.m_transverterDeltaFrequency;
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

QString PlutoSDROutputSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("centerFrequency") || force) {
        ostr << " m_centerFrequency: " << m_centerFrequency;
    }
    if (settingsKeys.contains("LOppmTenths") || force) {
        ostr << " m_LOppmTenths: " << m_LOppmTenths;
    }
    if (settingsKeys.contains("log2Interp") || force) {
        ostr << " m_log2Interp: " << m_log2Interp;
    }
    if (settingsKeys.contains("devSampleRate") || force) {
        ostr << " m_devSampleRate: " << m_devSampleRate;
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
    if (settingsKeys.contains("lpfFIRlog2Interp") || force) {
        ostr << " m_lpfFIRlog2Interp: " << m_lpfFIRlog2Interp;
    }
    if (settingsKeys.contains("lpfFIRGain") || force) {
        ostr << " m_lpfFIRGain: " << m_lpfFIRGain;
    }
    if (settingsKeys.contains("att") || force) {
        ostr << " m_att: " << m_att;
    }
    if (settingsKeys.contains("antennaPath") || force) {
        ostr << " m_antennaPath: " << m_antennaPath;
    }
    if (settingsKeys.contains("transverterMode") || force) {
        ostr << " m_transverterMode: " << m_transverterMode;
    }
    if (settingsKeys.contains("transverterDeltaFrequency") || force) {
        ostr << " m_transverterDeltaFrequency: " << m_transverterDeltaFrequency;
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
