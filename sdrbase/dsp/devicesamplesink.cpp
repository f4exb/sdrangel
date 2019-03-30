///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2019 F4EXB                                                 //
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

#include "dsp/devicesamplesink.h"

DeviceSampleSink::DeviceSampleSink() :
    m_sampleSourceFifo(1<<19),
    m_guiMessageQueue(0)
{
	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

DeviceSampleSink::~DeviceSampleSink()
{
}

void DeviceSampleSink::handleInputMessages()
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

qint64 DeviceSampleSink::calculateDeviceCenterFrequency(
            quint64 centerFrequency,
            qint64 transverterDeltaFrequency,
            int log2Interp,
            fcPos_t fcPos,
            quint32 devSampleRate,
            bool transverterMode)
{
    qint64 deviceCenterFrequency = centerFrequency;
    deviceCenterFrequency -= transverterMode ? transverterDeltaFrequency : 0;
    deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;
    qint64 f_img = deviceCenterFrequency;

    deviceCenterFrequency -= calculateFrequencyShift(log2Interp, fcPos, devSampleRate);
    f_img -= 2*calculateFrequencyShift(log2Interp, fcPos, devSampleRate);

    qDebug() << "DeviceSampleSink::calculateDeviceCenterFrequency:"
            << " desired center freq: " << centerFrequency << " Hz"
            << " device center freq: " << deviceCenterFrequency << " Hz"
            << " device sample rate: " << devSampleRate << "S/s"
            << " Actual sample rate: " << devSampleRate/(1<<log2Interp) << "S/s"
            << " center freq position code: " << fcPos
            << " image frequency: " << f_img << "Hz";

    return deviceCenterFrequency;
}

qint64 DeviceSampleSink::calculateCenterFrequency(
            quint64 deviceCenterFrequency,
            qint64 transverterDeltaFrequency,
            int log2Interp,
            fcPos_t fcPos,
            quint32 devSampleRate,
            bool transverterMode)
{
    qint64 centerFrequency = deviceCenterFrequency;
    centerFrequency += calculateFrequencyShift(log2Interp, fcPos, devSampleRate);
    centerFrequency += transverterMode ? transverterDeltaFrequency : 0;
    centerFrequency = centerFrequency < 0 ? 0 : centerFrequency;

    qDebug() << "DeviceSampleSink::calculateCenterFrequency:"
            << " desired center freq: " << centerFrequency << " Hz"
            << " device center freq: " << deviceCenterFrequency << " Hz"
            << " device sample rate: " << devSampleRate << "S/s"
            << " Actual sample rate: " << devSampleRate/(1<<log2Interp) << "S/s"
            << " center freq position code: " << fcPos;

    return centerFrequency;
}

/**
 * log2Interp = 0:  no shift
 *
 * n = log2Interp <= 2: fc = +/- 1/2^(n-1)
 *      center
 *  |     ^     |
 *  | inf | sup |
 *     ^     ^
 *
 * n = log2Interp > 2: fc = +/- 1/2^n
 *         center
 *  |         ^         |
 *  |  |inf|  |  |sup|  |
 *       ^         ^
 */
qint32 DeviceSampleSink::calculateFrequencyShift(
            int log2Interp,
            fcPos_t fcPos,
            quint32 devSampleRate)
{
    if (log2Interp == 0) { // no shift at all
        return 0;
    } else if (log2Interp < 3) {
        if (fcPos == FC_POS_INFRA) { // shift in the square next to center frequency
            return -(devSampleRate / (1<<(log2Interp+1)));
        } else if (fcPos == FC_POS_SUPRA) {
            return devSampleRate / (1<<(log2Interp+1));
        } else {
            return 0;
        }
    } else {
        if (fcPos == FC_POS_INFRA) { // shift centered in the square next to center frequency
            return -(devSampleRate / (1<<(log2Interp)));
        } else if (fcPos == FC_POS_SUPRA) {
            return devSampleRate / (1<<(log2Interp));
        } else {
            return 0;
        }
    }
}

