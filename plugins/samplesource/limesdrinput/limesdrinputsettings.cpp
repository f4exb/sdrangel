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

#include "util/simpleserializer.h"
#include "limesdrinputsettings.h"

LimeSDRInputSettings::LimeSDRInputSettings()
{
    resetToDefaults();
}

void LimeSDRInputSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_devSampleRate = 5000000;
    m_log2HardDecim = 3;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_log2SoftDecim = 0;
    m_lpfBW = 4.5e6f;
    m_lpfFIREnable = false;
    m_lpfFIRBW = 2.5e6f;
    m_gain = 50;
    m_ncoEnable = false;
    m_ncoFrequency = 0;
    m_antennaPath = PATH_RFE_NONE;
    m_gainMode = GAIN_AUTO;
    m_lnaGain = 15;
    m_tiaGain = 2;
    m_pgaGain = 16;
    m_extClock = false;
    m_extClockFreq = 10000000; // 10 MHz
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_iqOrder = true;
    m_gpioDir = 0;
    m_gpioPins = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray LimeSDRInputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
    s.writeU32(2, m_log2HardDecim);
    s.writeBool(3, m_dcBlock);
    s.writeBool(4, m_iqCorrection);
    s.writeU32(5, m_log2SoftDecim);
    s.writeFloat(7, m_lpfBW);
    s.writeBool(8, m_lpfFIREnable);
    s.writeFloat(9, m_lpfFIRBW);
    s.writeU32(10, m_gain);
    s.writeBool(11, m_ncoEnable);
    s.writeS32(12, m_ncoFrequency);
    s.writeS32(13, (int) m_antennaPath);
    s.writeS32(14, (int) m_gainMode);
    s.writeU32(15, m_lnaGain);
    s.writeU32(16, m_tiaGain);
    s.writeU32(17, m_pgaGain);
    s.writeBool(18, m_extClock);
    s.writeU32(19, m_extClockFreq);
    s.writeBool(20, m_transverterMode);
    s.writeS64(21, m_transverterDeltaFrequency);
    s.writeU32(22, m_gpioDir);
    s.writeU32(23, m_gpioPins);
    s.writeBool(24, m_useReverseAPI);
    s.writeString(25, m_reverseAPIAddress);
    s.writeU32(26, m_reverseAPIPort);
    s.writeU32(27, m_reverseAPIDeviceIndex);
    s.writeBool(28, m_iqOrder);

    return s.final();
}

bool LimeSDRInputSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &m_devSampleRate, 5000000);
        d.readU32(2, &m_log2HardDecim, 2);
        d.readBool(3, &m_dcBlock, false);
        d.readBool(4, &m_iqCorrection, false);
        d.readU32(5, &m_log2SoftDecim, 0);
        d.readFloat(7, &m_lpfBW, 1.5e6);
        d.readBool(8, &m_lpfFIREnable, false);
        d.readFloat(9, &m_lpfFIRBW, 1.5e6);
        d.readU32(10, &m_gain, 50);
        d.readBool(11, &m_ncoEnable, false);
        d.readS32(12, &m_ncoFrequency, 0);
        d.readS32(13, &intval, 0);
        m_antennaPath = (PathRFE) intval;
        d.readS32(14, &intval, 0);
        m_gainMode = (GainMode) intval;
        d.readU32(15, &m_lnaGain, 15);
        d.readU32(16, &m_tiaGain, 2);
        d.readU32(17, &m_pgaGain, 16);
        d.readBool(18, &m_extClock, false);
        d.readU32(19, &m_extClockFreq, 10000000);
        d.readBool(20, &m_transverterMode, false);
        d.readS64(21, &m_transverterDeltaFrequency, 0);
        d.readU32(22, &uintval, 0);
        m_gpioDir = uintval & 0xFF;
        d.readU32(23, &uintval, 0);
        m_gpioPins = uintval & 0xFF;
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

void LimeSDRInputSettings::applySettings(const QStringList& settingsKeys, const LimeSDRInputSettings& settings)
{
    if (settingsKeys.contains("centerFrequency")) {
        m_centerFrequency = settings.m_centerFrequency;
    }
    if (settingsKeys.contains("devSampleRate")) {
        m_devSampleRate = settings.m_devSampleRate;
    }
    if (settingsKeys.contains("log2HardDecim")) {
        m_log2HardDecim = settings.m_log2HardDecim;
    }
    if (settingsKeys.contains("dcBlock")) {
        m_dcBlock = settings.m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection")) {
        m_iqCorrection = settings.m_iqCorrection;
    }
    if (settingsKeys.contains("log2SoftDecim")) {
        m_log2SoftDecim = settings.m_log2SoftDecim;
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
    if (settingsKeys.contains("gain")) {
        m_gain = settings.m_gain;
    }
    if (settingsKeys.contains("ncoEnable")) {
        m_ncoEnable = settings.m_ncoEnable;
    }
    if (settingsKeys.contains("ncoFrequency")) {
        m_ncoFrequency = settings.m_ncoFrequency;
    }
    if (settingsKeys.contains("antennaPath")) {
        m_antennaPath = settings.m_antennaPath;
    }
    if (settingsKeys.contains("gainMode")) {
        m_gainMode = settings.m_gainMode;
    }
    if (settingsKeys.contains("lnaGain")) {
        m_lnaGain = settings.m_lnaGain;
    }
    if (settingsKeys.contains("tiaGain")) {
        m_tiaGain = settings.m_tiaGain;
    }
    if (settingsKeys.contains("pgaGain")) {
        m_pgaGain = settings.m_pgaGain;
    }
    if (settingsKeys.contains("extClock")) {
        m_extClock = settings.m_extClock;
    }
    if (settingsKeys.contains("extClockFreq")) {
        m_extClockFreq = settings.m_extClockFreq;
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
    if (settingsKeys.contains("gpioDir")) {
        m_gpioDir = settings.m_gpioDir;
    }
    if (settingsKeys.contains("gpioPins")) {
        m_gpioPins = settings.m_gpioPins;
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

QString LimeSDRInputSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("centerFrequency") || force) {
        ostr << " m_centerFrequency: " << m_centerFrequency;
    }
    if (settingsKeys.contains("devSampleRate") || force) {
        ostr << " m_devSampleRate: " << m_devSampleRate;
    }
    if (settingsKeys.contains("log2HardDecim") || force) {
        ostr << " m_log2HardDecim: " << m_log2HardDecim;
    }
    if (settingsKeys.contains("dcBlock") || force) {
        ostr << " m_dcBlock: " << m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection") || force) {
        ostr << " m_iqCorrection: " << m_iqCorrection;
    }
    if (settingsKeys.contains("log2SoftDecim") || force) {
        ostr << " m_log2SoftDecim: " << m_log2SoftDecim;
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
    if (settingsKeys.contains("gain") || force) {
        ostr << " m_gain: " << m_gain;
    }
    if (settingsKeys.contains("ncoEnable") || force) {
        ostr << " m_ncoEnable: " << m_ncoEnable;
    }
    if (settingsKeys.contains("ncoFrequency") || force) {
        ostr << " m_ncoFrequency: " << m_ncoFrequency;
    }
    if (settingsKeys.contains("antennaPath") || force) {
        ostr << " m_antennaPath: " << m_antennaPath;
    }
    if (settingsKeys.contains("gainMode") || force) {
        ostr << " m_gainMode: " << m_gainMode;
    }
    if (settingsKeys.contains("lnaGain") || force) {
        ostr << " m_lnaGain: " << m_lnaGain;
    }
    if (settingsKeys.contains("tiaGain") || force) {
        ostr << " m_tiaGain: " << m_tiaGain;
    }
    if (settingsKeys.contains("pgaGain") || force) {
        ostr << " m_pgaGain: " << m_pgaGain;
    }
    if (settingsKeys.contains("extClock") || force) {
        ostr << " m_extClock: " << m_extClock;
    }
    if (settingsKeys.contains("extClockFreq") || force) {
        ostr << " m_extClockFreq: " << m_extClockFreq;
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
    if (settingsKeys.contains("gpioDir") || force) {
        ostr << " m_gpioDir: " << m_gpioDir;
    }
    if (settingsKeys.contains("gpioPins") || force) {
        ostr << " m_gpioPins: " << m_gpioPins;
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
