///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include "sdrplayv3settings.h"

#include <QtGlobal>
#include "util/simpleserializer.h"

SDRPlayV3Settings::SDRPlayV3Settings()
{
    resetToDefaults();
}

void SDRPlayV3Settings::resetToDefaults()
{
    m_centerFrequency = 7040*1000;
    m_LOppmTenths = 0;
    m_ifFrequencyIndex = 0;
    m_bandwidthIndex = 3;
    m_devSampleRate = 2000000;
    m_log2Decim = 0;
    m_fcPos = FC_POS_CENTER;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_lnaIndex = 0;
    m_ifAGC = true;
    m_ifGain = -40;
    m_amNotch = false;
    m_fmNotch = false;
    m_dabNotch = false;
    m_biasTee = false;
    m_tuner = 0;
    m_antenna = 0;
    m_extRef = false;
    m_transverterMode = false;
    m_iqOrder = true;
	m_transverterDeltaFrequency = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray SDRPlayV3Settings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_LOppmTenths);
    s.writeU32(3, m_ifFrequencyIndex);
    s.writeU32(5, m_bandwidthIndex);
    s.writeU32(6, m_devSampleRate);
    s.writeU32(7, m_log2Decim);
    s.writeS32(8, (int) m_fcPos);
    s.writeBool(9, m_dcBlock);
    s.writeBool(10, m_iqCorrection);
    s.writeS32(11, m_lnaIndex);
    s.writeBool(13, m_ifAGC);
    s.writeS32(14, m_ifGain);
    s.writeBool(15, m_useReverseAPI);
    s.writeString(16, m_reverseAPIAddress);
    s.writeU32(17, m_reverseAPIPort);
    s.writeU32(18, m_reverseAPIDeviceIndex);
    s.writeBool(19, m_amNotch);
    s.writeBool(20, m_fmNotch);
    s.writeBool(21, m_dabNotch);
    s.writeBool(22, m_biasTee);
    s.writeS32(23, m_tuner);
    s.writeS32(24, m_antenna);
    s.writeBool(25, m_extRef);
    s.writeBool(26, m_transverterMode);
    s.writeS64(27, m_transverterDeltaFrequency);
    s.writeBool(28, m_iqOrder);

    return s.final();
}

bool SDRPlayV3Settings::deserialize(const QByteArray& data)
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
        d.readU32(3, &m_ifFrequencyIndex, 0);
        d.readU32(5, &m_bandwidthIndex, 3);
        d.readU32(6, &m_devSampleRate, 2000000);
        d.readU32(7, &m_log2Decim, 0);
        d.readS32(8, &intval, 0);
        m_fcPos = (fcPos_t) intval;
        d.readBool(9, &m_dcBlock, false);
        d.readBool(10, &m_iqCorrection, false);
        d.readS32(11, &m_lnaIndex, 0);
        d.readBool(13, &m_ifAGC, true);
        d.readS32(14, &m_ifGain, -40);
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
        d.readBool(19, &m_amNotch, false);
        d.readBool(20, &m_fmNotch, false);
        d.readBool(21, &m_dabNotch, false);
        d.readBool(22, &m_biasTee, false);
        d.readS32(23, &m_tuner, 0);
        d.readS32(24, &m_antenna, 0);
        d.readBool(25, &m_extRef, false);
        d.readBool(26, &m_transverterMode, false);
        d.readS64(27, &m_transverterDeltaFrequency, 0);
        d.readBool(28, &m_iqOrder, true);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void SDRPlayV3Settings::applySettings(const QStringList& settingsKeys, const SDRPlayV3Settings& settings)
{
    if (settingsKeys.contains("centerFrequency")) {
        m_centerFrequency = settings.m_centerFrequency;
    }
    if (settingsKeys.contains("LOppmTenths")) {
        m_LOppmTenths = settings.m_LOppmTenths;
    }
    if (settingsKeys.contains("ifFrequencyIndex")) {
        m_ifFrequencyIndex = settings.m_ifFrequencyIndex;
    }
    if (settingsKeys.contains("bandwidthIndex")) {
        m_bandwidthIndex = settings.m_bandwidthIndex;
    }
    if (settingsKeys.contains("devSampleRate")) {
        m_devSampleRate = settings.m_devSampleRate;
    }
    if (settingsKeys.contains("log2Decim")) {
        m_log2Decim = settings.m_log2Decim;
    }
    if (settingsKeys.contains("fcPos")) {
        m_fcPos = settings.m_fcPos;
    }
    if (settingsKeys.contains("dcBlock")) {
        m_dcBlock = settings.m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection")) {
        m_iqCorrection = settings.m_iqCorrection;
    }
    if (settingsKeys.contains("lnaIndex")) {
        m_lnaIndex = settings.m_lnaIndex;
    }
    if (settingsKeys.contains("ifAGC")) {
        m_ifAGC = settings.m_ifAGC;
    }
    if (settingsKeys.contains("ifGain")) {
        m_ifGain = settings.m_ifGain;
    }
    if (settingsKeys.contains("amNotch")) {
        m_amNotch = settings.m_amNotch;
    }
    if (settingsKeys.contains("fmNotch")) {
        m_fmNotch = settings.m_fmNotch;
    }
    if (settingsKeys.contains("dabNotch")) {
        m_dabNotch = settings.m_dabNotch;
    }
    if (settingsKeys.contains("biasTee")) {
        m_biasTee = settings.m_biasTee;
    }
    if (settingsKeys.contains("tuner")) {
        m_tuner = settings.m_tuner;
    }
    if (settingsKeys.contains("antenna")) {
        m_antenna = settings.m_antenna;
    }
    if (settingsKeys.contains("extRef")) {
        m_extRef = settings.m_extRef;
    }
    if (settingsKeys.contains("transverterMode")) {
        m_transverterMode = settings.m_transverterMode;
    }
    if (settingsKeys.contains("iqOrder")) {
        m_iqOrder = settings.m_iqOrder;
    }
    if (settingsKeys.contains("m_transverterDeltaFrequency")) {
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

QString SDRPlayV3Settings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("centerFrequency") || force) {
        ostr << " m_centerFrequency: " << m_centerFrequency;
    }
    if (settingsKeys.contains("LOppmTenths") || force) {
        ostr << " m_LOppmTenths: " << m_LOppmTenths;
    }
    if (settingsKeys.contains("ifFrequencyIndex") || force) {
        ostr << " m_ifFrequencyIndex: " << m_ifFrequencyIndex;
    }
    if (settingsKeys.contains("bandwidthIndex") || force) {
        ostr << " m_bandwidthIndex: " << m_bandwidthIndex;
    }
    if (settingsKeys.contains("devSampleRate") || force) {
        ostr << " m_devSampleRate: " << m_devSampleRate;
    }
    if (settingsKeys.contains("log2Decim") || force) {
        ostr << " m_log2Decim: " << m_log2Decim;
    }
    if (settingsKeys.contains("fcPos") || force) {
        ostr << " m_fcPos: " << m_fcPos;
    }
    if (settingsKeys.contains("dcBlock") || force) {
        ostr << " m_dcBlock: " << m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection") || force) {
        ostr << " m_iqCorrection: " << m_iqCorrection;
    }
    if (settingsKeys.contains("lnaIndex") || force) {
        ostr << " m_lnaIndex: " << m_lnaIndex;
    }
    if (settingsKeys.contains("ifAGC") || force) {
        ostr << " m_ifAGC: " << m_ifAGC;
    }
    if (settingsKeys.contains("ifGain") || force) {
        ostr << " m_ifGain: " << m_ifGain;
    }
    if (settingsKeys.contains("amNotch") || force) {
        ostr << " m_amNotch: " << m_amNotch;
    }
    if (settingsKeys.contains("fmNotch") || force) {
        ostr << " m_fmNotch: " << m_fmNotch;
    }
    if (settingsKeys.contains("dabNotch") || force) {
        ostr << " m_dabNotch: " << m_dabNotch;
    }
    if (settingsKeys.contains("biasTee") || force) {
        ostr << " m_biasTee: " << m_biasTee;
    }
    if (settingsKeys.contains("tuner") || force) {
        ostr << " m_tuner: " << m_tuner;
    }
    if (settingsKeys.contains("antenna") || force) {
        ostr << " m_antenna: " << m_antenna;
    }
    if (settingsKeys.contains("extRef") || force) {
        ostr << " m_extRef: " << m_extRef;
    }
    if (settingsKeys.contains("transverterMode") || force) {
        ostr << " m_transverterMode: " << m_transverterMode;
    }
    if (settingsKeys.contains("iqOrder") || force) {
        ostr << " m_iqOrder: " << m_iqOrder;
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
    if (settingsKeys.contains("everseAPIDeviceIndex") || force) {
        ostr << " m_reverseAPIDeviceIndex: " << m_reverseAPIDeviceIndex;
    }

    return QString(ostr.str().c_str());
}
