///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include "freqscanneraddrangedialog.h"
#include "ui_freqscanneraddrangedialog.h"

FreqScannerAddRangeDialog::FreqScannerAddRangeDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::FreqScannerAddRangeDialog)
{
    ui->setupUi(this);

    ui->start->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->start->setValueRange(false, 11, 0, 99999999999);
    ui->stop->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->stop->setValueRange(false, 11, 0, 99999999999);

    // Airband frequency range
    ui->start->setValue(118000000);
    ui->stop->setValue(137000000);
}

FreqScannerAddRangeDialog::~FreqScannerAddRangeDialog()
{
    delete ui;
}

void FreqScannerAddRangeDialog::accept()
{
    m_start = ui->start->getValue();
    m_stop = ui->stop->getValue();
    m_step = ui->step->currentText().toLongLong();
    QDialog::accept();
}
