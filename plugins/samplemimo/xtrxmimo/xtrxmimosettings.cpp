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

#include "util/simpleserializer.h"
#include "xtrxmimosettings.h"

XTRXMIMOSettings::XTRXMIMOSettings()
{
    resetToDefaults();
}

void XTRXMIMOSettings::resetToDefaults()
{
    // common
    m_extClock = false;
    m_extClockFreq = 0; // Auto
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    // Rx
    m_rxDevSampleRate = 5e6;
    m_rxCenterFrequency = 435000*1000;
    m_log2HardDecim = 2;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_log2SoftDecim = 0;
    m_ncoEnableRx = false;
    m_ncoFrequencyRx = 0;
    m_antennaPathRx = RXANT_LO;
    m_iqOrder = true;
    // Rx0
    m_lpfBWRx0 = 4.5e6f;
    m_gainRx0 = 50;
    m_gainModeRx0 = GAIN_AUTO;
    m_lnaGainRx0 = 15;
    m_tiaGainRx0 = 2;
    m_pgaGainRx0 = 16;
    m_pwrmodeRx0 = 4;
    // Rx1
    m_lpfBWRx1 = 4.5e6f;
    m_gainRx1 = 50;
    m_gainModeRx1 = GAIN_AUTO;
    m_lnaGainRx1 = 15;
    m_tiaGainRx1 = 2;
    m_pgaGainRx1 = 16;
    m_pwrmodeRx1 = 4;
    // Tx
    m_txDevSampleRate = 5e6;
    m_txCenterFrequency = 435000*1000;
    m_log2HardInterp = 2;
    m_log2SoftInterp = 4;
    m_ncoEnableTx = true;
    m_ncoFrequencyTx = 500000;
    m_antennaPathTx = TXANT_WI;
    // Tx0
    m_lpfBWTx0 = 4.5e6f;
    m_gainTx0 = 20;
    m_pwrmodeTx0 = 4;
    // Tx1
    m_lpfBWTx1 = 4.5e6f;
    m_gainTx1 = 20;
    m_pwrmodeTx1 = 4;
}

QByteArray XTRXMIMOSettings::serialize() const
{
    SimpleSerializer s(1);

    // common
    s.writeBool(2, m_extClock);
    s.writeU32(3, m_extClockFreq);
    s.writeBool(5, m_useReverseAPI);
    s.writeString(6, m_reverseAPIAddress);
    s.writeU32(7, m_reverseAPIPort);
    s.writeU32(8, m_reverseAPIDeviceIndex);
    // Rx
    s.writeU32(20, m_log2HardDecim);
    s.writeU32(21, m_log2SoftDecim);
    s.writeBool(22, m_dcBlock);
    s.writeBool(23, m_iqCorrection);
    s.writeBool(24, m_ncoEnableRx);
    s.writeS32(25, m_ncoFrequencyRx);
    s.writeS32(26, (int) m_antennaPathRx);
    s.writeDouble(27, m_rxDevSampleRate);
    s.writeBool(28, m_iqOrder);
    // Rx0
    s.writeFloat(30, m_lpfBWRx0);
    s.writeU32(31, m_gainRx0);
    s.writeS32(34, (int) m_gainModeRx0);
    s.writeU32(35, m_lnaGainRx0);
    s.writeU32(36, m_tiaGainRx0);
    s.writeU32(37, m_pgaGainRx0);
    s.writeU32(38, m_pwrmodeRx0);
    // Rx1
    s.writeFloat(50, m_lpfBWRx0);
    s.writeU32(51, m_gainRx0);
    s.writeS32(54, (int) m_gainModeRx0);
    s.writeU32(55, m_lnaGainRx0);
    s.writeU32(56, m_tiaGainRx0);
    s.writeU32(57, m_pgaGainRx0);
    s.writeU32(58, m_pwrmodeRx0);
    // Tx
    s.writeU32(70, m_log2HardInterp);
    s.writeU32(71, m_log2SoftInterp);
    s.writeBool(72, m_ncoEnableTx);
    s.writeS32(73, m_ncoFrequencyTx);
    s.writeS32(74, (int) m_antennaPathTx);
    s.writeDouble(75, m_txDevSampleRate);
    // Tx0
    s.writeFloat(80, m_lpfBWTx0);
    s.writeU32(81, m_gainTx0);
    s.writeU32(82, m_pwrmodeTx0);
    // Tx1
    s.writeFloat(90, m_lpfBWTx1);
    s.writeU32(91, m_gainTx1);
    s.writeU32(92, m_pwrmodeTx1);

    return s.final();
}

bool XTRXMIMOSettings::deserialize(const QByteArray& data)
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

        // common
        d.readBool(2, &m_extClock, false);
        d.readU32(3, &m_extClockFreq, 0);
        d.readBool(5, &m_useReverseAPI, false);
        d.readString(6, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(7, &uintval, 0);
        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }
        d.readU32(8, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;
        // Rx
        d.readU32(20, &m_log2HardDecim, 1);
        d.readU32(21, &m_log2SoftDecim, 0);
        d.readBool(22, &m_dcBlock, false);
        d.readBool(23, &m_iqCorrection, false);
        d.readBool(24, &m_ncoEnableRx, false);
        d.readS32(25, &m_ncoFrequencyRx, 0);
        d.readS32(26, &intval, 0);
        m_antennaPathRx = (RxAntenna) intval;
        d.readDouble(27, &m_rxDevSampleRate, 5e6);
        d.readBool(28, &m_iqOrder, true);
        // Rx0
        d.readFloat(30, &m_lpfBWRx0, 1.5e6);
        d.readU32(31, &m_gainRx0, 50);
        d.readS32(34, &intval, 0);
        m_gainModeRx0 = (GainMode) intval;
        d.readU32(35, &m_lnaGainRx0, 15);
        d.readU32(36, &m_tiaGainRx0, 2);
        d.readU32(37, &m_pgaGainRx0, 16);
        d.readU32(38, &m_pwrmodeRx0, 4);
        // Rx1
        d.readFloat(50, &m_lpfBWRx1, 1.5e6);
        d.readU32(51, &m_gainRx1, 50);
        d.readS32(54, &intval, 0);
        m_gainModeRx1 = (GainMode) intval;
        d.readU32(55, &m_lnaGainRx1, 15);
        d.readU32(56, &m_tiaGainRx1, 2);
        d.readU32(57, &m_pgaGainRx1, 16);
        d.readU32(58, &m_pwrmodeRx1, 4);
        // Tx
        d.readU32(70, &m_log2HardInterp, 2);
        d.readU32(71, &m_log2SoftInterp, 0);
        d.readS32(72, &intval, 0);
        d.readBool(73, &m_ncoEnableTx, true);
        d.readS32(74, &m_ncoFrequencyTx, 500000);
        m_antennaPathTx = (TxAntenna) intval;
        d.readDouble(75, &m_txDevSampleRate, 5e6);
        // Tx0
        d.readFloat(80, &m_lpfBWTx0, 1.5e6);
        d.readU32(81, &m_gainTx0, 20);
        d.readU32(82, &m_pwrmodeTx0, 4);
        // Tx1
        d.readFloat(90, &m_lpfBWTx1, 1.5e6);
        d.readU32(91, &m_gainTx1, 20);
        d.readU32(92, &m_pwrmodeTx1, 4);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }

}

void XTRXMIMOSettings::applySettings(const QStringList& settingsKeys, const XTRXMIMOSettings& settings)
{
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
    if (settingsKeys.contains("rxDevSampleRate")) {
        m_rxDevSampleRate = settings.m_rxDevSampleRate;
    }
    if (settingsKeys.contains("rxCenterFrequency")) {
        m_rxCenterFrequency = settings.m_rxCenterFrequency;
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
    if (settingsKeys.contains("ncoEnableRx")) {
        m_ncoEnableRx = settings.m_ncoEnableRx;
    }
    if (settingsKeys.contains("ncoFrequencyRx")) {
        m_ncoFrequencyRx = settings.m_ncoFrequencyRx;
    }
    if (settingsKeys.contains("antennaPathRx")) {
        m_antennaPathRx = settings.m_antennaPathRx;
    }
    if (settingsKeys.contains("iqOrder")) {
        m_iqOrder = settings.m_iqOrder;
    }
    if (settingsKeys.contains("lpfBWRx0")) {
        m_lpfBWRx0 = settings.m_lpfBWRx0;
    }
    if (settingsKeys.contains("gainRx0")) {
        m_gainRx0 = settings.m_gainRx0;
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
    if (settingsKeys.contains("pwrmodeRx0")) {
        m_pwrmodeRx0 = settings.m_pwrmodeRx0;
    }
    if (settingsKeys.contains("lpfBWRx1")) {
        m_lpfBWRx1 = settings.m_lpfBWRx1;
    }
    if (settingsKeys.contains("gainRx1")) {
        m_gainRx1 = settings.m_gainRx1;
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
    if (settingsKeys.contains("pwrmodeRx1")) {
        m_pwrmodeRx1 = settings.m_pwrmodeRx1;
    }
    if (settingsKeys.contains("txDevSampleRate")) {
        m_txDevSampleRate = settings.m_txDevSampleRate;
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
    if (settingsKeys.contains("ncoEnableTx")) {
        m_ncoEnableTx = settings.m_ncoEnableTx;
    }
    if (settingsKeys.contains("ncoFrequencyTx")) {
        m_ncoFrequencyTx = settings.m_ncoFrequencyTx;
    }
    if (settingsKeys.contains("antennaPathTx")) {
        m_antennaPathTx = settings.m_antennaPathTx;
    }
    if (settingsKeys.contains("lpfBWTx0")) {
        m_lpfBWTx0 = settings.m_lpfBWTx0;
    }
    if (settingsKeys.contains("gainTx0")) {
        m_gainTx0 = settings.m_gainTx0;
    }
    if (settingsKeys.contains("pwrmodeTx0")) {
        m_pwrmodeTx0 = settings.m_pwrmodeTx0;
    }
    if (settingsKeys.contains("lpfBWTx1")) {
        m_lpfBWTx1 = settings.m_lpfBWTx1;
    }
    if (settingsKeys.contains("gainTx1")) {
        m_gainTx1 = settings.m_gainTx1;
    }
    if (settingsKeys.contains("pwrmodeTx1")) {
        m_pwrmodeTx1 = settings.m_pwrmodeTx1;
    }
}

QString XTRXMIMOSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

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
    if (settingsKeys.contains("rxDevSampleRate") || force) {
        ostr << " m_rxDevSampleRate: " << m_rxDevSampleRate;
    }
    if (settingsKeys.contains("rxCenterFrequency") || force) {
        ostr << " m_rxCenterFrequency: " << m_rxCenterFrequency;
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
    if (settingsKeys.contains("ncoEnableRx") || force) {
        ostr << " m_ncoEnableRx: " << m_ncoEnableRx;
    }
    if (settingsKeys.contains("ncoFrequencyRx") || force) {
        ostr << " m_ncoFrequencyRx: " << m_ncoFrequencyRx;
    }
    if (settingsKeys.contains("antennaPathRx") || force) {
        ostr << " m_antennaPathRx: " << m_antennaPathRx;
    }
    if (settingsKeys.contains("iqOrder") || force) {
        ostr << " m_iqOrder: " << m_iqOrder;
    }
    if (settingsKeys.contains("lpfBWRx0") || force) {
        ostr << " m_lpfBWRx0: " << m_lpfBWRx0;
    }
    if (settingsKeys.contains("gainRx0") || force) {
        ostr << " m_gainRx0: " << m_gainRx0;
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
    if (settingsKeys.contains("pwrmodeRx0") || force) {
        ostr << " m_pwrmodeRx0: " << m_pwrmodeRx0;
    }
    if (settingsKeys.contains("lpfBWRx1") || force) {
        ostr << " m_lpfBWRx1: " << m_lpfBWRx1;
    }
    if (settingsKeys.contains("gainRx1") || force) {
        ostr << " m_gainRx1: " << m_gainRx1;
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
    if (settingsKeys.contains("pwrmodeRx1") || force) {
        ostr << " m_pwrmodeRx1: " << m_pwrmodeRx1;
    }
    if (settingsKeys.contains("txDevSampleRate") || force) {
        ostr << " m_txDevSampleRate: " << m_txDevSampleRate;
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
    if (settingsKeys.contains("ncoEnableTx") || force) {
        ostr << " m_ncoEnableTx: " << m_ncoEnableTx;
    }
    if (settingsKeys.contains("ncoFrequencyTx") || force) {
        ostr << " m_ncoFrequencyTx: " << m_ncoFrequencyTx;
    }
    if (settingsKeys.contains("antennaPathTx") || force) {
        ostr << " m_antennaPathTx: " << m_antennaPathTx;
    }
    if (settingsKeys.contains("lpfBWTx0") || force) {
        ostr << " m_lpfBWTx0: " << m_lpfBWTx0;
    }
    if (settingsKeys.contains("gainTx0") || force) {
        ostr << " m_gainTx0: " << m_gainTx0;
    }
    if (settingsKeys.contains("pwrmodeTx0") || force) {
        ostr << " m_pwrmodeTx0: " << m_pwrmodeTx0;
    }
    if (settingsKeys.contains("lpfBWTx1") || force) {
        ostr << " m_lpfBWTx1: " << m_lpfBWTx1;
    }
    if (settingsKeys.contains("gainTx1") || force) {
        ostr << " m_gainTx1: " << m_gainTx1;
    }
    if (settingsKeys.contains("pwrmodeTx1") || force) {
        ostr << " m_pwrmodeTx1: " << m_pwrmodeTx1;
    }

    return QString(ostr.str().c_str());
}
