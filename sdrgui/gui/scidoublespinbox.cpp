///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#include "scidoublespinbox.h"

SciDoubleSpinBox::SciDoubleSpinBox(QWidget *parent) :
    QDoubleSpinBox(parent)
{
}

double SciDoubleSpinBox::valueFromText(const QString &text) const
{
    return text.toDouble();
}

QValidator::State SciDoubleSpinBox::validate(QString &input, int &pos) const
{
    (void) pos;

    bool ok;
    input.toDouble(&ok);
    if (ok) {
        return QValidator::Acceptable;
    }

    QString validChars = "0123456789+-.e"; // 'C' locale, so no , separator

    for (int i = 0; i < input.size(); i++)
    {
        if (validChars.indexOf(input[i]) == -1) {
            return QValidator::Invalid;
        }
    }
    if (input.count('e') > 1) {
        return QValidator::Invalid;
    }
    if (input.count('.') > 1) {
        return QValidator::Invalid;
    }
    if (input.count('+') > 2) {
        return QValidator::Invalid;
    }
    if (input.count('-') > 2) {
        return QValidator::Invalid;
    }

    return QValidator::Intermediate;
}
