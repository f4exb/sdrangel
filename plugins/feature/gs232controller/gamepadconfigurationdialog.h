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

#ifndef INCLUDE_GAMEPADCONFIGURATIONDIALOG_H
#define INCLUDE_GAMEPADCONFIGURATIONDIALOG_H

#include "ui_gamepadconfigurationdialog.h"

class QGamepad;

class GamepadConfigurationDialog : public QDialog {
    Q_OBJECT

public:
    explicit GamepadConfigurationDialog(QGamepad *gamepad, QWidget* parent = 0);
    ~GamepadConfigurationDialog();

private slots:
    void accept();
    void on_config0_clicked();
    void on_config1_clicked();
    void on_config2_clicked();
    void on_config3_clicked();
    void axisRightXChanged(double value);
    void axisRightYChanged(double value);
    void axisLeftXChanged(double value);
    void axisLeftYChanged(double value);

private:
    Ui::GamepadConfigurationDialog* ui;
    QGamepad *m_gamepad;
};

#endif // INCLUDE_GAMEPADCONFIGURATIONDIALOG_H

