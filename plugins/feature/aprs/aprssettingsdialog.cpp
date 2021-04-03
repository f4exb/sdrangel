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

#include "aprssettingsdialog.h"

APRSSettingsDialog::APRSSettingsDialog(APRSSettings* settings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::APRSSettingsDialog),
    m_settings(settings)
{
    ui->setupUi(this);
    ui->igateServer->setCurrentText(m_settings->m_igateServer);
    ui->igateCallsign->setText(m_settings->m_igateCallsign);
    ui->igatePasscode->setText(m_settings->m_igatePasscode);
    ui->igateFilter->setText(m_settings->m_igateFilter);
    ui->altitudeUnits->setCurrentIndex((int)m_settings->m_altitudeUnits);
    ui->speedUnits->setCurrentIndex((int)m_settings->m_speedUnits);
    ui->temperatureUnits->setCurrentIndex((int)m_settings->m_temperatureUnits);
    ui->rainfallUnits->setCurrentIndex((int)m_settings->m_rainfallUnits);
}

APRSSettingsDialog::~APRSSettingsDialog()
{
    delete ui;
}

void APRSSettingsDialog::accept()
{
    m_settings->m_igateServer = ui->igateServer->currentText();
    m_settings->m_igateCallsign = ui->igateCallsign->text();
    m_settings->m_igatePasscode = ui->igatePasscode->text();
    m_settings->m_igateFilter = ui->igateFilter->text();
    m_settings->m_altitudeUnits = (APRSSettings::AltitudeUnits)ui->altitudeUnits->currentIndex();
    m_settings->m_speedUnits = (APRSSettings::SpeedUnits)ui->speedUnits->currentIndex();
    m_settings->m_temperatureUnits = (APRSSettings::TemperatureUnits)ui->temperatureUnits->currentIndex();
    m_settings->m_rainfallUnits = (APRSSettings::RainfallUnits)ui->rainfallUnits->currentIndex();
    QDialog::accept();
}
