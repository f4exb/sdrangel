///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>               //
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
struct InputControllerSettings;

class GamepadConfigurationDialog : public QDialog {
    Q_OBJECT

public:
    explicit GamepadConfigurationDialog(QGamepad *gamepad, InputControllerSettings *settings, bool supportsConfiguraiton, QWidget* parent = 0);
    ~GamepadConfigurationDialog();

private slots:
    void accept();
    void on_configReset_clicked();
    void on_config0_clicked();
    void on_config1_clicked();
    void on_config2_clicked();
    void on_config3_clicked();
    void axisRightXChanged(double value);
    void axisRightYChanged(double value);
    void axisLeftXChanged(double value);
    void axisLeftYChanged(double value);
    void on_lowSensitivity_logValueChanged(double value);
    void on_highSensitivity_logValueChanged(double value);
    void on_deadzone0_valueChanged(int value);
    void on_deadzone1_valueChanged(int value);
    void on_deadzone2_valueChanged(int value);
    void on_deadzone3_valueChanged(int value);

private:
    Ui::GamepadConfigurationDialog* ui;
    QGamepad *m_gamepad;
    InputControllerSettings *m_settings;
};

#endif // INCLUDE_GAMEPADCONFIGURATIONDIALOG_H

