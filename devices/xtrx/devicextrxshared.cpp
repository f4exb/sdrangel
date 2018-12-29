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

MESSAGE_CLASS_DEFINITION(DeviceXTRXShared::MsgReportBuddyChange, Message)
MESSAGE_CLASS_DEFINITION(DeviceXTRXShared::MsgReportClockSourceChange, Message)
MESSAGE_CLASS_DEFINITION(DeviceXTRXShared::MsgReportDeviceInfo, Message)

const float  DeviceXTRXShared::m_sampleFifoLengthInSeconds = 0.25;
const int    DeviceXTRXShared::m_sampleFifoMinSize = 48000; // 192kS/s knee



double DeviceXTRXShared::set_samplerate(double rate,
                                        double master,
                                        bool output)
{
    if (output)
    {
        m_outputRate = rate;

        if (master != 0.0) {
            m_masterRate = master;
        }
    }
    else
    {
        m_inputRate = rate;

        if (master != 0.0) {
            m_masterRate = master;
        }
    }

    double actualcgen;
    double actualrx;
    double actualtx;

    int res = xtrx_set_samplerate(m_deviceParams->getDevice(),
          m_masterRate,
          m_inputRate,
          m_outputRate,
          0,
          &actualcgen,
          &actualrx,
          &actualtx);

    if (res)
    {
        qCritical("DeviceXTRXShared::set_samplerate: Unable to set %s samplerate, m_masterRate: %f, m_inputRate: %f, m_outputRate: %f, error=%d\n",
                output ? "output" : "input", m_masterRate, m_inputRate, m_outputRate, res);
        return 0;
    }
    else
    {
        qDebug() << "DeviceXTRXShared::set_samplerate: sample rate set: "
                << "output: "<< output
                << "m_masterRate: " << m_masterRate
                << "m_inputRate: " << m_inputRate
                << "m_outputRate: " << m_outputRate
                << "actualcgen: " << actualcgen
                << "actualrx: " << actualrx
                << "actualtx: " << actualtx;
    }

    if (output) {
        return m_outputRate;
    }

    return m_inputRate;
}

double DeviceXTRXShared::get_board_temperature()
{
    uint64_t val = 0;

    int res = xtrx_val_get(m_deviceParams->getDevice(), XTRX_TRX, XTRX_CH_AB, XTRX_BOARD_TEMP, &val);

    if (res) {
        return 0;
    }

    return val;
}

bool DeviceXTRXShared::get_gps_status()
{
    uint64_t val = 0;

    int res = xtrx_val_get(m_deviceParams->getDevice(), XTRX_TRX, XTRX_CH_AB, XTRX_WAIT_1PPS, &val);

    if (res) {
        return false;
    }

    return val != 0;
}
