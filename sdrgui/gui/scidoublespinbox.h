///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2016, 2018-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2020, 2022 Jon Beniston, M7RCE <jon@beniston.com>               //
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

#ifndef SDRGUI_GUI_SCIDOUBLESPINBOX_H
#define SDRGUI_GUI_SCIDOUBLESPINBOX_H

#include <QDoubleSpinBox>

#include "export.h"

// Scientific double spin box.
// Like a double spin box, except that we additionally scientific E notation for entry
// (E.g. 4.5e-4)
class SDRGUI_API SciDoubleSpinBox : public QDoubleSpinBox {
    Q_OBJECT

public:

    explicit SciDoubleSpinBox(QWidget *parent = nullptr);

    virtual double valueFromText(const QString &text) const;
    virtual QValidator::State validate(QString &input, int &pos) const;
};

#endif // SDRGUI_GUI_SCIDOUBLESPINBOX_H
