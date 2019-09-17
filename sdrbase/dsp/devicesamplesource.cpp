///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2017 F4EXB                                                 //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
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

#include "dsp/devicesamplestatic.h"
#include "dsp/devicesamplesource.h"

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
            FrequencyShiftScheme frequencyShiftScheme,
            bool transverterMode)
{
    return DeviceSampleStatic::calculateSourceDeviceCenterFrequency(
        centerFrequency,
        transverterDeltaFrequency,
        log2Decim,
        (DeviceSampleStatic::fcPos_t) fcPos,
        devSampleRate,
        (DeviceSampleStatic::FrequencyShiftScheme) frequencyShiftScheme,
        transverterMode
    );
}

qint64 DeviceSampleSource::calculateCenterFrequency(
            quint64 deviceCenterFrequency,
            qint64 transverterDeltaFrequency,
            int log2Decim,
            fcPos_t fcPos,
            quint32 devSampleRate,
            FrequencyShiftScheme frequencyShiftScheme,
            bool transverterMode)
{
    return DeviceSampleStatic::calculateSourceCenterFrequency(
        deviceCenterFrequency,
        transverterDeltaFrequency,
        log2Decim,
        (DeviceSampleStatic::fcPos_t) fcPos,
        devSampleRate,
        (DeviceSampleStatic::FrequencyShiftScheme) frequencyShiftScheme,
        transverterMode
    );
}

/**
 * log2Decim = 0:  no shift
 *
 * n = log2Decim <= 2: fc = +/- 1/2^(n-1)
 *      center
 *  |     ^     |
 *  | inf | sup |
 *     ^     ^
 *
 * n = log2Decim > 2: fc = +/- 1/2^n
 *         center
 *  |         ^         |
 *  |  |inf|  |  |sup|  |
 *       ^         ^
 */
qint32 DeviceSampleSource::calculateFrequencyShift(
            int log2Decim,
            fcPos_t fcPos,
            quint32 devSampleRate,
            FrequencyShiftScheme frequencyShiftScheme)
{
    return DeviceSampleStatic::calculateSourceFrequencyShift(
        log2Decim,
        (DeviceSampleStatic::fcPos_t) fcPos,
        devSampleRate,
        (DeviceSampleStatic::FrequencyShiftScheme) frequencyShiftScheme
    );
}
