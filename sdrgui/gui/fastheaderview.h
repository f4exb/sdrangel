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

#ifndef SDRGUI_GUI_FASTHEADERVIEW_H
#define SDRGUI_GUI_FASTHEADERVIEW_H

#include <QHeaderView>

#include "export.h"

// Draws header sections without going through QStyle, for the same reason as FastItemDelegate
class SDRGUI_API FastHeaderView : public QHeaderView {
    Q_OBJECT

public:

    FastHeaderView(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:

    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;

};

#endif // SDRGUI_GUI_FASTHEADERVIEW_H
