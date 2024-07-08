///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#include "wdsprxeqdialog.h"
#include "ui_wdsprxeqdialog.h"

WDSPRxEqDialog::WDSPRxEqDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::WDSPRxEqDialog)
{
    ui->setupUi(this);
}

WDSPRxEqDialog::~WDSPRxEqDialog()
{
    delete ui;
}

void WDSPRxEqDialog::setEqF(const std::array<float, 11>& eqF)
{
    m_eqF = eqF;
    ui->f1->blockSignals(true);
    ui->f2->blockSignals(true);
    ui->f3->blockSignals(true);
    ui->f4->blockSignals(true);
    ui->f5->blockSignals(true);
    ui->f6->blockSignals(true);
    ui->f7->blockSignals(true);
    ui->f8->blockSignals(true);
    ui->f9->blockSignals(true);
    ui->f10->blockSignals(true);
    ui->f1->setValue((int) m_eqF[1]);
    ui->f2->setValue((int) m_eqF[2]);
    ui->f3->setValue((int) m_eqF[3]);
    ui->f4->setValue((int) m_eqF[4]);
    ui->f5->setValue((int) m_eqF[5]);
    ui->f6->setValue((int) m_eqF[6]);
    ui->f7->setValue((int) m_eqF[7]);
    ui->f8->setValue((int) m_eqF[8]);
    ui->f9->setValue((int) m_eqF[9]);
    ui->f10->setValue((int) m_eqF[10]);
    ui->f1->blockSignals(false);
    ui->f2->blockSignals(false);
    ui->f3->blockSignals(false);
    ui->f4->blockSignals(false);
    ui->f5->blockSignals(false);
    ui->f6->blockSignals(false);
    ui->f7->blockSignals(false);
    ui->f8->blockSignals(false);
    ui->f9->blockSignals(false);
    ui->f10->blockSignals(false);
}

void WDSPRxEqDialog::setEqG(const std::array<float, 11>& eqG)
{
    m_eqG = eqG;
    ui->preampGain->blockSignals(true);
    ui->f1Gain->blockSignals(true);
    ui->f2Gain->blockSignals(true);
    ui->f3Gain->blockSignals(true);
    ui->f4Gain->blockSignals(true);
    ui->f5Gain->blockSignals(true);
    ui->f6Gain->blockSignals(true);
    ui->f7Gain->blockSignals(true);
    ui->f8Gain->blockSignals(true);
    ui->f9Gain->blockSignals(true);
    ui->f10Gain->blockSignals(true);
    ui->preampGain->setValue((int) m_eqG[0]);
    ui->f1Gain->setValue((int) m_eqG[1]);
    ui->f2Gain->setValue((int) m_eqG[2]);
    ui->f3Gain->setValue((int) m_eqG[3]);
    ui->f4Gain->setValue((int) m_eqG[4]);
    ui->f5Gain->setValue((int) m_eqG[5]);
    ui->f6Gain->setValue((int) m_eqG[6]);
    ui->f7Gain->setValue((int) m_eqG[7]);
    ui->f8Gain->setValue((int) m_eqG[8]);
    ui->f9Gain->setValue((int) m_eqG[9]);
    ui->f10Gain->setValue((int) m_eqG[10]);
    ui->preampGain->blockSignals(false);
    ui->f1Gain->blockSignals(false);
    ui->f2Gain->blockSignals(false);
    ui->f3Gain->blockSignals(false);
    ui->f4Gain->blockSignals(false);
    ui->f5Gain->blockSignals(false);
    ui->f6Gain->blockSignals(false);
    ui->f7Gain->blockSignals(false);
    ui->f8Gain->blockSignals(false);
    ui->f9Gain->blockSignals(false);
    ui->f10Gain->blockSignals(false);
    ui->preampGainText->setText(tr("%1 dB").arg((int) m_eqG[0]));
    ui->f1GainText->setText(tr("%1 dB").arg((int) m_eqG[1]));
    ui->f2GainText->setText(tr("%1 dB").arg((int) m_eqG[2]));
    ui->f3GainText->setText(tr("%1 dB").arg((int) m_eqG[3]));
    ui->f4GainText->setText(tr("%1 dB").arg((int) m_eqG[4]));
    ui->f5GainText->setText(tr("%1 dB").arg((int) m_eqG[5]));
    ui->f6GainText->setText(tr("%1 dB").arg((int) m_eqG[6]));
    ui->f7GainText->setText(tr("%1 dB").arg((int) m_eqG[7]));
    ui->f8GainText->setText(tr("%1 dB").arg((int) m_eqG[8]));
    ui->f9GainText->setText(tr("%1 dB").arg((int) m_eqG[9]));
    ui->f10GainText->setText(tr("%1 dB").arg((int) m_eqG[10]));
}

void WDSPRxEqDialog::on_f1_valueChanged(int value)
{
    m_eqF[1] = (float) value;
    emit valueChanged(ChangedFrequency);
}

void WDSPRxEqDialog::on_f2_valueChanged(int value)
{
    m_eqF[2] = (float) value;
    emit valueChanged(ChangedFrequency);
}

void WDSPRxEqDialog::on_f3_valueChanged(int value)
{
    m_eqF[3] = (float) value;
    emit valueChanged(ChangedFrequency);
}

void WDSPRxEqDialog::on_f4_valueChanged(int value)
{
    m_eqF[4] = (float) value;
    emit valueChanged(ChangedFrequency);
}

void WDSPRxEqDialog::on_f5_valueChanged(int value)
{
    m_eqF[5] = (float) value;
    emit valueChanged(ChangedFrequency);
}

void WDSPRxEqDialog::on_f6_valueChanged(int value)
{
    m_eqF[6] = (float) value;
    emit valueChanged(ChangedFrequency);
}

void WDSPRxEqDialog::on_f7_valueChanged(int value)
{
    m_eqF[7] = (float) value;
    emit valueChanged(ChangedFrequency);
}

void WDSPRxEqDialog::on_f8_valueChanged(int value)
{
    m_eqF[8] = (float) value;
    emit valueChanged(ChangedFrequency);
}

void WDSPRxEqDialog::on_f9_valueChanged(int value)
{
    m_eqF[9] = (float) value;
    emit valueChanged(ChangedFrequency);
}

void WDSPRxEqDialog::on_f10_valueChanged(int value)
{
    m_eqF[10] = (float) value;
    emit valueChanged(ChangedFrequency);
}

void WDSPRxEqDialog::on_preampGain_valueChanged(int value)
{
    m_eqG[0] = (float) value;
    ui->preampGainText->setText(tr("%1 dB").arg(value));
    emit valueChanged(ChangedGain);
}

void WDSPRxEqDialog::on_f1Gain_valueChanged(int value)
{
    m_eqG[1] = (float) value;
    ui->f1GainText->setText(tr("%1 dB").arg(value));
    emit valueChanged(ChangedGain);
}

void WDSPRxEqDialog::on_f2Gain_valueChanged(int value)
{
    m_eqG[2] = (float) value;
    ui->f2GainText->setText(tr("%1 dB").arg(value));
    emit valueChanged(ChangedGain);
}

void WDSPRxEqDialog::on_f3Gain_valueChanged(int value)
{
    m_eqG[3] = (float) value;
    ui->f3GainText->setText(tr("%1 dB").arg(value));
    emit valueChanged(ChangedGain);
}

void WDSPRxEqDialog::on_f4Gain_valueChanged(int value)
{
    m_eqG[4] = (float) value;
    ui->f4GainText->setText(tr("%1 dB").arg(value));
    emit valueChanged(ChangedGain);
}

void WDSPRxEqDialog::on_f5Gain_valueChanged(int value)
{
    m_eqG[5] = (float) value;
    ui->f5GainText->setText(tr("%1 dB").arg(value));
    emit valueChanged(ChangedGain);
}

void WDSPRxEqDialog::on_f6Gain_valueChanged(int value)
{
    m_eqG[6] = (float) value;
    ui->f6GainText->setText(tr("%1 dB").arg(value));
    emit valueChanged(ChangedGain);
}

void WDSPRxEqDialog::on_f7Gain_valueChanged(int value)
{
    m_eqG[7] = (float) value;
    ui->f7GainText->setText(tr("%1 dB").arg(value));
    emit valueChanged(ChangedGain);
}

void WDSPRxEqDialog::on_f8Gain_valueChanged(int value)
{
    m_eqG[8] = (float) value;
    ui->f8GainText->setText(tr("%1 dB").arg(value));
    emit valueChanged(ChangedGain);
}

void WDSPRxEqDialog::on_f9Gain_valueChanged(int value)
{
    m_eqG[9] = (float) value;
    ui->f9GainText->setText(tr("%1 dB").arg(value));
    emit valueChanged(ChangedGain);
}

void WDSPRxEqDialog::on_f10Gain_valueChanged(int value)
{
    m_eqG[10] = (float) value;
    ui->f10GainText->setText(tr("%1 dB").arg(value));
    emit valueChanged(ChangedGain);
}

