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

#ifndef _PLUTOSDR_PLUTOSDRMIMOSETTINGS_H_
#define _PLUTOSDR_PLUTOSDRMIMOSETTINGS_H_

#include <QtGlobal>
#include <QString>
#include <stdint.h>

struct PlutoSDRMIMOSettings {
	typedef enum {
		FC_POS_INFRA = 0,
		FC_POS_SUPRA,
		FC_POS_CENTER,
		FC_POS_END
	} fcPos_t;

    enum RFPathRx
    {
        RFPATHRX_A_BAL = 0,
        RFPATHRX_B_BAL,
        RFPATHRX_C_BAL,
        RFPATHRX_A_NEG,
        RFPATHRX_A_POS,
        RFPATHRX_B_NEG,
        RFPATHRX_B_POS,
        RFPATHRX_C_NEG,
        RFPATHRX_C_POS,
        RFPATHRX_TX1MON,
        RFPATHRX_TX2MON,
        RFPATHRX_TX3MON,
        RFPATHRX_END
    };

    enum RFPathTx
    {
        RFPATHTX_A = 0,
        RFPATHTX_B,
        RFPATHTX_END
    };

    typedef enum {
        GAIN_MANUAL,
        GAIN_AGC_SLOW,
        GAIN_AGC_FAST,
        GAIN_HYBRID,
        GAIN_END
    } GainMode;

    // Common
    quint64 m_devSampleRate;      //!< Host interface sample rate
    qint32  m_LOppmTenths;        //!< XO correction

    // Common Rx
	quint64 m_rxCenterFrequency;
    bool    m_dcBlock;
    bool    m_iqCorrection;
    bool    m_hwBBDCBlock;     //!< Hardware baseband DC blocking
    bool    m_hwRFDCBlock;     //!< Hardware RF DC blocking
    bool    m_hwIQCorrection;  //!< Hardware IQ correction
    fcPos_t m_fcPosRx;
    bool    m_rxTransverterMode;
    qint64  m_rxTransverterDeltaFrequency;
    bool    m_iqOrder;
    quint32 m_lpfBWRx;              //!< analog lowpass filter bandwidth (Hz)
    bool    m_lpfRxFIREnable;       //!< enable digital lowpass FIR filter
    quint32 m_lpfRxFIRBW;           //!< digital lowpass FIR filter bandwidth (Hz)
    quint32 m_lpfRxFIRlog2Decim;    //!< digital lowpass FIR filter log2 of decimation factor (0..2)
    int     m_lpfRxFIRGain;         //!< digital lowpass FIR filter gain (dB)
    quint32 m_log2Decim;

    // Rx0
    quint32  m_rx0Gain;              //!< "hardware" gain
    GainMode m_rx0GainMode;
    RFPathRx m_rx0AntennaPath;

    // Rx1
    quint32  m_rx1Gain;              //!< "hardware" gain
    GainMode m_rx1GainMode;
    RFPathRx m_rx1AntennaPath;

    // Common Tx
    quint64 m_txCenterFrequency;
    fcPos_t m_fcPosTx;
    bool    m_txTransverterMode;
    qint64  m_txTransverterDeltaFrequency;
    quint32 m_lpfBWTx;              //!< analog lowpass filter bandwidth (Hz)
    bool    m_lpfTxFIREnable;       //!< enable digital lowpass FIR filter
    quint32 m_lpfTxFIRBW;           //!< digital lowpass FIR filter bandwidth (Hz)
    quint32 m_lpfTxFIRlog2Interp;   //!< digital lowpass FIR filter log2 of interpolation factor (0..2)
    int     m_lpfTxFIRGain;         //!< digital lowpass FIR filter gain (dB)
    quint32 m_log2Interp;

    // Tx0
    qint32   m_tx0Att;                //!< "hardware" attenuation in dB fourths
    RFPathTx m_tx0AntennaPath;

    // Tx1
    qint32   m_tx1Att;                //!< "hardware" attenuation in dB fourths
    RFPathTx m_tx1AntennaPath;

    // global settings to be saved
	// common device settings
    // individual channel settings
    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

    static const int m_plutoSDRBlockSizeSamples = 64*256; //complex samples per buffer (must be multiple of 64)

	PlutoSDRMIMOSettings();
	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const PlutoSDRMIMOSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
    static void translateRFPathRx(RFPathRx path, QString& s);
    static void translateGainMode(GainMode mod, QString& s);
    static void translateRFPathTx(RFPathTx path, QString& s);
};

#endif /* _PLUTOSDR_PLUTOSDRMIMOSETTINGS_H_ */
