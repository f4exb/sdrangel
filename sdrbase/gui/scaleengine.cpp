///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#include <math.h>
#include <QFontMetrics>
#include <QDataStream>
#include "gui/scaleengine.h"

/*
static double trunc(double d)
{
	return (d > 0) ? floor(d) : ceil(d);
}
*/

QString ScaleEngine::formatTick(double value, int decimalPlaces, bool fancyTime)
{
	if((m_physicalUnit != Unit::Time) || (!fancyTime) || 1)
	{
		return QString("%1").arg(m_makeOpposite ? -value : value, 0, 'f', decimalPlaces);
	}
	else
	{
		QString str;
		double orig = fabs(value);
		double tmp;

		if(orig >= 86400.0) {
			tmp = floor(value / 86400.0);
			str = QString("%1.").arg(tmp, 0, 'f', 0);
			value -= tmp * 86400.0;
			if(value < 0.0)
				value *= -1.0;
		}

		if(orig >= 3600.0) {
			tmp = floor(value / 3600.0);
			str += QString("%1:").arg(tmp, 2, 'f', 0, QChar('0'));
			value -= tmp * 3600.0;
			if(value < 0.0)
				value *= -1.0;
		}

		if(orig >= 60.0) {
			tmp = floor(value / 60.0);
			str += QString("%1:").arg(tmp, 2, 'f', 0, QChar('0'));
			value -= tmp * 60.0;
			if(value < 0.0)
				value *= -1.0;
		}

		tmp = m_makeOpposite ? -value : value;
		str += QString("%1").arg(tmp, 2, 'f', decimalPlaces, QChar('0'));

		return str;
	}
}

void ScaleEngine::calcCharSize()
{
	QFontMetricsF fontMetrics(m_font);

	if(m_orientation == Qt::Vertical) {
		m_charSize = fontMetrics.height();
	} else {
		QString str("012345679.,-");
		int i;
		float size;
		float max = 0.0f;
		for(i = 0; i < str.length(); i++) {
			size = fontMetrics.width(QString(str[i]));
			if(size > max)
				max = size;
		}
		m_charSize = max;
	}
}

void ScaleEngine::calcScaleFactor()
{
	double median, range, freqBase;

	median = ((m_rangeMax - m_rangeMin) / 2.0) + m_rangeMin;
	range = (m_rangeMax - m_rangeMin);
	freqBase = (median == 0 ? range : median);
	m_scale = 1.0;

	switch(m_physicalUnit) {
		case Unit::None:
			m_unitStr.clear();
			break;

		case Unit::Frequency:
			if(freqBase < 1000.0) {
				m_unitStr = QObject::tr("Hz");
			} else if(freqBase < 1000000.0) {
				m_unitStr = QObject::tr("kHz");
				m_scale = 1000.0;
			} else if(freqBase < 1000000000.0) {
				m_unitStr = QObject::tr("MHz");
				m_scale = 1000000.0;
			} else if(freqBase < 1000000000000.0){
				m_unitStr = QObject::tr("GHz");
				m_scale = 1000000000.0;
			} else {
				m_unitStr = QObject::tr("THz");
				m_scale = 1000000000000.0;
			}
			break;

		case Unit::Information:
			if(median < 1024.0) {
				m_unitStr = QObject::tr("Bytes");
			} else if(median < 1048576.0) {
				m_unitStr = QObject::tr("KiBytes");
				m_scale = 1024.0;
			} else if(median < 1073741824.0) {
				m_unitStr = QObject::tr("MiBytes");
				m_scale = 1048576.0;
			} else if(median < 1099511627776.0) {
				m_unitStr = QObject::tr("GiBytes");
				m_scale = 1073741824.0;
			} else if(median < 1125899906842624.0) {
				m_unitStr = QObject::tr("TiBytes");
				m_scale = 1099511627776.0;
			} else {
				m_unitStr = QObject::tr("PiBytes");
				m_scale = 1125899906842624.0;
			}
			break;

		case Unit::Percent:
			m_unitStr = QString("%");
			break;

		case Unit::Decibel:
			m_unitStr = QString("dB");
			break;

		case Unit::DecibelMilliWatt:
			m_unitStr = QString("dBm");
			break;

		case Unit::DecibelMicroVolt:
			m_unitStr = QString("dBµV");
			break;

		case Unit::AngleDegrees:
			m_unitStr = QString("°");
			break;

		case Unit::Time:
			if(median < 0.001) {
				m_unitStr = QString("µs");
				m_scale = 0.000001;
			} else if(median < 1.0) {
				m_unitStr = QString("ms");
				m_scale = 0.001;
			} else {
				m_unitStr = QString("s");
			}
			break;

		case Unit::Volt:
			if (median < 1e-9) {
				m_unitStr = QString("pV");
				m_scale = 1e-12;
			} else if (median < 1e-6) {
				m_unitStr = QString("nV");
				m_scale = 1e-9;
			} else if (median < 1e-3) {
				m_unitStr = QString("µV");
				m_scale = 1e-6;
			} else if (median < 1.0) {
				m_unitStr = QString("mV");
				m_scale = 1e-3;
			} else {
				m_unitStr = QString("V");
			}
			break;
	}
}

double ScaleEngine::calcMajorTickUnits(double distance, int* retDecimalPlaces)
{
	double sign;
	double log10x;
	double exponent;
	double base;
	int decimalPlaces;

	if(distance == 0.0)
		return 0.0;

	sign = (distance > 0.0) ? 1.0 : -1.0;
	log10x = log10(fabs(distance));
	exponent = floor(log10x);
	base = pow(10.0, log10x - exponent);
	decimalPlaces = (int)(-exponent);
/*
	if((m_physicalUnit == Unit::Time) && (distance >= 1.0)) {
		if(retDecimalPlaces != NULL)
			*retDecimalPlaces = 0;
		if(distance < 1.0)
			return 1.0;
		else if(distance < 5.0)
			return 5.0;
		else if(distance < 10.0)
			return 10.0;
		else if(distance < 15.0)
			return 15.0;
		else if(distance < 30.0)
			return 30.0;
		else if(distance < 60.0)
			return 60.0;
		else if(distance < 5.0 * 60.0)
			return 5.0 * 60.0;
		else if(distance < 10.0 * 60.0)
			return 10.0 * 60.0;
		else if(distance < 15.0 * 60.0)
			return 15.0 * 60.0;
		else if(distance < 30.0 * 60.0)
			return 30.0 * 60.0;
		else if(distance < 3600.0)
			return 3600.0;
		else if(distance < 2.0 * 3600.0)
			return 2.0 * 3600.0;
		else if(distance < 3.0 * 3600.0)
			return 3.0 * 3600.0;
		else if(distance < 6.0 * 3600.0)
			return 6.0 * 3600.0;
		else if(distance < 12.0 * 3600.0)
			return 12.0 * 3600.0;
		else if(distance < 86000.0)
			return 86000.0;
		else if(distance < 2.0 * 86000.0)
			return 2.0 * 86000.0;
		else if(distance < 7.0 * 86000.0)
			return 7.0 * 86000.0;
		else if(distance < 10.0 * 86000.0)
			return 10.0 * 86000.0;
		else if(distance < 30.0 * 86000.0)
			return 30.0 * 86000.0;
		else return 90.0 * 86000.0;
	} else {*/
		if(base <= 1.0) {
			base = 1.0;
		} else if(base <= 2.0) {
			base = 2.0;
		} else if(base <= 2.5) {
			base = 2.5;
			if(decimalPlaces >= 0)
				decimalPlaces++;
		} else if(base <= 5.0) {
			base = 5.0;
		} else {
			base = 10.0;
		}/*
	}*/

	if(retDecimalPlaces != 0) {
		if(decimalPlaces < 0)
			decimalPlaces = 0;
		*retDecimalPlaces = decimalPlaces;
	}

	return sign * base * pow(10.0, exponent);
}

int ScaleEngine::calcTickTextSize()
{
	int tmp;
	int tickLen;
	int decimalPlaces;

	tickLen = 1;
	tmp = formatTick(m_rangeMin / m_scale, 0).length();
	if(tmp > tickLen)
		tickLen = tmp;
	tmp = formatTick(m_rangeMax / m_scale, 0).length();
	if(tmp > tickLen)
		tickLen = tmp;

	calcMajorTickUnits((m_rangeMax - m_rangeMin) / m_scale, &decimalPlaces);

	return tickLen + decimalPlaces + 1;
}

void ScaleEngine::forceTwoTicks()
{
	Tick tick;
	QFontMetricsF fontMetrics(m_font);

	m_tickList.clear();
	tick.major = true;

	tick.pos = getPosFromValue(m_rangeMin);
	tick.text = formatTick(m_rangeMin / m_scale, m_decimalPlaces);
	tick.textSize = fontMetrics.boundingRect(tick.text).width();
	if(m_orientation == Qt::Vertical)
		tick.textPos = tick.pos - fontMetrics.ascent() / 2;
	else tick.textPos = tick.pos - fontMetrics.boundingRect(tick.text).width() / 2;
	m_tickList.append(tick);

	tick.pos = getPosFromValue(m_rangeMax);
	tick.text = formatTick(m_rangeMax / m_scale, m_decimalPlaces);
	tick.textSize = fontMetrics.boundingRect(tick.text).width();
	if(m_orientation == Qt::Vertical)
		tick.textPos = tick.pos - fontMetrics.ascent() / 2;
	else tick.textPos = tick.pos - fontMetrics.boundingRect(tick.text).width() / 2;
	m_tickList.append(tick);
}

void ScaleEngine::reCalc()
{
	float majorTickSize;
	double rangeMinScaled;
	double rangeMaxScaled;
	int maxNumMajorTicks;
	int numMajorTicks;
	int step;
	int skip;
	double value;
	double value2;
	int i;
	int j;
	Tick tick;
	float pos;
	QString str;
	QFontMetricsF fontMetrics(m_font);
	float endPos;
	float lastEndPos;
	bool done;

	if(!m_recalc)
		return;
	m_recalc = false;

	m_tickList.clear();

	calcScaleFactor();
	rangeMinScaled = m_rangeMin / m_scale;
	rangeMaxScaled = m_rangeMax / m_scale;

	if(m_orientation == Qt::Vertical) {
		maxNumMajorTicks = (int)(m_size / (fontMetrics.lineSpacing() * 1.3f));
	} else {
		majorTickSize = (calcTickTextSize() + 2) * m_charSize;
		if(majorTickSize != 0.0)
			maxNumMajorTicks = (int)(m_size / majorTickSize);
			else maxNumMajorTicks = 20;
	}

	m_majorTickValueDistance = calcMajorTickUnits((rangeMaxScaled - rangeMinScaled) / maxNumMajorTicks, &m_decimalPlaces);
	numMajorTicks = (int)((rangeMaxScaled - rangeMinScaled) / m_majorTickValueDistance);

	if(numMajorTicks == 0) {
		forceTwoTicks();
		return;
	}

	if(maxNumMajorTicks > 0)
		m_numMinorTicks = (int)(m_size / (maxNumMajorTicks * fontMetrics.height()));
		else m_numMinorTicks = 0;
	if(m_numMinorTicks < 1)
		m_numMinorTicks = 0;
	else if(m_numMinorTicks < 2)
		m_numMinorTicks = 1;
	else if(m_numMinorTicks < 5)
		m_numMinorTicks = 2;
	else if(m_numMinorTicks < 10)
		m_numMinorTicks = 5;
	else m_numMinorTicks = 10;

	m_firstMajorTickValue = floor(rangeMinScaled / m_majorTickValueDistance) * m_majorTickValueDistance;

	skip = 0;

	if(rangeMinScaled == rangeMaxScaled)
		return;

	while(true) {
		m_tickList.clear();

		step = 0;
		lastEndPos = -100000000;
		done = true;

		for(i = 0; true; i++) {
			value = majorTickValue(i);

			for(j = 1; j < m_numMinorTicks; j++) {
				value2 = value + minorTickValue(j);
				if(value2 < rangeMinScaled)
					continue;
				if(value2 > rangeMaxScaled)
					break;
				pos = getPosFromValue((value + minorTickValue(j)) * m_scale);
				if((pos >= 0) && (pos < m_size)) {
					tick.pos = pos;
					tick.major = false;
					tick.textPos = -1;
					tick.textSize = -1;
					tick.text.clear();
				}
				m_tickList.append(tick);
			}

			pos = getPosFromValue(value * m_scale);
			if(pos < 0.0)
				continue;
			if(pos >= m_size)
				break;

			tick.pos = pos;
			tick.major = true;
			tick.textPos = -1;
			tick.textSize = -1;
			tick.text.clear();

			if(step % (skip + 1) != 0) {
				m_tickList.append(tick);
				step++;
				continue;
			}
			step++;

			str = formatTick(value, m_decimalPlaces);
			tick.text = str;
			tick.textSize = fontMetrics.boundingRect(tick.text).width();
			if(m_orientation == Qt::Vertical) {
				tick.textPos = pos - fontMetrics.ascent() / 2;
				endPos = tick.textPos + fontMetrics.ascent();
			} else {
				tick.textPos = pos - fontMetrics.boundingRect(tick.text).width() / 2;
				endPos = tick.textPos + tick.textSize;
			}

			if(lastEndPos >= tick.textPos) {
				done = false;
				break;
			} else {
				lastEndPos = endPos;
			}

			m_tickList.append(tick);
		}
		if(done)
			break;
		skip++;
	}

	// make sure we have at least two major ticks with numbers
	numMajorTicks = 0;
	for(i = 0; i < m_tickList.count(); i++) {
		tick = m_tickList.at(i);
		if(tick.major)
			numMajorTicks++;
	}
	if(numMajorTicks < 2)
		forceTwoTicks();
}

double ScaleEngine::majorTickValue(int tick)
{
	return m_firstMajorTickValue + (tick * m_majorTickValueDistance);
}

double ScaleEngine::minorTickValue(int tick)
{
	if(m_numMinorTicks < 1)
		return 0.0;
	return (m_majorTickValueDistance * tick) / m_numMinorTicks;
}

ScaleEngine::ScaleEngine() :
	m_orientation(Qt::Horizontal),
	m_charSize(8),
    m_size(1.0f),
    m_physicalUnit(Unit::None),
    m_rangeMin(-1.0),
    m_rangeMax(1.0),
    m_recalc(true),
    m_scale(1.0f),
	m_majorTickValueDistance(1.0),
    m_firstMajorTickValue(1.0),
    m_numMinorTicks(1),
    m_decimalPlaces(1),
    m_makeOpposite(false)
{
}

void ScaleEngine::setOrientation(Qt::Orientation orientation)
{
	m_orientation = orientation;
	m_recalc = true;
}

void ScaleEngine::setFont(const QFont& font)
{
	m_font = font;
	m_recalc = true;
	calcCharSize();
}

void ScaleEngine::setSize(float size)
{
	if(size > 0.0f) {
		m_size = size;
	} else {
		m_size = 1.0f;
	}
	m_recalc = true;
}

void ScaleEngine::setRange(Unit::Physical physicalUnit, float rangeMin, float rangeMax)
{
	double tmpRangeMin;
	double tmpRangeMax;
/*
	if(rangeMin < rangeMax) {
		tmpRangeMin = rangeMin;
		tmpRangeMax = rangeMax;
	} else if(rangeMin > rangeMax) {
		tmpRangeMin = rangeMax;
		tmpRangeMax = rangeMin;
	} else {
		tmpRangeMin = rangeMin * 0.99;
		tmpRangeMax = rangeMin * 1.01 + 0.01;
	}
*/
	tmpRangeMin = rangeMin;
	tmpRangeMax = rangeMax;

	if((tmpRangeMin != m_rangeMin) || (tmpRangeMax != m_rangeMax) || (m_physicalUnit != physicalUnit)) {
		m_physicalUnit = physicalUnit;
		m_rangeMin = tmpRangeMin;
		m_rangeMax = tmpRangeMax;
		m_recalc = true;
	}
}

float ScaleEngine::getPosFromValue(double value)
{
	return ((value - m_rangeMin) / (m_rangeMax - m_rangeMin)) * (m_size - 1.0);
}

float ScaleEngine::getValueFromPos(double pos)
{
	return ((pos * (m_rangeMax - m_rangeMin)) / (m_size - 1.0)) + m_rangeMin;
}

const ScaleEngine::TickList& ScaleEngine::getTickList()
{
	reCalc();
	return m_tickList;
}

QString ScaleEngine::getRangeMinStr()
{
	if(m_unitStr.length() > 0)
		return QString("%1 %2").arg(formatTick(m_rangeMin / m_scale, m_decimalPlaces, false)).arg(m_unitStr);
		else return QString("%1").arg(formatTick(m_rangeMin / m_scale, m_decimalPlaces, false));
}

QString ScaleEngine::getRangeMaxStr()
{
	if(m_unitStr.length() > 0)
		return QString("%1 %2").arg(formatTick(m_rangeMax / m_scale, m_decimalPlaces, false)).arg(m_unitStr);
		else return QString("%1").arg(formatTick(m_rangeMax / m_scale, m_decimalPlaces, false));
}

float ScaleEngine::getScaleWidth()
{
	float max;
	float len;
	int i;

	reCalc();
	max = 0.0f;
	for(i = 0; i < m_tickList.count(); i++) {
		len = m_tickList[i].textSize;
		if(len > max)
			max = len;
	}
	return max;
}
