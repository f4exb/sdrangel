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

#include "bladerf1inputsettings.h"

#include <QtGlobal>
#include "util/simpleserializer.h"


BladeRF1InputSettings::BladeRF1InputSettings()
{
	resetToDefaults();
}

void BladeRF1InputSettings::resetToDefaults()
{
	m_centerFrequency = 435000*1000;
    m_devSampleRate = 3072000;
	m_lnaGain = 0;
	m_vga1 = 20;
	m_vga2 = 9;
	m_bandwidth = 1500000;
	m_log2Decim = 0;
	m_fcPos = FC_POS_INFRA;
	m_xb200 = false;
	m_xb200Path = BLADERF_XB200_MIX;
	m_xb200Filter = BLADERF_XB200_AUTO_1DB;
	m_dcBlock = false;
	m_iqCorrection = false;
    m_iqOrder = true;
	m_fileRecordName = "";
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray BladeRF1InputSettings::serialize() const
{
	SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
	s.writeS32(2, m_lnaGain);
	s.writeS32(3, m_vga1);
	s.writeS32(4, m_vga2);
	s.writeS32(5, m_bandwidth);
	s.writeU32(6, m_log2Decim);
	s.writeS32(7, (int) m_fcPos);
	s.writeBool(8, m_xb200);
	s.writeS32(9, (int) m_xb200Path);
	s.writeS32(10, (int) m_xb200Filter);
	s.writeBool(11, m_dcBlock);
	s.writeBool(12, m_iqCorrection);
    s.writeBool(13, m_useReverseAPI);
    s.writeString(14, m_reverseAPIAddress);
    s.writeU32(15, m_reverseAPIPort);
    s.writeU32(16, m_reverseAPIDeviceIndex);
    s.writeBool(17, m_iqOrder);

	return s.final();
}

bool BladeRF1InputSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &m_devSampleRate, 3072000);
		d.readS32(2, &m_lnaGain);
		d.readS32(3, &m_vga1);
		d.readS32(4, &m_vga2);
		d.readS32(5, &m_bandwidth);
		d.readU32(6, &m_log2Decim);
		d.readS32(7, &intval);
		m_fcPos = (fcPos_t) intval;
		d.readBool(8, &m_xb200);
		d.readS32(9, &intval);
		m_xb200Path = (bladerf_xb200_path) intval;
		d.readS32(10, &intval);
		m_xb200Filter = (bladerf_xb200_filter) intval;
		d.readBool(11, &m_dcBlock);
		d.readBool(12, &m_iqCorrection);
        d.readBool(13, &m_useReverseAPI, false);
        d.readString(14, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(15, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(16, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;
        d.readBool(17, &m_iqOrder);

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

void BladeRF1InputSettings::applySettings(const QStringList& settingsKeys, const BladeRF1InputSettings& settings)
{
    if (settingsKeys.contains("centerFrequency")) {
        m_centerFrequency = settings.m_centerFrequency;
    }
    if (settingsKeys.contains("devSampleRate")) {
        m_devSampleRate = settings.m_devSampleRate;
    }
    if (settingsKeys.contains("lnaGain")) {
        m_lnaGain = settings.m_lnaGain;
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
    if (settingsKeys.contains("fcPos")) {
        m_fcPos = settings.m_fcPos;
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
    if (settingsKeys.contains("dcBlock")) {
        m_dcBlock = settings.m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection")) {
        m_iqCorrection = settings.m_iqCorrection;
    }
    if (settingsKeys.contains("iqOrder")) {
        m_iqOrder = settings.m_iqOrder;
    }
    if (settingsKeys.contains("fileRecordName")) {
        m_fileRecordName = settings.m_fileRecordName;
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

QString BladeRF1InputSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("centerFrequency") || force) {
        ostr << " m_centerFrequency: " << m_centerFrequency;
    }
    if (settingsKeys.contains("devSampleRate") || force) {
        ostr << " m_devSampleRate: " << m_devSampleRate;
    }
    if (settingsKeys.contains("lnaGain") || force) {
        ostr << " m_lnaGain: " << m_lnaGain;
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
    if (settingsKeys.contains("log2Decim") || force) {
        ostr << " m_log2Decim: " << m_log2Decim;
    }
    if (settingsKeys.contains("fcPos") || force) {
        ostr << " m_fcPos: " << m_fcPos;
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
    if (settingsKeys.contains("dcBlock") || force) {
        ostr << " m_dcBlock: " << m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection") || force) {
        ostr << " m_iqCorrection: " << m_iqCorrection;
    }
    if (settingsKeys.contains("iqOrder") || force) {
        ostr << " m_iqOrder: " << m_iqOrder;
    }
    if (settingsKeys.contains("fileRecordName") || force) {
        ostr << " m_fileRecordName: " << m_fileRecordName.toStdString();
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

