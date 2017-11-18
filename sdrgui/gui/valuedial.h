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

#include <QWidget>
#include <QTimer>
#include "gui/colormapper.h"
#include "util/export.h"

class SDRANGEL_API ValueDial : public QWidget {
	Q_OBJECT

public:
	ValueDial(QWidget* parent = NULL, ColorMapper colorMapper = ColorMapper(ColorMapper::Normal));

	void setValue(quint64 value);
	void setValueRange(uint numDigits, quint64 min, quint64 max);
	void setFont(const QFont& font);
	void setBold(bool bold);
	void setColorMapper(ColorMapper colorMapper);
	quint64 getValue() const { return m_value; }
	quint64 getValueNew() const { return m_valueNew; }

signals:
	void changed(quint64 value);

private:
	QLinearGradient m_background;
	int m_numDigits;
	int m_numDecimalPoints;
	int m_digitWidth;
	int m_digitHeight;
	int m_hightlightedDigit;
	int m_cursor;
	bool m_cursorState;
	quint64 m_value;
	quint64 m_valueMax;
	quint64 m_valueMin;
	QString m_text;

	quint64 m_valueNew;
	QString m_textNew;
	int m_animationState;
	QTimer m_animationTimer;
	QTimer m_blinkTimer;
	QChar m_groupSeparator;

	ColorMapper m_colorMapper;

	quint64 findExponent(int digit);
	QChar digitNeigh(QChar c, bool dir);
	QString formatText(quint64 value);

	void paintEvent(QPaintEvent*);

	void mousePressEvent(QMouseEvent*);
	void mouseMoveEvent(QMouseEvent*);
	void wheelEvent(QWheelEvent*);
	void leaveEvent(QEvent*);
	void keyPressEvent(QKeyEvent*);
	void focusInEvent(QFocusEvent*);
	void focusOutEvent(QFocusEvent*);

private slots:
	void animate();
	void blink();
};
