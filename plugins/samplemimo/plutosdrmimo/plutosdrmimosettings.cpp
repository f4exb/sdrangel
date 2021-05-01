///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
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

#include "plutosdrmimosettings.h"

#include <QtGlobal>
#include "util/simpleserializer.h"


PlutoSDRMIMOSettings::PlutoSDRMIMOSettings()
{
	resetToDefaults();
}

void PlutoSDRMIMOSettings::resetToDefaults()
{
	m_devSampleRate = 2500 * 1000;
	m_LOppmTenths = 0;

	m_rxCenterFrequency = 435000 * 1000;
	m_fcPosRx = FC_POS_CENTER;
	m_log2Decim = 0;
	m_dcBlock = false;
	m_iqCorrection = false;
    m_hwBBDCBlock = true;
    m_hwRFDCBlock = true;
    m_hwIQCorrection = true;
	m_lpfBWRx = 1500000;
	m_lpfRxFIREnable = false;
	m_lpfRxFIRBW = 500000U;
	m_lpfRxFIRlog2Decim = 0;
	m_lpfRxFIRGain = 0;
	m_rxTransverterMode = false;
	m_rxTransverterDeltaFrequency = 0;
    m_iqOrder = true;

	m_rx0Gain = 40;
	m_rx0AntennaPath = RFPATHRX_A_BAL;
	m_rx0GainMode = GAIN_MANUAL;

	m_rx1Gain = 40;
	m_rx1AntennaPath = RFPATHRX_A_BAL;
	m_rx1GainMode = GAIN_MANUAL;

	m_txCenterFrequency = 435000 * 1000;
    m_fcPosTx = FC_POS_CENTER;
	m_log2Interp = 0;
	m_lpfBWTx = 1500000;
	m_lpfTxFIREnable = false;
	m_lpfTxFIRBW = 500000U;
	m_lpfTxFIRlog2Interp = 0;
	m_lpfTxFIRGain = 0;
    m_txTransverterMode = false;
    m_txTransverterDeltaFrequency = 0;

	m_tx0Att = -50;
	m_tx0AntennaPath = RFPATHTX_A;

	m_tx1Att = -50;
	m_tx1AntennaPath = RFPATHTX_A;

    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray PlutoSDRMIMOSettings::serialize() const
{
	SimpleSerializer s(1);

    // Common
    s.writeU64(1,   m_devSampleRate);
    s.writeS32(2,   m_LOppmTenths);

    // Rx
    s.writeU64(10,  m_rxCenterFrequency);
	s.writeS32(11,  m_fcPosRx);
    s.writeU32(12,  m_log2Decim);
	s.writeBool(13, m_dcBlock);
    s.writeBool(14, m_iqCorrection);
    s.writeBool(15, m_hwBBDCBlock);
    s.writeBool(16, m_hwRFDCBlock);
    s.writeBool(17, m_hwIQCorrection);
    s.writeU32(18,  m_lpfBWRx);
    s.writeBool(19, m_lpfRxFIREnable);
    s.writeS32(20,  m_lpfRxFIRGain);
    s.writeU32(21,  m_lpfRxFIRlog2Decim);
    s.writeU32(22,  m_lpfRxFIRBW);
    s.writeBool(23, m_rxTransverterMode);
    s.writeS64(24,  m_rxTransverterDeltaFrequency);
    s.writeBool(25, m_iqOrder);

    // Rx0
    s.writeU32(40, m_rx0Gain);
    s.writeS32(41, (int) m_rx0AntennaPath);
    s.writeS32(42, (int) m_rx0GainMode);

    // Rx1
    s.writeU32(50, m_rx1Gain);
    s.writeS32(51, (int) m_rx1AntennaPath);
    s.writeS32(52, (int) m_rx1GainMode);

    // Tx
    s.writeU64(60,  m_txCenterFrequency);
    s.writeS32(61,  m_fcPosTx);
    s.writeU32(62,  m_log2Interp);
    s.writeU32(63,  m_lpfBWTx);
    s.writeBool(64, m_lpfTxFIREnable);
    s.writeU32(65,  m_lpfTxFIRBW);
    s.writeU32(66,  m_lpfTxFIRlog2Interp);
    s.writeS32(67,  m_lpfTxFIRGain);
    s.writeBool(68, m_txTransverterMode);
    s.writeS64(69,  m_txTransverterDeltaFrequency);

    // Tx0
    s.writeS32(80, m_tx0Att);
    s.writeS32(81, (int) m_tx0AntennaPath);

    // Tx1
    s.writeS32(90, m_tx1Att);
    s.writeS32(91, (int) m_tx1AntennaPath);

    // Reverse API
    s.writeBool(100,   m_useReverseAPI);
    s.writeString(101, m_reverseAPIAddress);
    s.writeU32(102,    m_reverseAPIPort);
    s.writeU32(103,    m_reverseAPIDeviceIndex);

	return s.final();
}

bool PlutoSDRMIMOSettings::deserialize(const QByteArray& data)
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

        // Common
        d.readU64(1, &m_devSampleRate, 2500 * 1000);
        d.readS32(2, &m_LOppmTenths, 0);

        // Rx
        d.readU64(10, &m_rxCenterFrequency, 435000*1000);
		d.readS32(11, &intval, 0);
		if ((intval < 0) || (intval > 2)) {
		    m_fcPosRx = FC_POS_CENTER;
		} else {
	        m_fcPosRx = (fcPos_t) intval;
		}
        d.readU32(12,  &m_log2Decim, 0);
        d.readBool(13, &m_dcBlock, false);
        d.readBool(14, &m_iqCorrection, false);
        d.readBool(15, &m_hwBBDCBlock, true);
        d.readBool(16, &m_hwRFDCBlock, true);
        d.readBool(17, &m_hwIQCorrection, true);
        d.readU32(18,  &m_lpfBWRx, 1500000);
        d.readBool(19, &m_lpfRxFIREnable, false);
        d.readS32(20,  &m_lpfRxFIRGain, 0);
        d.readU32(21,  &uintval, 0);
        if (uintval > 2) {
            m_lpfRxFIRlog2Decim = 2;
        } else {
            m_lpfRxFIRlog2Decim = uintval;
        }
        d.readU32(22,  &m_lpfRxFIRBW, 500000U);
        d.readBool(23, &m_rxTransverterMode, false);
        d.readS64(24,  &m_rxTransverterDeltaFrequency, 0);
        d.readBool(25, &m_iqOrder, true);

        // Rx0
        d.readU32(40, &m_rx0Gain, 40);
        d.readS32(41, &intval, 0);
        if ((intval >= 0) && (intval < (int) RFPATHRX_END)) {
            m_rx0AntennaPath = (RFPathRx) intval;
        } else {
            m_rx0AntennaPath = RFPATHRX_A_BAL;
        }
        d.readS32(42, &intval, 0);
        if ((intval >= 0) && (intval < (int) GAIN_END)) {
            m_rx0GainMode = (GainMode) intval;
        } else {
            m_rx0GainMode = GAIN_MANUAL;
        }

        // Rx1
        d.readU32(50, &m_rx0Gain, 40);
        d.readS32(51, &intval, 0);
        if ((intval >= 0) && (intval < (int) RFPATHRX_END)) {
            m_rx0AntennaPath = (RFPathRx) intval;
        } else {
            m_rx0AntennaPath = RFPATHRX_A_BAL;
        }
        d.readS32(52, &intval, 0);
        if ((intval >= 0) && (intval < (int) GAIN_END)) {
            m_rx0GainMode = (GainMode) intval;
        } else {
            m_rx0GainMode = GAIN_MANUAL;
        }

        // Tx
        d.readU64(60, &m_txCenterFrequency, 435000*1000);
		d.readS32(61, &intval, 0);
		if ((intval < 0) || (intval > 2)) {
		    m_fcPosTx = FC_POS_CENTER;
		} else {
	        m_fcPosTx = (fcPos_t) intval;
		}
        d.readU32(62, &m_log2Interp, 0);
        d.readU32(63, &m_lpfBWTx, 1500000);
        d.readBool(64, &m_lpfTxFIREnable, false);
        d.readU32(65, &m_lpfTxFIRBW, 500000U);
        d.readU32(66, &uintval, 0);
        if (uintval > 2) {
            m_lpfTxFIRlog2Interp = 2;
        } else {
            m_lpfTxFIRlog2Interp = uintval;
        }
        d.readS32(67, &m_lpfTxFIRGain, 0);
        d.readBool(68, &m_txTransverterMode, false);
        d.readS64(69, &m_txTransverterDeltaFrequency, 0);

        // Tx0
        d.readS32(80, &m_tx0Att, -50);
        d.readS32(81, &intval, 0);
        if ((intval >= 0) && (intval < (int) RFPATHTX_END)) {
            m_tx0AntennaPath = (RFPathTx) intval;
        } else {
            m_tx0AntennaPath = RFPATHTX_A;
        }

        // Tx1
        d.readS32(80, &m_tx1Att, -50);
        d.readS32(81, &intval, 0);
        if ((intval >= 0) && (intval < (int) RFPATHTX_END)) {
            m_tx1AntennaPath = (RFPathTx) intval;
        } else {
            m_tx1AntennaPath = RFPATHTX_A;
        }

        // Reverse API
        d.readBool(100, &m_useReverseAPI, false);
        d.readString(101, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(102, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(103, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

		return true;
    }
	else
	{
		resetToDefaults();
		return false;
	}
}

void PlutoSDRMIMOSettings::translateRFPathTx(RFPathTx path, QString& s)
{
    switch(path)
    {
    case RFPATHTX_A:
        s = "A";
        break;
    case RFPATHTX_B:
        s = "B";
        break;
    default:
        s = "A";
        break;
    }
}

void PlutoSDRMIMOSettings::translateRFPathRx(RFPathRx path, QString& s)
{
    switch(path)
    {
    case RFPATHRX_A_BAL:
        s = "A_BALANCED";
        break;
    case RFPATHRX_B_BAL:
        s = "B_BALANCED";
        break;
    case RFPATHRX_C_BAL:
        s = "C_BALANCED";
        break;
    case RFPATHRX_A_NEG:
        s = "A_N";
        break;
    case RFPATHRX_A_POS:
        s = "A_P";
        break;
    case RFPATHRX_B_NEG:
        s = "B_N";
        break;
    case RFPATHRX_B_POS:
        s = "B_P";
        break;
    case RFPATHRX_C_NEG:
        s = "C_N";
        break;
    case RFPATHRX_C_POS:
        s = "C_P";
        break;
    case RFPATHRX_TX1MON:
        s = "TX_MONITOR1";
        break;
    case RFPATHRX_TX2MON:
        s = "TX_MONITOR2";
        break;
    case RFPATHRX_TX3MON:
        s = "TX_MONITOR3";
        break;
    default:
        s = "A_BALANCED";
        break;
    }
}

void PlutoSDRMIMOSettings::translateGainMode(GainMode mode, QString& s)
{
    switch(mode)
    {
    case GAIN_MANUAL:
        s = "manual";
        break;
    case GAIN_AGC_SLOW:
        s = "slow_attack";
        break;
    case GAIN_AGC_FAST:
        s = "fast_attack";
        break;
    case GAIN_HYBRID:
        s = "hybrid";
        break;
    default:
        s = "manual";
        break;
    }
}
