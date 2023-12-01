///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef SDRGUI_GUI_INT64DELGATE_H
#define SDRGUI_GUI_INT64DELGATE_H

#include <QStyledItemDelegate>

#include "export.h"

// Delegate for table to display a qint64 with input range validation
// Also supports "" as a value
class SDRGUI_API Int64Delegate : public QStyledItemDelegate {

public:
    Int64Delegate();
    Int64Delegate(qint64 min, qint64 max);
    virtual QString displayText(const QVariant &value, const QLocale &locale) const override;
    virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setMin(qint64 min) { m_min = min; }
    void setMax(qint64 max) { m_max = max; }
    void setRange(qint64 min, qint64 max);
    qint64 min() const { return m_min; }
    qint64 max() const { return m_max; }

private:
    qint64 m_min;
    qint64 m_max;

};

#endif // SDRGUI_GUI_INT64DELGATE_H
