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

#include "ieee_802_15_4_modtxsettingsdialog.h"

IEEE_802_15_4_ModTXSettingsDialog::IEEE_802_15_4_ModTXSettingsDialog(int rampUpBits, int rampDownBits,
        int rampRange, bool modulateWhileRamping, int modulation, int bitRate,
        int pulseShaping, float beta, int symbolSpan,
        bool scramble, int polynomial,
        int lpfTaps, bool bbNoise, bool writeToFile,
        QWidget* parent) :
    QDialog(parent),
    ui(new Ui::IEEE_802_15_4_ModTXSettingsDialog)
{
    ui->setupUi(this);
    ui->rampUp->setValue(rampUpBits);
    ui->rampDown->setValue(rampDownBits);
    ui->rampRange->setValue(rampRange);
    ui->modulateWhileRamping->setChecked(modulateWhileRamping);
    ui->modulation->setCurrentIndex(modulation);
    ui->bitRate->setValue(bitRate);
    ui->pulseShaping->setCurrentIndex(pulseShaping);
    ui->beta->setValue(beta);
    ui->symbolSpan->setValue(symbolSpan);
    ui->scramble->setChecked(scramble);
    ui->polynomial->setValue(polynomial);
    ui->lpfTaps->setValue(lpfTaps);
    ui->bbNoise->setChecked(bbNoise);
    ui->writeToFile->setChecked(writeToFile);
}

IEEE_802_15_4_ModTXSettingsDialog::~IEEE_802_15_4_ModTXSettingsDialog()
{
    delete ui;
}

void IEEE_802_15_4_ModTXSettingsDialog::accept()
{
    m_rampUpBits = ui->rampUp->value();
    m_rampDownBits = ui->rampDown->value();
    m_rampRange = ui->rampRange->value();
    m_modulateWhileRamping = ui->modulateWhileRamping->isChecked();
    m_modulation = ui->modulation->currentIndex();
    m_bitRate = ui->bitRate->value();
    m_pulseShaping = ui->pulseShaping->currentIndex();
    m_beta = ui->beta->value();
    m_symbolSpan = ui->symbolSpan->value();
    m_scramble = ui->scramble->isChecked();
    m_polynomial = ui->polynomial->value();
    m_lpfTaps = ui->lpfTaps->value();
    m_bbNoise = ui->bbNoise->isChecked();
    m_writeToFile = ui->writeToFile->isChecked();

    QDialog::accept();
}
