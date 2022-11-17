///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// Same as ValueDial but handles optionally positive and negative numbers with   //
// sign display.                                                                 //
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

#include <QWidget>
#include <QTimer>
#include "gui/colormapper.h"
#include "export.h"

class SDRGUI_API ValueDialZ : public QWidget {
	Q_OBJECT

public:
	ValueDialZ(bool positiveOnly = true, QWidget* parent = NULL, ColorMapper colorMapper = ColorMapper(ColorMapper::Normal));

	void setValue(qint64 value);
	void setValueRange(bool positiveOnly, uint numDigits, qint64 min, qint64 max, int decimalPos = 0);
	void setFont(const QFont& font);
	void setBold(bool bold);
	void setColorMapper(ColorMapper colorMapper);
	qint64 getValue() const { return m_value; }
	qint64 getValueNew() const { return m_valueNew; }

signals:
	void changed(qint64 value);

private:
	QLinearGradient m_background;
	int m_numDigits;
	int m_numThousandPoints;
	int m_digitWidth;
	int m_digitHeight;
	int m_hightlightedDigit;
	int m_cursor;
	bool m_cursorState;
	qint64 m_value;
	qint64 m_valueMax;
	qint64 m_valueMin;
	bool m_positiveOnly;
	int m_decimalPos; //!< for fixed point
	QString m_text;

	qint64 m_valueNew;
	QString m_textNew;
	int m_animationState;
	QTimer m_animationTimer;
	QTimer m_blinkTimer;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QString m_groupSeparator;
	QString m_decSeparator;
#else
	QChar m_groupSeparator;
	QChar m_decSeparator;
#endif

	ColorMapper m_colorMapper;

    quint64 findExponent(int digit);
	QChar digitNeigh(QChar c, bool dir);
	QString formatText(qint64 value);

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
