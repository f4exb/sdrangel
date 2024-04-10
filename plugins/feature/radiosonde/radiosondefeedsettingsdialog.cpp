///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE                                        //
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

#include "radiosondefeedsettingsdialog.h"

RadiosondeFeedSettingsDialog::RadiosondeFeedSettingsDialog(RadiosondeSettings *settings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::RadiosondeFeedSettingsDialog),
    m_settings(settings)
{
    ui->setupUi(this);

    ui->callsign->setText(m_settings->m_callsign);
    ui->antenna->setText(m_settings->m_antenna);
    ui->displayPosition->setChecked(m_settings->m_displayPosition);
    ui->mobile->setChecked(m_settings->m_mobile);
    ui->email->setText(m_settings->m_email);
}

RadiosondeFeedSettingsDialog::~RadiosondeFeedSettingsDialog()
{
    delete ui;
}

void RadiosondeFeedSettingsDialog::accept()
{
    m_settings->m_callsign = ui->callsign->text();
    m_settings->m_antenna = ui->antenna->text();
    m_settings->m_displayPosition = ui->displayPosition->isChecked();
    m_settings->m_mobile = ui->mobile->isChecked();
    m_settings->m_email = ui->email->text();

    QDialog::accept();
}
