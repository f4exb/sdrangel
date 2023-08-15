///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef SDRGUI_GUI_NANOSECONDSDELGATE_H
#define SDRGUI_GUI_NANOSECONDSDELGATE_H

#include <QStyledItemDelegate>

#include "export.h"

// Delegate for table to display a time that's been measured in nanoseconds in s, ms, us or ns
class SDRGUI_API NanoSecondsDelegate : public QStyledItemDelegate {

public:
    NanoSecondsDelegate();
    virtual QString displayText(const QVariant &value, const QLocale &locale) const override;

};

#endif // SDRGUI_GUI_NANOSECONDSDELGATE_H
