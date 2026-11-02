///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                              //
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

#ifndef SDRGUI_GUI_FASTITEMDELEGATE_H
#define SDRGUI_GUI_FASTITEMDELEGATE_H

#include <QStyledItemDelegate>

#include "export.h"

// Draws a table cell without going through QStyle.
//
// On Windows, QStyledItemDelegate::paint() asks Windows whether
// high contrast is on - SystemParametersInfo(SPI_GETHIGHCONTRAST), uncached.
// That costs 28-170 us. At one call per cell, a table showing a few hundred cells costs
// 15-50 ms every time it repaints.
//
// So these cells are drawn directly: background, icon, text. Anything with a check box,
// or an editor open, falls back to the full implementation, since those are rare enough
// not to matter and not worth reimplementing.
class SDRGUI_API FastItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:

    explicit FastItemDelegate(QObject *parent = nullptr) :
        QStyledItemDelegate(parent)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

};

#endif // SDRGUI_GUI_FASTITEMDELEGATE_H
