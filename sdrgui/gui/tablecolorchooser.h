///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023-2024 Jon Beniston, M7RCE                                   //
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

#ifndef INCLUDE_GUI_TABLECOLORCHOOSER_H
#define INCLUDE_GUI_TABLECOLORCHOOSER_H

#include <QObject>

#include "export.h"

class QTableWidget;
class QToolButton;

// An widget for use in tables, that displays a color, and when clicked, opens a ColorDialog, allowing the user to select a color
class SDRGUI_API TableColorChooser : public QObject {
    Q_OBJECT
public:

    TableColorChooser(QTableWidget *table, int row, int col, bool noColor, quint32 color);

public slots:
    void on_color_clicked();

private:
    QToolButton *m_colorButton;

public:
    // Have copies of settings, so we don't change unless main dialog is accepted
    bool m_noColor;
    quint32 m_color;

};

#endif // INCLUDE_GUI_TABLECOLORDIALOG_H
