///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_SAMPLESOURCE_LIMESDROUTPUT_LIMESDROUTPUTSETTINGS_H_
#define PLUGINS_SAMPLESOURCE_LIMESDROUTPUT_LIMESDROUTPUTSETTINGS_H_

#include <QByteArray>
#include <stdint.h>

/**
 * These are the settings individual to each hardware channel or software Tx chain
 * Plus the settings to be saved in the presets
 */
struct LimeSDROutputSettings
{
    typedef enum {
        FC_POS_INFRA = 0,
        FC_POS_SUPRA,
        FC_POS_CENTER
    } fcPos_t;

    enum PathRFE
    {
        PATH_RFE_NONE = 0,
        PATH_RFE_TXRF1,
        PATH_RFE_TXEF2
    };

    // global settings to be saved
    uint64_t m_centerFrequency;
    int      m_devSampleRate;
    uint32_t m_log2HardInterp;
    // channel settings
    uint32_t m_log2SoftInterp;
    float    m_lpfBW;        //!< LMS amalog lowpass filter bandwidth (Hz)
    bool     m_lpfFIREnable; //!< Enable LMS digital lowpass FIR filters
    float    m_lpfFIRBW;     //!< LMS digital lowpass FIR filters bandwidth (Hz)
    uint32_t m_gain;         //!< Optimally distributed gain (dB)
    bool     m_ncoEnable;    //!< Enable TSP NCO and mixing
    int      m_ncoFrequency; //!< Actual NCO frequency (the resulting frequency with mixing is displayed)
    PathRFE  m_antennaPath;
    bool     m_extClock;     //!< True if external clock source
    uint32_t m_extClockFreq; //!< Frequency (Hz) of external clock source

    LimeSDROutputSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* PLUGINS_SAMPLESOURCE_LIMESDRINPUT_LIMESDRINPUTSETTINGS_H_ */
