///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2019 F4EXB                                                 //
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
    return DeviceSampleStatic::calculateSinkDeviceCenterFrequency(
        centerFrequency,
        transverterDeltaFrequency,
        log2Interp,
        (DeviceSampleStatic::fcPos_t) fcPos,
        devSampleRate,
        transverterMode
    );
}

qint64 DeviceSampleSink::calculateCenterFrequency(
            quint64 deviceCenterFrequency,
            qint64 transverterDeltaFrequency,
            int log2Interp,
            fcPos_t fcPos,
            quint32 devSampleRate,
            bool transverterMode)
{
    return DeviceSampleStatic::calculateSinkCenterFrequency(
        deviceCenterFrequency,
        transverterDeltaFrequency,
        log2Interp,
        (DeviceSampleStatic::fcPos_t) fcPos,
        devSampleRate,
        transverterMode
    );
}

/**
 * log2Interp = 0:  no shift
 *
 * log2Interp = 1:  middle of side band (inf or sup: 1/2)
 *        ^     |     ^
 *  | inf | inf | sup | sup |
 *
 * log2Interp = 2:  middle of far side half side band (inf, inf or sup, sup: 1/2 + 1/4)
 *     ^        |        ^
 *  | inf | inf | sup | sup |
 *
 * log2Interp = 3:  inf, inf, sup or sup, sup, inf: 1/2 + 1/4 - 1/8 = 5/8
 * log2Interp = 4:  inf, inf, sup, inf or sup, sup, inf, sup: 1/2 + 1/4 - 1/8 + 1/16 = 11/16
 * log2Interp = 5:  inf, inf, sup, inf, sup or sup, sup, inf, sup, inf: 1/2 + 1/4 - 1/8 + 1/16 - 1/32 = 21/32
 * log2Interp = 6:  inf, sup, inf, sup, inf, sup or sup, inf, sup, inf, sup, inf: 1/2 - 1/4 + 1/8 -1/16 + 1/32 - 1/64 = 21/64
 *
 */
qint32 DeviceSampleSink::calculateFrequencyShift(
            int log2Interp,
            fcPos_t fcPos,
            quint32 devSampleRate)
{
    return DeviceSampleStatic::calculateSinkFrequencyShift(
        log2Interp,
        (DeviceSampleStatic::fcPos_t) fcPos,
        devSampleRate
    );
}

