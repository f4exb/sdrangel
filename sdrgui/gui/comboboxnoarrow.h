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

#ifndef SDRGUI_GUI_COMBOBOXNOARROW_H_
#define SDRGUI_GUI_COMBOBOXNOARROW_H_

#include <QComboBox>

///////////////////////////////////////////////////////////////////////////////////////////////////
/// This class is a custom QComboBox which does NOT display the down arrow. The down arrow takes
/// a lot of real estate when you're trying to make them narrow. So much real estate that you can't
/// see short lines of text such as "CH 1" without the digit cut off. The only thing that this
/// custom widget does is to override the paint function. The new paint function draws the combo
/// box (using all style sheet info) without the down arrow.
///////////////////////////////////////////////////////////////////////////////////////////////////
class ComboBoxNoArrow : public QComboBox
{
    Q_OBJECT
public:
    ComboBoxNoArrow (QWidget *parent) : QComboBox(parent) {}
    virtual ~ComboBoxNoArrow() {}
    void  paintEvent (QPaintEvent *ev);
};

#endif /* SDRGUI_GUI_COMBOBOXNOARROW_H_ */
