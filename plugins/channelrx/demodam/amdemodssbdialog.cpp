///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
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

#include "amdemodssbdialog.h"
#include "ui_amdemodssb.h"

AMDemodSSBDialog::AMDemodSSBDialog(bool usb, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::AMDemodSSBDialog),
    m_usb(usb)
{
    ui->setupUi(this);
    ui->usb->setChecked(usb);
    ui->lsb->setChecked(!usb);
}

AMDemodSSBDialog::~AMDemodSSBDialog()
{
    delete ui;
}

void AMDemodSSBDialog::accept()
{
    m_usb = ui->usb->isChecked();
    QDialog::accept();
}


