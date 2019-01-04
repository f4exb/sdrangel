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

#include <QDebug>

#include "devicextrxshared.h"
#include "devicextrx.h"

MESSAGE_CLASS_DEFINITION(DeviceXTRXShared::MsgReportBuddyChange, Message)
MESSAGE_CLASS_DEFINITION(DeviceXTRXShared::MsgReportClockSourceChange, Message)
MESSAGE_CLASS_DEFINITION(DeviceXTRXShared::MsgReportDeviceInfo, Message)

const float  DeviceXTRXShared::m_sampleFifoLengthInSeconds = 0.25;
const int    DeviceXTRXShared::m_sampleFifoMinSize = 48000; // 192kS/s knee

DeviceXTRXShared::DeviceXTRXShared() :
    m_dev(0),
    m_channel(-1),
    m_source(0),
    m_sink(0),

    m_thread(0),
    m_threadWasRunning(false)
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

    int res = xtrx_val_get(m_dev->getDevice(), XTRX_TRX, XTRX_CH_AB, XTRX_WAIT_1PPS, &val);

    if (res) {
        return false;
    }

    return val != 0;
}
