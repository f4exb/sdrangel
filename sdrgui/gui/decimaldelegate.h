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

#ifndef SDRGUI_GUI_DECIMALDELGATE_H
#define SDRGUI_GUI_DECIMALDELGATE_H

#include <QStyledItemDelegate>

#include "export.h"

// Deligate for table to control precision used to display floating point values - also supports strings
class SDRGUI_API DecimalDelegate : public QStyledItemDelegate {

public:
    DecimalDelegate(int precision = 2);

    virtual QString displayText(const QVariant &value, const QLocale &locale) const override;
    int getPrecision() const { return m_precision; }
    void setPrecision(int precision) { m_precision = precision; }

private:
    int m_precision;

};

#endif // SDRGUI_GUI_DECIMALDELGATE_H
