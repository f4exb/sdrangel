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
#include <string.h>
#include "limesdrinput.h"


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
        m_deviceParams = *(sourceBuddy->getBuddySharedPtr()); // copy parameters

        if (m_deviceAPI->getSourceBuddies().size() == m_deviceParams.m_nbRxChannels)
        {
            return false; // no more Rx channels available in device
        }
        // look for unused channel number
        char *busyChannels = new char[m_deviceParams.m_nbRxChannels];
        memset(busyChannels, 0, m_deviceParams.m_nbRxChannels);

        for (int i = 0; i < m_deviceAPI->getSourceBuddies().size(); i++)
        {
            DeviceSinkAPI *buddy = m_deviceAPI->getSourceBuddies()[i];
            DeviceLimeSDRParams *buddyParms = buddy->getBuddySharedPtr();
            busyChannels[buddyParms->m_channel] = 1;
        }

        int ch = 0;

        for (;ch < m_deviceParams.m_nbRxChannels; ch++)
        {
            if (busyChannels[ch] == 0) {
                break; // first available is the good one
            }
        }

        m_deviceParams.m_channel = ch;
        delete[] busyChannels;
    }
    // look for Tx buddies and get reference to common parameters
    // take the first Rx channel
    else if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        m_deviceParams = *(sinkBuddy->getBuddySharedPtr()); // copy parameters
        m_deviceParams.m_channel = 0; // take first channel
    }
    // There are no buddies then create the first LimeSDR common parameters
    // open the device this will also populate common fields
    // take the first Rx channel
    else
    {
        m_deviceParams.open((lms_info_str_t) qPrintable(m_deviceAPI->getSampleSourceSerial()));
        m_deviceParams.m_channel = 0; // take first channel
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceParams); // propagate common parameters to API

    // TODO: acquire the channel

    return true;
}

void LimeSDRInput::closeDevice()
{
    // TODO: release the channel

    // No buddies effectively close the device
    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        m_deviceParams.close();
    }
}

bool LimeSDRInput::start(int device)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_deviceParams.m_dev) {
        return false;
    }

    if (m_running) stop();

    if ((m_limeSDRInputThread = new LimeSDRInputThread(m_deviceParams.m_dev, &m_sampleFifo)) == 0)
    {
        qFatal("LimeSDRInput::start: out of memory");
        stop();
        return false;
    }

    m_limeSDRInputThread->setSamplerate(m_settings.m_devSampleRate);
    m_limeSDRInputThread->setLog2Decimation(m_settings.m_log2Decim);
    m_limeSDRInputThread->setFcPos((int) m_settings.m_fcPos);

    m_limeSDRInputThread->startWork();

    mutexLocker.unlock();

    applySettings(m_settings, true);
    m_running = true;

    return true;
}

void LimeSDRInput::stop()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_limeSDRInputThread != 0)
    {
        m_limeSDRInputThread->stopWork();
        delete m_limeSDRInputThread;
        m_limeSDRInputThread = 0;
    }

    m_running = false;
}

