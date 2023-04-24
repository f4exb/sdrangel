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

#include <QGamepadManager>
#include <QGamepad>

#include "gamepadconfigurationdialog.h"

GamepadConfigurationDialog::GamepadConfigurationDialog(QGamepad *gamepad, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::GamepadConfigurationDialog),
    m_gamepad(gamepad)
{
    ui->setupUi(this);
    connect(m_gamepad, &QGamepad::axisRightXChanged, this, &GamepadConfigurationDialog::axisRightXChanged);
    connect(m_gamepad, &QGamepad::axisRightYChanged, this, &GamepadConfigurationDialog::axisRightYChanged);
    connect(m_gamepad, &QGamepad::axisLeftXChanged, this, &GamepadConfigurationDialog::axisLeftXChanged);
    connect(m_gamepad, &QGamepad::axisLeftYChanged, this, &GamepadConfigurationDialog::axisLeftYChanged);
}

GamepadConfigurationDialog::~GamepadConfigurationDialog()
{
    delete ui;
}

void GamepadConfigurationDialog::accept()
{
    QDialog::accept();
}

void GamepadConfigurationDialog::on_config0_clicked()
{
    if (ui->config0->text() == "Configure")
    {
        ui->config0->setText("Done");
        ui->config1->setEnabled(false);
        ui->config2->setEnabled(false);
        ui->config3->setEnabled(false);
        QGamepadManager::instance()->configureAxis(m_gamepad->deviceId(), QGamepadManager ::AxisRightX);
    }
    else
    {
        ui->config0->setText("Configure");
        ui->config1->setEnabled(true);
        ui->config2->setEnabled(true);
        ui->config3->setEnabled(true);
    }
}

void GamepadConfigurationDialog::on_config1_clicked()
{
    if (ui->config1->text() == "Configure")
    {
        ui->config1->setText("Done");
        ui->config0->setEnabled(false);
        ui->config2->setEnabled(false);
        ui->config3->setEnabled(false);
        QGamepadManager::instance()->configureAxis(m_gamepad->deviceId(), QGamepadManager ::AxisRightY);
    }
    else
    {
        ui->config1->setText("Configure");
        ui->config0->setEnabled(true);
        ui->config2->setEnabled(true);
        ui->config3->setEnabled(true);
    }
}

void GamepadConfigurationDialog::on_config2_clicked()
{
    if (ui->config2->text() == "Configure")
    {
        ui->config2->setText("Done");
        ui->config0->setEnabled(false);
        ui->config1->setEnabled(false);
        ui->config3->setEnabled(false);
        QGamepadManager::instance()->configureAxis(m_gamepad->deviceId(), QGamepadManager ::AxisLeftX);
    }
    else
    {
        ui->config2->setText("Configure");
        ui->config0->setEnabled(true);
        ui->config1->setEnabled(true);
        ui->config3->setEnabled(true);
    }
}

void GamepadConfigurationDialog::on_config3_clicked()
{
    if (ui->config3->text() == "Configure")
    {
        ui->config3->setText("Done");
        ui->config0->setEnabled(false);
        ui->config1->setEnabled(false);
        ui->config2->setEnabled(false);
        QGamepadManager::instance()->configureAxis(m_gamepad->deviceId(), QGamepadManager ::AxisLeftY);
    }
    else
    {
        ui->config3->setText("Configure");
        ui->config0->setEnabled(true);
        ui->config1->setEnabled(true);
        ui->config2->setEnabled(true);
    }
}

void GamepadConfigurationDialog::axisRightXChanged(double value)
{
    ui->value0->setText(QString::number(value));
}

void GamepadConfigurationDialog::axisRightYChanged(double value)
{
    ui->value1->setText(QString::number(value));
}

void GamepadConfigurationDialog::axisLeftXChanged(double value)
{
    ui->value2->setText(QString::number(value));
}

void GamepadConfigurationDialog::axisLeftYChanged(double value)
{
    ui->value3->setText(QString::number(value));
}
