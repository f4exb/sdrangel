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

#include "plutosdrinput.h"

MESSAGE_CLASS_DEFINITION(PlutoSDRInput::MsgFileRecord, Message)

PlutoSDRInput::PlutoSDRInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_fileSink(0),
    m_deviceDescription("PlutoSDR"),
    m_running(false)
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
    return false;
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

}

void PlutoSDRInput::closeDevice()
{

}

void PlutoSDRInput::suspendBuddies()
{

}

void PlutoSDRInput::resumeBuddies()
{

}
