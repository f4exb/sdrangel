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

#include "radioastronomycalibrationdialog.h"

RadioAstronomyCalibrationDialog::RadioAstronomyCalibrationDialog(RadioAstronomySettings *settings, QWidget* parent) :
    QDialog(parent),
    m_settings(settings),
    ui(new Ui::RadioAstronomyCalibrationDialog)
{
    ui->setupUi(this);
    ui->gpioEnabled->setChecked(settings->m_gpioEnabled);
    ui->gpioPin->setValue(settings->m_gpioPin);
    ui->gpioSense->setCurrentIndex(settings->m_gpioSense);
    ui->startCalCommand->setText(settings->m_startCalCommand);
    ui->stopCalCommand->setText(settings->m_stopCalCommand);
    ui->calCommandDelay->setValue(settings->m_calCommandDelay);
}

RadioAstronomyCalibrationDialog::~RadioAstronomyCalibrationDialog()
{
    delete ui;
}

void RadioAstronomyCalibrationDialog::accept()
{
    m_settings->m_gpioEnabled = ui->gpioEnabled->isChecked();
    m_settings->m_gpioPin = ui->gpioPin->value();
    m_settings->m_gpioSense = ui->gpioSense->currentIndex();
    m_settings->m_startCalCommand = ui->stopCalCommand->text().trimmed();
    m_settings->m_stopCalCommand = ui->stopCalCommand->text().trimmed();
    m_settings->m_calCommandDelay = ui->calCommandDelay->value();
    QDialog::accept();
}
