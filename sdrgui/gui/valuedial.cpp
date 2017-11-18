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

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QLocale>

#include "gui/valuedial.h"

ValueDial::ValueDial(QWidget* parent, ColorMapper colorMapper) :
	QWidget(parent),
	m_animationState(0),
	m_colorMapper(colorMapper)
{
	setAutoFillBackground(false);
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setAttribute(Qt::WA_NoSystemBackground, true);
	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);

	m_background.setStart(0, 0);
	m_background.setFinalStop(0, 1);
	m_background.setCoordinateMode(QGradient::ObjectBoundingMode);

	ColorMapper::colormap::const_iterator cmit = m_colorMapper.getDialBackgroundColorMap().begin();
	ColorMapper::colormap::const_iterator cmitEnd = m_colorMapper.getDialBackgroundColorMap().end();

	for (; cmit != cmitEnd; ++ cmit) {
		m_background.setColorAt(cmit->first, cmit->second);
	}

	m_value =  0;
	m_valueMin = 0;
	m_valueMax = 2200000;
	m_numDigits = 7;
	m_numDecimalPoints = m_numDigits / 3;
	m_cursor = -1;

	m_hightlightedDigit = -1;
	m_text = formatText(m_value);
	m_cursorState = false;

    const QLocale & cLocale = QLocale::c();
    m_groupSeparator = cLocale.groupSeparator();

    connect(&m_animationTimer, SIGNAL(timeout()), this, SLOT(animate()));
	connect(&m_blinkTimer, SIGNAL(timeout()), this, SLOT(blink()));
}

void ValueDial::setFont(const QFont& font)
{
	QWidget::setFont(font);

	QFontMetrics fm(font);
	m_digitWidth = fm.width('0');
	m_digitHeight = fm.ascent();
	if(m_digitWidth < m_digitHeight)
		m_digitWidth = m_digitHeight;
	setFixedWidth((m_numDigits + m_numDecimalPoints) * m_digitWidth + 2);
	setFixedHeight(m_digitHeight * 2 + 2);
}

void ValueDial::setBold(bool bold)
{
	QFont f = font();
	f.setBold(bold);
	setFont(f);
}

void ValueDial::setColorMapper(ColorMapper colorMapper)
{
	m_colorMapper = colorMapper;

	ColorMapper::colormap::const_iterator cmit = m_colorMapper.getDialBackgroundColorMap().begin();
	ColorMapper::colormap::const_iterator cmitEnd = m_colorMapper.getDialBackgroundColorMap().end();

	for (; cmit != cmitEnd; ++ cmit) {
		m_background.setColorAt(cmit->first, cmit->second);
	}
}


void ValueDial::setValue(quint64 value)
{
	m_valueNew = value;
	if(m_valueNew < m_valueMin)
		m_valueNew = m_valueMin;
	else if(m_valueNew > m_valueMax)
		m_valueNew = m_valueMax;
	if(m_valueNew < m_value)
		m_animationState = 1;
	else if(m_valueNew > m_value)
		m_animationState = -1;
	else return;
	m_animationTimer.start(20);
	m_textNew = formatText(m_valueNew);
}

void ValueDial::setValueRange(uint numDigits, quint64 min, quint64 max)
{
	m_numDigits = numDigits;
	//m_numDecimalPoints = m_numDigits / 3;
	m_numDecimalPoints = m_numDigits < 3 ? 0 : (m_numDigits%3) == 0 ? (m_numDigits/3)-1 : m_numDigits/3;

	m_valueMin = min;
	m_valueMax = max;

	if(m_value < min)
		setValue(min);
	else if(m_value > max)
		setValue(max);
	setFixedWidth((m_numDigits + m_numDecimalPoints) * m_digitWidth + 2);
}

quint64 ValueDial::findExponent(int digit)
{
	quint64 e = 1;
	int d = (m_numDigits + m_numDecimalPoints) - digit;
	d = d - (d / 4) - 1;
	for(int i = 0; i < d; i++)
		e *= 10;
	return e;
}

QChar ValueDial::digitNeigh(QChar c, bool dir)
{
	if(dir) {
		if(c == QChar('0'))
			return QChar('9');
		else return QChar::fromLatin1(c.toLatin1() - 1);
	} else {
		if(c == QChar('9'))
			return QChar('0');
		else return QChar::fromLatin1(c.toLatin1() + 1);
	}
}

QString ValueDial::formatText(quint64 value)
{
	QString str = QString("%1").arg(value, m_numDigits, 10, QChar('0'));

	for(int i = 0; i < m_numDecimalPoints; i++)
	{
	    int ipoint = m_numDigits - 3 - 3 * i;

	    if (ipoint != 0) { // do not insert leading point
            const QLocale & cLocale = QLocale::c();
	        str.insert(ipoint, cLocale.groupSeparator());
	    }
	}

	return str;
}

void ValueDial::paintEvent(QPaintEvent*)
{
	QPainter painter(this);

	painter.setPen(Qt::black);
	painter.setBrush(m_background);

	painter.drawRect(0, 0, width() - 1, height() - 1);

	painter.setPen(m_colorMapper.getBoundaryColor());
	painter.setBrush(Qt::NoBrush);
	for(int i = 1; i < m_numDigits + m_numDecimalPoints; i++) {
		painter.setPen(m_colorMapper.getBoundaryColor());
		painter.drawLine(1 + i * m_digitWidth, 1, 1 + i * m_digitWidth, height() - 1);
		painter.setPen(m_colorMapper.getBoundaryAlphaColor());
		painter.drawLine(0 + i * m_digitWidth, 1, 0 + i * m_digitWidth, height() - 1);
		painter.drawLine(2 + i * m_digitWidth, 1, 2 + i * m_digitWidth, height() - 1);
	}
	painter.setPen(m_colorMapper.getBoundaryAlphaColor());
	painter.drawLine(1, 1, 1, height() - 1);
	painter.drawLine(width() - 2, 1, width() - 2, height() - 1);

	// dial borders
    painter.setPen(m_colorMapper.getDarkBorderColor());
    painter.drawLine(0, 0, width() - 2, 0);
    painter.drawLine(0, height() - 1, 0, 0);
	painter.setPen(m_colorMapper.getLightBorderColor());
    painter.drawLine(1, height() - 1, width() - 1, height() - 1);
    painter.drawLine(width() - 1, height() - 1, width() - 1, 0);

	if(m_hightlightedDigit >= 0) {
		painter.setPen(Qt::NoPen);
		painter.setBrush(m_colorMapper.getHighlightColor());
		painter.drawRect(2 + m_hightlightedDigit * m_digitWidth, 1, m_digitWidth - 1, height() - 1);
	}

	if(m_animationState == 0) {
		for(int i = 0; i < m_text.length(); i++) {
			painter.setClipRect(1 + i * m_digitWidth, 1, m_digitWidth, m_digitHeight * 2);
			painter.setPen(m_colorMapper.getSecondaryForegroundColor());
			painter.drawText(QRect(1 + i * m_digitWidth, m_digitHeight * 0.6, m_digitWidth, m_digitHeight), Qt::AlignCenter, m_text.mid(i, 1));
			if(m_text[i] != m_groupSeparator) {
				painter.setPen(m_colorMapper.getForegroundColor());
				painter.drawText(QRect(1 + i * m_digitWidth, m_digitHeight * -0.7, m_digitWidth, m_digitHeight), Qt::AlignCenter, digitNeigh(m_text[i], true));
				painter.drawText(QRect(1 + i * m_digitWidth, m_digitHeight * 1.9, m_digitWidth, m_digitHeight), Qt::AlignCenter, digitNeigh(m_text[i], false));
			}
		}
		painter.setClipping(false);
		if((m_cursor >= 0) && (m_cursorState)) {
			painter.setPen(Qt::NoPen);
			painter.setBrush(m_colorMapper.getSecondaryForegroundColor());
			painter.drawRect(4 + m_cursor * m_digitWidth, 1 + m_digitHeight * 1.5, m_digitWidth - 5, m_digitHeight / 6);
		}
	} else {
		if(m_animationState != 0) {
			for(int i = 0; i < m_text.length(); i++) {
				if(m_text[i] == m_textNew[i]) {
					painter.setClipRect(1 + i * m_digitWidth, 1, m_digitWidth, m_digitHeight * 2);
					painter.setPen(m_colorMapper.getSecondaryForegroundColor());
					painter.drawText(QRect(1 + i * m_digitWidth, m_digitHeight * 0.6, m_digitWidth, m_digitHeight), Qt::AlignCenter, m_text.mid(i, 1));
					if(m_text[i] != m_groupSeparator) {
						painter.setPen(m_colorMapper.getForegroundColor());
						painter.drawText(QRect(1 + i * m_digitWidth, m_digitHeight * -0.7, m_digitWidth, m_digitHeight), Qt::AlignCenter, digitNeigh(m_text[i], true));
						painter.drawText(QRect(1 + i * m_digitWidth, m_digitHeight * 1.9, m_digitWidth, m_digitHeight), Qt::AlignCenter, digitNeigh(m_text[i], false));
					}
				} else {
					int h = m_digitHeight * 0.6 + m_digitHeight * m_animationState / 2.0;
					painter.setClipRect(1 + i * m_digitWidth, 1, m_digitWidth, m_digitHeight * 2);
					painter.setPen(m_colorMapper.getSecondaryForegroundColor());
					painter.drawText(QRect(1 + i * m_digitWidth, h, m_digitWidth, m_digitHeight), Qt::AlignCenter, m_text.mid(i, 1));
					if(m_text[i] != m_groupSeparator) {
						painter.setPen(m_colorMapper.getForegroundColor());
						painter.drawText(QRect(1 + i * m_digitWidth, h + m_digitHeight * -0.7, m_digitWidth, m_digitHeight), Qt::AlignCenter, digitNeigh(m_text[i], true));
						painter.drawText(QRect(1 + i * m_digitWidth, h + m_digitHeight * 1.9, m_digitWidth, m_digitHeight), Qt::AlignCenter, digitNeigh(m_text[i], false));
					}
				}
			}
		}
	}
}

void ValueDial::mousePressEvent(QMouseEvent* event)
{
	int i;

	i = (event->x() - 1) / m_digitWidth;

	if(m_text[i] == m_groupSeparator)
	{
		i++;

		if(i > m_numDigits + m_numDecimalPoints) {
			return;
		}
	}

    Qt::MouseButton mouseButton = event->button();

    if (mouseButton == Qt::RightButton) // ceil value at current digit
    {
        if(m_cursor >= 0)
        {
            m_cursor = -1;
            m_blinkTimer.stop();
            update();
        }

        quint64 e = findExponent(i);
        m_valueNew = (m_value / e) * e;
        setValue(m_valueNew);
        emit changed(m_valueNew);
        //qDebug("ValueDial::mousePressEvent: Qt::RightButton: i: %d e: %llu new: %llu", i, e, valueNew);
    }
    else if (mouseButton == Qt::LeftButton) // set cursor at current digit
    {
        m_cursor = i;
        m_cursorState = true;
        m_blinkTimer.start(400);

        update();
    }
}

void ValueDial::mouseMoveEvent(QMouseEvent* event)
{
	int i;

	i = (event->x() - 1) / m_digitWidth;
	if(m_text[i] == m_groupSeparator)
		i = -1;

	if(i != m_hightlightedDigit) {
		m_hightlightedDigit = i;
		update();
	}
}

void ValueDial::wheelEvent(QWheelEvent* event)
{
	int i;

	i = (event->x() - 1) / m_digitWidth;
	if(m_text[i] != m_groupSeparator)
		m_hightlightedDigit = i;
	else
		return;

	if(m_cursor >= 0) {
		m_cursor = -1;
		m_blinkTimer.stop();
		update();
	}
	quint64 e = findExponent(m_hightlightedDigit);

	if(m_animationState == 0) {
		if(event->delta() < 0) {
            if (event->modifiers() & Qt::ShiftModifier) {
                e *= 5;
            } else if (event->modifiers() & Qt::ControlModifier) {
                e *= 2;
            }
			if(m_value < e)
				m_valueNew = m_valueMin;
			else m_valueNew = m_value - e;
		} else {
            if (event->modifiers() & Qt::ShiftModifier) {
                e *= 5;
            } else if (event->modifiers() & Qt::ControlModifier) {
                e *= 2;
            }
			if(m_valueMax - m_value < e)
				m_valueNew = m_valueMax;
			else m_valueNew = m_value + e;
		}
		setValue(m_valueNew);
		emit changed(m_valueNew);
	}
}

void ValueDial::leaveEvent(QEvent*)
{
	if(m_hightlightedDigit != -1) {
		m_hightlightedDigit = -1;
		update();
	}
}

void ValueDial::keyPressEvent(QKeyEvent* value)
{
	if(m_cursor >= 0) {
		if((value->key() == Qt::Key_Return) || (value->key() == Qt::Key_Enter) || (value->key() == Qt::Key_Escape)) {
			m_cursor = -1;
			m_cursorState = false;
			m_blinkTimer.stop();
			update();
			return;
		}
	}

	if((m_cursor < 0) && (m_hightlightedDigit >= 0)) {
		m_cursor = m_hightlightedDigit;
		if(m_text[m_cursor] == m_groupSeparator)
		   m_cursor++;
		if(m_cursor >= m_numDigits + m_numDecimalPoints)
			return;
		m_cursorState = true;
		m_blinkTimer.start(400);
		update();
	}

	if(m_cursor < 0)
		return;

	if((value->key() == Qt::Key_Left) || (value->key() == Qt::Key_Backspace)) {
		if(m_cursor > 0) {
			m_cursor--;
			if(m_text[m_cursor] == m_groupSeparator)
				m_cursor--;
			if(m_cursor < 0)
				m_cursor++;
			m_cursorState = true;
			update();
			return;
		}
	} else if(value->key() == Qt::Key_Right) {
		if(m_cursor < m_numDecimalPoints + m_numDigits) {
			m_cursor++;
			if(m_text[m_cursor] == m_groupSeparator)
				m_cursor++;
			if(m_cursor >= m_numDecimalPoints + m_numDigits)
				m_cursor--;
			m_cursorState = true;
			update();
			return;
		}
	} else if(value->key() == Qt::Key_Up) {
		quint64 e = findExponent(m_cursor);
        if (value->modifiers() & Qt::ShiftModifier) {
            e *= 5;
        } else if (value->modifiers() & Qt::ControlModifier) {
            e *= 2;
        }
		if(m_animationState != 0)
			m_value = m_valueNew;
		if(m_valueMax - m_value < e)
			m_valueNew = m_valueMax;
		else m_valueNew = m_value + e;
		setValue(m_valueNew);
		emit changed(m_valueNew);
	} else if(value->key() == Qt::Key_Down) {
		quint64 e = findExponent(m_cursor);
        if (value->modifiers() & Qt::ShiftModifier) {
            e *= 5;
        } else if (value->modifiers() & Qt::ControlModifier) {
            e *= 2;
        }
		if(m_animationState != 0)
			m_value = m_valueNew;
		if(m_value < e)
			m_valueNew = m_valueMin;
		else m_valueNew = m_value - e;
		setValue(m_valueNew);
		emit changed(m_valueNew);
	}

	if(value->text().length() != 1)
		return;
	QChar c = value->text()[0];

	if(c >= QChar('0') && (c <= QChar('9'))) {
		int d = c.toLatin1() - '0';
		quint64 e = findExponent(m_cursor);
		quint64 v = (m_value / e) % 10;
		if(m_animationState != 0)
			m_value = m_valueNew;
		v = m_value - v * e;
		v += d * e;
		setValue(v);
		emit changed(m_valueNew);
		m_cursor++;
		if(m_text[m_cursor] == m_groupSeparator)
		   m_cursor++;
		if(m_cursor >= m_numDigits + m_numDecimalPoints) {
			m_cursor = -1;
			m_blinkTimer.stop();
		} else {
			m_cursorState = true;
		}
		update();
	}
}

void ValueDial::focusInEvent(QFocusEvent*)
{
	if(m_cursor == -1) {
		m_cursor = 0;
		m_cursorState = true;
		m_blinkTimer.start(400);
		update();
	}
}

void ValueDial::focusOutEvent(QFocusEvent*)
{
	m_cursor = -1;
	m_blinkTimer.stop();
	update();
}

void ValueDial::animate()
{
	update();

	if(m_animationState > 0)
		m_animationState++;
	else if(m_animationState < 0)
		m_animationState--;
	else {
		m_animationTimer.stop();
		m_animationState = 0;
		return;
	}

	if(abs(m_animationState) >= 4) {
		m_animationState = 0;
		m_animationTimer.stop();
		m_value = m_valueNew;
		m_text = m_textNew;
	}
}

void ValueDial::blink()
{
	if(m_cursor >= 0) {
		m_cursorState = !m_cursorState;
		update();
	}
}
