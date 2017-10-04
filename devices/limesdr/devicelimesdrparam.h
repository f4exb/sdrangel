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

#ifndef DEVICES_LIMESDR_DEVICELIMESDRPARAM_H_
#define DEVICES_LIMESDR_DEVICELIMESDRPARAM_H_

#include "lime/LimeSuite.h"

/**
 * This structure refers to one physical device shared among parties (logical devices represented by
 * the DeviceSinkAPI or DeviceSourceAPI).
 * It allows storing information on the common resources in one place and is shared among participants.
 * There is only one copy that is constructed by the first participant and destroyed by the last.
 * A participant knows it is the first or last by checking the lists of buddies (Rx + Tx).
 */
struct DeviceLimeSDRParams
{
    lms_device_t *m_dev;         //!< device handle
    uint32_t     m_nbRxChannels; //!< number of Rx channels (normally 2, we'll see if we really use it...)
    uint32_t     m_nbTxChannels; //!< number of Tx channels (normally 2, we'll see if we really use it...)
    lms_range_t  m_lpfRangeRx;   //!< Low pass filter range information (Rx side)
    lms_range_t  m_lpfRangeTx;   //!< Low pass filter range information (Tx side)
    lms_range_t  m_loRangeRx;    //!< LO range for Rx
    lms_range_t  m_loRangeTx;    //!< LO range for Tx
    lms_range_t  m_srRangeRx;    //!< ADC sample rate range
    lms_range_t  m_srRangeTx;    //!< DAC sample rate range
    float        m_sampleRate;   //!< ADC/DAC sample rate
    int          m_log2OvSRRx;   //!< log2 of Rx oversampling (0..5)
    int          m_log2OvSRTx;   //!< log2 of Tx oversampling (0..5)
    float        m_rxFrequency;  //!< Rx frequency
    float        m_txFrequency;  //!< Tx frequency

    DeviceLimeSDRParams() :
        m_dev(0),
        m_nbRxChannels(0),
        m_nbTxChannels(0),
        m_sampleRate(1e6),
        m_log2OvSRRx(0),
        m_log2OvSRTx(0),
        m_rxFrequency(1e6),
        m_txFrequency(1e6)
    {
    }

    /**
     * Opens and initialize the device and obtain information (# channels, ranges, ...)
     */
    bool open(lms_info_str_t deviceStr);
    void close();
    lms_device_t *getDevice() { return m_dev; }

    ~DeviceLimeSDRParams()
    {
    }
};

#endif /* DEVICES_LIMESDR_DEVICELIMESDRPARAM_H_ */
