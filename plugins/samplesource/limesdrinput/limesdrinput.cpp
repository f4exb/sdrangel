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

#include <QMutexLocker>
#include <cstddef>
#include <string.h>
#include "lime/LimeSuite.h"

#include "limesdrinput.h"
#include "limesdr/devicelimesdrparam.h"


LimeSDRInput::LimeSDRInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_limeSDRInputThread(0),
    m_deviceDescription(),
    m_running(false)
{
    openDevice();
}

LimeSDRInput::~LimeSDRInput()
{
    if (m_running) stop();
    closeDevice();
}

bool LimeSDRInput::openDevice()
{
    // look for Rx buddies and get reference to common parameters
    // if there is a channel left take the first available
    if (m_deviceAPI->getSourceBuddies().size() > 0) // look source sibling first
    {
        DeviceSinkAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        m_deviceShared = *(sourceBuddy->getBuddySharedPtr());              // copy shared data
        DeviceLimeSDRParams *deviceParams = m_deviceShared.m_deviceParams; // get device parameters

        if (deviceParams == 0)
        {
            qCritical("LimeSDRInput::openDevice: cannot get device parameters from Rx buddy");
            return false; // the device params should have been created by the buddy
        }

        if (m_deviceAPI->getSourceBuddies().size() == deviceParams->m_nbRxChannels)
        {
            return false; // no more Rx channels available in device
        }
        // look for unused channel number
        char *busyChannels = new char[deviceParams->m_nbRxChannels];
        memset(busyChannels, 0, deviceParams->m_nbRxChannels);

        for (int i = 0; i < m_deviceAPI->getSourceBuddies().size(); i++)
        {
            DeviceSinkAPI *buddy = m_deviceAPI->getSourceBuddies()[i];
            DeviceLimeSDRShared *buddyShared = buddy->getBuddySharedPtr();
            busyChannels[buddyShared->m_channel] = 1;
        }

        std::size_t ch = 0;

        for (;ch < deviceParams->m_nbRxChannels; ch++)
        {
            if (busyChannels[ch] == 0) {
                break; // first available is the good one
            }
        }

        m_deviceShared.m_channel = ch;
        delete[] busyChannels;
    }
    // look for Tx buddies and get reference to common parameters
    // take the first Rx channel
    else if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        m_deviceShared = *(sinkBuddy->getBuddySharedPtr()); // copy parameters

        if (m_deviceShared.m_deviceParams == 0)
        {
            qCritical("LimeSDRInput::openDevice: cannot get device parameters from Tx buddy");
            return false; // the device params should have been created by the buddy
        }

        m_deviceShared.m_channel = 0; // take first channel
    }
    // There are no buddies then create the first LimeSDR common parameters
    // open the device this will also populate common fields
    // take the first Rx channel
    else
    {
        m_deviceShared.m_deviceParams = new DeviceLimeSDRParams();
        m_deviceShared.m_deviceParams->open((lms_info_str_t) qPrintable(m_deviceAPI->getSampleSourceSerial()));
        m_deviceShared.m_channel = 0; // take first channel
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    // acquire the channel

    if (LMS_EnableChannel(m_deviceShared.m_deviceParams->getDevice(), LMS_CH_RX, m_deviceShared.m_channel, true) != 0)
    {
        qCritical("LimeSDRInput::openDevice: cannot enable Rx channel %u", m_deviceShared.m_channel);
        return false;
    }

    return true;
}

void LimeSDRInput::closeDevice()
{
    // release the channel

    if (LMS_EnableChannel(m_deviceShared.m_deviceParams->getDevice(), LMS_CH_RX, m_deviceShared.m_channel, false) != 0)
    {
        qWarning("LimeSDRInput::closeDevice: cannot disable Rx channel %u", m_deviceShared.m_channel);
    }

    m_deviceShared.m_channel = -1;

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        m_deviceShared.m_deviceParams->close();
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = 0;
    }
}

bool LimeSDRInput::start()
{
    if (!m_deviceShared.m_deviceParams->getDevice()) {
        return false;
    }

    if (m_running) stop();

    // set up the stream

    m_streamId.channel =  m_deviceShared.m_channel; //channel number
    m_streamId.fifoSize = 1024 * 128;               //fifo size in samples
    m_streamId.throughputVsLatency = 1.0;           //optimize for max throughput
    m_streamId.isTx = false;                        //RX channel
    m_streamId.dataFmt = lms_stream_t::LMS_FMT_I12; //12-bit integers

    if (LMS_SetupStream(m_deviceShared.m_deviceParams->getDevice(), &m_streamId) != 0)
    {
        qCritical("LimeSDRInput::start: cannot setup the stream on Rx channel %u", m_deviceShared.m_channel);
        return false;
    }

    // start / stop streaming is done in the thread.

    if ((m_limeSDRInputThread = new LimeSDRInputThread(&m_streamId, &m_sampleFifo)) == 0)
    {
        qFatal("LimeSDRInput::start: out of memory");
        stop();
        return false;
    }

    m_limeSDRInputThread->setLog2Decimation(m_settings.m_log2SoftDecim);
    m_limeSDRInputThread->setFcPos((int) m_settings.m_fcPos);

    m_limeSDRInputThread->startWork();

    applySettings(m_settings, true);
    m_running = true;

    return true;
}

void LimeSDRInput::stop()
{
    if (m_limeSDRInputThread != 0)
    {
        m_limeSDRInputThread->stopWork();
        delete m_limeSDRInputThread;
        m_limeSDRInputThread = 0;
    }

    // destroy the stream
    LMS_DestroyStream(m_deviceShared.m_deviceParams->getDevice(), &m_streamId);

    m_running = false;
}

