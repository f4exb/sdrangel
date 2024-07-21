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

#include "wdsprxpandialog.h"
#include "ui_wdsprxpandialog.h"

WDSPRxPanDialog::WDSPRxPanDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::WDSPRxPanDialog)
{
    ui->setupUi(this);
}

WDSPRxPanDialog::~WDSPRxPanDialog()
{
    delete ui;
}

void WDSPRxPanDialog::setPan(double pan)
{
    ui->pan->blockSignals(true);
    ui->pan->setValue((int) ((pan - 0.5)*200.0));
    ui->pan->blockSignals(false);
    ui->panText->setText(tr("%1").arg(ui->pan->value()));
    m_pan = pan;
}

void WDSPRxPanDialog::on_zero_clicked()
{
    ui->pan->setValue(0);
    ui->panText->setText(tr("%1").arg(ui->pan->value()));
    m_pan = 0.5;
    emit valueChanged(ChangedPan);
}

void WDSPRxPanDialog::on_pan_valueChanged(int value)
{
    ui->panText->setText(tr("%1").arg(ui->pan->value()));
    m_pan = 0.5 + (value / 200.0);
    emit valueChanged(ChangedPan);
}
