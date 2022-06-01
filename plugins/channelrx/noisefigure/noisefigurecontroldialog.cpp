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

#include <QDebug>

#include "noisefigurecontroldialog.h"

NoiseFigureControlDialog::NoiseFigureControlDialog(NoiseFigureSettings *settings, QWidget* parent) :
    QDialog(parent),
    m_settings(settings),
    ui(new Ui::NoiseFigureControlDialog)
{
    ui->setupUi(this);
    ui->powerOnCommand->setText(settings->m_powerOnCommand);
    ui->powerOffCommand->setText(settings->m_powerOffCommand);
    ui->device->setText(settings->m_visaDevice);
    ui->powerOnSCPI->setPlainText(settings->m_powerOnSCPI);
    ui->powerOffSCPI->setPlainText(settings->m_powerOffSCPI);
    ui->delay->setValue(settings->m_powerDelay);
}

NoiseFigureControlDialog::~NoiseFigureControlDialog()
{
    delete ui;
}

void NoiseFigureControlDialog::accept()
{
    m_settings->m_powerOnCommand = ui->powerOnCommand->text().trimmed();
    m_settings->m_powerOffCommand = ui->powerOffCommand->text().trimmed();
    m_settings->m_visaDevice = ui->device->text().trimmed();
    m_settings->m_powerOnSCPI = ui->powerOnSCPI->toPlainText();
    m_settings->m_powerOffSCPI = ui->powerOffSCPI->toPlainText();
    m_settings->m_powerDelay = ui->delay->value();
    QDialog::accept();
}
