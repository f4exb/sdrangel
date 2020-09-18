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

#define _USE_MATH_DEFINES
#include <cmath>
#include "fmpreemphasisdialog.h"
#include "ui_fmpreemphasisdialog.h"

// Convert from s to us
#define TAU_SCALE 1000000.0

FMPreemphasisDialog::FMPreemphasisDialog(float tau, float highFreq, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::FMPreemphasisDialog)
{
    ui->setupUi(this);
    ui->tau->setValue(tau*TAU_SCALE);
    ui->lowFreq->setValue(1.0/(2.0*M_PI*tau));
    ui->highFreq->setValue(highFreq);
}

FMPreemphasisDialog::~FMPreemphasisDialog()
{
    delete ui;
}

void FMPreemphasisDialog::accept()
{
    m_tau = ui->tau->value()/TAU_SCALE;
    m_highFreq = ui->highFreq->value();

    QDialog::accept();
}

void FMPreemphasisDialog::on_tau_valueChanged(double value)
{
    // Set corresponding low frequency
    ui->lowFreq->blockSignals(true);
    ui->lowFreq->setValue(1.0/(2.0*M_PI*(value/TAU_SCALE)));
    ui->lowFreq->blockSignals(false);
}

void FMPreemphasisDialog::on_lowFreq_valueChanged(double value)
{
    // Set corresponding tau
    ui->tau->blockSignals(true);
    ui->tau->setValue(TAU_SCALE*1.0/(2.0*M_PI*value));
    ui->tau->blockSignals(false);
}
