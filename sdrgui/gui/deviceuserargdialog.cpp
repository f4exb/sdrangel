///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "ui_deviceuserargdialog.h"
#include "deviceuserargdialog.h"

DeviceUserArgDialog::DeviceUserArgDialog(DeviceEnumerator* deviceEnumerator, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::DeviceUserArgDialog),
	m_deviceEnumerator(deviceEnumerator)
{
	ui->setupUi(this);
}

DeviceUserArgDialog::~DeviceUserArgDialog()
{
    delete ui;
}

void DeviceUserArgDialog::accept()
{
    QDialog::accept();
}

void DeviceUserArgDialog::reject()
{
    QDialog::reject();
}

void DeviceUserArgDialog::on_keySelect_currentIndexChanged(int index)
{

}

void DeviceUserArgDialog::on_deleteKey_clicked(bool checked)
{

}

void DeviceUserArgDialog::on_keyEdit_returnPressed()
{

}

void DeviceUserArgDialog::on_addKey_clicked(bool checked)
{

}

void DeviceUserArgDialog::on_refreshKeys_clicked(bool checked)
{

}

void DeviceUserArgDialog::on_valueEdit_returnPressed()
{

}