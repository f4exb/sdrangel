///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// Subclasses QTextEdit to implement and editingFinished() signal like           //
// QLineEdit's one                                                               //
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

#include "customtextedit.h"

CustomTextEdit::CustomTextEdit(QWidget* parent) : QTextEdit(parent), textChanged(false), trackChange(false)
{
    connect(this,&QTextEdit::textChanged,this,&CustomTextEdit::handleTextChange);
}

void CustomTextEdit::focusInEvent(QFocusEvent* e)
{
    trackChange = true;
    textChanged = false;
    QTextEdit::focusInEvent(e);
}

void CustomTextEdit::focusOutEvent(QFocusEvent *e)
{
    QTextEdit::focusOutEvent(e);
    trackChange = false;

    if (textChanged)
    {
        textChanged = false;
        emit editingFinished();
    }
}

void CustomTextEdit::handleTextChange()
{
    if (trackChange) {
        textChanged = true;
    }
}
