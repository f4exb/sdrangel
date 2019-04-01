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
            FrequencyShiftScheme frequencyShiftScheme,
            bool transverterMode)
{
    qint64 deviceCenterFrequency = centerFrequency;
    deviceCenterFrequency -= transverterMode ? transverterDeltaFrequency : 0;
    deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;
    qint64 f_img = deviceCenterFrequency;

    deviceCenterFrequency -= calculateFrequencyShift(log2Decim, fcPos, devSampleRate, frequencyShiftScheme);
    f_img -= 2*calculateFrequencyShift(log2Decim, fcPos, devSampleRate, frequencyShiftScheme);

    qDebug() << "DeviceSampleSource::calculateDeviceCenterFrequency:"
            << " frequencyShiftScheme: " << frequencyShiftScheme
            << " desired center freq: " << centerFrequency << " Hz"
            << " device center freq: " << deviceCenterFrequency << " Hz"
            << " device sample rate: " << devSampleRate << "S/s"
            << " Actual sample rate: " << devSampleRate/(1<<log2Decim) << "S/s"
            << " center freq position code: " << fcPos
            << " image frequency: " << f_img << "Hz";

    return deviceCenterFrequency;
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
    qint64 centerFrequency = deviceCenterFrequency;
    centerFrequency += calculateFrequencyShift(log2Decim, fcPos, devSampleRate, frequencyShiftScheme);
    centerFrequency += transverterMode ? transverterDeltaFrequency : 0;
    centerFrequency = centerFrequency < 0 ? 0 : centerFrequency;

    qDebug() << "DeviceSampleSource::calculateCenterFrequency:"
            << " frequencyShiftScheme: " << frequencyShiftScheme
            << " desired center freq: " << centerFrequency << " Hz"
            << " device center freq: " << deviceCenterFrequency << " Hz"
            << " device sample rate: " << devSampleRate << "S/s"
            << " Actual sample rate: " << devSampleRate/(1<<log2Decim) << "S/s"
            << " center freq position code: " << fcPos;

    return centerFrequency;
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
    if (frequencyShiftScheme == FSHIFT_STD)
    {
        if (log2Decim == 0) { // no shift at all
            return 0;
        } else if (log2Decim < 3) {
            if (fcPos == FC_POS_INFRA) { // shift in the square next to center frequency
                return -(devSampleRate / (1<<(log2Decim+1)));
            } else if (fcPos == FC_POS_SUPRA) {
                return devSampleRate / (1<<(log2Decim+1));
            } else {
                return 0;
            }
        } else {
            if (fcPos == FC_POS_INFRA) { // shift centered in the square next to center frequency
                return -(devSampleRate / (1<<(log2Decim)));
            } else if (fcPos == FC_POS_SUPRA) {
                return devSampleRate / (1<<(log2Decim));
            } else {
                return 0;
            }
        }
    }
    else if (frequencyShiftScheme == FSHIFT_TXSYNC)
    {
        if (fcPos == FC_POS_CENTER) {
            return 0;
        }

        int sign = fcPos == FC_POS_INFRA ? -1 : 1;
        int halfSampleRate = devSampleRate / 2; // fractions are relative to sideband thus based on half the sample rate

        if (log2Decim == 0) {
            return 0;
        } else if (log2Decim == 1) {
            return sign * (halfSampleRate / 2);         // inf or sup: 1/2
        } else if (log2Decim == 2) {
            return sign * ((halfSampleRate * 3) / 4);   // inf, inf or sup, sup: 1/2 + 1/4
        } else if (log2Decim == 3) {
            return sign * ((halfSampleRate * 5) / 8);   // inf, inf, sup or sup, sup, inf: 1/2 + 1/4 - 1/8 = 5/8
        } else if (log2Decim == 4) {
            return sign * ((halfSampleRate * 11) / 16); // inf, inf, sup, inf or sup, sup, inf, sup: 1/2 + 1/4 - 1/8 + 1/16 = 11/16
        } else if (log2Decim == 5) {
            return sign * ((halfSampleRate * 21) / 32); // inf, inf, sup, inf, sup or sup, sup, inf, sup, inf: 1/2 + 1/4 - 1/8 + 1/16 - 1/32 = 21/32
        } else if (log2Decim == 6) {
            return sign * ((halfSampleRate * 21) / 64); // inf, sup, inf, sup, inf, sup or sup, inf, sup, inf, sup, inf: 1/2 - 1/4 + 1/8 -1/16 + 1/32 - 1/64 = 21/64
        } else {
            return 0;
        }
    }
    else
    {
        return 0;
    }
}
