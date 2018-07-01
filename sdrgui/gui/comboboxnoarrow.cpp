///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "comboboxnoarrow.h"

void ComboBoxNoArrow::paintEvent (QPaintEvent *ev __attribute__((unused)))
{
    QPainter p;
    p.begin (this);
    QStyleOptionComboBox opt;
    opt.initFrom (this);
    style()->drawPrimitive (QStyle::PE_PanelButtonBevel, &opt, &p, this);
    style()->drawPrimitive (QStyle::PE_PanelButtonCommand, &opt, &p, this);
    style()->drawItemText (&p, rect(), Qt::AlignCenter, palette(), isEnabled(), currentText());
    p.end();
}

