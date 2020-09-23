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

#include "packetmodtxsettingsdialog.h"

PacketModTXSettingsDialog::PacketModTXSettingsDialog(int rampUpBits, int rampDownBits,
        int rampRange, bool modulateWhileRamping, int modulation, int baud,
        int markFrequency, int spaceFrequency,
        bool pulseShaping, float beta, int symbolSpan,
        bool scramble, int polynomial,
        int ax25PreFlags, int ax25PostFlags,
        int ax25Control, int ax25PID,
        int lpfTaps, bool bbNoise, bool rfNoise, bool writeToFile,
        QWidget* parent) :
    QDialog(parent),
    ui(new Ui::PacketModTXSettingsDialog)
{
    ui->setupUi(this);
    ui->rampUp->setValue(rampUpBits);
    ui->rampDown->setValue(rampDownBits);
    ui->rampRange->setValue(rampRange);
    ui->modulateWhileRamping->setChecked(modulateWhileRamping);
    ui->modulation->setCurrentIndex(modulation);
    ui->baud->setValue(baud);
    ui->markFrequency->setValue(markFrequency);
    ui->pulseShaping->setChecked(pulseShaping);
    ui->beta->setValue(beta);
    ui->symbolSpan->setValue(symbolSpan);
    ui->spaceFrequency->setValue(spaceFrequency);
    ui->scramble->setChecked(scramble);
    ui->polynomial->setValue(polynomial);
    ui->ax25PreFlags->setValue(ax25PreFlags);
    ui->ax25PostFlags->setValue(ax25PostFlags);
    ui->ax25Control->setValue(ax25Control);
    ui->ax25PID->setValue(ax25PID);
    ui->lpfTaps->setValue(lpfTaps);
    ui->bbNoise->setChecked(bbNoise);
    ui->rfNoise->setChecked(rfNoise);
    ui->writeToFile->setChecked(writeToFile);
}

PacketModTXSettingsDialog::~PacketModTXSettingsDialog()
{
    delete ui;
}

void PacketModTXSettingsDialog::accept()
{
    m_rampUpBits = ui->rampUp->value();
    m_rampDownBits = ui->rampDown->value();
    m_rampRange = ui->rampRange->value();
    m_modulateWhileRamping = ui->modulateWhileRamping->isChecked();
    m_modulation = ui->modulation->currentIndex();
    m_baud = ui->baud->value();
    m_markFrequency = ui->markFrequency->value();
    m_spaceFrequency = ui->spaceFrequency->value();
    m_pulseShaping = ui->pulseShaping->isChecked();
    m_beta = ui->beta->value();
    m_symbolSpan = ui->symbolSpan->value();
    m_scramble = ui->scramble->isChecked();
    m_polynomial = ui->polynomial->value();
    m_ax25PreFlags = ui->ax25PreFlags->value();
    m_ax25PostFlags = ui->ax25PostFlags->value();
    m_ax25Control = ui->ax25Control->value();
    m_ax25PID = ui->ax25PID->value();
    m_lpfTaps = ui->lpfTaps->value();
    m_bbNoise = ui->bbNoise->isChecked();
    m_rfNoise = ui->rfNoise->isChecked();
    m_writeToFile = ui->writeToFile->isChecked();

    QDialog::accept();
}
