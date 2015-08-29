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

#ifndef INCLUDE_INDICATOR_H
#define INCLUDE_INDICATOR_H

#include <QWidget>
#include "util/export.h"

class SDRANGEL_API Indicator : public QWidget {
private:
	Q_OBJECT;

	QColor m_color;
	QString m_text;

protected:
	void paintEvent(QPaintEvent* event);
	QSize sizeHint() const;

public:
	Indicator(const QString& text, QWidget* parent = NULL);

	void setColor(const QColor& color);
};

#endif // INCLUDE_INDICATOR_H
