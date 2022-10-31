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

#ifndef PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMIMOSETTINGS_H_
#define PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMIMOSETTINGS_H_

#include <QtGlobal>
#include <QString>

struct LimeSDRMIMOSettings
{
    typedef enum {
        FC_POS_INFRA = 0,
        FC_POS_SUPRA,
        FC_POS_CENTER
    } fcPos_t;

    enum PathRxRFE
    {
        PATH_RFE_RX_NONE = 0,
        PATH_RFE_LNAH,
        PATH_RFE_LNAL,
        PATH_RFE_LNAW,
        PATH_RFE_LB1,
        PATH_RFE_LB2
    };

    typedef enum {
        GAIN_AUTO,
        GAIN_MANUAL
    } RxGainMode;

    enum PathTxRFE
    {
        PATH_RFE_TX_NONE = 0,
        PATH_RFE_TXRF1,
        PATH_RFE_TXRF2,
    };

    // General
    qint32    m_devSampleRate;
    uint8_t   m_gpioDir;          //!< GPIO pin direction LSB first; 0 input, 1 output
    uint8_t   m_gpioPins;         //!< GPIO pins to write; LSB first
    bool      m_extClock;         //!< True if external clock source
    uint32_t  m_extClockFreq;     //!< Frequency (Hz) of external clock source
    bool      m_useReverseAPI;
    QString   m_reverseAPIAddress;
    uint16_t  m_reverseAPIPort;
    uint16_t  m_reverseAPIDeviceIndex;

    // Rx general
    quint64   m_rxCenterFrequency;
    uint32_t  m_log2HardDecim;
    uint32_t  m_log2SoftDecim;
    bool      m_dcBlock;
    bool      m_iqCorrection;
    bool      m_rxTransverterMode;
    qint64    m_rxTransverterDeltaFrequency;
    bool      m_iqOrder;
    bool       m_ncoEnableRx;     //!< Rx Enable TSP NCO and mixing
    int        m_ncoFrequencyRx;  //!< Rx Actual NCO frequency (the resulting frequency with mixing is displayed)

    // Rx channel 0
    float      m_lpfBWRx0;        //!< Rx[0] LMS analog lowpass filter bandwidth (Hz)
    bool       m_lpfFIREnableRx0; //!< Rx[0] Enable LMS digital lowpass FIR filters
    float      m_lpfFIRBWRx0;     //!< Rx[0] LMS digital lowpass FIR filters bandwidth (Hz)
    uint32_t   m_gainRx0;         //!< Rx[0] Optimally distributed gain (dB)
    PathRxRFE  m_antennaPathRx0;  //!< Rx[0] Antenna connection
    RxGainMode m_gainModeRx0;     //!< Rx[0] Gain mode: auto or manual
    uint32_t   m_lnaGainRx0;      //!< Rx[0] Manual LAN gain
    uint32_t   m_tiaGainRx0;      //!< Rx[0] Manual TIA gain
    uint32_t   m_pgaGainRx0;      //!< Rx[0] Manual PGA gain

    // Rx channel 1
    float      m_lpfBWRx1;        //!< Rx[1] LMS analog lowpass filter bandwidth (Hz)
    bool       m_lpfFIREnableRx1; //!< Rx[1] Enable LMS digital lowpass FIR filters
    float      m_lpfFIRBWRx1;     //!< Rx[1] LMS digital lowpass FIR filters bandwidth (Hz)
    uint32_t   m_gainRx1;         //!< Rx[1] Optimally distributed gain (dB)
    PathRxRFE  m_antennaPathRx1;  //!< Rx[1] Antenna connection
    RxGainMode m_gainModeRx1;     //!< Rx[1] Gain mode: auto or manual
    uint32_t   m_lnaGainRx1;      //!< Rx[1] Manual LAN gain
    uint32_t   m_tiaGainRx1;      //!< Rx[1] Manual TIA gain
    uint32_t   m_pgaGainRx1;      //!< Rx[1] Manual PGA gain

    // Tx general
    quint64   m_txCenterFrequency;
    uint32_t  m_log2HardInterp;
    uint32_t  m_log2SoftInterp;
    bool      m_txTransverterMode;
    qint64    m_txTransverterDeltaFrequency;
    bool      m_ncoEnableTx;     //!< Tx Enable TSP NCO and mixing
    int       m_ncoFrequencyTx;  //!< Tx Actual NCO frequency (the resulting frequency with mixing is displayed)

    // Tx channel 0
    float     m_lpfBWTx0;        //!< Tx[0] LMS amalog lowpass filter bandwidth (Hz)
    bool      m_lpfFIREnableTx0; //!< Tx[0] Enable LMS digital lowpass FIR filters
    float     m_lpfFIRBWTx0;     //!< Tx[0] LMS digital lowpass FIR filters bandwidth (Hz)
    uint32_t  m_gainTx0;         //!< Tx[0] Optimally distributed gain (dB)
    PathTxRFE m_antennaPathTx0;  //!< Tx[0] Antenna connection

    // Tx channel 1
    float     m_lpfBWTx1;        //!< Tx[1] LMS amalog lowpass filter bandwidth (Hz)
    bool      m_lpfFIREnableTx1; //!< Tx[1] Enable LMS digital lowpass FIR filters
    float     m_lpfFIRBWTx1;     //!< Tx[1] LMS digital lowpass FIR filters bandwidth (Hz)
    uint32_t  m_gainTx1;         //!< Tx[1] Optimally distributed gain (dB)
    PathTxRFE m_antennaPathTx1;  //!< Tx[1] Antenna connection

    LimeSDRMIMOSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const LimeSDRMIMOSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif // PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMIMOSETTINGS_H_
