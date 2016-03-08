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
#include "gui/scaleengine.h"
#include "util/export.h"

class SDRANGEL_API Scale : public QWidget {
	Q_OBJECT

public:
	Scale(QWidget* parent = NULL);

	void setOrientation(Qt::Orientation orientation);
	void setRange(Unit::Physical physicalUnit, float rangeMin, float rangeMax);

private:
	Qt::Orientation m_orientation;
	ScaleEngine m_scaleEngine;

	void paintEvent(QPaintEvent*);
	void resizeEvent(QResizeEvent*);
};
