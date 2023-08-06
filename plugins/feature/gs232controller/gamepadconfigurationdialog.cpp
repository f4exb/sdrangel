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
#include "inputcontrollersettings.h"

GamepadConfigurationDialog::GamepadConfigurationDialog(QGamepad *gamepad, InputControllerSettings *settings, bool supportsConfiguraiton, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::GamepadConfigurationDialog),
    m_gamepad(gamepad),
    m_settings(settings)
{
    ui->setupUi(this);
    connect(m_gamepad, &QGamepad::axisRightXChanged, this, &GamepadConfigurationDialog::axisRightXChanged);
    connect(m_gamepad, &QGamepad::axisRightYChanged, this, &GamepadConfigurationDialog::axisRightYChanged);
    connect(m_gamepad, &QGamepad::axisLeftXChanged, this, &GamepadConfigurationDialog::axisLeftXChanged);
    connect(m_gamepad, &QGamepad::axisLeftYChanged, this, &GamepadConfigurationDialog::axisLeftYChanged);
    ui->deadzone0->setValue(settings->m_deadzone[0]);
    ui->deadzone1->setValue(settings->m_deadzone[1]);
    ui->deadzone2->setValue(settings->m_deadzone[2]);
    ui->deadzone3->setValue(settings->m_deadzone[3]);
    ui->lowSensitivity->blockSignals(true);
    ui->lowSensitivity->setRange(0.01, 100);
    ui->lowSensitivity->blockSignals(false);
    ui->lowSensitivity->setValue((double)settings->m_lowSensitivity);
    ui->highSensitivity->blockSignals(true);
    ui->highSensitivity->setRange(0.01, 100);
    ui->highSensitivity->blockSignals(false);
    ui->highSensitivity->setValue((double)settings->m_highSensitivity);
    ui->configurationGroup->setEnabled(supportsConfiguraiton);
}

GamepadConfigurationDialog::~GamepadConfigurationDialog()
{
    delete ui;
}

void GamepadConfigurationDialog::accept()
{
    QDialog::accept();
}

void GamepadConfigurationDialog::on_configReset_clicked()
{
    QGamepadManager::instance()->resetConfiguration(m_gamepad->deviceId());
}

void GamepadConfigurationDialog::on_config0_clicked()
{
    ui->config0->setText("Configuring");
    ui->config0->setEnabled(false);
    ui->config1->setEnabled(false);
    ui->config2->setEnabled(false);
    ui->config3->setEnabled(false);
    QGamepadManager::instance()->configureAxis(m_gamepad->deviceId(), QGamepadManager ::AxisRightX);
}

void GamepadConfigurationDialog::on_config1_clicked()
{
    ui->config1->setText("Configuring");
    ui->config0->setEnabled(false);
    ui->config1->setEnabled(false);
    ui->config2->setEnabled(false);
    ui->config3->setEnabled(false);
    QGamepadManager::instance()->configureAxis(m_gamepad->deviceId(), QGamepadManager ::AxisRightY);
}

void GamepadConfigurationDialog::on_config2_clicked()
{
    ui->config2->setText("Configuring");
    ui->config0->setEnabled(false);
    ui->config1->setEnabled(false);
    ui->config2->setEnabled(false);
    ui->config3->setEnabled(false);
    QGamepadManager::instance()->configureAxis(m_gamepad->deviceId(), QGamepadManager ::AxisLeftX);
}

void GamepadConfigurationDialog::on_config3_clicked()
{
    ui->config3->setText("Configuring");
    ui->config0->setEnabled(false);
    ui->config1->setEnabled(false);
    ui->config2->setEnabled(false);
    ui->config3->setEnabled(false);
    QGamepadManager::instance()->configureAxis(m_gamepad->deviceId(), QGamepadManager ::AxisLeftY);
}

void GamepadConfigurationDialog::axisRightXChanged(double value)
{
    ui->value0->setText(QString::number(value));
    ui->config0->setText("Configure");
    ui->config0->setEnabled(true);
    ui->config1->setEnabled(true);
    ui->config2->setEnabled(true);
    ui->config3->setEnabled(true);
}

void GamepadConfigurationDialog::axisRightYChanged(double value)
{
    ui->value1->setText(QString::number(value));
    ui->config1->setText("Configure");
    ui->config0->setEnabled(true);
    ui->config1->setEnabled(true);
    ui->config2->setEnabled(true);
    ui->config3->setEnabled(true);
}

void GamepadConfigurationDialog::axisLeftXChanged(double value)
{
    ui->value2->setText(QString::number(value));
    ui->config2->setText("Configure");
    ui->config0->setEnabled(true);
    ui->config1->setEnabled(true);
    ui->config2->setEnabled(true);
    ui->config3->setEnabled(true);
}

void GamepadConfigurationDialog::axisLeftYChanged(double value)
{
    ui->value3->setText(QString::number(value));
    ui->config3->setText("Configure");
    ui->config0->setEnabled(true);
    ui->config1->setEnabled(true);
    ui->config2->setEnabled(true);
    ui->config3->setEnabled(true);
}

// Convert double to string and remove trailing zeros
static QString toSimpleFloat(double v, int prec)
{
    QString s = QString::number(v, 'f', prec);
    while (s.back() == '0') {
        s.chop(1);
    }
    if (s.back() == '.') {
        s.chop(1);
    }
    return s;
}

void GamepadConfigurationDialog::on_lowSensitivity_logValueChanged(double value)
{
    m_settings->m_lowSensitivity = value;
    ui->lowSensitivityText->setText(QString("%1%").arg(toSimpleFloat(m_settings->m_lowSensitivity, 4)));
}

void GamepadConfigurationDialog::on_highSensitivity_logValueChanged(double value)
{
    m_settings->m_highSensitivity = value;
    ui->highSensitivityText->setText(QString("%1%").arg(toSimpleFloat(m_settings->m_highSensitivity, 4)));
}

void GamepadConfigurationDialog::on_deadzone0_valueChanged(int value)
{
    m_settings->m_deadzone[0] = value;
    ui->deadzone0Text->setText(QString("%1%").arg(m_settings->m_deadzone[0]));
}

void GamepadConfigurationDialog::on_deadzone1_valueChanged(int value)
{
    m_settings->m_deadzone[1] = value;
    ui->deadzone1Text->setText(QString("%1%").arg(m_settings->m_deadzone[1]));
}

void GamepadConfigurationDialog::on_deadzone2_valueChanged(int value)
{
    m_settings->m_deadzone[2] = value;
    ui->deadzone2Text->setText(QString("%1%").arg(m_settings->m_deadzone[2]));
}

void GamepadConfigurationDialog::on_deadzone3_valueChanged(int value)
{
    m_settings->m_deadzone[3] = value;
    ui->deadzone3Text->setText(QString("%1%").arg(m_settings->m_deadzone[3]));
}
