///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_LIMERFESETTINGS_H_
#define INCLUDE_FEATURE_LIMERFESETTINGS_H_

#include <QByteArray>
#include <QString>

#include "limerfeusbcalib.h"

class Serializable;

struct LimeRFESettings
{
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

    // Rx
    ChannelGroups m_rxChannels;
    WidebandChannel m_rxWidebandChannel;
    HAMChannel m_rxHAMChannel;
    CellularChannel m_rxCellularChannel;
    RxPort m_rxPort;
    unsigned int m_attenuationFactor; //!< Attenuation is 2 times this factor in dB (0..7 => 0..14dB)
    bool m_amfmNotch;
    // Tx
    ChannelGroups m_txChannels;
    WidebandChannel m_txWidebandChannel;
    HAMChannel m_txHAMChannel;
    CellularChannel m_txCellularChannel;
    TxPort m_txPort;
    bool m_swrEnable;
    SWRSource m_swrSource;
    // Rx/Tx
    bool m_txRxDriven; //!< Tx settings set according to Rx settings
    // Common
    QString m_devicePath;
    QString m_title;
    quint32 m_rgbColor;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    LimeRFEUSBCalib m_calib;

    LimeRFESettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void applySettings(const QStringList& settingsKeys, const LimeRFESettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif
