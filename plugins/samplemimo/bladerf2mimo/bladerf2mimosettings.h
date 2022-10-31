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

#ifndef PLUGINS_SAMPLEMIMO_BLADERF2MIMO_BLADERF2MIMOSETTINGS_H_
#define PLUGINS_SAMPLEMIMO_BLADERF2MIMO_BLADERF2MIMOSETTINGS_H_

#include <QtGlobal>
#include <QString>

struct BladeRF2MIMOSettings {
    typedef enum {
        FC_POS_INFRA = 0,
        FC_POS_SUPRA,
        FC_POS_CENTER
    } fcPos_t;

    qint32   m_devSampleRate;
    qint32   m_LOppmTenths;

    quint64  m_rxCenterFrequency;
    quint32  m_log2Decim;
    fcPos_t  m_fcPosRx;
    qint32   m_rxBandwidth;
    int      m_rx0GainMode;
    int      m_rx0GlobalGain;
    int      m_rx1GainMode;
    int      m_rx1GlobalGain;
    bool     m_rxBiasTee;
    bool     m_dcBlock;
    bool     m_iqCorrection;
    bool     m_rxTransverterMode;
    qint64   m_rxTransverterDeltaFrequency;
    bool     m_iqOrder; //!< true: IQ - false: QI

    quint64  m_txCenterFrequency;
    quint32  m_log2Interp;
    fcPos_t  m_fcPosTx;
    qint32   m_txBandwidth;
    int      m_tx0GlobalGain;
    int      m_tx1GlobalGain;
    bool     m_txBiasTee;
    bool     m_txTransverterMode;
    qint64   m_txTransverterDeltaFrequency;

    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

    BladeRF2MIMOSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const BladeRF2MIMOSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};




#endif /* PLUGINS_SAMPLEMIMO_BLADERF2MIMO_BLADERF2MIMOSETTINGS_H_ */
