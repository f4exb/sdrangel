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

#include "dsp/filerecord.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "plutosdr/deviceplutosdrparams.h"
#include "plutosdr/deviceplutosdrbox.h"

#include "plutosdrinput.h"

MESSAGE_CLASS_DEFINITION(PlutoSDRInput::MsgFileRecord, Message)

PlutoSDRInput::PlutoSDRInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_fileSink(0),
    m_deviceDescription("PlutoSDR"),
    m_running(false),
    m_plutoRxBuffer(0)
{
    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);
}


PlutoSDRInput::~PlutoSDRInput()
{
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
}

bool PlutoSDRInput::start()
{
    if (!m_deviceShared.m_deviceParams->getBox()) {
        return false;
    }

    if (m_running) stop();

    applySettings(m_settings, true);

    // start / stop streaming is done in the thread.

//    if ((m_limeSDRInputThread = new LimeSDRInputThread(&m_streamId, &m_sampleFifo)) == 0)
//    {
//        qFatal("LimeSDRInput::start: cannot create thread");
//        stop();
//        return false;
//    }
//    else
//    {
//        qDebug("LimeSDRInput::start: thread created");
//    }
//
//    m_limeSDRInputThread->setLog2Decimation(m_settings.m_log2SoftDecim);
//
//    m_limeSDRInputThread->startWork();
//
//    m_deviceShared.m_thread = m_limeSDRInputThread;
//    m_running = true;

    return true;
}

void PlutoSDRInput::stop()
{
}

const QString& PlutoSDRInput::getDeviceDescription() const
{
    return m_deviceDescription;
}
int PlutoSDRInput::getSampleRate() const
{
    return (m_settings.m_devSampleRate / (1<<m_settings.m_log2Decim));
}

quint64 PlutoSDRInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}


bool PlutoSDRInput::handleMessage(const Message& message)
{
    if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "PlutoSDRInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop()) {
            m_fileSink->startRecording();
        } else {
            m_fileSink->stopRecording();
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool PlutoSDRInput::openDevice()
{
    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("PlutoSDRInput::openDevice: could not allocate SampleFifo");
        return false;
    }
    else
    {
        qDebug("PlutoSDRInput::openDevice: allocated SampleFifo");
    }

    // look for Tx buddy and get reference to common parameters
    if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        qDebug("PlutoSDRInput::openDevice: look at Tx buddy");

        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        m_deviceShared = *((DevicePlutoSDRShared *) sinkBuddy->getBuddySharedPtr()); // copy parameters

        if (m_deviceShared.m_deviceParams == 0)
        {
            qCritical("PlutoSDRInput::openDevice: cannot get device parameters from Tx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("PlutoSDRInput::openDevice: getting device parameters from Tx buddy");
        }
    }
    // There is no buddy then create the first PlutoSDR common parameters
    // open the device this will also populate common fields
    else
    {
        qDebug("PlutoSDRInput::openDevice: open device here");

        m_deviceShared.m_deviceParams = new DevicePlutoSDRParams();
        char serial[256];
        strcpy(serial, qPrintable(m_deviceAPI->getSampleSourceSerial()));
        m_deviceShared.m_deviceParams->open(serial);
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    // acquire the channel
    DevicePlutoSDRBox *plutoBox =  m_deviceShared.m_deviceParams->getBox();
    plutoBox->openRx();
    m_plutoRxBuffer = plutoBox->createRxBuffer(1024*1024, false);

    return true;
}

void PlutoSDRInput::closeDevice()
{
    if (m_deviceShared.m_deviceParams->getBox() == 0) { // was never open
        return;
    }

    if (m_deviceAPI->getSinkBuddies().size() == 0)
    {
        m_deviceShared.m_deviceParams->close();
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = 0;
    }
}

void PlutoSDRInput::suspendBuddies()
{
    // suspend Tx buddy's thread

    for (unsigned int i = 0; i < m_deviceAPI->getSinkBuddies().size(); i++)
    {
        DeviceSinkAPI *buddy = m_deviceAPI->getSinkBuddies()[i];
        DevicePlutoSDRShared *buddyShared = (DevicePlutoSDRShared *) buddy->getBuddySharedPtr();

        if (buddyShared->m_thread) {
            buddyShared->m_thread->stopWork();
        }
    }
}

void PlutoSDRInput::resumeBuddies()
{
    // resume Tx buddy's thread

    for (unsigned int i = 0; i < m_deviceAPI->getSinkBuddies().size(); i++)
    {
        DeviceSinkAPI *buddy = m_deviceAPI->getSinkBuddies()[i];
        DevicePlutoSDRShared *buddyShared = (DevicePlutoSDRShared *) buddy->getBuddySharedPtr();

        if (buddyShared->m_thread) {
            buddyShared->m_thread->startWork();
        }
    }
}

bool PlutoSDRInput::applySettings(const PlutoSDRInputSettings& settings, bool force)
{
    return false;
}
