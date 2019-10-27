///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#include "fftwindow.h"
#include "util/simpleserializer.h"
#include "glspectrumsettings.h"

GLSpectrumSettings::GLSpectrumSettings()
{
    resetToDefaults();
}

GLSpectrumSettings::~GLSpectrumSettings()
{}


void GLSpectrumSettings::resetToDefaults()
{
	m_fftSize = 1024;
	m_fftOverlap = 0;
	m_fftWindow = FFTWindow::Hanning;
	m_refLevel = 0;
	m_powerRange = 100;
	m_decay = 1;
	m_decayDivisor = 1;
	m_histogramStroke = 30;
	m_displayGridIntensity = 5,
	m_displayWaterfall = true;
	m_invertedWaterfall = false;
	m_displayMaxHold = false;
	m_displayHistogram = false;
	m_displayGrid = false;
	m_invert = true;
	m_averagingMode = AvgModeNone;
	m_averagingIndex = 0;
	m_linear = false;
}

QByteArray GLSpectrumSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(1, m_fftSize);
	s.writeS32(2, m_fftOverlap);
	s.writeS32(3, m_fftWindow);
	s.writeReal(4, m_refLevel);
	s.writeReal(5, m_powerRange);
	s.writeBool(6, m_displayWaterfall);
	s.writeBool(7, m_invertedWaterfall);
	s.writeBool(8, m_displayMaxHold);
	s.writeBool(9, m_displayHistogram);
	s.writeS32(10, m_decay);
	s.writeBool(11, m_displayGrid);
	s.writeBool(12, m_invert);
	s.writeS32(13, m_displayGridIntensity);
	s.writeS32(14, m_decayDivisor);
	s.writeS32(15, m_histogramStroke);
	s.writeBool(16, m_displayCurrent);
	s.writeS32(17, m_displayTraceIntensity);
	s.writeReal(18, m_waterfallShare);
	s.writeS32(19, (int) m_averagingMode);
	s.writeS32(20, (qint32) getAveragingValue(m_averagingIndex, m_averagingMode));
	s.writeBool(21, m_linear);

	return s.final();
}

bool GLSpectrumSettings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	int tmp;

	if (d.getVersion() == 1)
    {
		d.readS32(1, &m_fftSize, 1024);
		d.readS32(2, &m_fftOverlap, 0);
		d.readS32(3, &m_fftWindow, FFTWindow::Hanning);
		d.readReal(4, &m_refLevel, 0);
		d.readReal(5, &m_powerRange, 100);
		d.readBool(6, &m_displayWaterfall, true);
		d.readBool(7, &m_invertedWaterfall, false);
		d.readBool(8, &m_displayMaxHold, false);
		d.readBool(9, &m_displayHistogram, false);
		d.readS32(10, &m_decay, 1);
		d.readBool(11, &m_displayGrid, false);
		d.readBool(12, &m_invert, true);
		d.readS32(13, &m_displayGridIntensity, 5);
		d.readS32(14, &m_decayDivisor, 1);
		d.readS32(15, &m_histogramStroke, 30);
		d.readBool(16, &m_displayCurrent, false);
		d.readS32(17, &m_displayTraceIntensity, 50);
		d.readReal(18, &m_waterfallShare, 0.66);
		d.readS32(19, &tmp, 0);
		m_averagingMode = tmp < 0 ? AvgModeNone : tmp > 3 ? AvgModeMax : (AveragingMode) tmp;
		d.readS32(20, &tmp, 0);
		m_averagingIndex = getAveragingIndex(tmp, m_averagingMode);
	    m_averagingNb = getAveragingValue(m_averagingIndex, m_averagingMode);
	    d.readBool(21, &m_linear, false);

		return true;
	}
    else
    {
		resetToDefaults();
		return false;
	}
}

int GLSpectrumSettings::getAveragingMaxScale(AveragingMode averagingMode)
{
    if (averagingMode == AvgModeMoving) {
        return 2;
    } else {
        return 5;
    }
}

int GLSpectrumSettings::getAveragingValue(int averagingIndex, AveragingMode averagingMode)
{
    if (averagingIndex <= 0) {
        return 1;
    }

    int v = averagingIndex - 1;
    int m = pow(10.0, v/3 > getAveragingMaxScale(averagingMode) ? getAveragingMaxScale(averagingMode) : v/3);
    int x = 1;

    if (v % 3 == 0) {
        x = 2;
    } else if (v % 3 == 1) {
        x = 5;
    } else if (v % 3 == 2) {
        x = 10;
    }

    return x * m;
}

int GLSpectrumSettings::getAveragingIndex(int averagingValue, AveragingMode averagingMode)
{
    if (averagingValue <= 1) {
        return 0;
    }

    int v = averagingValue;
    int j = 0;

    for (int i = 0; i <= getAveragingMaxScale(averagingMode); i++)
    {
        if (v < 20)
        {
            if (v < 2) {
                j = 0;
            } else if (v < 5) {
                j = 1;
            } else if (v < 10) {
                j = 2;
            } else {
                j = 3;
            }

            return 3*i + j;
        }

        v /= 10;
    }

    return 3*getAveragingMaxScale(averagingMode) + 3;
}