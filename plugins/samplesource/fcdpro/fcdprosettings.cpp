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
#include "fcdprosettings.h"

FCDProSettings::FCDProSettings()
{
	resetToDefaults();
}

void FCDProSettings::resetToDefaults()
{
	m_centerFrequency = 435000 * 1000;
	m_dcBlock = false;
	m_iqCorrection = false;
	m_LOppmTenths = 0;
	m_lnaGainIndex = 0;
	m_rfFilterIndex = 0;
	m_lnaEnhanceIndex = 0;
	m_bandIndex = 0;
	m_mixerGainIndex = 0;
	m_mixerFilterIndex = 0;
	m_biasCurrentIndex = 0;
	m_modeIndex = 0;
	m_gain1Index = 0;
	m_rcFilterIndex = 0;
	m_gain2Index = 0;
	m_gain3Index = 0;
	m_gain4Index = 0;
	m_ifFilterIndex = 0;
	m_gain5Index = 0;
	m_gain6Index = 0;
	m_log2Decim = 0;
	m_fcPos = FC_POS_CENTER;
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_iqOrder = true;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray FCDProSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeBool(1, m_dcBlock);
	s.writeBool(2, m_iqCorrection);
	s.writeS32(3, m_LOppmTenths);
	s.writeS32(4, m_lnaGainIndex);
	s.writeS32(5, m_rfFilterIndex);
	s.writeS32(6, m_lnaEnhanceIndex);
	s.writeS32(7, m_bandIndex);
	s.writeS32(8, m_mixerGainIndex);
	s.writeS32(9, m_mixerFilterIndex);
	s.writeS32(10, m_biasCurrentIndex);
	s.writeS32(11, m_modeIndex);
	s.writeS32(12, m_gain1Index);
	s.writeS32(13, m_rcFilterIndex);
	s.writeS32(14, m_gain2Index);
	s.writeS32(15, m_gain3Index);
	s.writeS32(16, m_gain4Index);
	s.writeS32(17, m_ifFilterIndex);
	s.writeS32(18, m_gain5Index);
	s.writeS32(19, m_gain6Index);
	s.writeU32(20, m_log2Decim);
	s.writeS32(21, (int) m_fcPos);
    s.writeBool(22, m_transverterMode);
    s.writeS64(23, m_transverterDeltaFrequency);
    s.writeBool(24, m_useReverseAPI);
    s.writeString(25, m_reverseAPIAddress);
    s.writeU32(26, m_reverseAPIPort);
    s.writeU32(27, m_reverseAPIDeviceIndex);
    s.writeBool(28, m_iqOrder);

	return s.final();
}

bool FCDProSettings::deserialize(const QByteArray& data)
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

		d.readBool(1, &m_dcBlock, false);
		d.readBool(2, &m_iqCorrection, false);
		d.readS32(3, &m_LOppmTenths, 0);
		d.readS32(4, &m_lnaGainIndex, 0);
		d.readS32(5, &m_rfFilterIndex, 0);
		d.readS32(6, &m_lnaEnhanceIndex, 0);
		d.readS32(7, &m_bandIndex, 0);
		d.readS32(8, &m_mixerGainIndex, 0);
		d.readS32(9, &m_mixerFilterIndex, 0);
		d.readS32(10, &m_biasCurrentIndex, 0);
		d.readS32(11, &m_modeIndex, 0);
		d.readS32(12, &m_gain1Index, 0);
		d.readS32(13, &m_rcFilterIndex, 0);
		d.readS32(14, &m_gain2Index, 0);
		d.readS32(15, &m_gain3Index, 0);
		d.readS32(16, &m_gain4Index, 0);
		d.readS32(17, &m_ifFilterIndex, 0);
		d.readS32(18, &m_gain5Index, 0);
		d.readS32(19, &m_gain6Index, 0);
		d.readU32(20, &m_log2Decim, 0);
		d.readS32(21, &intval, 2);
		m_fcPos = (fcPos_t) intval;
        d.readBool(22, &m_transverterMode, false);
        d.readS64(23, &m_transverterDeltaFrequency, 0);
        d.readBool(24, &m_useReverseAPI, false);
        d.readString(25, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(26, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(27, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;
        d.readBool(28, &m_iqOrder, true);

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

void FCDProSettings::applySettings(const QStringList& settingsKeys, const FCDProSettings& settings)
{
    if (settingsKeys.contains("centerFrequency")) {
        m_centerFrequency = settings.m_centerFrequency;
    }
    if (settingsKeys.contains("dcBlock")) {
        m_dcBlock = settings.m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection")) {
        m_iqCorrection = settings.m_iqCorrection;
    }
    if (settingsKeys.contains("LOppmTenths")) {
        m_LOppmTenths = settings.m_LOppmTenths;
    }
    if (settingsKeys.contains("lnaGainIndex")) {
        m_lnaGainIndex = settings.m_lnaGainIndex;
    }
    if (settingsKeys.contains("rfFilterIndex")) {
        m_rfFilterIndex = settings.m_rfFilterIndex;
    }
    if (settingsKeys.contains("lnaEnhanceIndex")) {
        m_lnaEnhanceIndex = settings.m_lnaEnhanceIndex;
    }
    if (settingsKeys.contains("bandIndex")) {
        m_bandIndex = settings.m_bandIndex;
    }
    if (settingsKeys.contains("mixerGainIndex")) {
        m_mixerGainIndex = settings.m_mixerGainIndex;
    }
    if (settingsKeys.contains("mixerFilterIndex")) {
        m_mixerFilterIndex = settings.m_mixerFilterIndex;
    }
    if (settingsKeys.contains("biasCurrentIndex")) {
        m_biasCurrentIndex = settings.m_biasCurrentIndex;
    }
    if (settingsKeys.contains("modeIndex")) {
        m_modeIndex = settings.m_modeIndex;
    }
    if (settingsKeys.contains("gain1Index")) {
        m_gain1Index = settings.m_gain1Index;
    }
    if (settingsKeys.contains("rcFilterIndex")) {
        m_rcFilterIndex = settings.m_rcFilterIndex;
    }
    if (settingsKeys.contains("gain2Index")) {
        m_gain2Index = settings.m_gain2Index;
    }
    if (settingsKeys.contains("gain3Index")) {
        m_gain3Index = settings.m_gain3Index;
    }
    if (settingsKeys.contains("gain4Index")) {
        m_gain4Index = settings.m_gain4Index;
    }
    if (settingsKeys.contains("ifFilterIndex")) {
        m_ifFilterIndex = settings.m_ifFilterIndex;
    }
    if (settingsKeys.contains("gain5Index")) {
        m_gain5Index = settings.m_gain5Index;
    }
    if (settingsKeys.contains("gain6Index")) {
        m_gain6Index = settings.m_gain6Index;
    }
    if (settingsKeys.contains("log2Decim")) {
        m_log2Decim = settings.m_log2Decim;
    }
    if (settingsKeys.contains("fcPos")) {
        m_fcPos = settings.m_fcPos;
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

QString FCDProSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("centerFrequency") || force) {
        ostr << " m_centerFrequency: " << m_centerFrequency;
    }
    if (settingsKeys.contains("dcBlock") || force) {
        ostr << " m_dcBlock: " << m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection") || force) {
        ostr << " m_iqCorrection: " << m_iqCorrection;
    }
    if (settingsKeys.contains("LOppmTenths") || force) {
        ostr << " m_LOppmTenths: " << m_LOppmTenths;
    }
    if (settingsKeys.contains("lnaGainIndex") || force) {
        ostr << " m_lnaGainIndex: " << m_lnaGainIndex;
    }
    if (settingsKeys.contains("rfFilterIndex") || force) {
        ostr << " m_rfFilterIndex: " << m_rfFilterIndex;
    }
    if (settingsKeys.contains("lnaEnhanceIndex") || force) {
        ostr << " m_lnaEnhanceIndex: " << m_lnaEnhanceIndex;
    }
    if (settingsKeys.contains("bandIndex") || force) {
        ostr << " m_bandIndex: " << m_bandIndex;
    }
    if (settingsKeys.contains("mixerGainIndex") || force) {
        ostr << " m_mixerGainIndex: " << m_mixerGainIndex;
    }
    if (settingsKeys.contains("mixerFilterIndex") || force) {
        ostr << " m_mixerFilterIndex: " << m_mixerFilterIndex;
    }
    if (settingsKeys.contains("biasCurrentIndex") || force) {
        ostr << " m_biasCurrentIndex: " << m_biasCurrentIndex;
    }
    if (settingsKeys.contains("modeIndex") || force) {
        ostr << " m_modeIndex: " << m_modeIndex;
    }
    if (settingsKeys.contains("gain1Index") || force) {
        ostr << " m_gain1Index: " << m_gain1Index;
    }
    if (settingsKeys.contains("rcFilterIndex") || force) {
        ostr << " m_rcFilterIndex: " << m_rcFilterIndex;
    }
    if (settingsKeys.contains("gain2Index") || force) {
        ostr << " m_gain2Index: " << m_gain2Index;
    }
    if (settingsKeys.contains("gain3Index") || force) {
        ostr << " m_gain3Index: " << m_gain3Index;
    }
    if (settingsKeys.contains("gain4Index") || force) {
        ostr << " m_gain4Index: " << m_gain4Index;
    }
    if (settingsKeys.contains("ifFilterIndex") || force) {
        ostr << " m_ifFilterIndex: " << m_ifFilterIndex;
    }
    if (settingsKeys.contains("gain5Index") || force) {
        ostr << " m_gain5Index: " << m_gain5Index;
    }
    if (settingsKeys.contains("gain6Index") || force) {
        ostr << " m_gain6Index: " << m_gain6Index;
    }
    if (settingsKeys.contains("log2Decim") || force) {
        ostr << " m_log2Decim: " << m_log2Decim;
    }
    if (settingsKeys.contains("fcPos") || force) {
        ostr << " m_fcPos: " << m_fcPos;
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
