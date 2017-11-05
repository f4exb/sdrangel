///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// OpenGL interface modernization.                                               //
// See: http://doc.qt.io/qt-5/qopenglshaderprogram.html                          //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "externalclockdialog.h"
#include "ui_externalclockdialog.h"


ExternalClockDialog::ExternalClockDialog(qint64& externalClockFrequency, bool& externalClockFrequencyActive, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::ExternalClockDialog),
    m_externalClockFrequency(externalClockFrequency),
    m_externalClockFrequencyActive(externalClockFrequencyActive)
{
    ui->setupUi(this);
    ui->externalClockFrequencyLabel->setText("f");
    ui->externalClockFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->externalClockFrequency->setValueRange(true, 9, 5000000L, 300000000L);
    ui->externalClockFrequency->setValue(externalClockFrequency);
    ui->externalClockFrequencyActive->setChecked(externalClockFrequencyActive);
}

ExternalClockDialog::~ExternalClockDialog()
{
    delete ui;
}

void ExternalClockDialog::accept()
{
    m_externalClockFrequency = ui->externalClockFrequency->getValueNew();
    m_externalClockFrequencyActive = ui->externalClockFrequencyActive->isChecked();
    QDialog::accept();
}
