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

#include <QtGlobal>
#include "util/simpleserializer.h"
#include "airspysettings.h"

AirspySettings::AirspySettings()
{
	resetToDefaults();
}

void AirspySettings::resetToDefaults()
{
	m_centerFrequency = 435000*1000;
	m_devSampleRateIndex = 0;
	m_LOppmTenths = 0;
	m_lnaGain = 14;
	m_mixerGain = 15;
	m_vgaGain = 4;
	m_lnaAGC = false;
	m_mixerAGC = false;
	m_log2Decim = 0;
	m_fcPos = FC_POS_CENTER;
	m_biasT = false;
	m_dcBlock = false;
	m_iqCorrection = false;
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_iqOrder = true;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray AirspySettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(1, m_LOppmTenths);
	s.writeU32(2, m_devSampleRateIndex);
	s.writeU32(3, m_log2Decim);
	s.writeS32(4, m_fcPos);
	s.writeU32(5, m_lnaGain);
	s.writeU32(6, m_mixerGain);
	s.writeU32(7, m_vgaGain);
	s.writeBool(8, m_biasT);
	s.writeBool(9, m_dcBlock);
	s.writeBool(10, m_iqCorrection);
	s.writeBool(11, m_lnaAGC);
	s.writeBool(12, m_mixerAGC);
    s.writeBool(13, m_transverterMode);
    s.writeS64(14, m_transverterDeltaFrequency);
    s.writeBool(15, m_useReverseAPI);
    s.writeString(16, m_reverseAPIAddress);
    s.writeU32(17, m_reverseAPIPort);
    s.writeU32(18, m_reverseAPIDeviceIndex);
    s.writeBool(19, m_iqOrder);

	return s.final();
}

bool AirspySettings::deserialize(const QByteArray& data)
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
		d.readU32(2, &m_devSampleRateIndex, 0);
		d.readU32(3, &m_log2Decim, 0);
		d.readS32(4, &intval, 0);
		m_fcPos = (fcPos_t) intval;
		d.readU32(5, &m_lnaGain, 14);
		d.readU32(6, &m_mixerGain, 15);
		d.readU32(7, &m_vgaGain, 4);
		d.readBool(8, &m_biasT, false);
		d.readBool(9, &m_dcBlock, false);
		d.readBool(10, &m_iqCorrection, false);
		d.readBool(11, &m_lnaAGC, false);
		d.readBool(12, &m_mixerAGC, false);
        d.readBool(13, &m_transverterMode, false);
        d.readS64(14, &m_transverterDeltaFrequency, 0);
        d.readBool(15, &m_useReverseAPI, false);
        d.readString(16, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(17, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(18, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;
        d.readBool(19, &m_iqOrder, true);

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

void AirspySettings::applySettings(const QStringList& settingsKeys, const AirspySettings& settings)
{
    if (settingsKeys.contains("centerFrequency")) {
        m_centerFrequency = settings.m_centerFrequency;
    }
    if (settingsKeys.contains("LOppmTenths")) {
        m_LOppmTenths = settings.m_LOppmTenths;
    }
    if (settingsKeys.contains("devSampleRateIndex")) {
        m_devSampleRateIndex = settings.m_devSampleRateIndex;
    }
    if (settingsKeys.contains("log2Decim")) {
        m_log2Decim = settings.m_log2Decim;
    }
    if (settingsKeys.contains("fcPos")) {
        m_fcPos = settings.m_fcPos;
    }
    if (settingsKeys.contains("lnaGain")) {
        m_lnaGain = settings.m_lnaGain;
    }
    if (settingsKeys.contains("mixerGain")) {
        m_mixerGain = settings.m_mixerGain;
    }
    if (settingsKeys.contains("vgaGain")) {
        m_vgaGain = settings.m_vgaGain;
    }
    if (settingsKeys.contains("biasT")) {
        m_biasT = settings.m_biasT;
    }
    if (settingsKeys.contains("dcBlock")) {
        m_dcBlock = settings.m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection")) {
        m_iqCorrection = settings.m_iqCorrection;
    }
    if (settingsKeys.contains("lnaAGC")) {
        m_lnaAGC = settings.m_lnaAGC;
    }
    if (settingsKeys.contains("mixerAGC")) {
        m_mixerAGC = settings.m_mixerAGC;
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
    if (settingsKeys.contains("iqOrder")) {
        m_iqOrder = settings.m_iqOrder;
    }
}

QString AirspySettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("centerFrequency") || force) {
        ostr << " m_centerFrequency: " << m_centerFrequency;
    }
    if (settingsKeys.contains("LOppmTenths") || force) {
        ostr << " m_LOppmTenths: " << m_LOppmTenths;
    }
    if (settingsKeys.contains("devSampleRateIndex") || force) {
        ostr << " m_devSampleRateIndex: " << m_devSampleRateIndex;
    }
    if (settingsKeys.contains("log2Decim") || force) {
        ostr << " m_log2Decim: " << m_log2Decim;
    }
    if (settingsKeys.contains("fcPos") || force) {
        ostr << " m_fcPos: " << m_fcPos;
    }
    if (settingsKeys.contains("lnaGain") || force) {
        ostr << " m_lnaGain: " << m_lnaGain;
    }
    if (settingsKeys.contains("mixerGain") || force) {
        ostr << " m_mixerGain: " << m_mixerGain;
    }
    if (settingsKeys.contains("vgaGain") || force) {
        ostr << " m_vgaGain: " << m_vgaGain;
    }
    if (settingsKeys.contains("biasT") || force) {
        ostr << " m_biasT: " << m_biasT;
    }
    if (settingsKeys.contains("dcBlock") || force) {
        ostr << " m_dcBlock: " << m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection") || force) {
        ostr << " m_iqCorrection: " << m_iqCorrection;
    }
    if (settingsKeys.contains("lnaAGC") || force) {
        ostr << " m_lnaAGC: " << m_lnaAGC;
    }
    if (settingsKeys.contains("mixerAGC") || force) {
        ostr << " m_mixerAGC: " << m_mixerAGC;
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
    if (settingsKeys.contains("iqOrder") || force) {
        ostr << " m_iqOrder: " << m_iqOrder;
    }

    return QString(ostr.str().c_str());
}
