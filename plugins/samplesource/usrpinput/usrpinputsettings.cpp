///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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
#include "usrpinputsettings.h"

USRPInputSettings::USRPInputSettings()
{
    resetToDefaults();
}

void USRPInputSettings::resetToDefaults()
{
    m_masterClockRate = -1; // Calculated by UHD
    m_centerFrequency = 435000*1000;
    m_devSampleRate = 3000000;
    m_loOffset = 0;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_log2SoftDecim = 0;
    m_lpfBW = 10e6f;
    m_gain = 50;
    m_antennaPath = "TX/RX";
    m_gainMode = GAIN_AUTO;
    m_clockSource = "internal";
    m_transverterMode = false;
    m_transverterDeltaFrequency = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray USRPInputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
    s.writeBool(2, m_dcBlock);
    s.writeBool(3, m_iqCorrection);
    s.writeU32(4, m_log2SoftDecim);
    s.writeFloat(5, m_lpfBW);
    s.writeU32(6, m_gain);
    s.writeString(7, m_antennaPath);
    s.writeS32(8, (int) m_gainMode);
    s.writeString(9, m_clockSource);
    s.writeBool(10, m_transverterMode);
    s.writeS64(11, m_transverterDeltaFrequency);
    s.writeBool(12, m_useReverseAPI);
    s.writeString(13, m_reverseAPIAddress);
    s.writeU32(14, m_reverseAPIPort);
    s.writeU32(15, m_reverseAPIDeviceIndex);
    s.writeS32(16, m_loOffset);

    return s.final();
}

bool USRPInputSettings::deserialize(const QByteArray& data)
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
        d.readBool(2, &m_dcBlock, false);
        d.readBool(3, &m_iqCorrection, false);
        d.readU32(4, &m_log2SoftDecim, 0);
        d.readFloat(5, &m_lpfBW, 1.5e6);
        d.readU32(6, &m_gain, 50);
        d.readString(7, &m_antennaPath, "TX/RX");
        d.readS32(8, &intval, 0);
        m_gainMode = (GainMode) intval;
        d.readString(9, &m_clockSource, "internal");
        d.readBool(10, &m_transverterMode, false);
        d.readS64(11, &m_transverterDeltaFrequency, 0);
        d.readBool(12, &m_useReverseAPI, false);
        d.readString(13, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(14, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(15, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;
        d.readS32(16, &m_loOffset, 0);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }

}

void USRPInputSettings::applySettings(const QStringList& settingsKeys, const USRPInputSettings& settings)
{
    if (settingsKeys.contains("masterClockRate")) {
        m_masterClockRate = settings.m_masterClockRate;
    }
    if (settingsKeys.contains("centerFrequency")) {
        m_centerFrequency = settings.m_centerFrequency;
    }
    if (settingsKeys.contains("devSampleRate")) {
        m_devSampleRate = settings.m_devSampleRate;
    }
    if (settingsKeys.contains("loOffset")) {
        m_loOffset = settings.m_loOffset;
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
    if (settingsKeys.contains("gain")) {
        m_gain = settings.m_gain;
    }
    if (settingsKeys.contains("antennaPath")) {
        m_antennaPath = settings.m_antennaPath;
    }
    if (settingsKeys.contains("gainMode")) {
        m_gainMode = settings.m_gainMode;
    }
    if (settingsKeys.contains("clockSource")) {
        m_clockSource = settings.m_clockSource;
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

QString USRPInputSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("masterClockRate") || force) {
        ostr << " m_masterClockRate: " << m_masterClockRate;
    }
    if (settingsKeys.contains("centerFrequency") || force) {
        ostr << " m_centerFrequency: " << m_centerFrequency;
    }
    if (settingsKeys.contains("devSampleRate") || force) {
        ostr << " m_devSampleRate: " << m_devSampleRate;
    }
    if (settingsKeys.contains("loOffset") || force) {
        ostr << " m_loOffset: " << m_loOffset;
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
    if (settingsKeys.contains("gain") || force) {
        ostr << " m_gain: " << m_gain;
    }
    if (settingsKeys.contains("antennaPath") || force) {
        ostr << " m_antennaPath: " << m_antennaPath.toStdString();
    }
    if (settingsKeys.contains("gainMode") || force) {
        ostr << " m_gainMode: " << m_gainMode;
    }
    if (settingsKeys.contains("clockSource") || force) {
        ostr << " m_clockSource: " << m_clockSource.toStdString();
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
