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
#include "gui/scale.h"

Scale::Scale(QWidget* parent) :
	QWidget(parent)
{
}

void Scale::setOrientation(Qt::Orientation orientation)
{
	m_orientation = orientation;
	m_scaleEngine.setOrientation(orientation);
	m_scaleEngine.setFont(font());
	QFontMetrics fm(font());
	switch(m_orientation) {
		case Qt::Horizontal:
			m_scaleEngine.setSize(width());
			setMinimumWidth(0);
			setMaximumWidth(QWIDGETSIZE_MAX);
			setMinimumHeight(3 + fontMetrics().ascent());
			setMaximumHeight(3 + fontMetrics().ascent());
			break;
		case Qt::Vertical:
			m_scaleEngine.setSize(height());
			setMinimumWidth(30);
			setMaximumWidth(30);
			setMinimumHeight(0);
			setMaximumHeight(QWIDGETSIZE_MAX);
			break;
	}
}

void Scale::setRange(Unit::Physical physicalUnit, float rangeMin, float rangeMax)
{
	m_scaleEngine.setRange(physicalUnit, rangeMin, rangeMax);
	update();
}

void Scale::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	const ScaleEngine::TickList& tickList = m_scaleEngine.getTickList();
	QFontMetricsF fontMetrics(font());
	const ScaleEngine::Tick* tick;
	int i;
	float bottomLine;

	switch(m_orientation) {
		case Qt::Horizontal: {
			painter.setPen(Qt::black);

			// Zwischenlinien für x-Achse zeichnen
			for(i = 0; i < tickList.count(); i++) {
				tick = &tickList[i];
				if(!tick->major)
					painter.drawLine(QLineF(tick->pos, 0, tick->pos, 1));
			}

			// Skala am Rand zeichnen
			painter.drawLine(QLineF(0, 0, width() - 1, 0));

			// Hauptlinien und Beschriftung für x-Achse zeichnen
			for(i = 0; i < tickList.count(); i++) {
				tick = &tickList[i];
				if(tick->major) {
					painter.drawLine(QLineF(tick->pos - 1, 0, tick->pos - 1, 3));
					if(tick->textSize > 0) {
						painter.drawText(QPointF(tick->textPos, 3 + fontMetrics.ascent()), tick->text);
					}
				}
			}
			break;
		}
		case Qt::Vertical: {
			bottomLine = height() - 1;
			painter.setPen(Qt::black);

			// Zwischenlinien für y-Achse zeichnen
			for(i = 0; i < tickList.count(); i++) {
				tick = &tickList[i];
				if(!tick->major)
					painter.drawLine(QLineF(width() - 2, bottomLine - tick->pos, width() - 1, bottomLine - tick->pos));
			}

			// Skala am Rand zeichnen
			painter.drawLine(QLineF(width() - 1, 0, width() - 1, height() - 1));

			// Hauptlinien und Beschriftung für y-Achse zeichnen
			for(i = 0; i < tickList.count(); i++) {
				tick = &tickList[i];
				if(tick->major) {
					painter.drawLine(QLineF(width() - 4, bottomLine - tick->pos, width() - 1, bottomLine - tick->pos));
					if(tick->textSize > 0)
						painter.drawText(QPointF(width() - 4 - tick->textSize, bottomLine - tick->textPos), tick->text);
				}
			}

		}
	}
}

void Scale::resizeEvent(QResizeEvent*)
{
	switch(m_orientation) {
		case Qt::Horizontal:
			m_scaleEngine.setSize(width());
			break;
		case Qt::Vertical:
			m_scaleEngine.setSize(height());
			break;
	}
}
