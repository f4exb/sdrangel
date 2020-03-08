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

#ifndef SDRBASE_LIMERFE_LIMERFECONTROLLER_H_
#define SDRBASE_LIMERFE_LIMERFECONTROLLER_H_

#include <string>
#include <map>
#include "lime/limeRFE.h"
#include "export.h"

class SDRBASE_API LimeRFEController
{
public:
    enum ChannelGroups
    {
        ChannelsWideband,
        ChannelsHAM,
        ChannelsCellular
    };

    enum WidebandChannel
    {
        WidebandLow, //!< 1 - 1000 MHz
        WidebandHigh //!< 1000 - 4000 MHz
    };

    enum HAMChannel
    {
        HAM_30M,
        HAM_50_70MHz,
        HAM_144_146MHz,
        HAM_220_225MHz,
        HAM_430_440MHz,
        HAM_902_928MHz,
        HAM_1240_1325MHz,
        HAM_2300_2450MHz,
        HAM_3300_3500MHz
    };

    enum CellularChannel
    {
        CellularBand1,
        CellularBand2,
        CellularBand3,
        CellularBand7,
        CellularBand38
    };

    enum RxPort
    {
        RxPortJ3, //!< Rx/Tx
        RxPortJ5  //!< Rx/Tx HF
    };

    enum TxPort
    {
        TxPortJ3, //!< Rx/Tx
        TxPortJ4, //!< Tx
        TxPortJ5  //!< Rx/Tx HF
    };

    enum SWRSource
    {
        SWRExternal,
        SWRCellular
    };

    struct SDRBASE_API LimeRFESettings
    {
        LimeRFESettings();
        // Rx
        LimeRFEController::ChannelGroups m_rxChannels;
        LimeRFEController::WidebandChannel m_rxWidebandChannel;
        LimeRFEController::HAMChannel m_rxHAMChannel;
        LimeRFEController::CellularChannel m_rxCellularChannel;
        LimeRFEController::RxPort m_rxPort;
        unsigned int m_attenuationFactor; //!< Attenuation is 2 times this factor in dB (0..7 => 0..14dB)
        bool m_amfmNotch;
        // Tx
        LimeRFEController::ChannelGroups m_txChannels;
        LimeRFEController::WidebandChannel m_txWidebandChannel;
        LimeRFEController::HAMChannel m_txHAMChannel;
        LimeRFEController::CellularChannel m_txCellularChannel;
        LimeRFEController::TxPort m_txPort;
        bool m_swrEnable;
        LimeRFEController::SWRSource m_swrSource;
        // Rx/Tx
        bool m_txRxDriven; //!< Tx settings set according to Rx settings
        bool m_rxOn;
        bool m_txOn;
    };

    LimeRFEController();
    ~LimeRFEController();

    int openDevice(const std::string& serialDeviceName);
    void closeDevice();
    int configure();
    int getState();
    static std::string getError(int errorCode);
    int setRx(LimeRFESettings& settings, bool rxOn);
    int setTx(LimeRFESettings& settings, bool txOn);
    int getFwdPower(int& powerDB);
    int getRefPower(int& powerDB);

    void settingsToState(const LimeRFESettings& settings);
    void stateToSettings(LimeRFESettings& settings);

private:
    rfe_dev_t *m_rfeDevice;
    rfe_boardState m_rfeBoardState;
    static const std::map<int, std::string> m_errorCodesMap;
};

#endif // SDRBASE_LIMERFE_LIMERFECONTROLLER_H_
