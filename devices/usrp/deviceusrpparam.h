///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef DEVICES_USRP_DEVICEUSRPPARAM_H_
#define DEVICES_USRP_DEVICEUSRPPARAM_H_

#include <QStringList>

#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/exception.hpp>
#include <uhd/types/tune_request.hpp>

#include "export.h"

/**
 * This structure refers to one physical device shared among parties (logical devices represented by
 * the DeviceAPI with single Rx or Tx stream type).
 * It allows storing information on the common resources in one place and is shared among participants.
 * There is only one copy that is constructed by the first participant and destroyed by the last.
 * A participant knows it is the first or last by checking the lists of buddies (Rx + Tx).
 */
struct DEVICES_API DeviceUSRPParams
{
    uhd::usrp::multi_usrp::sptr m_dev;          //!< device handle
    uint32_t     m_nbRxChannels;                //!< number of Rx channels
    uint32_t     m_nbTxChannels;                //!< number of Tx channels
    uhd::meta_range_t m_lpfRangeRx;             //!< Low pass filter range information (Rx side)
    uhd::meta_range_t m_lpfRangeTx;             //!< Low pass filter range information (Tx side)
    uhd::freq_range_t m_loRangeRx;              //!< LO range for Rx
    uhd::freq_range_t m_loRangeTx;              //!< LO range for Tx
    uhd::meta_range_t m_srRangeRx;              //!< Sample rate range
    uhd::meta_range_t m_srRangeTx;              //!< Sample rate range
    uhd::gain_range_t m_gainRangeRx;            //!< Gain range for Rx
    uhd::gain_range_t m_gainRangeTx;            //!< Gain range for Tx
    QStringList  m_txAntennas;                  //!< List of Tx antenna names
    QStringList  m_rxAntennas;                  //!< List of Rx antenna names
    QStringList  m_rxGainNames;                 //!< List of Rx gain stages - Currently this seems limited to "PGA"
    QStringList  m_clockSources;                //!< List of clock sources E.g. "internal", "external", "gpsdo"

    DeviceUSRPParams() :
        m_dev(),
        m_nbRxChannels(0),
        m_nbTxChannels(0),
        m_lpfRangeRx(),
        m_lpfRangeTx(),
        m_loRangeRx(),
        m_loRangeTx(),
        m_srRangeRx(),
        m_srRangeTx(),
        m_gainRangeRx(),
        m_gainRangeTx(),
        m_txAntennas(),
        m_rxAntennas(),
        m_rxGainNames(),
        m_clockSources()
    {
    }

    /**
     * Opens and initialize the device and obtain information (# channels, ranges, ...)
     */
    bool open(const QString &deviceStr, bool channelNumOnly);
    void close();
    uhd::usrp::multi_usrp::sptr getDevice() { return m_dev; }

    ~DeviceUSRPParams()
    {
        close();
    }

};

#endif /* DEVICES_USRP_DEVICEUSRPPARAM_H_ */
