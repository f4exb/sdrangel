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
#include "dsp/ctcssfrequencies.h"
#include "wdsprxfmdialog.h"
#include "ui_wdsprxfmdialog.h"

WDSPRxFMDialog::WDSPRxFMDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::WDSPRxFMDialog)
{
    ui->setupUi(this);

    for (int i = 0; i < CTCSSFrequencies::m_nbFreqs; i++) {
        ui->ctcssNotchFrequency->addItem(QString("%1").arg((double) CTCSSFrequencies::m_Freqs[i], 0, 'f', 1));
    }
}

WDSPRxFMDialog::~WDSPRxFMDialog()
{
    delete ui;
}

void WDSPRxFMDialog::setDeviation(double deviation)
{
    ui->deviation->blockSignals(true);
    ui->deviation->setValue((int) round(deviation/100.0));
    ui->deviationText->setText(QString("%1").arg(deviation/1000.0, 0, 'f', 1));
    ui->deviation->blockSignals(false);
    m_deviation = deviation;
}

void WDSPRxFMDialog::setAFLow(double afLow)
{
    ui->afLow->blockSignals(true);
    ui->afLow->setValue((int) round(afLow/100.0));
    ui->afLowText->setText(QString("%1").arg(afLow/1000.0, 0, 'f', 1));
    ui->afLow->blockSignals(false);
    m_afLow = afLow;
}

void WDSPRxFMDialog::setAFHigh(double afHigh)
{
    ui->afHigh->blockSignals(true);
    ui->afHigh->setValue((int) round(afHigh/100.0));
    ui->afHighText->setText(QString("%1").arg(afHigh/1000.0, 0, 'f', 1));
    ui->afHigh->blockSignals(false);
    m_afHigh = afHigh;
}

void WDSPRxFMDialog::setAFLimiter(bool afLimiter)
{
    ui->afLimiter->blockSignals(true);
    ui->afLimiter->setChecked(afLimiter);
    ui->afLimiter->blockSignals(false);
    m_afLimiter = afLimiter;
}

void WDSPRxFMDialog::setAFLimiterGain(double gain)
{
    ui->afLimiterGain->blockSignals(true);
    ui->afLimiterGain->setValue((int) gain);
    ui->afLimiterGainText->setText(QString("%1").arg(gain, 0, 'f', 0));
    ui->afLimiterGain->blockSignals(false);
    m_afLimiterGain = gain;
}

void WDSPRxFMDialog::setCTCSSNotch(bool notch)
{
    ui->ctcssNotch->blockSignals(true);
    ui->ctcssNotch->setChecked(notch);
    ui->ctcssNotch->blockSignals(false);
    m_ctcssNotch = notch;
}

void WDSPRxFMDialog::setCTCSSNotchFrequency(double frequency)
{
    int i = 0;

    for (; i < CTCSSFrequencies::m_nbFreqs; i++)
    {
        if (frequency <= CTCSSFrequencies::m_Freqs[i]) {
            break;
        }
    }

    ui->ctcssNotchFrequency->blockSignals(true);
    ui->ctcssNotchFrequency->setCurrentIndex(i);
    ui->ctcssNotchFrequency->blockSignals(false);
    m_ctcssNotchFrequency = CTCSSFrequencies::m_Freqs[i];
}

void WDSPRxFMDialog::on_deviation_valueChanged(int value)
{
    m_deviation = value * 100.0;
    ui->deviationText->setText(QString("%1").arg(value/10.0, 0, 'f', 1));
    emit valueChanged(ChangedDeviation);
}

void WDSPRxFMDialog::on_afLow_valueChanged(int value)
{
    m_afLow = value * 100.0;
    ui->afLowText->setText(QString("%1").arg(value/10.0, 0, 'f', 1));
    emit valueChanged(ChangedAFLow);
}
void WDSPRxFMDialog::on_afHigh_valueChanged(int value)
{
    m_afHigh = value * 100.0;
    ui->afHighText->setText(QString("%1").arg(value/10.0, 0, 'f', 1));
    emit valueChanged(ChangedAFHigh);
}

void WDSPRxFMDialog::on_afLimiter_clicked(bool checked)
{
    m_afLimiter = checked;
    emit valueChanged(ChangedAFLimiter);
}

void WDSPRxFMDialog::on_afLimiterGain_valueChanged(int value)
{
    m_afLimiterGain = (double) value;
    ui->afLimiterGainText->setText(QString("%1").arg(value));
    emit valueChanged(ChangedAFLimiterGain);
}

void WDSPRxFMDialog::on_ctcssNotch_clicked(bool checked)
{
    m_ctcssNotch = checked;
    emit valueChanged(ChangedCTCSSNotch);
}

void WDSPRxFMDialog::on_ctcssNotchFrequency_valueChanged(int value)
{
    if (value > CTCSSFrequencies::m_nbFreqs) {
        return;
    }

    m_ctcssNotchFrequency = CTCSSFrequencies::m_Freqs[value];
    emit valueChanged(ChangedCTCSSNotchFrequency);
}
