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

#include <QDebug>
#include "devicelimesdrparam.h"

bool DeviceLimeSDRParams::open(lms_info_str_t deviceStr)
{
    qDebug("DeviceLimeSDRParams::open: serial: %s", (const char *) deviceStr);

    if (LMS_Open(&m_dev, deviceStr, 0) < 0)
    {
        qCritical() << "DeviceLimeSDRParams::open: cannot open device " << deviceStr;
        return false;
    }

    if (LMS_Init(m_dev) < 0)
    {
        qCritical() << "DeviceLimeSDRParams::open: cannot init device " << deviceStr;
        return false;
    }

    int n;

    if ((n = LMS_GetNumChannels(m_dev, LMS_CH_RX)) < 0)
    {
        qCritical() << "DeviceLimeSDRParams::open: cannot get the number of Rx channels for device " << deviceStr;
        return false;
    }
    else
    {
        m_nbRxChannels = n;
        qDebug() << "DeviceLimeSDRParams::open: " << n << " Rx channels for device " << deviceStr;
    }

    if ((n = LMS_GetNumChannels(m_dev, LMS_CH_TX)) < 0)
    {
        qCritical() << "DeviceLimeSDRParams::open: cannot get the number of Tx channels for device " << deviceStr;
        return false;
    }
    else
    {
        m_nbTxChannels = n;
        qDebug() << "DeviceLimeSDRParams::open: " << n << " Tx channels for device " << deviceStr;
    }

    if (LMS_GetLPFBWRange(m_dev, LMS_CH_RX, &m_lpfRangeRx) < 0)
    {
        qCritical() << "DeviceLimeSDRParams::open: cannot get the Rx LPF range for device " << deviceStr;
        return false;
    }

    if (LMS_GetLPFBWRange(m_dev, LMS_CH_TX, &m_lpfRangeTx) < 0)
    {
        qCritical() << "DeviceLimeSDRParams::open: cannot get the Tx LPF range for device " << deviceStr;
        return false;
    }

    if (LMS_GetLOFrequencyRange(m_dev, LMS_CH_RX, &m_loRangeRx) < 0)
    {
        qDebug() << "DeviceLimeSDRParams::open: cannot get the Rx LO range for device " << deviceStr;
        return false;
    }

    if (LMS_GetLOFrequencyRange(m_dev, LMS_CH_TX, &m_loRangeTx) < 0)
    {
        qCritical() << "DeviceLimeSDRParams::open: cannot get the Tx LO range for device " << deviceStr;
        return false;
    }

    if (LMS_GetSampleRateRange(m_dev, LMS_CH_RX, &m_srRangeRx) < 0)
    {
        qCritical() << "DeviceLimeSDRParams::open: cannot get the Rx sample rate range for device " << deviceStr;
        return false;
    }

    if (LMS_GetSampleRateRange(m_dev, LMS_CH_TX, &m_srRangeTx) < 0)
    {
        qCritical() << "DeviceLimeSDRParams::open: cannot get the Tx sample rate range for device " << deviceStr;
        return false;
    }

    return true;
}

void DeviceLimeSDRParams::close()
{
    if (m_dev)
    {
        LMS_Close(m_dev);
        m_dev = 0;
    }
}

