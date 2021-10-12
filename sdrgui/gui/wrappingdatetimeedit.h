///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef SDRGUI_GUI_WRAPPINGDATETIMEEDIT_H
#define SDRGUI_GUI_WRAPPINGDATETIMEEDIT_H

#include <QDateTimeEdit>

#include "export.h"

// Same as QDateTimeEdit, except allows minutes to wrap to hours and hours to
// days when scrolling up or down
class SDRGUI_API WrappingDateTimeEdit : public QDateTimeEdit {

public:
    explicit WrappingDateTimeEdit(QWidget *parent = nullptr);

    void stepBy(int steps) override;

protected:
    void clipAndSetDate(QDate date);
    void clipAndSetDateTime(QDateTime dateTime);
};

#endif // SDRGUI_GUI_WRAPPINGDATETIMEEDIT_H
