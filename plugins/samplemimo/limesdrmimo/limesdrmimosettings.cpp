///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include "limesdrmimosettings.h"

#include "util/simpleserializer.h"

LimeSDRMIMOSettings::LimeSDRMIMOSettings()
{
    resetToDefaults();
}

void LimeSDRMIMOSettings::resetToDefaults()
{
    m_devSampleRate = 3200000;
    m_gpioDir = 0;
    m_gpioPins = 0;
    m_extClock = false;
    m_extClockFreq = 10000000; // 10 MHz
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;

    m_rxCenterFrequency = 435000*1000;
    m_log2HardDecim = 3;
    m_log2SoftDecim = 0;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_rxTransverterMode = false;
    m_rxTransverterDeltaFrequency = 0;
    m_iqOrder = true;
    m_ncoEnableRx = false;
    m_ncoFrequencyRx = 0;

    m_lpfBWRx0 = 4.5e6f;
    m_lpfFIREnableRx0 = false;
    m_lpfFIRBWRx0 = 2.5e6f;
    m_gainRx0 = 50;
    m_antennaPathRx0 = PATH_RFE_RX_NONE;
    m_gainModeRx0 = GAIN_AUTO;
    m_lnaGainRx0 = 15;
    m_tiaGainRx0 = 2;
    m_pgaGainRx0 = 16;

    m_lpfBWRx1 = 4.5e6f;
    m_lpfFIREnableRx1 = false;
    m_lpfFIRBWRx1 = 2.5e6f;
    m_gainRx1 = 50;
    m_antennaPathRx1 = PATH_RFE_RX_NONE;
    m_gainModeRx1 = GAIN_AUTO;
    m_lnaGainRx1 = 15;
    m_tiaGainRx1 = 2;
    m_pgaGainRx1 = 16;

    m_txCenterFrequency = 435000*1000;
    m_log2HardInterp = 3;
    m_log2SoftInterp = 0;
    m_txTransverterMode = false;
    m_txTransverterDeltaFrequency = 0;
    m_ncoEnableTx = false;
    m_ncoFrequencyTx = 0;

    m_lpfBWTx0 = 5.5e6f;
    m_lpfFIREnableTx0 = false;
    m_lpfFIRBWTx0 = 2.5e6f;
    m_gainTx0 = 4;
    m_antennaPathTx0 = PATH_RFE_TX_NONE;

    m_lpfBWTx1 = 5.5e6f;
    m_lpfFIREnableTx1 = false;
    m_lpfFIRBWTx1 = 2.5e6f;
    m_gainTx1 = 4;
    m_antennaPathTx1 = PATH_RFE_TX_NONE;
}

QByteArray LimeSDRMIMOSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
    s.writeU32(3, m_gpioDir);
    s.writeU32(4, m_gpioPins);
    s.writeBool(5, m_extClock);
    s.writeU32(6, m_extClockFreq);
    s.writeBool(8, m_useReverseAPI);
    s.writeString(9, m_reverseAPIAddress);
    s.writeU32(10, m_reverseAPIPort);
    s.writeU32(11, m_reverseAPIDeviceIndex);

    s.writeU64(20, m_rxCenterFrequency);
    s.writeU32(21, m_log2HardDecim);
    s.writeU32(22, m_log2SoftDecim);
    s.writeBool(23, m_dcBlock);
    s.writeBool(24, m_iqCorrection);
    s.writeBool(25, m_rxTransverterMode);
    s.writeS64(26, m_rxTransverterDeltaFrequency);
    s.writeBool(27, m_ncoEnableRx);
    s.writeS32(28, m_ncoFrequencyRx);
    s.writeBool(29, m_iqOrder);

    s.writeFloat(30, m_lpfBWRx0);
    s.writeBool(31, m_lpfFIREnableRx0);
    s.writeFloat(32, m_lpfFIRBWRx0);
    s.writeU32(33, m_gainRx0);
    s.writeS32(34, (int) m_antennaPathRx0);
    s.writeS32(35, (int) m_gainModeRx0);
    s.writeU32(36, m_lnaGainRx0);
    s.writeU32(37, m_tiaGainRx0);
    s.writeU32(38, m_pgaGainRx0);

    s.writeFloat(50, m_lpfBWRx1);
    s.writeBool(51, m_lpfFIREnableRx1);
    s.writeFloat(52, m_lpfFIRBWRx1);
    s.writeU32(53, m_gainRx1);
    s.writeS32(54, (int) m_antennaPathRx1);
    s.writeS32(55, (int) m_gainModeRx1);
    s.writeU32(56, m_lnaGainRx1);
    s.writeU32(57, m_tiaGainRx1);
    s.writeU32(58, m_pgaGainRx1);

    s.writeU64(70, m_txCenterFrequency);
    s.writeU32(71, m_log2HardInterp);
    s.writeU32(72, m_log2SoftInterp);
    s.writeBool(73, m_txTransverterMode);
    s.writeS64(74, m_txTransverterDeltaFrequency);
    s.writeBool(75, m_ncoEnableTx);
    s.writeS32(76, m_ncoFrequencyTx);

    s.writeFloat(80, m_lpfBWTx0);
    s.writeBool(81, m_lpfFIREnableTx0);
    s.writeFloat(82, m_lpfFIRBWTx0);
    s.writeU32(83, m_gainTx0);
    s.writeS32(84, (int) m_antennaPathTx0);

    s.writeFloat(90, m_lpfBWTx1);
    s.writeBool(91, m_lpfFIREnableTx1);
    s.writeFloat(92, m_lpfFIRBWTx1);
    s.writeU32(93, m_gainTx1);
    s.writeS32(94, (int) m_antennaPathTx1);

    return s.final();
}

bool LimeSDRMIMOSettings::deserialize(const QByteArray& data)
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
        d.readU32(3, &uintval, 0);
        m_gpioDir = uintval & 0xFF;
        d.readU32(4, &uintval, 0);
        m_gpioPins = uintval & 0xFF;
        d.readBool(5, &m_extClock, false);
        d.readU32(6, &m_extClockFreq, 10000000);
        d.readBool(8, &m_useReverseAPI, false);
        d.readString(9, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(10, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(11, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        d.readU64(20, &m_rxCenterFrequency, 435000*1000);
        d.readU32(21, &m_log2HardDecim, 2);
        d.readU32(22, &m_log2SoftDecim, 0);
        d.readBool(23, &m_dcBlock, false);
        d.readBool(24, &m_iqCorrection, false);
        d.readBool(25, &m_rxTransverterMode, false);
        d.readS64(26, &m_rxTransverterDeltaFrequency, 0);
        d.readBool(27, &m_ncoEnableRx, false);
        d.readS32(28, &m_ncoFrequencyRx, 0);
        d.readBool(29, &m_iqOrder, true);

        d.readFloat(30, &m_lpfBWRx0, 1.5e6);
        d.readBool(31, &m_lpfFIREnableRx0, false);
        d.readFloat(32, &m_lpfFIRBWRx0, 1.5e6);
        d.readU32(33, &m_gainRx0, 50);
        d.readS32(34, &intval, 0);
        m_antennaPathRx0 = (PathRxRFE) intval;
        d.readS32(35, &intval, 0);
        m_gainModeRx0 = (RxGainMode) intval;
        d.readU32(36, &m_lnaGainRx0, 15);
        d.readU32(37, &m_tiaGainRx0, 2);
        d.readU32(38, &m_pgaGainRx0, 16);

        d.readFloat(50, &m_lpfBWRx1, 1.5e6);
        d.readBool(51, &m_lpfFIREnableRx1, false);
        d.readFloat(52, &m_lpfFIRBWRx1, 1.5e6);
        d.readU32(53, &m_gainRx1, 50);
        d.readS32(54, &intval, 0);
        m_antennaPathRx1 = (PathRxRFE) intval;
        d.readS32(55, &intval, 0);
        m_gainModeRx1 = (RxGainMode) intval;
        d.readU32(56, &m_lnaGainRx1, 15);
        d.readU32(57, &m_tiaGainRx1, 2);
        d.readU32(58, &m_pgaGainRx1, 16);

        d.readU64(70, &m_txCenterFrequency, 435000*1000);
        d.readU32(71, &m_log2HardInterp, 2);
        d.readU32(72, &m_log2SoftInterp, 0);
        d.readBool(73, &m_txTransverterMode, false);
        d.readS64(74, &m_txTransverterDeltaFrequency, 0);
        d.readBool(75, &m_ncoEnableTx, false);
        d.readS32(76, &m_ncoFrequencyTx, 0);

        d.readFloat(80, &m_lpfBWTx0, 1.5e6);
        d.readBool(81, &m_lpfFIREnableTx0, false);
        d.readFloat(82, &m_lpfFIRBWTx0, 1.5e6);
        d.readU32(83, &m_gainTx0, 4);
        d.readS32(84, &intval, 0);
        m_antennaPathTx0 = (PathTxRFE) intval;

        d.readFloat(90, &m_lpfBWTx1, 1.5e6);
        d.readBool(91, &m_lpfFIREnableTx1, false);
        d.readFloat(92, &m_lpfFIRBWTx1, 1.5e6);
        d.readU32(93, &m_gainTx1, 4);
        d.readS32(94, &intval, 0);
        m_antennaPathTx1 = (PathTxRFE) intval;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void LimeSDRMIMOSettings::applySettings(const QStringList& settingsKeys, const LimeSDRMIMOSettings& settings)
{
    if (settingsKeys.contains("devSampleRate")) {
        m_devSampleRate = settings.m_devSampleRate;
    }
    if (settingsKeys.contains("gpioDir")) {
        m_gpioDir = settings.m_gpioDir;
    }
    if (settingsKeys.contains("gpioPins")) {
        m_gpioPins = settings.m_gpioPins;
    }
    if (settingsKeys.contains("extClock")) {
        m_extClock = settings.m_extClock;
    }
    if (settingsKeys.contains("extClockFreq")) {
        m_extClockFreq = settings.m_extClockFreq;
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
    if (settingsKeys.contains("rxCenterFrequency")) {
        m_rxCenterFrequency = settings.m_rxCenterFrequency;
    }
    if (settingsKeys.contains("log2HardDecim")) {
        m_log2HardDecim = settings.m_log2HardDecim;
    }
    if (settingsKeys.contains("log2SoftDecim")) {
        m_log2SoftDecim = settings.m_log2SoftDecim;
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
    if (settingsKeys.contains("iqOrder")) {
        m_iqOrder = settings.m_iqOrder;
    }
    if (settingsKeys.contains("ncoEnableRx")) {
        m_ncoEnableRx = settings.m_ncoEnableRx;
    }
    if (settingsKeys.contains("ncoFrequencyRx")) {
        m_ncoFrequencyRx = settings.m_ncoFrequencyRx;
    }
    if (settingsKeys.contains("lpfBWRx0")) {
        m_lpfBWRx0 = settings.m_lpfBWRx0;
    }
    if (settingsKeys.contains("lpfFIREnableRx0")) {
        m_lpfFIREnableRx0 = settings.m_lpfFIREnableRx0;
    }
    if (settingsKeys.contains("lpfFIRBWRx0")) {
        m_lpfFIRBWRx0 = settings.m_lpfFIRBWRx0;
    }
    if (settingsKeys.contains("gainRx0")) {
        m_gainRx0 = settings.m_gainRx0;
    }
    if (settingsKeys.contains("antennaPathRx0")) {
        m_antennaPathRx0 = settings.m_antennaPathRx0;
    }
    if (settingsKeys.contains("gainModeRx0")) {
        m_gainModeRx0 = settings.m_gainModeRx0;
    }
    if (settingsKeys.contains("lnaGainRx0")) {
        m_lnaGainRx0 = settings.m_lnaGainRx0;
    }
    if (settingsKeys.contains("tiaGainRx0")) {
        m_tiaGainRx0 = settings.m_tiaGainRx0;
    }
    if (settingsKeys.contains("pgaGainRx0")) {
        m_pgaGainRx0 = settings.m_pgaGainRx0;
    }
    if (settingsKeys.contains("lpfBWRx1")) {
        m_lpfBWRx1 = settings.m_lpfBWRx1;
    }
    if (settingsKeys.contains("lpfFIREnableRx1")) {
        m_lpfFIREnableRx1 = settings.m_lpfFIREnableRx1;
    }
    if (settingsKeys.contains("lpfFIRBWRx1")) {
        m_lpfFIRBWRx1 = settings.m_lpfFIRBWRx1;
    }
    if (settingsKeys.contains("gainRx1")) {
        m_gainRx1 = settings.m_gainRx1;
    }
    if (settingsKeys.contains("antennaPathRx1")) {
        m_antennaPathRx1 = settings.m_antennaPathRx1;
    }
    if (settingsKeys.contains("gainModeRx1")) {
        m_gainModeRx1 = settings.m_gainModeRx1;
    }
    if (settingsKeys.contains("lnaGainRx1")) {
        m_lnaGainRx1 = settings.m_lnaGainRx1;
    }
    if (settingsKeys.contains("tiaGainRx1")) {
        m_tiaGainRx1 = settings.m_tiaGainRx1;
    }
    if (settingsKeys.contains("pgaGainRx1")) {
        m_pgaGainRx1 = settings.m_pgaGainRx1;
    }
    if (settingsKeys.contains("txCenterFrequency")) {
        m_txCenterFrequency = settings.m_txCenterFrequency;
    }
    if (settingsKeys.contains("log2HardInterp")) {
        m_log2HardInterp = settings.m_log2HardInterp;
    }
    if (settingsKeys.contains("log2SoftInterp")) {
        m_log2SoftInterp = settings.m_log2SoftInterp;
    }
    if (settingsKeys.contains("txTransverterMode")) {
        m_txTransverterMode = settings.m_txTransverterMode;
    }
    if (settingsKeys.contains("txTransverterDeltaFrequency")) {
        m_txTransverterDeltaFrequency = settings.m_txTransverterDeltaFrequency;
    }
    if (settingsKeys.contains("ncoEnableTx")) {
        m_ncoEnableTx = settings.m_ncoEnableTx;
    }
    if (settingsKeys.contains("ncoFrequencyTx")) {
        m_ncoFrequencyTx = settings.m_ncoFrequencyTx;
    }
    if (settingsKeys.contains("lpfBWTx0")) {
        m_lpfBWTx0 = settings.m_lpfBWTx0;
    }
    if (settingsKeys.contains("lpfFIREnableTx0")) {
        m_lpfFIREnableTx0 = settings.m_lpfFIREnableTx0;
    }
    if (settingsKeys.contains("lpfFIRBWTx0")) {
        m_lpfFIRBWTx0 = settings.m_lpfFIRBWTx0;
    }
    if (settingsKeys.contains("gainTx0")) {
        m_gainTx0 = settings.m_gainTx0;
    }
    if (settingsKeys.contains("antennaPathTx0")) {
        m_antennaPathTx0 = settings.m_antennaPathTx0;
    }
    if (settingsKeys.contains("lpfBWTx1")) {
        m_lpfBWTx1 = settings.m_lpfBWTx1;
    }
    if (settingsKeys.contains("lpfFIREnableTx1")) {
        m_lpfFIREnableTx1 = settings.m_lpfFIREnableTx1;
    }
    if (settingsKeys.contains("lpfFIRBWTx1")) {
        m_lpfFIRBWTx1 = settings.m_lpfFIRBWTx1;
    }
    if (settingsKeys.contains("gainTx1")) {
        m_gainTx1 = settings.m_gainTx1;
    }
    if (settingsKeys.contains("antennaPathTx1")) {
        m_antennaPathTx1 = settings.m_antennaPathTx1;
    }
}

QString LimeSDRMIMOSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("devSampleRate") || force) {
        ostr << " m_devSampleRate: " << m_devSampleRate;
    }
    if (settingsKeys.contains("gpioDir") || force) {
        ostr << " m_gpioDir: " << m_gpioDir;
    }
    if (settingsKeys.contains("gpioPins") || force) {
        ostr << " m_gpioPins: " << m_gpioPins;
    }
    if (settingsKeys.contains("extClock") || force) {
        ostr << " m_extClock: " << m_extClock;
    }
    if (settingsKeys.contains("extClockFreq") || force) {
        ostr << " m_extClockFreq: " << m_extClockFreq;
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
    if (settingsKeys.contains("rxCenterFrequency") || force) {
        ostr << " m_rxCenterFrequency: " << m_rxCenterFrequency;
    }
    if (settingsKeys.contains("log2HardDecim") || force) {
        ostr << " m_log2HardDecim: " << m_log2HardDecim;
    }
    if (settingsKeys.contains("log2SoftDecim") || force) {
        ostr << " m_log2SoftDecim: " << m_log2SoftDecim;
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
    if (settingsKeys.contains("iqOrder") || force) {
        ostr << " m_iqOrder: " << m_iqOrder;
    }
    if (settingsKeys.contains("ncoEnableRx") || force) {
        ostr << " m_ncoEnableRx: " << m_ncoEnableRx;
    }
    if (settingsKeys.contains("ncoFrequencyRx") || force) {
        ostr << " m_ncoFrequencyRx: " << m_ncoFrequencyRx;
    }
    if (settingsKeys.contains("lpfBWRx0") || force) {
        ostr << " m_lpfBWRx0: " << m_lpfBWRx0;
    }
    if (settingsKeys.contains("lpfFIREnableRx0") || force) {
        ostr << " m_lpfFIREnableRx0: " << m_lpfFIREnableRx0;
    }
    if (settingsKeys.contains("lpfFIRBWRx0") || force) {
        ostr << " m_lpfFIRBWRx0: " << m_lpfFIRBWRx0;
    }
    if (settingsKeys.contains("gainRx0") || force) {
        ostr << " m_gainRx0: " << m_gainRx0;
    }
    if (settingsKeys.contains("antennaPathRx0") || force) {
        ostr << " m_antennaPathRx0: " << m_antennaPathRx0;
    }
    if (settingsKeys.contains("gainModeRx0") || force) {
        ostr << " m_gainModeRx0: " << m_gainModeRx0;
    }
    if (settingsKeys.contains("lnaGainRx0") || force) {
        ostr << " m_lnaGainRx0: " << m_lnaGainRx0;
    }
    if (settingsKeys.contains("tiaGainRx0") || force) {
        ostr << " m_tiaGainRx0: " << m_tiaGainRx0;
    }
    if (settingsKeys.contains("pgaGainRx0") || force) {
        ostr << " m_pgaGainRx0: " << m_pgaGainRx0;
    }
    if (settingsKeys.contains("lpfBWRx1") || force) {
        ostr << " m_lpfBWRx1: " << m_lpfBWRx1;
    }
    if (settingsKeys.contains("lpfFIREnableRx1") || force) {
        ostr << " m_lpfFIREnableRx1: " << m_lpfFIREnableRx1;
    }
    if (settingsKeys.contains("lpfFIRBWRx1") || force) {
        ostr << " m_lpfFIRBWRx1: " << m_lpfFIRBWRx1;
    }
    if (settingsKeys.contains("gainRx1") || force) {
        ostr << " m_gainRx1: " << m_gainRx1;
    }
    if (settingsKeys.contains("antennaPathRx1") || force) {
        ostr << " m_antennaPathRx1: " << m_antennaPathRx1;
    }
    if (settingsKeys.contains("gainModeRx1") || force) {
        ostr << " m_gainModeRx1: " << m_gainModeRx1;
    }
    if (settingsKeys.contains("lnaGainRx1") || force) {
        ostr << " m_lnaGainRx1: " << m_lnaGainRx1;
    }
    if (settingsKeys.contains("tiaGainRx1") || force) {
        ostr << " m_tiaGainRx1: " << m_tiaGainRx1;
    }
    if (settingsKeys.contains("pgaGainRx1") || force) {
        ostr << " m_pgaGainRx1: " << m_pgaGainRx1;
    }
    if (settingsKeys.contains("txCenterFrequency") || force) {
        ostr << " m_txCenterFrequency: " << m_txCenterFrequency;
    }
    if (settingsKeys.contains("log2HardInterp") || force) {
        ostr << " m_log2HardInterp: " << m_log2HardInterp;
    }
    if (settingsKeys.contains("log2SoftInterp") || force) {
        ostr << " m_log2SoftInterp: " << m_log2SoftInterp;
    }
    if (settingsKeys.contains("txTransverterMode") || force) {
        ostr << " m_txTransverterMode: " << m_txTransverterMode;
    }
    if (settingsKeys.contains("txTransverterDeltaFrequency") || force) {
        ostr << " m_txTransverterDeltaFrequency: " << m_txTransverterDeltaFrequency;
    }
    if (settingsKeys.contains("ncoEnableTx") || force) {
        ostr << " m_ncoEnableTx: " << m_ncoEnableTx;
    }
    if (settingsKeys.contains("ncoFrequencyTx") || force) {
        ostr << " m_ncoFrequencyTx: " << m_ncoFrequencyTx;
    }
    if (settingsKeys.contains("lpfBWTx0") || force) {
        ostr << " m_lpfBWTx0: " << m_lpfBWTx0;
    }
    if (settingsKeys.contains("lpfFIREnableTx0") || force) {
        ostr << " m_lpfFIREnableTx0: " << m_lpfFIREnableTx0;
    }
    if (settingsKeys.contains("lpfFIRBWTx0") || force) {
        ostr << " m_lpfFIRBWTx0: " << m_lpfFIRBWTx0;
    }
    if (settingsKeys.contains("gainTx0") || force) {
        ostr << " m_gainTx0: " << m_gainTx0;
    }
    if (settingsKeys.contains("antennaPathTx0") || force) {
        ostr << " m_antennaPathTx0: " << m_antennaPathTx0;
    }
    if (settingsKeys.contains("lpfBWTx1") || force) {
        ostr << " m_lpfBWTx1: " << m_lpfBWTx1;
    }
    if (settingsKeys.contains("lpfFIREnableTx1") || force) {
        ostr << " m_lpfFIREnableTx1: " << m_lpfFIREnableTx1;
    }
    if (settingsKeys.contains("lpfFIRBWTx1") || force) {
        ostr << " m_lpfFIRBWTx1: " << m_lpfFIRBWTx1;
    }
    if (settingsKeys.contains("gainTx1") || force) {
        ostr << " m_gainTx1: " << m_gainTx1;
    }
    if (settingsKeys.contains("antennaPathTx1") || force) {
        ostr << " m_antennaPathTx1: " << m_antennaPathTx1;
    }

    return QString(ostr.str().c_str());
}
