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

#include "wdsprxcwpeakdialog.h"
#include "ui_wdsprxcwpeakdialog.h"

WDSPRxCWPeakDialog::WDSPRxCWPeakDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::WDSPRxCWPeakDialog)
{
    ui->setupUi(this);
}

WDSPRxCWPeakDialog::~WDSPRxCWPeakDialog()
{
    delete ui;
}

void WDSPRxCWPeakDialog::setCWPeakFrequency(double cwPeakFrequency)
{
    ui->cwPeakFrequency->blockSignals(true);
    ui->cwPeakFrequency->setValue(cwPeakFrequency);
    ui->cwPeakFrequency->blockSignals(false);
    m_cwPeakFrequency = cwPeakFrequency;
}

void WDSPRxCWPeakDialog::setCWBandwidth(double cwBandwidth)
{
    ui->cwBandwidth->blockSignals(true);
    ui->cwBandwidth->setValue(cwBandwidth);
    ui->cwBandwidth->blockSignals(false);
    m_cwBandwidth = cwBandwidth;
}

void WDSPRxCWPeakDialog::setCWGain(double cwGain)
{
    ui->cwGain->blockSignals(true);
    ui->cwGain->setValue(cwGain);
    ui->cwGain->blockSignals(false);
    m_cwGain = cwGain;
}

void WDSPRxCWPeakDialog::on_cwPeakFrequency_valueChanged(double value)
{
    m_cwPeakFrequency = value;
    emit valueChanged(ChangedCWPeakFrequency);
}

void WDSPRxCWPeakDialog::on_cwBandwidth_valueChanged(double value)
{
    m_cwBandwidth = value;
    emit valueChanged(ChangedCWBandwidth);
}

void WDSPRxCWPeakDialog::on_cwGain_valueChanged(double value)
{
    m_cwGain = value;
    emit valueChanged(ChangedCWGain);
}
