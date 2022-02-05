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

#ifndef INCLUDE_APTDEMODSELECTDIALOG_H
#define INCLUDE_APTDEMODSELECTDIALOG_H

#include "ui_aptdemodselectdialog.h"
#include "aptdemodsettings.h"

class APTDemodSelectDialog : public QDialog {
    Q_OBJECT

public:
    explicit APTDemodSelectDialog(const QStringList &list, QWidget* parent = 0);
    ~APTDemodSelectDialog();
    QStringList getSelected() const { return m_selected; }

private slots:
    void accept();

private:
    QStringList m_selected;
    Ui::APTDemodSelectDialog* ui;
};

#endif // INCLUDE_APTDEMODSELECTDIALOG_H
