///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMOSETTINGS_H_
#define PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMOSETTINGS_H_

#include <stdint.h>

#include <QtGlobal>
#include <QString>

struct USRPMIMOSettings
{
    typedef enum {
        GAIN_AUTO,
        GAIN_MANUAL
    } GainMode;

    QString   m_title;
    int       m_masterClockRate; //!< Read-only, reported by device (-1 if unknown)
    int       m_devSampleRate;
    QString   m_clockSource;
    uint8_t   m_gpioDir;
    uint8_t   m_gpioPins;
    quint64   m_rxCenterFrequency;
    int       m_rxLOOffset;
    quint32   m_log2SoftDecim;
    float     m_rxLpfBW;
    bool      m_dcBlock;
    bool      m_iqCorrection;
    bool      m_rxTransverterMode;
    qint64    m_rxTransverterDeltaFrequency;
    GainMode  m_rx0GainMode;
    quint32   m_rx0Gain;
    QString   m_rx0AntennaPath;
    GainMode  m_rx1GainMode;
    quint32   m_rx1Gain;
    QString   m_rx1AntennaPath;
    quint64   m_txCenterFrequency;
    int       m_txLOOffset;
    quint32   m_log2SoftInterp;
    float     m_txLpfBW;
    bool      m_txTransverterMode;
    qint64    m_txTransverterDeltaFrequency;
    quint32   m_tx0Gain;
    QString   m_tx0AntennaPath;
    quint32   m_tx1Gain;
    QString   m_tx1AntennaPath;

    bool      m_useReverseAPI;
    QString   m_reverseAPIAddress;
    uint16_t  m_reverseAPIPort;
    uint16_t  m_reverseAPIDeviceIndex;

    USRPMIMOSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const USRPMIMOSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMOSETTINGS_H_ */
