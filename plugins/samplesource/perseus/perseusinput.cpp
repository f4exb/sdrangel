///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "dsp/filerecord.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/devicesourceapi.h"

#include "perseusinput.h"
#include "perseusthread.h"

MESSAGE_CLASS_DEFINITION(PerseusInput::MsgConfigurePerseus, Message)
MESSAGE_CLASS_DEFINITION(PerseusInput::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(PerseusInput::MsgStartStop, Message)

PerseusInput::PerseusInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_fileSink(0),
    m_deviceDescription("PerseusInput"),
    m_running(false),
    m_perseusThread(0),
    m_perseusDescriptor(0)
{
    openDevice();
    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);
}

PerseusInput::~PerseusInput()
{
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    closeDevice();
}

void PerseusInput::destroy()
{
    delete this;
}

void PerseusInput::init()
{
    applySettings(m_settings, true);
}

bool PerseusInput::start()
{
    if (m_running) stop();

    applySettings(m_settings, true);

    // start / stop streaming is done in the thread.

    if ((m_plutoSDRInputThread = new PlutoSDRInputThread(PLUTOSDR_BLOCKSIZE_SAMPLES, m_deviceShared.m_deviceParams->getBox(), &m_sampleFifo)) == 0)
    {
        qFatal("PlutoSDRInput::start: cannot create thread");
        stop();
        return false;
    }
    else
    {
        qDebug("PlutoSDRInput::start: thread created");
    }

    m_perseusThread->setLog2Decimation(m_settings.m_log2Decim);
    m_perseusThread->startWork();

    m_running = true;

    return true;
}

void PerseusInput::stop()
{
    if (m_perseusThread != 0)
    {
        m_perseusThread->stopWork();
        delete m_perseusThread;
        m_perseusThread = 0;
    }

    m_running = false;
}

QByteArray PerseusInput::serialize() const
{
    return m_settings.serialize();
}

bool PerseusInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigurePerseus* message = MsgConfigurePerseus::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigurePerseus* messageToGUI = MsgConfigurePerseus::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& PerseusInput::getDeviceDescription() const
{
    return m_deviceDescription;
}
int PerseusInput::getSampleRate() const
{
    return (m_settings.m_devSampleRate / (1<<m_settings.m_log2Decim));
}

quint64 PerseusInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void PerseusInput::setCenterFrequency(qint64 centerFrequency)
{
    PerseusSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigurePerseus* message = MsgConfigurePerseus::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigurePerseus* messageToGUI = MsgConfigurePerseus::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool PerseusInput::openDevice()
{
    m_deviceAPI->getSampleSourceSerial();
    int deviceSequence = DevicePerseus::instance().getSequenceFromSerial(m_deviceAPI->getSampleSourceSerial().toStdString());

    if ((m_perseusDescriptor = perseus_open(deviceSequence)) == 0)
    {
        qCritical("PerseusInput::openDevice: cannot open device: %s", perseus_errorstr());
        return false;
    }

    int buf[32];
    m_sampleRates.clear();

    if (perseus_get_sampling_rates(m_perseusDescriptor, buf, sizeof(buf)/sizeof(buf[0])) < 0)
    {
        qCritical("PerseusInput::openDevice: cannot get sampling rates: %s", perseus_errorstr());
        perseus_close(m_perseusDescriptor);
        return false;
    }
    else
    {
         for (int i = 0; (i < 32) && (buf[i] != 0); i++)
         {
             qDebug("PerseusInput::openDevice: sample rate: %d", buf[i]);
             m_sampleRates.push_back(buf[i]);
         }
    }

    return true;
}

void PerseusInput::closeDevice()
{
    if (m_perseusDescriptor)
    {
        perseus_stop_async_input(m_perseusDescriptor);
        perseus_close(m_perseusDescriptor);
    }
}

