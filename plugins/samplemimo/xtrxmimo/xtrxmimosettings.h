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

#ifndef PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMIMOSETTINGS_H_
#define PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMIMOSETTINGS_H_

#include <stdint.h>

#include <QtGlobal>
#include <QString>

struct XTRXMIMOSettings
{
    typedef enum {
        GAIN_AUTO,
        GAIN_MANUAL
    } GainMode;

    typedef enum {
        RXANT_LO,
        RXANT_WI,
        RXANT_HI
    } RxAntenna;

    typedef enum {
        TXANT_HI,
        TXANT_WI
    } TxAntenna;

    // common
    bool     m_extClock;     //!< True if external clock source
    uint32_t m_extClockFreq; //!< Frequency (Hz) of external clock source
    uint8_t  m_gpioDir;      //!< GPIO pin direction LSB first; 0 input, 1 output
    uint8_t  m_gpioPins;     //!< GPIO pins to write; LSB first
    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    // Rx
    double   m_rxDevSampleRate;
    uint32_t m_log2HardDecim;
    uint32_t m_log2SoftDecim;
    uint64_t m_rxCenterFrequency;
    bool     m_dcBlock;
    bool     m_iqCorrection;
    bool     m_ncoEnableRx;     //!< Enable TSP NCO and mixing
    int      m_ncoFrequencyRx;  //!< Actual NCO frequency (the resulting frequency with mixing is displayed)
    RxAntenna m_antennaPathRx;
    bool     m_iqOrder;
    // Rx0
    float    m_lpfBWRx0;        //!< LMS analog lowpass filter bandwidth (Hz)
    uint32_t m_gainRx0;         //!< Optimally distributed gain (dB)
    GainMode m_gainModeRx0;     //!< Gain mode: auto or manual
    uint32_t m_lnaGainRx0;      //!< Manual LNA gain
    uint32_t m_tiaGainRx0;      //!< Manual TIA gain
    uint32_t m_pgaGainRx0;      //!< Manual PGA gain
    uint32_t m_pwrmodeRx0;
    // Rx1
    float    m_lpfBWRx1;        //!< LMS analog lowpass filter bandwidth (Hz)
    uint32_t m_gainRx1;         //!< Optimally distributed gain (dB)
    GainMode m_gainModeRx1;     //!< Gain mode: auto or manual
    uint32_t m_lnaGainRx1;      //!< Manual LNA gain
    uint32_t m_tiaGainRx1;      //!< Manual TIA gain
    uint32_t m_pgaGainRx1;      //!< Manual PGA gain
    uint32_t m_pwrmodeRx1;
    // Tx
    double   m_txDevSampleRate;
    uint32_t m_log2HardInterp;
    uint32_t m_log2SoftInterp;
    uint64_t m_txCenterFrequency;
    bool     m_ncoEnableTx;     //!< Enable TSP NCO and mixing
    int      m_ncoFrequencyTx;  //!< Actual NCO frequency (the resulting frequency with mixing is displayed)
    TxAntenna m_antennaPathTx;
    // Tx0
    float    m_lpfBWTx0;        //!< LMS analog lowpass filter bandwidth (Hz)
    uint32_t m_gainTx0;         //!< Optimally distributed gain (dB)
    uint32_t m_pwrmodeTx0;
    // Tx1
    float    m_lpfBWTx1;        //!< LMS analog lowpass filter bandwidth (Hz)
    uint32_t m_gainTx1;         //!< Optimally distributed gain (dB)
    uint32_t m_pwrmodeTx1;

    XTRXMIMOSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const XTRXMIMOSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};


#endif // PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMIMOSETTINGS_H_
