///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <sstream>

#include <QtGlobal>

#include "util/simpleserializer.h"
#include "usrpmimosettings.h"

USRPMIMOSettings::USRPMIMOSettings()
{
    resetToDefaults();
}

void USRPMIMOSettings::resetToDefaults()
{
    m_title = "USRP";
    m_masterClockRate = -1;
    m_devSampleRate = 3000000;
    m_clockSource = "internal";
    m_gpioDir = 0;
    m_gpioPins = 0;
    m_rxCenterFrequency = 435000*1000;
    m_rxLOOffset = 0;
    m_log2SoftDecim = 0;
    m_rxLpfBW = 10e6f;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_rxTransverterMode = false;
    m_rxTransverterDeltaFrequency = 0;
    m_rx0GainMode = GAIN_AUTO;
    m_rx0Gain = 50;
    m_rx0AntennaPath = "RX2";
    m_rx1GainMode = GAIN_AUTO;
    m_rx1Gain = 50;
    m_rx1AntennaPath = "RX2";
    m_txCenterFrequency = 435000*1000;
    m_txLOOffset = 0;
    m_log2SoftInterp = 0;
    m_txLpfBW = 10e6f;
    m_txTransverterMode = false;
    m_txTransverterDeltaFrequency = 0;
    m_tx0Gain = 50;
    m_tx0AntennaPath = "TX/RX";
    m_tx1Gain = 50;
    m_tx1AntennaPath = "TX/RX";
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray USRPMIMOSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_title);
    s.writeS32(2, m_devSampleRate);
    s.writeString(3, m_clockSource);
    s.writeU32(4, m_gpioDir);
    s.writeU32(5, m_gpioPins);
    s.writeU64(6, m_rxCenterFrequency);
    s.writeS32(7, m_rxLOOffset);
    s.writeU32(8, m_log2SoftDecim);
    s.writeFloat(9, m_rxLpfBW);
    s.writeBool(10, m_dcBlock);
    s.writeBool(11, m_iqCorrection);
    s.writeBool(12, m_rxTransverterMode);
    s.writeS64(13, m_rxTransverterDeltaFrequency);
    s.writeS32(14, (int) m_rx0GainMode);
    s.writeU32(15, m_rx0Gain);
    s.writeString(16, m_rx0AntennaPath);
    s.writeS32(17, (int) m_rx1GainMode);
    s.writeU32(18, m_rx1Gain);
    s.writeString(19, m_rx1AntennaPath);
    s.writeU64(20, m_txCenterFrequency);
    s.writeS32(21, m_txLOOffset);
    s.writeU32(22, m_log2SoftInterp);
    s.writeFloat(23, m_txLpfBW);
    s.writeBool(24, m_txTransverterMode);
    s.writeS64(25, m_txTransverterDeltaFrequency);
    s.writeU32(26, m_tx0Gain);
    s.writeString(27, m_tx0AntennaPath);
    s.writeU32(28, m_tx1Gain);
    s.writeString(29, m_tx1AntennaPath);
    s.writeBool(30, m_useReverseAPI);
    s.writeString(31, m_reverseAPIAddress);
    s.writeU32(32, m_reverseAPIPort);
    s.writeU32(33, m_reverseAPIDeviceIndex);

    return s.final();
}

bool USRPMIMOSettings::deserialize(const QByteArray& data)
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

        d.readString(1, &m_title, "USRP");
        d.readS32(2, &m_devSampleRate, 3000000);
        d.readString(3, &m_clockSource, "internal");
        d.readU32(4, &uintval, 0);
        m_gpioDir = uintval & 0xFF;
        d.readU32(5, &uintval, 0);
        m_gpioPins = uintval & 0xFF;
        d.readU64(6, &m_rxCenterFrequency, 435000*1000);
        d.readS32(7, &m_rxLOOffset, 0);
        d.readU32(8, &m_log2SoftDecim, 0);
        m_log2SoftDecim = m_log2SoftDecim > 6 ? 6 : m_log2SoftDecim;
        d.readFloat(9, &m_rxLpfBW, 10e6f);
        d.readBool(10, &m_dcBlock, false);
        d.readBool(11, &m_iqCorrection, false);
        d.readBool(12, &m_rxTransverterMode, false);
        d.readS64(13, &m_rxTransverterDeltaFrequency, 0);
        d.readS32(14, &intval, (int) GAIN_AUTO);
        m_rx0GainMode = intval == 0 ? GAIN_AUTO : GAIN_MANUAL;
        d.readU32(15, &m_rx0Gain, 50);
        d.readString(16, &m_rx0AntennaPath, "RX2");
        d.readS32(17, &intval, (int) GAIN_AUTO);
        m_rx1GainMode = intval == 0 ? GAIN_AUTO : GAIN_MANUAL;
        d.readU32(18, &m_rx1Gain, 50);
        d.readString(19, &m_rx1AntennaPath, "RX2");
        d.readU64(20, &m_txCenterFrequency, 435000*1000);
        d.readS32(21, &m_txLOOffset, 0);
        d.readU32(22, &m_log2SoftInterp, 0);
        m_log2SoftInterp = m_log2SoftInterp > 6 ? 6 : m_log2SoftInterp;
        d.readFloat(23, &m_txLpfBW, 10e6f);
        d.readBool(24, &m_txTransverterMode, false);
        d.readS64(25, &m_txTransverterDeltaFrequency, 0);
        d.readU32(26, &m_tx0Gain, 50);
        d.readString(27, &m_tx0AntennaPath, "TX/RX");
        d.readU32(28, &m_tx1Gain, 50);
        d.readString(29, &m_tx1AntennaPath, "TX/RX");
        d.readBool(30, &m_useReverseAPI, false);
        d.readString(31, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(32, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(33, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void USRPMIMOSettings::applySettings(const QStringList& settingsKeys, const USRPMIMOSettings& settings)
{
    if (settingsKeys.contains("masterClockRate")) {
        m_masterClockRate = settings.m_masterClockRate;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("devSampleRate")) {
        m_devSampleRate = settings.m_devSampleRate;
    }
    if (settingsKeys.contains("clockSource")) {
        m_clockSource = settings.m_clockSource;
    }
    if (settingsKeys.contains("gpioDir")) {
        m_gpioDir = settings.m_gpioDir;
    }
    if (settingsKeys.contains("gpioPins")) {
        m_gpioPins = settings.m_gpioPins;
    }
    if (settingsKeys.contains("rxCenterFrequency")) {
        m_rxCenterFrequency = settings.m_rxCenterFrequency;
    }
    if (settingsKeys.contains("rxLOOffset")) {
        m_rxLOOffset = settings.m_rxLOOffset;
    }
    if (settingsKeys.contains("log2SoftDecim")) {
        m_log2SoftDecim = settings.m_log2SoftDecim;
    }
    if (settingsKeys.contains("rxLpfBW")) {
        m_rxLpfBW = settings.m_rxLpfBW;
    }
    if (settingsKeys.contains("dcBlock")) {
        m_dcBlock = settings.m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection")) {
        m_iqCorrection = settings.m_iqCorrection;
    }
    if (settingsKeys.contains("rxTransverterMode")) {
        m_rxTransverterMode = settings.m_rxTransverterMode;
    }
    if (settingsKeys.contains("rxTransverterDeltaFrequency")) {
        m_rxTransverterDeltaFrequency = settings.m_rxTransverterDeltaFrequency;
    }
    if (settingsKeys.contains("rx0GainMode")) {
        m_rx0GainMode = settings.m_rx0GainMode;
    }
    if (settingsKeys.contains("rx0Gain")) {
        m_rx0Gain = settings.m_rx0Gain;
    }
    if (settingsKeys.contains("rx0AntennaPath")) {
        m_rx0AntennaPath = settings.m_rx0AntennaPath;
    }
    if (settingsKeys.contains("rx1GainMode")) {
        m_rx1GainMode = settings.m_rx1GainMode;
    }
    if (settingsKeys.contains("rx1Gain")) {
        m_rx1Gain = settings.m_rx1Gain;
    }
    if (settingsKeys.contains("rx1AntennaPath")) {
        m_rx1AntennaPath = settings.m_rx1AntennaPath;
    }
    if (settingsKeys.contains("txCenterFrequency")) {
        m_txCenterFrequency = settings.m_txCenterFrequency;
    }
    if (settingsKeys.contains("txLOOffset")) {
        m_txLOOffset = settings.m_txLOOffset;
    }
    if (settingsKeys.contains("log2SoftInterp")) {
        m_log2SoftInterp = settings.m_log2SoftInterp;
    }
    if (settingsKeys.contains("txLpfBW")) {
        m_txLpfBW = settings.m_txLpfBW;
    }
    if (settingsKeys.contains("txTransverterMode")) {
        m_txTransverterMode = settings.m_txTransverterMode;
    }
    if (settingsKeys.contains("txTransverterDeltaFrequency")) {
        m_txTransverterDeltaFrequency = settings.m_txTransverterDeltaFrequency;
    }
    if (settingsKeys.contains("tx0Gain")) {
        m_tx0Gain = settings.m_tx0Gain;
    }
    if (settingsKeys.contains("tx0AntennaPath")) {
        m_tx0AntennaPath = settings.m_tx0AntennaPath;
    }
    if (settingsKeys.contains("tx1Gain")) {
        m_tx1Gain = settings.m_tx1Gain;
    }
    if (settingsKeys.contains("tx1AntennaPath")) {
        m_tx1AntennaPath = settings.m_tx1AntennaPath;
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

QString USRPMIMOSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("masterClockRate") || force) {
        ostr << " m_masterClockRate: " << m_masterClockRate;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("devSampleRate") || force) {
        ostr << " m_devSampleRate: " << m_devSampleRate;
    }
    if (settingsKeys.contains("clockSource") || force) {
        ostr << " m_clockSource: " << m_clockSource.toStdString();
    }
    if (settingsKeys.contains("gpioDir") || force) {
        ostr << " m_gpioDir: " << (int) m_gpioDir;
    }
    if (settingsKeys.contains("gpioPins") || force) {
        ostr << " m_gpioPins: " << (int) m_gpioPins;
    }
    if (settingsKeys.contains("rxCenterFrequency") || force) {
        ostr << " m_rxCenterFrequency: " << m_rxCenterFrequency;
    }
    if (settingsKeys.contains("rxLOOffset") || force) {
        ostr << " m_rxLOOffset: " << m_rxLOOffset;
    }
    if (settingsKeys.contains("log2SoftDecim") || force) {
        ostr << " m_log2SoftDecim: " << m_log2SoftDecim;
    }
    if (settingsKeys.contains("rxLpfBW") || force) {
        ostr << " m_rxLpfBW: " << m_rxLpfBW;
    }
    if (settingsKeys.contains("dcBlock") || force) {
        ostr << " m_dcBlock: " << m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection") || force) {
        ostr << " m_iqCorrection: " << m_iqCorrection;
    }
    if (settingsKeys.contains("rxTransverterMode") || force) {
        ostr << " m_rxTransverterMode: " << m_rxTransverterMode;
    }
    if (settingsKeys.contains("rxTransverterDeltaFrequency") || force) {
        ostr << " m_rxTransverterDeltaFrequency: " << m_rxTransverterDeltaFrequency;
    }
    if (settingsKeys.contains("rx0GainMode") || force) {
        ostr << " m_rx0GainMode: " << m_rx0GainMode;
    }
    if (settingsKeys.contains("rx0Gain") || force) {
        ostr << " m_rx0Gain: " << m_rx0Gain;
    }
    if (settingsKeys.contains("rx0AntennaPath") || force) {
        ostr << " m_rx0AntennaPath: " << m_rx0AntennaPath.toStdString();
    }
    if (settingsKeys.contains("rx1GainMode") || force) {
        ostr << " m_rx1GainMode: " << m_rx1GainMode;
    }
    if (settingsKeys.contains("rx1Gain") || force) {
        ostr << " m_rx1Gain: " << m_rx1Gain;
    }
    if (settingsKeys.contains("rx1AntennaPath") || force) {
        ostr << " m_rx1AntennaPath: " << m_rx1AntennaPath.toStdString();
    }
    if (settingsKeys.contains("txCenterFrequency") || force) {
        ostr << " m_txCenterFrequency: " << m_txCenterFrequency;
    }
    if (settingsKeys.contains("txLOOffset") || force) {
        ostr << " m_txLOOffset: " << m_txLOOffset;
    }
    if (settingsKeys.contains("log2SoftInterp") || force) {
        ostr << " m_log2SoftInterp: " << m_log2SoftInterp;
    }
    if (settingsKeys.contains("txLpfBW") || force) {
        ostr << " m_txLpfBW: " << m_txLpfBW;
    }
    if (settingsKeys.contains("txTransverterMode") || force) {
        ostr << " m_txTransverterMode: " << m_txTransverterMode;
    }
    if (settingsKeys.contains("txTransverterDeltaFrequency") || force) {
        ostr << " m_txTransverterDeltaFrequency: " << m_txTransverterDeltaFrequency;
    }
    if (settingsKeys.contains("tx0Gain") || force) {
        ostr << " m_tx0Gain: " << m_tx0Gain;
    }
    if (settingsKeys.contains("tx0AntennaPath") || force) {
        ostr << " m_tx0AntennaPath: " << m_tx0AntennaPath.toStdString();
    }
    if (settingsKeys.contains("tx1Gain") || force) {
        ostr << " m_tx1Gain: " << m_tx1Gain;
    }
    if (settingsKeys.contains("tx1AntennaPath") || force) {
        ostr << " m_tx1AntennaPath: " << m_tx1AntennaPath.toStdString();
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
