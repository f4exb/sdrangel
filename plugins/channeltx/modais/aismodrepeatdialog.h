///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_AISMODREPEATDIALOG_H
#define INCLUDE_AISMODREPEATDIALOG_H

#include "ui_aismodrepeatdialog.h"

class AISModRepeatDialog : public QDialog {
    Q_OBJECT

public:
    explicit AISModRepeatDialog(float repeatDelay, int repeatCount, QWidget* parent = 0);
    ~AISModRepeatDialog();

    float m_repeatDelay;        // Delay in seconds between messages
    int m_repeatCount;          // Number of messages to transmit (-1 = infinite)

private slots:
    void accept();

private:
    Ui::AISModRepeatDialog* ui;
};

#endif // INCLUDE_AISMODREPEATDIALOG_H
