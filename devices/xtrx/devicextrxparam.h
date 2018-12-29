///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
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

#ifndef DEVICES_XTRX_DEVICEXTRXPARAM_H_
#define DEVICES_XTRX_DEVICEXTRXPARAM_H_

#include "xtrx_api.h"


struct DeviceXTRXParams
{
    struct xtrx_dev *m_dev;          //!< device handle
    uint32_t         m_nbRxChannels; //!< number of Rx channels
    uint32_t         m_nbTxChannels; //!< number of Tx channels

    unsigned         m_log2OvSRRx;
    double           m_sampleRate;   //!< ADC/DAC sample rate
    double           m_rxFrequency;  //!< Rx frequency
    double           m_txFrequency;  //!< Tx frequency

    DeviceXTRXParams() :
        m_dev(0),
        m_nbRxChannels(2),
        m_nbTxChannels(2),
        m_log2OvSRRx(0),
        m_sampleRate(5e6),
        m_rxFrequency(30e6),
        m_txFrequency(30e6)
    {
    }

    /**
     * Opens and initialize the device and obtain information (# channels, ranges, ...)
     */
    bool open(const char* deviceStr);
    void close();

    struct xtrx_dev *getDevice() { return m_dev; }

    ~DeviceXTRXParams()
    {
    }
};

#endif /* DEVICES_XTRX_DEVICEXTRXPARAM_H_ */
