///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include "bladerf1outputsettings.h"

#include <QtGlobal>
#include "util/simpleserializer.h"


BladeRF1OutputSettings::BladeRF1OutputSettings()
{
	resetToDefaults();
}

void BladeRF1OutputSettings::resetToDefaults()
{
	m_centerFrequency = 435000*1000;
	m_devSampleRate = 3072000;
	m_vga1 = -20;
	m_vga2 = 20;
	m_bandwidth = 1500000;
	m_log2Interp = 0;
	m_xb200 = false;
	m_xb200Path = BLADERF_XB200_MIX;
	m_xb200Filter = BLADERF_XB200_AUTO_1DB;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray BladeRF1OutputSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(1, m_devSampleRate);
	s.writeS32(2, m_vga1);
	s.writeS32(3, m_vga2);
	s.writeS32(4, m_bandwidth);
	s.writeU32(5, m_log2Interp);
	s.writeBool(6, m_xb200);
	s.writeS32(7, (int) m_xb200Path);
	s.writeS32(8, (int) m_xb200Filter);
    s.writeBool(9, m_useReverseAPI);
    s.writeString(10, m_reverseAPIAddress);
    s.writeU32(11, m_reverseAPIPort);
    s.writeU32(12, m_reverseAPIDeviceIndex);

	return s.final();
}

bool BladeRF1OutputSettings::deserialize(const QByteArray& data)
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

		d.readS32(1, &m_devSampleRate);
		d.readS32(2, &m_vga1);
		d.readS32(3, &m_vga2);
		d.readS32(4, &m_bandwidth);
		d.readU32(5, &m_log2Interp);
		d.readBool(6, &m_xb200);
		d.readS32(7, &intval);
		m_xb200Path = (bladerf_xb200_path) intval;
		d.readS32(8, &intval);
		m_xb200Filter = (bladerf_xb200_filter) intval;
        d.readBool(9, &m_useReverseAPI, false);
        d.readString(10, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(11, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(12, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

void BladeRF1OutputSettings::applySettings(const QStringList& settingsKeys, const BladeRF1OutputSettings& settings)
{
    if (settingsKeys.contains("centerFrequency")) {
        m_centerFrequency = settings.m_centerFrequency;
    }
    if (settingsKeys.contains("devSampleRate")) {
        m_devSampleRate = settings.m_devSampleRate;
    }
    if (settingsKeys.contains("vga1")) {
        m_vga1 = settings.m_vga1;
    }
    if (settingsKeys.contains("vga2")) {
        m_vga2 = settings.m_vga2;
    }
    if (settingsKeys.contains("bandwidth")) {
        m_bandwidth = settings.m_bandwidth;
    }
    if (settingsKeys.contains("log2Interp")) {
        m_log2Interp = settings.m_log2Interp;
    }
    if (settingsKeys.contains("xb200")) {
        m_xb200 = settings.m_xb200;
    }
    if (settingsKeys.contains("xb200Path")) {
        m_xb200Path = settings.m_xb200Path;
    }
    if (settingsKeys.contains("xb200Filter")) {
        m_xb200Filter = settings.m_xb200Filter;
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

QString BladeRF1OutputSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("centerFrequency") || force) {
        ostr << " m_centerFrequency: " << m_centerFrequency;
    }
    if (settingsKeys.contains("devSampleRate") || force) {
        ostr << " m_devSampleRate: " << m_devSampleRate;
    }
    if (settingsKeys.contains("vga1") || force) {
        ostr << " m_vga1: " << m_vga1;
    }
    if (settingsKeys.contains("vga2") || force) {
        ostr << " m_vga2: " << m_vga2;
    }
    if (settingsKeys.contains("bandwidth") || force) {
        ostr << " m_bandwidth: " << m_bandwidth;
    }
    if (settingsKeys.contains("log2Interp") || force) {
        ostr << " m_log2Interp: " << m_log2Interp;
    }
    if (settingsKeys.contains("xb200") || force) {
        ostr << " m_xb200: " << m_xb200;
    }
    if (settingsKeys.contains("xb200Path") || force) {
        ostr << " m_xb200Path: " << m_xb200Path;
    }
    if (settingsKeys.contains("xb200Filter") || force) {
        ostr << " m_xb200Filter: " << m_xb200Filter;
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
