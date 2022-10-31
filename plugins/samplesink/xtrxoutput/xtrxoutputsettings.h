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

#ifndef PLUGINS_SAMPLESINK_XTRXOUTPUT_XTRXOUTPUTSETTINGS_H_
#define PLUGINS_SAMPLESINK_XTRXOUTPUT_XTRXOUTPUTSETTINGS_H_

#include <stdint.h>

#include <QByteArray>
#include <QString>

#include "xtrx_api.h"

/**
 * These are the settings individual to each hardware channel or software Tx chain
 * Plus the settings to be saved in the presets
 */
struct XTRXOutputSettings
{
    // global settings to be saved
    uint64_t m_centerFrequency;
    double   m_devSampleRate;
    uint32_t m_log2HardInterp;
    // channel settings
    uint32_t m_log2SoftInterp;
    float    m_lpfBW;        //!< LMS analog lowpass filter bandwidth (Hz)
    uint32_t m_gain;         //!< Optimally distributed gain (dB)
    bool     m_ncoEnable;    //!< Enable TSP NCO and mixing
    int      m_ncoFrequency; //!< Actual NCO frequency (the resulting frequency with mixing is displayed)
    xtrx_antenna_t  m_antennaPath;
    bool     m_extClock;     //!< True if external clock source
    uint32_t m_extClockFreq; //!< Frequency (Hz) of external clock source
    uint32_t m_pwrmode;
    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

    XTRXOutputSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const XTRXOutputSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* PLUGINS_SAMPLESINK_XTRXOUTPUT_XTRXOUTPUTSETTINGS_H_ */
