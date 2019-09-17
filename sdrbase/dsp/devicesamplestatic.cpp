///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
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

#include "devicesamplestatic.h"

int64_t DeviceSampleStatic::calculateSourceDeviceCenterFrequency(
            uint64_t centerFrequency,
            int64_t transverterDeltaFrequency,
            int log2Decim,
            fcPos_t fcPos,
            uint32_t devSampleRate,
            FrequencyShiftScheme frequencyShiftScheme,
            bool transverterMode)
{
    int64_t deviceCenterFrequency = centerFrequency;
    deviceCenterFrequency -= transverterMode ? transverterDeltaFrequency : 0;
    deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;
    int64_t f_img = deviceCenterFrequency;

    deviceCenterFrequency -= calculateSourceFrequencyShift(log2Decim, fcPos, devSampleRate, frequencyShiftScheme);
    f_img -= 2*calculateSourceFrequencyShift(log2Decim, fcPos, devSampleRate, frequencyShiftScheme);

    qDebug() << "DeviceSampleStatic::calculateSourceDeviceCenterFrequency:"
            << " frequencyShiftScheme: " << frequencyShiftScheme
            << " desired center freq: " << centerFrequency << " Hz"
            << " device center freq: " << deviceCenterFrequency << " Hz"
            << " device sample rate: " << devSampleRate << "S/s"
            << " Actual sample rate: " << devSampleRate/(1<<log2Decim) << "S/s"
            << " center freq position code: " << fcPos
            << " image frequency: " << f_img << "Hz";

    return deviceCenterFrequency;
}

int64_t DeviceSampleStatic::calculateSourceCenterFrequency(
            uint64_t deviceCenterFrequency,
            int64_t transverterDeltaFrequency,
            int log2Decim,
            fcPos_t fcPos,
            uint32_t devSampleRate,
            FrequencyShiftScheme frequencyShiftScheme,
            bool transverterMode)
{
    qint64 centerFrequency = deviceCenterFrequency;
    centerFrequency += calculateSourceFrequencyShift(log2Decim, fcPos, devSampleRate, frequencyShiftScheme);
    centerFrequency += transverterMode ? transverterDeltaFrequency : 0;
    centerFrequency = centerFrequency < 0 ? 0 : centerFrequency;

    qDebug() << "DeviceSampleStatic::calculateSourceCenterFrequency:"
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
int DeviceSampleStatic::calculateSourceFrequencyShift(
            int log2Decim,
            fcPos_t fcPos,
            uint32_t devSampleRate,
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
    else // frequencyShiftScheme == FSHIFT_TXSYNC
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
}

int64_t DeviceSampleStatic::calculateSinkDeviceCenterFrequency(
            uint64_t centerFrequency,
            int64_t transverterDeltaFrequency,
            int log2Interp,
            fcPos_t fcPos,
            uint32_t devSampleRate,
            bool transverterMode)
{
    int64_t deviceCenterFrequency = centerFrequency;
    deviceCenterFrequency -= transverterMode ? transverterDeltaFrequency : 0;
    deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;
    int64_t f_img = deviceCenterFrequency;

    deviceCenterFrequency -= calculateSinkFrequencyShift(log2Interp, fcPos, devSampleRate);
    f_img -= 2*calculateSinkFrequencyShift(log2Interp, fcPos, devSampleRate);

    qDebug() << "DeviceSampleStatic::calculateSinkDeviceCenterFrequency:"
            << " desired center freq: " << centerFrequency << " Hz"
            << " device center freq: " << deviceCenterFrequency << " Hz"
            << " device sample rate: " << devSampleRate << "S/s"
            << " Actual sample rate: " << devSampleRate/(1<<log2Interp) << "S/s"
            << " center freq position code: " << fcPos
            << " image frequency: " << f_img << "Hz";

    return deviceCenterFrequency;
}

int64_t DeviceSampleStatic::calculateSinkCenterFrequency(
            uint64_t deviceCenterFrequency,
            int64_t transverterDeltaFrequency,
            int log2Interp,
            fcPos_t fcPos,
            uint32_t devSampleRate,
            bool transverterMode)
{
    int64_t centerFrequency = deviceCenterFrequency;
    centerFrequency += calculateSinkFrequencyShift(log2Interp, fcPos, devSampleRate);
    centerFrequency += transverterMode ? transverterDeltaFrequency : 0;
    centerFrequency = centerFrequency < 0 ? 0 : centerFrequency;

    qDebug() << "DeviceSampleStatic::calculateSinkCenterFrequency:"
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
int DeviceSampleStatic::calculateSinkFrequencyShift(
            int log2Interp,
            fcPos_t fcPos,
            uint32_t devSampleRate)
{
    if (fcPos == FC_POS_CENTER) {
        return 0;
    }

    int sign = fcPos == FC_POS_INFRA ? -1 : 1;
    int halfSampleRate = devSampleRate / 2; // fractions are relative to sideband thus based on half the sample rate

    if (log2Interp == 0) {
        return 0;
    } else if (log2Interp == 1) {
        return sign * (halfSampleRate / 2);
    } else if (log2Interp == 2) {
        return sign * ((halfSampleRate * 3) / 4);
    } else if (log2Interp == 3) {
        return sign * ((halfSampleRate * 5) / 8);
    } else if (log2Interp == 4) {
        return sign * ((halfSampleRate * 11) / 16);
    } else if (log2Interp == 5) {
        return sign * ((halfSampleRate * 21) / 32);
    } else if (log2Interp == 6) {
        return sign * ((halfSampleRate * 21) / 64);
    } else {
        return 0;
    }
}

