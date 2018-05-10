///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2017 F4EXB                                                 //
// written by Edouard Griffiths                                                  //
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
#include <dsp/devicesamplesource.h>

DeviceSampleSource::DeviceSampleSource() :
    m_guiMessageQueue(0)
{
	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

DeviceSampleSource::~DeviceSampleSource()
{
}

void DeviceSampleSource::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		if (handleMessage(*message))
		{
			delete message;
		}
	}
}

qint64 DeviceSampleSource::calculateDeviceCenterFrequency(
            quint64 centerFrequency,
            qint64 transverterDeltaFrequency,
            int log2Decim,
            fcPos_t fcPos,
            quint32 devSampleRate,
            bool transverterMode)
{
    qint64 deviceCenterFrequency = centerFrequency;
    deviceCenterFrequency -= transverterMode ? transverterDeltaFrequency : 0;
    deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;
    qint64 f_img = deviceCenterFrequency;

    if ((log2Decim == 0) || (fcPos == FC_POS_CENTER))
    {
        f_img = deviceCenterFrequency;
    }
    else
    {
        if (fcPos == FC_POS_INFRA)
        {
            deviceCenterFrequency += (devSampleRate / 4);
            f_img = deviceCenterFrequency + devSampleRate/2;
        }
        else if (fcPos == FC_POS_SUPRA)
        {
            deviceCenterFrequency -= (devSampleRate / 4);
            f_img = deviceCenterFrequency - devSampleRate/2;
        }
    }

    qDebug() << "DeviceSampleSource::calculateDeviceCenterFrequency:"
            << " desired center freq: " << centerFrequency << " Hz"
            << " device center freq: " << deviceCenterFrequency << " Hz"
            << " device sample rate: " << devSampleRate << "S/s"
            << " Actual sample rate: " << devSampleRate/(1<<log2Decim) << "S/s"
            << " center freq position code: " << fcPos
            << " image frequency: " << f_img << "Hz";

    return deviceCenterFrequency;
}
