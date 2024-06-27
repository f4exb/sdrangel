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

#include "wdsprxagcdialog.h"
#include "ui_wdsprxagcdialog.h"

WDSPRxAGCDialog::WDSPRxAGCDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::WDSPRxAGCDialog)
{
    ui->setupUi(this);
}

WDSPRxAGCDialog::~WDSPRxAGCDialog()
{
    delete ui;
}

void WDSPRxAGCDialog::setAGCMode(WDSPRxProfile::WDSPRxAGCMode mode)
{
    ui->agcMode->blockSignals(true);
    ui->agcMode->setCurrentIndex((int) mode);
    ui->agcMode->blockSignals(false);
    m_agcMode = mode;
}

void WDSPRxAGCDialog::setAGCSlope(int slope)
{
    ui->agcSlope->blockSignals(true);
    ui->agcSlope->setValue(slope);
    ui->agcSlopeText->setText(tr("%1 dB").arg(slope/10.0, 0, 'f', 1));
    ui->agcSlope->blockSignals(false);
    m_agcSlope = slope;
}

void WDSPRxAGCDialog::setAGCHangThreshold(int hangThreshold)
{
    ui->agcHangThreshold->blockSignals(true);
    ui->agcHangThreshold->setValue(hangThreshold);
    ui->agcHangThresholdText->setText(tr("%1").arg(hangThreshold));
    ui->agcHangThreshold->blockSignals(false);
}

void WDSPRxAGCDialog::on_agcMode_currentIndexChanged(int index)
{
    m_agcMode = (WDSPRxProfile::WDSPRxAGCMode) index;
    emit valueChanged(ChangedMode);
}

void WDSPRxAGCDialog::on_agcSlope_valueChanged(int value)
{
    m_agcSlope = value;
    ui->agcSlopeText->setText(tr("%1 dB").arg(value/10.0, 0, 'f', 1));
    emit valueChanged(ChangedSlope);
}

void WDSPRxAGCDialog::on_agcHangThreshold_valueChanged(int value)
{
    m_agcHangThreshold = value;
    ui->agcHangThresholdText->setText(tr("%1").arg(value));
    emit valueChanged(ChangedHangThreshold);
}
