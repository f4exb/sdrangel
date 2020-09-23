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
#include "packetmodbpfdialog.h"
#include "ui_packetmodbpfdialog.h"

PacketModBPFDialog::PacketModBPFDialog(float lowFreq, float highFreq, int taps, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::PacketModBPFDialog)
{
    ui->setupUi(this);
    ui->lowFreq->setValue(lowFreq);
    ui->highFreq->setValue(highFreq);
    ui->taps->setValue(taps);
}

PacketModBPFDialog::~PacketModBPFDialog()
{
    delete ui;
}

void PacketModBPFDialog::accept()
{
    m_lowFreq = ui->lowFreq->value();
    m_highFreq = ui->highFreq->value();
    m_taps = ui->taps->value();

    QDialog::accept();
}
