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

#ifndef INCLUDE_PSK31MODREPEATDIALOG_H
#define INCLUDE_PSK31MODREPEATDIALOG_H

#include "ui_psk31modrepeatdialog.h"

class PSK31RepeatDialog : public QDialog {
    Q_OBJECT

public:
    explicit PSK31RepeatDialog(int repeatCount, QWidget* parent = 0);
    ~PSK31RepeatDialog();

    int m_repeatCount;          // Number of times to transmit

private slots:
    void accept();

private:
    Ui::PSK31RepeatDialog* ui;
};

#endif // INCLUDE_PSK31MODREPEATDIALOG_H
