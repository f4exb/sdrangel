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

#include <QLineEdit>

#include "adsbdemodfeeddialog.h"
#include "adsbdemodsettings.h"

ADSBDemodFeedDialog::ADSBDemodFeedDialog(ADSBDemodSettings *settings, QWidget* parent) :
    QDialog(parent),
    m_settings(settings),
    ui(new Ui::ADSBDemodFeedDialog)
{
    ui->setupUi(this);
    ui->exportClientEnabled->setChecked(m_settings->m_exportClientEnabled);
    ui->exportClientHost->lineEdit()->setText(m_settings->m_exportClientHost);
    ui->exportClientPort->setValue(m_settings->m_exportClientPort);
    ui->exportClientFormat->setCurrentIndex((int)m_settings->m_exportClientFormat);
    ui->exportServerEnabled->setChecked(m_settings->m_exportServerEnabled);
    ui->exportServerPort->setValue(m_settings->m_exportServerPort);
    ui->importEnabled->setChecked(m_settings->m_importEnabled);
    ui->importHost->setCurrentIndex(ui->importHost->findText(m_settings->m_importHost));
    ui->importUsername->setText(m_settings->m_importUsername);
    ui->importPassword->setText(m_settings->m_importPassword);
    ui->importParameters->setText(m_settings->m_importParameters);
    ui->importPeriod->setValue(m_settings->m_importPeriod);
    ui->latitudeMin->setText(m_settings->m_importMinLatitude);
    ui->latitudeMax->setText(m_settings->m_importMaxLatitude);
    ui->longitudeMin->setText(m_settings->m_importMinLongitude);
    ui->longitudeMax->setText(m_settings->m_importMaxLongitude);
}

ADSBDemodFeedDialog::~ADSBDemodFeedDialog()
{
    delete ui;
}

void ADSBDemodFeedDialog::accept()
{
    m_settings->m_exportClientEnabled = ui->exportClientEnabled->isChecked();
    m_settings->m_exportClientHost = ui->exportClientHost->currentText();
    m_settings->m_exportClientPort = ui->exportClientPort->value();
    m_settings->m_exportClientFormat = (ADSBDemodSettings::FeedFormat )ui->exportClientFormat->currentIndex();
    m_settings->m_exportServerEnabled = ui->exportServerEnabled->isChecked();
    m_settings->m_exportServerPort = ui->exportServerPort->value();
    m_settings->m_importEnabled = ui->importEnabled->isChecked();
    m_settings->m_importHost = ui->importHost->currentText();
    m_settings->m_importUsername = ui->importUsername->text();
    m_settings->m_importPassword = ui->importPassword->text();
    m_settings->m_importParameters = ui->importParameters->text();
    m_settings->m_importPeriod = ui->importPeriod->value();
    m_settings->m_importMinLatitude = ui->latitudeMin->text();
    m_settings->m_importMaxLatitude = ui->latitudeMax->text();
    m_settings->m_importMinLongitude = ui->longitudeMin->text();
    m_settings->m_importMaxLongitude = ui->longitudeMax->text();

    QDialog::accept();
}

void ADSBDemodFeedDialog::on_exportClientHost_currentIndexChanged(int value)
{
    if (value == 0)
    {
        ui->exportClientHost->lineEdit()->setText("feed.adsbexchange.com");
        ui->exportClientPort->setValue(30005);
        ui->exportClientFormat->setCurrentIndex(0);
    }
    else if (value == 1)
    {
        ui->exportClientHost->lineEdit()->setText("data.adsbhub.org");
        ui->exportClientPort->setValue(5002);
        ui->exportClientFormat->setCurrentIndex(1);
    }
}
