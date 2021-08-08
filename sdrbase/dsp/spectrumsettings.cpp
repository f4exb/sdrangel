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

#include "util/simpleserializer.h"
#include "spectrumsettings.h"

SpectrumSettings::SpectrumSettings()
{
    resetToDefaults();
}

SpectrumSettings::~SpectrumSettings()
{}


void SpectrumSettings::resetToDefaults()
{
	m_fftSize = 1024;
	m_fftOverlap = 0;
	m_fftWindow = FFTWindow::Hanning;
	m_refLevel = 0;
	m_powerRange = 100;
	m_fpsPeriodMs = 50;
	m_decay = 1;
	m_decayDivisor = 1;
	m_histogramStroke = 30;
	m_displayGridIntensity = 5;
	m_displayTraceIntensity = 50;
	m_waterfallShare = 0.66;
	m_displayCurrent = true;
	m_displayWaterfall = true;
	m_invertedWaterfall = true;
	m_displayMaxHold = false;
	m_displayHistogram = false;
	m_displayGrid = false;
	m_averagingMode = AvgModeNone;
	m_averagingIndex = 0;
    m_averagingValue = 1;
	m_linear = false;
    m_ssb = false;
    m_usb = true;
    m_wsSpectrumAddress = "127.0.0.1";
    m_wsSpectrumPort = 8887;
}

QByteArray SpectrumSettings::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(1, m_fftSize);
	s.writeS32(2, m_fftOverlap);
	s.writeS32(3, (int) m_fftWindow);
	s.writeReal(4, m_refLevel);
	s.writeReal(5, m_powerRange);
	s.writeBool(6, m_displayWaterfall);
	s.writeBool(7, m_invertedWaterfall);
	s.writeBool(8, m_displayMaxHold);
	s.writeBool(9, m_displayHistogram);
	s.writeS32(10, m_decay);
	s.writeBool(11, m_displayGrid);
	s.writeS32(13, m_displayGridIntensity);
	s.writeS32(14, m_decayDivisor);
	s.writeS32(15, m_histogramStroke);
	s.writeBool(16, m_displayCurrent);
	s.writeS32(17, m_displayTraceIntensity);
	s.writeReal(18, m_waterfallShare);
	s.writeS32(19, (int) m_averagingMode);
	s.writeS32(20, (qint32) getAveragingValue(m_averagingIndex, m_averagingMode));
	s.writeBool(21, m_linear);
    s.writeString(22, m_wsSpectrumAddress);
    s.writeU32(23, m_wsSpectrumPort);
    s.writeBool(24, m_ssb);
    s.writeBool(25, m_usb);
	s.writeS32(26, m_fpsPeriodMs);
	s.writeS32(100, m_histogramMarkers.size());

	for (int i = 0; i < m_histogramMarkers.size(); i++) {
		s.writeBlob(101+i, m_histogramMarkers[i].serialize());
	}

	s.writeS32(110, m_waterfallMarkers.size());

	for (int i = 0; i < m_waterfallMarkers.size(); i++) {
		s.writeBlob(111+i, m_waterfallMarkers[i].serialize());
	}

	return s.final();
}

bool SpectrumSettings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	int tmp;
    uint32_t utmp;
	QByteArray bytetmp;

	if (d.getVersion() == 1)
    {
		d.readS32(1, &m_fftSize, 1024);
		d.readS32(2, &m_fftOverlap, 0);
        d.readS32(3, &tmp, (int) FFTWindow::Hanning);
		m_fftWindow = (FFTWindow::Function) tmp;
		d.readReal(4, &m_refLevel, 0);
		d.readReal(5, &m_powerRange, 100);
		d.readBool(6, &m_displayWaterfall, true);
		d.readBool(7, &m_invertedWaterfall, true);
		d.readBool(8, &m_displayMaxHold, false);
		d.readBool(9, &m_displayHistogram, false);
		d.readS32(10, &m_decay, 1);
		d.readBool(11, &m_displayGrid, false);
		d.readS32(13, &m_displayGridIntensity, 5);
		d.readS32(14, &m_decayDivisor, 1);
		d.readS32(15, &m_histogramStroke, 30);
		d.readBool(16, &m_displayCurrent, true);
		d.readS32(17, &m_displayTraceIntensity, 50);
		d.readReal(18, &m_waterfallShare, 0.66);
		d.readS32(19, &tmp, 0);
		m_averagingMode = tmp < 0 ? AvgModeNone : tmp > 3 ? AvgModeMax : (AveragingMode) tmp;
		d.readS32(20, &tmp, 0);
		m_averagingIndex = getAveragingIndex(tmp, m_averagingMode);
	    m_averagingValue = getAveragingValue(m_averagingIndex, m_averagingMode);
	    d.readBool(21, &m_linear, false);
        d.readString(22, &m_wsSpectrumAddress, "127.0.0.1");
        d.readU32(23, &utmp, 8887);
        m_wsSpectrumPort = utmp < 1024 ? 1024 : utmp > 65535 ? 65535 : utmp;
        d.readBool(24, &m_ssb, false);
        d.readBool(25, &m_usb, true);
		d.readS32(26, &tmp, 50);
		m_fpsPeriodMs = tmp < 5 ? 5 : tmp > 500 ? 500 : tmp;
		int histogramMarkersSize;

		d.readS32(100, &histogramMarkersSize, 0);
		histogramMarkersSize = histogramMarkersSize < 0 ? 0 :
			histogramMarkersSize > SpectrumHistogramMarker::m_maxNbOfMarkers ?
				SpectrumHistogramMarker::m_maxNbOfMarkers : histogramMarkersSize;
		m_histogramMarkers.clear();

		for (int i = 0; i < histogramMarkersSize; i++)
		{
			d.readBlob(101+i, &bytetmp);
			m_histogramMarkers.push_back(SpectrumHistogramMarker());
			m_histogramMarkers.back().deserialize(bytetmp);
		}

		int waterfallMarkersSize;

		d.readS32(110, &waterfallMarkersSize, 0);
		waterfallMarkersSize = waterfallMarkersSize < 0 ? 0 :
			waterfallMarkersSize > SpectrumWaterfallMarker::m_maxNbOfMarkers ?
				SpectrumWaterfallMarker::m_maxNbOfMarkers : waterfallMarkersSize;
		m_waterfallMarkers.clear();

		for (int i = 0; i < waterfallMarkersSize; i++)
		{
			d.readBlob(111+i, &bytetmp);
			m_waterfallMarkers.push_back(SpectrumWaterfallMarker());
			m_waterfallMarkers.back().deserialize(bytetmp);
		}

		return true;
	}
    else
    {
		resetToDefaults();
		return false;
	}
}

int SpectrumSettings::getAveragingMaxScale(AveragingMode averagingMode)
{
    if (averagingMode == AvgModeMoving) {
        return 3; // max 10k
    } else {
        return 5; // max 1M
    }
}

int SpectrumSettings::getAveragingValue(int averagingIndex, AveragingMode averagingMode)
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

int SpectrumSettings::getAveragingIndex(int averagingValue, AveragingMode averagingMode)
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

uint64_t SpectrumSettings::getMaxAveragingValue(int fftSize, AveragingMode averagingMode)
{
	if (averagingMode == AvgModeMoving)
	{
		uint64_t limit = (1UL<<28) / (sizeof(double)*fftSize); // 256 MB max
		return limit > (1<<14) ? (1<<14) : limit; // limit to 16 kS anyway
	}
	else
	{
		return (1<<20); // fixed 1 MS
	}
}
