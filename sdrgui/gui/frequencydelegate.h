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

#ifndef SDRGUI_GUI_FREQUENCYDELGATE_H
#define SDRGUI_GUI_FREQUENCYDELGATE_H

#include <QStyledItemDelegate>

#include "export.h"

// Delegate for table to display frequency
class SDRGUI_API FrequencyDelegate : public QStyledItemDelegate {

public:
    FrequencyDelegate(QString units = "kHz", int precision=1, bool group=true);
    virtual QString displayText(const QVariant &value, const QLocale &locale) const override;

private:
    QString m_units;
    int m_precision;
    bool m_group;

};

#endif // SDRGUI_GUI_FREQUENCYDELGATE_H
