///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
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

#include <QDebug>

#include "devicextrxshared.h"
#include "devicextrx.h"

MESSAGE_CLASS_DEFINITION(DeviceXTRXShared::MsgReportBuddyChange, Message)
MESSAGE_CLASS_DEFINITION(DeviceXTRXShared::MsgReportClockSourceChange, Message)
MESSAGE_CLASS_DEFINITION(DeviceXTRXShared::MsgReportDeviceInfo, Message)

const unsigned int DeviceXTRXShared::m_sampleFifoMinRate = 48000;

DeviceXTRXShared::DeviceXTRXShared() :
    m_dev(0),
    m_channel(-1),
    m_source(0),
    m_sink(0),
    m_thread(0),
    m_threadWasRunning(false),
    m_first_1pps_count(true),
    m_last_1pps_count(0),
    m_no_1pps_count_change_counter(0)
{}

DeviceXTRXShared::~DeviceXTRXShared()
{}

double DeviceXTRXShared::get_board_temperature()
{
    uint64_t val = 0;

    int res = xtrx_val_get(m_dev->getDevice(), XTRX_TRX, XTRX_CH_AB, XTRX_BOARD_TEMP, &val);

    if (res) {
        return 0;
    }

    return val;
}

bool DeviceXTRXShared::get_gps_status()
{
    uint64_t val = 0;

    int res = xtrx_val_get(m_dev->getDevice(), (xtrx_direction_t) 0, XTRX_CH_AB, XTRX_OSC_LATCH_1PPS, &val);

    if (res)
    {
        return false;
    }
    else
    {
        if (m_first_1pps_count)
        {
            m_last_1pps_count = val;
            m_first_1pps_count = false;
        }
        else
        {
            if (m_last_1pps_count != val)
            {
                m_no_1pps_count_change_counter = 7;
                m_last_1pps_count = val;
            }
            else if (m_no_1pps_count_change_counter != 0)
            {
                m_no_1pps_count_change_counter--;
            }

        }

        //qDebug("DeviceXTRXShared::get_gps_status: XTRX_OSC_LATCH_1PPS: %lu %u", val, m_no_1pps_count_change_counter);
        return m_no_1pps_count_change_counter != 0;
    }
}
