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
#include "gui/indicator.h"

void Indicator::paintEvent(QPaintEvent*)
{
	QPainter painter(this);

	painter.setPen(Qt::black);
	painter.setBrush(m_color);

	painter.drawRect(0, 0, 18, 15);
	painter.drawText(0, 0, 19, 16, Qt::AlignCenter | Qt::AlignHCenter, m_text);
}

QSize Indicator::sizeHint() const
{
	return QSize(20, 16);
}

Indicator::Indicator(const QString& text, QWidget* parent) :
	QWidget(parent),
	m_color(Qt::gray),
	m_text(text)
{
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	QFont f = font();
	f.setBold(true);
	f.setPixelSize(8);
	setFont(f);
}

void Indicator::setColor(const QColor& color)
{
	if(m_color != color) {
		m_color = color;
		update();
	}
}
