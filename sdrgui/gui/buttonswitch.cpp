///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2017, 2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>       //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#include <QPainter>
#include "gui/buttonswitch.h"

ButtonSwitch::ButtonSwitch(QWidget* parent) :
	QToolButton(parent)
{
	setCheckable(true);
    setStyleSheet(QString("QToolButton{ background-color: %1; } QToolButton:checked{ background-color: %2; }")
        .arg(palette().button().color().name())
        .arg(palette().highlight().color().darker(150).name()));
}

void ButtonSwitch::doToggle(bool checked)
{
    setChecked(checked);
}

void ButtonSwitch::setColor(QColor color)
{
    setStyleSheet(QString("QToolButton{ background-color: %1; }").arg(color.name()));
}

void ButtonSwitch::resetColor()
{
    setStyleSheet(QString("QToolButton{ background-color: %1; } QToolButton:checked{ background-color: %2; }")
        .arg(palette().button().color().name())
        .arg(palette().highlight().color().darker(150).name()));
}
