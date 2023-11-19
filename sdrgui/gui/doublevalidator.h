///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
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

#ifndef SDRGUI_GUI_DOUBLEVALIDATOR_H_
#define SDRGUI_GUI_DOUBLEVALIDATOR_H_

#include <QDoubleValidator>

class DoubleValidator : public QDoubleValidator
{
public:
    DoubleValidator(double bottom, double top, int decimals, QObject * parent) :
        QDoubleValidator(bottom, top, decimals, parent)
    {
    }

    using QDoubleValidator::validate;
    QValidator::State validate(QString &s) const
    {
        if (s.isEmpty() || s == "-") {
            return QValidator::Intermediate;
        }

        QChar decimalPoint = locale().decimalPoint();

        if(s.indexOf(decimalPoint) != -1) {
            int charsAfterPoint = s.length() - s.indexOf(decimalPoint) - 1;

            if (charsAfterPoint > decimals()) {
                return QValidator::Invalid;
            }
        }

        bool ok;
        double d = locale().toDouble(s, &ok);

        if (ok && d >= bottom() && d <= top()) {
            return QValidator::Acceptable;
        } else {
            return QValidator::Invalid;
        }
    }
};

#endif // SDRGUI_GUI_DOUBLEVALIDATOR_H_
