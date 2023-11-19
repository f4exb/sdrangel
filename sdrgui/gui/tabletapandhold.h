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

#ifndef SDRGUI_GUI_TABLETAPANDHOLD_H
#define SDRGUI_GUI_TABLETAPANDHOLD_H

#include "export.h"

class QTableWidget;

// Emits a signal when tap and hold gesture occurs on a table
// Position passed with tapAndHold signal is adjusted by table header size,
// so can be used to call QTableWidget::itemAt(itemPos)
class SDRGUI_API TableTapAndHold : public QObject {
    Q_OBJECT
public:
    TableTapAndHold(QTableWidget *table);

signals:
    void tapAndHold(const QPoint& itemPos);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QTableWidget* m_table;
};

#endif /* SDRGUI_GUI_TABLETAPANDHOLD_H */
