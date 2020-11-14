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

#include <QDebug>

#include "limerfecontroller.h"

const std::map<int, std::string> LimeRFEController::m_errorCodesMap = {
    { 0, "OK"},
    {-4, "Error synchronizing communication"},
    {-3, "Non-configurable GPIO pin specified. Only pins 4 and 5 are configurable."},
    {-2, "Problem with .ini configuration file"},
    {-1, "Communication error"},
    { 1, "Wrong TX connector - not possible to route TX of the selecrted channel to the specified port"},
    { 2, "Wrong RX connector - not possible to route RX of the selecrted channel to the specified port"},
    { 3, "Mode TXRX not allowed - when the same port is selected for RX and TX, it is not allowed to use mode RX & TX"},
    { 4, "Wrong mode for cellular channel - Cellular FDD bands (1, 2, 3, and 7) are only allowed mode RX & TX, while TDD band 38 is allowed only RX or TX mode"},
    { 5, "Cellular channels must be the same both for RX and TX"},
    { 6, "Requested channel code is wrong"}
};

LimeRFEController::LimeRFESettings::LimeRFESettings()
{
    m_rxChannels = ChannelsWideband;
    m_rxWidebandChannel = WidebandLow;
    m_rxHAMChannel = HAM_144_146MHz;
    m_rxCellularChannel = CellularBand38;
    m_rxPort = RxPortJ3;
    m_amfmNotch = false;
    m_attenuationFactor = 0;
    m_txChannels = ChannelsWideband;
    m_txWidebandChannel = WidebandLow;
    m_txHAMChannel = HAM_144_146MHz;
    m_txCellularChannel = CellularBand38;
    m_txPort = TxPortJ3;
    m_swrEnable = false;
    m_swrSource = SWRExternal;
    m_txRxDriven = false;
    m_rxOn = false;
    m_txOn = false;
}

LimeRFEController::LimeRFEController() :
    m_rfeDevice(nullptr)
{}

LimeRFEController::~LimeRFEController()
{
    closeDevice();
}

int LimeRFEController::openDevice(const std::string& serialDeviceName)
{
    closeDevice();

    rfe_dev_t *rfeDevice = RFE_Open(serialDeviceName.c_str(), nullptr);

    if (rfeDevice != (void *) -1) {
        m_rfeDevice = rfeDevice;
        return 0;
    }
    else
    {
        return -1;
    }
}

void LimeRFEController::closeDevice()
{
    if (m_rfeDevice)
    {
        RFE_Close(m_rfeDevice);
        m_rfeDevice = nullptr;
    }
}

int LimeRFEController::configure()
{
    if (!m_rfeDevice) {
        return -1;
    }

    qDebug() << "LimeRFEController::configure: "
        << "attValue: " << (int) m_rfeBoardState.attValue
        << "channelIDRX: " << (int) m_rfeBoardState.channelIDRX
        << "channelIDTX: " << (int) m_rfeBoardState.channelIDTX
        << "mode: " << (int) m_rfeBoardState.mode
        << "notchOnOff: " << (int) m_rfeBoardState.notchOnOff
        << "selPortRX: " << (int) m_rfeBoardState.selPortRX
        << "selPortTX: " << (int) m_rfeBoardState.selPortTX
        << "enableSWR: " << (int) m_rfeBoardState.enableSWR
        << "sourceSWR: " << (int) m_rfeBoardState.sourceSWR;

    int rc = RFE_ConfigureState(m_rfeDevice, m_rfeBoardState);

    if (rc != 0) {
        qInfo("LimeRFEController::configure: %s", getError(rc).c_str());
    } else {
        qDebug() << "LimeRFEController::configure: done";
    }

    return rc;
}

int LimeRFEController::getState()
{
    if (!m_rfeDevice) {
        return -1;
    }

    int rc = RFE_GetState(m_rfeDevice, &m_rfeBoardState);

    qDebug() << "LimeRFEController::getState: "
        << "attValue: " << (int) m_rfeBoardState.attValue
        << "channelIDRX: " << (int) m_rfeBoardState.channelIDRX
        << "channelIDTX: " << (int) m_rfeBoardState.channelIDTX
        << "mode: " << (int) m_rfeBoardState.mode
        << "notchOnOff: " << (int) m_rfeBoardState.notchOnOff
        << "selPortRX: " << (int) m_rfeBoardState.selPortRX
        << "selPortTX: " << (int) m_rfeBoardState.selPortTX
        << "enableSWR: " << (int) m_rfeBoardState.enableSWR
        << "sourceSWR: " << (int) m_rfeBoardState.sourceSWR;

    if (rc != 0) {
        qInfo("LimeRFEController::getState: %s", getError(rc).c_str());
    }

    return rc;
}

std::string LimeRFEController::getError(int errorCode)
{
    std::map<int, std::string>::const_iterator it = m_errorCodesMap.find(errorCode);

    if (it == m_errorCodesMap.end()) {
        return "Unknown error";
    } else {
        return it->second;
    }
}

int LimeRFEController::setRx(LimeRFESettings& settings, bool rxOn)
{
    if (!m_rfeDevice) {
        return -1;
    }

    int mode = rxOn && settings.m_txOn ?
        RFE_MODE_TXRX : rxOn ?
            RFE_MODE_RX : settings.m_txOn ?
                RFE_MODE_TX :  RFE_MODE_NONE;

    int rc = RFE_Mode(m_rfeDevice, mode);

    if (rc == 0) {
        settings.m_rxOn = rxOn;
    }

    return rc;
}

int LimeRFEController::setTx(LimeRFESettings& settings, bool txOn)
{
    if (!m_rfeDevice) {
        return -1;
    }

    int mode = txOn && settings.m_rxOn ?
        RFE_MODE_TXRX : txOn ?
            RFE_MODE_TX : settings.m_rxOn ?
                RFE_MODE_RX :  RFE_MODE_NONE;

    int rc = RFE_Mode(m_rfeDevice, mode);

    if (rc == 0) {
        settings.m_txOn = txOn;
    }

    return rc;
}

int LimeRFEController::getFwdPower(int& powerDB)
{
    if (!m_rfeDevice) {
        return -1;
    }

    int power;
    int rc = RFE_ReadADC(m_rfeDevice, RFE_ADC1, &power);

    if (rc == 0) {
        powerDB = power;
    }

    return rc;
}

int LimeRFEController::getRefPower(int& powerDB)
{
    if (!m_rfeDevice) {
        return -1;
    }

    int power;
    int rc = RFE_ReadADC(m_rfeDevice, RFE_ADC2, &power);

    if (rc == 0) {
        powerDB = power;
    }

    return rc;
}

void LimeRFEController::settingsToState(const LimeRFESettings& settings)
{
    if (settings.m_rxChannels == ChannelsCellular)
    {
        if (settings.m_rxCellularChannel == CellularBand1)
        {
            m_rfeBoardState.channelIDRX = RFE_CID_CELL_BAND01;
            m_rfeBoardState.mode = RFE_MODE_TXRX;
        }
        else if (settings.m_rxCellularChannel == CellularBand2)
        {
            m_rfeBoardState.channelIDRX = RFE_CID_CELL_BAND02;
            m_rfeBoardState.mode = RFE_MODE_TXRX;
        }
        else if (settings.m_rxCellularChannel == CellularBand3)
        {
            m_rfeBoardState.channelIDRX = RFE_CID_CELL_BAND03;
            m_rfeBoardState.mode = RFE_MODE_TXRX;
        }
        else if (settings.m_rxCellularChannel == CellularBand38)
        {
            m_rfeBoardState.channelIDRX = RFE_CID_CELL_BAND38;
        }
        else if (settings.m_rxCellularChannel == CellularBand7)
        {
            m_rfeBoardState.channelIDRX = RFE_CID_CELL_BAND07;
            m_rfeBoardState.mode = RFE_MODE_TXRX;
        }

        m_rfeBoardState.selPortRX = RFE_PORT_1;
        m_rfeBoardState.selPortTX = RFE_PORT_1;
        m_rfeBoardState.channelIDTX = m_rfeBoardState.channelIDRX;
    }
    else
    {
        m_rfeBoardState.mode = settings.m_rxOn && settings.m_txOn ?
            RFE_MODE_TXRX : settings.m_rxOn ?
                RFE_MODE_RX : settings.m_txOn ?
                    RFE_MODE_TX :  RFE_MODE_NONE;

        if (settings.m_rxChannels == ChannelsWideband)
        {
            if (settings.m_rxWidebandChannel == WidebandLow) {
                m_rfeBoardState.channelIDRX = RFE_CID_WB_1000;
            } else if (settings.m_rxWidebandChannel == WidebandHigh) {
                m_rfeBoardState.channelIDRX = RFE_CID_WB_4000;
            }
        }
        else if (settings.m_rxChannels == ChannelsHAM)
        {
            if (settings.m_rxHAMChannel == HAM_30M) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_0030;
            } else if (settings.m_rxHAMChannel == HAM_50_70MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_0070;
            } else if (settings.m_rxHAMChannel == HAM_144_146MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_0145;
            } else if (settings.m_rxHAMChannel == HAM_220_225MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_0220;
            } else if (settings.m_rxHAMChannel == HAM_430_440MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_0435;
            } else if (settings.m_rxHAMChannel == HAM_902_928MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_0920;
            } else if (settings.m_rxHAMChannel == HAM_1240_1325MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_1280;
            } else if (settings.m_rxHAMChannel == HAM_2300_2450MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_2400;
            } else if (settings.m_rxHAMChannel == HAM_3300_3500MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_3500;
            }
        }

        if (settings.m_rxPort == RxPortJ3) {
            m_rfeBoardState.selPortRX = RFE_PORT_1;
        } else if (settings.m_rxPort == RxPortJ5) {
            m_rfeBoardState.selPortRX = RFE_PORT_3;
        }

        if (settings.m_txRxDriven)
        {
            m_rfeBoardState.channelIDTX = m_rfeBoardState.channelIDRX;
        }
        else
        {
            if (settings.m_txChannels == ChannelsWideband)
            {
                if (settings.m_txWidebandChannel == WidebandLow) {
                    m_rfeBoardState.channelIDTX = RFE_CID_WB_1000;
                } else if (settings.m_txWidebandChannel == WidebandHigh) {
                    m_rfeBoardState.channelIDTX = RFE_CID_WB_4000;
                }
            }
            else if (settings.m_txChannels == ChannelsHAM)
            {
                if (settings.m_txHAMChannel == HAM_30M) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_0030;
                } else if (settings.m_txHAMChannel == HAM_50_70MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_0070;
                } else if (settings.m_txHAMChannel == HAM_144_146MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_0145;
                } else if (settings.m_txHAMChannel == HAM_220_225MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_0220;
                } else if (settings.m_txHAMChannel == HAM_430_440MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_0435;
                } else if (settings.m_txHAMChannel == HAM_902_928MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_0920;
                } else if (settings.m_txHAMChannel == HAM_1240_1325MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_1280;
                } else if (settings.m_txHAMChannel == HAM_2300_2450MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_2400;
                } else if (settings.m_txHAMChannel == HAM_3300_3500MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_3500;
                }
            }
        }

        if (settings.m_txPort == TxPortJ3) {
            m_rfeBoardState.selPortTX = RFE_PORT_1;
        } else if (settings.m_txPort == TxPortJ4) {
            m_rfeBoardState.selPortTX = RFE_PORT_2;
        } else if (settings.m_txPort == TxPortJ5) {
            m_rfeBoardState.selPortTX = RFE_PORT_3;
        }
    }

    m_rfeBoardState.attValue = settings.m_attenuationFactor > 7 ? 7 : settings.m_attenuationFactor;
    m_rfeBoardState.notchOnOff = settings.m_amfmNotch;
    m_rfeBoardState.enableSWR = settings.m_swrEnable ? RFE_SWR_ENABLE : RFE_SWR_DISABLE;

    if (settings.m_swrSource == SWRExternal) {
        m_rfeBoardState.sourceSWR = RFE_SWR_SRC_EXT;
    } else if (settings.m_swrSource == SWRCellular) {
        m_rfeBoardState.sourceSWR = RFE_SWR_SRC_CELL;
    }
}

void LimeRFEController::stateToSettings(LimeRFESettings& settings)
{
    if (m_rfeBoardState.channelIDRX == RFE_CID_CELL_BAND01)
    {
        settings.m_rxChannels = ChannelsCellular;
        settings.m_rxCellularChannel = CellularBand1;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_CELL_BAND02)
    {
        settings.m_rxChannels = ChannelsCellular;
        settings.m_rxCellularChannel = CellularBand2;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_CELL_BAND03)
    {
        settings.m_rxChannels = ChannelsCellular;
        settings.m_rxCellularChannel = CellularBand3;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_CELL_BAND07)
    {
        settings.m_rxChannels = ChannelsCellular;
        settings.m_rxCellularChannel = CellularBand7;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_CELL_BAND38)
    {
        settings.m_rxChannels = ChannelsCellular;
        settings.m_rxCellularChannel = CellularBand38;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_WB_1000)
    {
        settings.m_rxChannels = ChannelsWideband;
        settings.m_rxWidebandChannel = WidebandLow;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_WB_4000)
    {
        settings.m_rxChannels = ChannelsWideband;
        settings.m_rxWidebandChannel = WidebandHigh;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_0030)
    {
        settings.m_rxChannels = ChannelsHAM;
        settings.m_rxHAMChannel = HAM_30M;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_0070)
    {
        settings.m_rxChannels = ChannelsHAM;
        settings.m_rxHAMChannel = HAM_50_70MHz;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_0145)
    {
        settings.m_rxChannels = ChannelsHAM;
        settings.m_rxHAMChannel = HAM_144_146MHz;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_0220)
    {
        settings.m_rxChannels = ChannelsHAM;
        settings.m_rxHAMChannel = HAM_220_225MHz;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_0435)
    {
        settings.m_rxChannels = ChannelsHAM;
        settings.m_rxHAMChannel = HAM_430_440MHz;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_0920)
    {
        settings.m_rxChannels = ChannelsHAM;
        settings.m_rxHAMChannel = HAM_902_928MHz;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_1280)
    {
        settings.m_rxChannels = ChannelsHAM;
        settings.m_rxHAMChannel = HAM_1240_1325MHz;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_2400)
    {
        settings.m_rxChannels = ChannelsHAM;
        settings.m_rxHAMChannel = HAM_2300_2450MHz;
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_3500)
    {
        settings.m_rxChannels = ChannelsHAM;
        settings.m_rxHAMChannel = HAM_3300_3500MHz;
    }

    if (m_rfeBoardState.selPortRX == RFE_PORT_1) {
        settings.m_rxPort = RxPortJ3;
    } else if (m_rfeBoardState.selPortRX == RFE_PORT_3) {
        settings.m_rxPort = RxPortJ5;
    }

    if (m_rfeBoardState.channelIDTX == RFE_CID_CELL_BAND01)
    {
        settings.m_txChannels = ChannelsCellular;
        settings.m_txCellularChannel = CellularBand1;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_CELL_BAND02)
    {
        settings.m_txChannels = ChannelsCellular;
        settings.m_txCellularChannel = CellularBand2;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_CELL_BAND03)
    {
        settings.m_txChannels = ChannelsCellular;
        settings.m_txCellularChannel = CellularBand3;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_CELL_BAND07)
    {
        settings.m_txChannels = ChannelsCellular;
        settings.m_txCellularChannel = CellularBand7;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_CELL_BAND38)
    {
        settings.m_txChannels = ChannelsCellular;
        settings.m_txCellularChannel = CellularBand38;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_WB_1000)
    {
        settings.m_txChannels = ChannelsWideband;
        settings.m_txWidebandChannel = WidebandLow;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_WB_4000)
    {
        settings.m_txChannels = ChannelsWideband;
        settings.m_txWidebandChannel = WidebandHigh;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_0030)
    {
        settings.m_txChannels = ChannelsHAM;
        settings.m_txHAMChannel = HAM_30M;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_0070)
    {
        settings.m_txChannels = ChannelsHAM;
        settings.m_txHAMChannel = HAM_50_70MHz;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_0145)
    {
        settings.m_txChannels = ChannelsHAM;
        settings.m_txHAMChannel = HAM_144_146MHz;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_0220)
    {
        settings.m_txChannels = ChannelsHAM;
        settings.m_txHAMChannel = HAM_220_225MHz;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_0435)
    {
        settings.m_txChannels = ChannelsHAM;
        settings.m_txHAMChannel = HAM_430_440MHz;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_0920)
    {
        settings.m_txChannels = ChannelsHAM;
        settings.m_txHAMChannel = HAM_902_928MHz;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_1280)
    {
        settings.m_txChannels = ChannelsHAM;
        settings.m_txHAMChannel = HAM_1240_1325MHz;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_2400)
    {
        settings.m_txChannels = ChannelsHAM;
        settings.m_txHAMChannel = HAM_2300_2450MHz;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_3500)
    {
        settings.m_txChannels = ChannelsHAM;
        settings.m_txHAMChannel = HAM_3300_3500MHz;
    }

    if (m_rfeBoardState.selPortTX == RFE_PORT_1) {
        settings.m_txPort = TxPortJ3;
    } else if (m_rfeBoardState.selPortTX == RFE_PORT_2) {
        settings.m_txPort = TxPortJ4;
    } else if (m_rfeBoardState.selPortTX == RFE_PORT_3) {
        settings.m_txPort = TxPortJ5;
    }

    settings.m_attenuationFactor = m_rfeBoardState.attValue;
    settings.m_amfmNotch =  m_rfeBoardState.notchOnOff == RFE_NOTCH_ON;

    if (m_rfeBoardState.mode == RFE_MODE_RX)
    {
        settings.m_rxOn = true;
        settings.m_txOn = false;
    }
    else if (m_rfeBoardState.mode == RFE_MODE_TX)
    {
        settings.m_rxOn = false;
        settings.m_txOn = true;
    }
    else if (m_rfeBoardState.mode == RFE_MODE_NONE)
    {
        settings.m_rxOn = false;
        settings.m_txOn = false;
    }
    else if (m_rfeBoardState.mode == RFE_MODE_TXRX)
    {
        settings.m_rxOn = true;
        settings.m_txOn = true;
    }

    settings.m_swrEnable = m_rfeBoardState.enableSWR == RFE_SWR_ENABLE;
    settings.m_swrSource = m_rfeBoardState.sourceSWR == RFE_SWR_SRC_CELL ? SWRCellular : SWRExternal;
}
