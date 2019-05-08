///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef DEVICES_LIMESDR_DEVICELIMESDRPARAM_H_
#define DEVICES_LIMESDR_DEVICELIMESDRPARAM_H_

#include "lime/LimeSuite.h"

#include "export.h"

/**
 * This structure refers to one physical device shared among parties (logical devices represented by
 * the DeviceAPI with single Rx or Tx stream type).
 * It allows storing information on the common resources in one place and is shared among participants.
 * There is only one copy that is constructed by the first participant and destroyed by the last.
 * A participant knows it is the first or last by checking the lists of buddies (Rx + Tx).
 */
struct DEVICES_API DeviceLimeSDRParams
{
    enum LimeType
    {
        LimeSPI,
        LimeMini,
        LimeUSB,
        LimeUndefined
    };

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
    LimeType     m_type;         //!< Hardware type

    DeviceLimeSDRParams() :
        m_dev(0),
        m_nbRxChannels(0),
        m_nbTxChannels(0),
        m_sampleRate(1e6),
        m_log2OvSRRx(0),
        m_log2OvSRTx(0),
        m_rxFrequency(1e6),
        m_txFrequency(1e6),
        m_type(LimeUndefined)
    {
        m_lpfRangeRx.max = 0.0f;
        m_lpfRangeRx.min = 0.0f;
        m_lpfRangeRx.step = 0.0f;

        m_lpfRangeTx.max = 0.0f;
        m_lpfRangeTx.min = 0.0f;
        m_lpfRangeTx.step = 0.0f;

        m_loRangeRx.max = 0.0f;
        m_loRangeRx.min = 0.0f;
        m_loRangeRx.step = 0.0f;

        m_loRangeTx.max = 0.0f;
        m_loRangeTx.min = 0.0f;
        m_loRangeTx.step = 0.0f;

        m_srRangeRx.max = 0.0f;
        m_srRangeRx.min = 0.0f;
        m_srRangeRx.step = 0.0f;

        m_srRangeTx.max = 0.0f;
        m_srRangeTx.min = 0.0f;
        m_srRangeTx.step = 0.0f;
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

private:
    void getHardwareType(const char *device_str);
};

#endif /* DEVICES_LIMESDR_DEVICELIMESDRPARAM_H_ */
