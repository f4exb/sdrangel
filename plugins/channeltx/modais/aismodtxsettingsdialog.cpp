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

#include "aismodtxsettingsdialog.h"

AISModTXSettingsDialog::AISModTXSettingsDialog(int rampUpBits, int rampDownBits,
        int rampRange,
        int baud, int symbolSpan,
        bool rfNoise, bool writeToFile,
        QWidget* parent) :
    QDialog(parent),
    ui(new Ui::AISModTXSettingsDialog)
{
    ui->setupUi(this);
    ui->rampUp->setValue(rampUpBits);
    ui->rampDown->setValue(rampDownBits);
    ui->rampRange->setValue(rampRange);
    ui->baud->setValue(baud);
    ui->symbolSpan->setValue(symbolSpan);
    ui->rfNoise->setChecked(rfNoise);
    ui->writeToFile->setChecked(writeToFile);
}

AISModTXSettingsDialog::~AISModTXSettingsDialog()
{
    delete ui;
}

void AISModTXSettingsDialog::accept()
{
    m_rampUpBits = ui->rampUp->value();
    m_rampDownBits = ui->rampDown->value();
    m_rampRange = ui->rampRange->value();
    m_baud = ui->baud->value();
    m_symbolSpan = ui->symbolSpan->value();
    m_rfNoise = ui->rfNoise->isChecked();
    m_writeToFile = ui->writeToFile->isChecked();

    QDialog::accept();
}
