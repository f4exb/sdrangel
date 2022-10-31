///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "bladerf2mimosettings.h"

#include "util/simpleserializer.h"

BladeRF2MIMOSettings::BladeRF2MIMOSettings()
{
    resetToDefaults();
}

void BladeRF2MIMOSettings::resetToDefaults()
{
    m_devSampleRate = 3072000;
    m_LOppmTenths = 0;

    m_rxCenterFrequency = 435000*1000;
    m_log2Decim = 0;
    m_fcPosRx = FC_POS_INFRA;
    m_rxBandwidth = 1500000;
    m_rx0GainMode = 0;
    m_rx0GlobalGain = 0;
    m_rx1GainMode = 0;
    m_rx1GlobalGain = 0;
    m_rxBiasTee = false;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_rxTransverterMode = false;
    m_rxTransverterDeltaFrequency = 0;
    m_iqOrder = true;

    m_txCenterFrequency = 435000*1000;
    m_log2Interp = 0;
    m_fcPosTx = FC_POS_CENTER;
    m_txBandwidth = 1500000;
    m_tx0GlobalGain = -3;
    m_tx1GlobalGain = -3;
    m_txBiasTee = false;
    m_txTransverterMode = false;
    m_txTransverterDeltaFrequency = 0;

    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray BladeRF2MIMOSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
    s.writeS32(2, m_LOppmTenths);

    s.writeU64(10, m_rxCenterFrequency);
    s.writeU32(11, m_log2Decim);
    s.writeS32(12, (int) m_fcPosRx);
    s.writeS32(13, m_rxBandwidth);
    s.writeS32(14, m_rx0GainMode);
    s.writeS32(15, m_rx0GlobalGain);
    s.writeS32(16, m_rx1GainMode);
    s.writeS32(17, m_rx1GlobalGain);
    s.writeBool(18, m_rxBiasTee);
    s.writeBool(19, m_dcBlock);
    s.writeBool(20, m_iqCorrection);
    s.writeBool(21, m_rxTransverterMode);
    s.writeS64(22, m_rxTransverterDeltaFrequency);
    s.writeBool(23, m_iqOrder);

    s.writeU64(30, m_txCenterFrequency);
    s.writeU32(31, m_log2Interp);
    s.writeS32(32, m_txBandwidth);
    s.writeS32(33, m_tx0GlobalGain);
    s.writeS32(34, m_tx1GlobalGain);
    s.writeBool(35, m_txBiasTee);
    s.writeBool(36, m_txTransverterMode);
    s.writeS64(37, m_txTransverterDeltaFrequency);
    s.writeS32(38, (int) m_fcPosTx);

    s.writeBool(51, m_useReverseAPI);
    s.writeString(52, m_reverseAPIAddress);
    s.writeU32(53, m_reverseAPIPort);
    s.writeU32(54, m_reverseAPIDeviceIndex);

    return s.final();
}

bool BladeRF2MIMOSettings::deserialize(const QByteArray& data)
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
        d.readS32(2, &m_LOppmTenths, 0);

        d.readU64(10, &m_rxCenterFrequency, 435000*1000);
        d.readU32(11, &m_log2Decim);
        d.readS32(12, &intval, 0);
        m_fcPosRx = (fcPos_t) intval;
        d.readS32(13, &m_rxBandwidth);
        d.readS32(14, &m_rx0GainMode);
        d.readS32(15, &m_rx0GlobalGain);
        d.readS32(16, &m_rx1GainMode);
        d.readS32(17, &m_rx1GlobalGain);
        d.readBool(18, &m_rxBiasTee);
        d.readBool(19, &m_dcBlock);
        d.readBool(20, &m_iqCorrection);
        d.readBool(21, &m_rxTransverterMode, false);
        d.readS64(22, &m_rxTransverterDeltaFrequency, 0);
        d.readBool(23, &m_iqOrder, true);

        d.readU64(30, &m_txCenterFrequency, 435000*1000);
        d.readU32(31, &m_log2Interp);
        d.readS32(32, &m_txBandwidth);
        d.readS32(33, &m_tx0GlobalGain);
        d.readS32(34, &m_tx1GlobalGain);
        d.readBool(35, &m_txBiasTee);
        d.readBool(36, &m_txTransverterMode, false);
        d.readS64(37, &m_txTransverterDeltaFrequency, 0);
        d.readS32(38, &intval, 2);
        m_fcPosTx = (fcPos_t) intval;

        d.readBool(51, &m_useReverseAPI, false);
        d.readString(52, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(53, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(54, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void BladeRF2MIMOSettings::applySettings(const QStringList& settingsKeys, const BladeRF2MIMOSettings& settings)
{
    if (settingsKeys.contains("devSampleRate")) {
        m_devSampleRate = settings.m_devSampleRate;
    }
    if (settingsKeys.contains("LOppmTenths")) {
        m_LOppmTenths = settings.m_LOppmTenths;
    }
    if (settingsKeys.contains("rxCenterFrequency")) {
        m_rxCenterFrequency = settings.m_rxCenterFrequency;
    }
    if (settingsKeys.contains("log2Decim")) {
        m_log2Decim = settings.m_log2Decim;
    }
    if (settingsKeys.contains("fcPosRx")) {
        m_fcPosRx = settings.m_fcPosRx;
    }
    if (settingsKeys.contains("rxBandwidth")) {
        m_rxBandwidth = settings.m_rxBandwidth;
    }
    if (settingsKeys.contains("rx0GainMode")) {
        m_rx0GainMode = settings.m_rx0GainMode;
    }
    if (settingsKeys.contains("rx0GlobalGain")) {
        m_rx0GlobalGain = settings.m_rx0GlobalGain;
    }
    if (settingsKeys.contains("rx1GainMode")) {
        m_rx1GainMode = settings.m_rx1GainMode;
    }
    if (settingsKeys.contains("rx1GlobalGain")) {
        m_rx1GlobalGain = settings.m_rx1GlobalGain;
    }
    if (settingsKeys.contains("rxBiasTee")) {
        m_rxBiasTee = settings.m_rxBiasTee;
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
    if (settingsKeys.contains("txCenterFrequency")) {
        m_txCenterFrequency = settings.m_txCenterFrequency;
    }
    if (settingsKeys.contains("log2Interp")) {
        m_log2Interp = settings.m_log2Interp;
    }
    if (settingsKeys.contains("fcPosTx")) {
        m_fcPosTx = settings.m_fcPosTx;
    }
    if (settingsKeys.contains("txBandwidth")) {
        m_txBandwidth = settings.m_txBandwidth;
    }
    if (settingsKeys.contains("tx0GlobalGain")) {
        m_tx0GlobalGain = settings.m_tx0GlobalGain;
    }
    if (settingsKeys.contains("tx1GlobalGain")) {
        m_tx1GlobalGain = settings.m_tx1GlobalGain;
    }
    if (settingsKeys.contains("txBiasTee")) {
        m_txBiasTee = settings.m_txBiasTee;
    }
    if (settingsKeys.contains("txTransverterMode")) {
        m_txTransverterMode = settings.m_txTransverterMode;
    }
    if (settingsKeys.contains("txTransverterDeltaFrequency")) {
        m_txTransverterDeltaFrequency = settings.m_txTransverterDeltaFrequency;
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

QString BladeRF2MIMOSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("devSampleRate") || force) {
        ostr << " m_devSampleRate: " << m_devSampleRate;
    }
    if (settingsKeys.contains("LOppmTenths") || force) {
        ostr << " m_LOppmTenths: " << m_LOppmTenths;
    }
    if (settingsKeys.contains("rxCenterFrequency") || force) {
        ostr << " m_rxCenterFrequency: " << m_rxCenterFrequency;
    }
    if (settingsKeys.contains("log2Decim") || force) {
        ostr << " m_log2Decim: " << m_log2Decim;
    }
    if (settingsKeys.contains("fcPosRx") || force) {
        ostr << " m_fcPosRx: " << m_fcPosRx;
    }
    if (settingsKeys.contains("rxBandwidth") || force) {
        ostr << " m_rxBandwidth: " << m_rxBandwidth;
    }
    if (settingsKeys.contains("rx0GainMode") || force) {
        ostr << " m_rx0GainMode: " << m_rx0GainMode;
    }
    if (settingsKeys.contains("rx0GlobalGain") || force) {
        ostr << " m_rx0GlobalGain: " << m_rx0GlobalGain;
    }
    if (settingsKeys.contains("rx1GainMode") || force) {
        ostr << " m_rx1GainMode: " << m_rx1GainMode;
    }
    if (settingsKeys.contains("rx1GlobalGain") || force) {
        ostr << " m_rx1GlobalGain: " << m_rx1GlobalGain;
    }
    if (settingsKeys.contains("rxBiasTee") || force) {
        ostr << " m_rxBiasTee: " << m_rxBiasTee;
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
    if (settingsKeys.contains("txCenterFrequency") || force) {
        ostr << " m_txCenterFrequency: " << m_txCenterFrequency;
    }
    if (settingsKeys.contains("log2Interp") || force) {
        ostr << " m_log2Interp: " << m_log2Interp;
    }
    if (settingsKeys.contains("fcPosTx") || force) {
        ostr << " m_fcPosTx: " << m_fcPosTx;
    }
    if (settingsKeys.contains("txBandwidth") || force) {
        ostr << " m_txBandwidth: " << m_txBandwidth;
    }
    if (settingsKeys.contains("tx0GlobalGain") || force) {
        ostr << " m_tx0GlobalGain: " << m_tx0GlobalGain;
    }
    if (settingsKeys.contains("tx1GlobalGain") || force) {
        ostr << " m_tx1GlobalGain: " << m_tx1GlobalGain;
    }
    if (settingsKeys.contains("txBiasTee") || force) {
        ostr << " m_txBiasTee: " << m_txBiasTee;
    }
    if (settingsKeys.contains("txTransverterMode") || force) {
        ostr << " m_txTransverterMode: " << m_txTransverterMode;
    }
    if (settingsKeys.contains("txTransverterDeltaFrequency") || force) {
        ostr << " m_txTransverterDeltaFrequency: " << m_txTransverterDeltaFrequency;
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



