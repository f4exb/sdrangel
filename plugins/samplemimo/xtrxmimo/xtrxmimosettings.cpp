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
