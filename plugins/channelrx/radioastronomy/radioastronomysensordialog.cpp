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

#include "util/visa.h"

#include "radioastronomysensordialog.h"

RadioAstronomySensorDialog::RadioAstronomySensorDialog(RadioAstronomySettings *settings, QWidget* parent) :
    QDialog(parent),
    m_settings(settings),
    ui(new Ui::RadioAstronomySensorDialog)
{
    ui->setupUi(this);
    ui->sensor1Enabled->setChecked(settings->m_sensorEnabled[0]);
    ui->sensor1Name->setText(settings->m_sensorName[0]);
    ui->sensor1Device->setText(settings->m_sensorDevice[0]);
    ui->sensor1Init->setPlainText(settings->m_sensorInit[0]);
    ui->sensor1Measure->setText(settings->m_sensorMeasure[0]);
    ui->sensor2Enabled->setChecked(settings->m_sensorEnabled[1]);
    ui->sensor2Name->setText(settings->m_sensorName[1]);
    ui->sensor2Device->setText(settings->m_sensorDevice[1]);
    ui->sensor2Init->setPlainText(settings->m_sensorInit[1]);
    ui->sensor2Measure->setText(settings->m_sensorMeasure[1]);
    ui->period->setValue(settings->m_sensorMeasurePeriod);
    VISA visa;
    if (!visa.isAvailable())
    {
        ui->sensor1Group->setEnabled(false);
        ui->sensor2Group->setEnabled(false);
    }
}

RadioAstronomySensorDialog::~RadioAstronomySensorDialog()
{
    delete ui;
}

void RadioAstronomySensorDialog::accept()
{
    m_settings->m_sensorEnabled[0] = ui->sensor1Enabled->isChecked();
    m_settings->m_sensorName[0] = ui->sensor1Name->text().trimmed();
    m_settings->m_sensorDevice[0] = ui->sensor1Device->text().trimmed();
    m_settings->m_sensorInit[0] = ui->sensor1Init->toPlainText();
    m_settings->m_sensorMeasure[0] = ui->sensor1Measure->text().trimmed();
    m_settings->m_sensorEnabled[1] = ui->sensor2Enabled->isChecked();
    m_settings->m_sensorName[1] = ui->sensor2Name->text().trimmed();
    m_settings->m_sensorDevice[1] = ui->sensor2Device->text().trimmed();
    m_settings->m_sensorInit[1] = ui->sensor2Init->toPlainText();
    m_settings->m_sensorMeasure[1] = ui->sensor2Measure->text().trimmed();
    m_settings->m_sensorMeasurePeriod = ui->period->value();
    QDialog::accept();
}
