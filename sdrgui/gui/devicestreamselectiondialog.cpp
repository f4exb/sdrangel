///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "ui_devicestreamselectiondialog.h"
#include "devicestreamselectiondialog.h"

DeviceStreamSelectionDialog::DeviceStreamSelectionDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DeviceStreamSelectionDialog),
    m_hasChanged(false)
{
    ui->setupUi(this);
    setStreamIndex(0);
}

DeviceStreamSelectionDialog::~DeviceStreamSelectionDialog()
{
    delete ui;
}

void DeviceStreamSelectionDialog::setNumberOfStreams(int nbStreams)
{
    ui->deviceStream->clear();

    for (int i = 0; i < nbStreams; i++) {
        ui->deviceStream->addItem(tr("%1").arg(i));
    }
}

void DeviceStreamSelectionDialog::setStreamIndex(int index)
{
    ui->deviceStream->setCurrentIndex(index);
    m_streamIndex = ui->deviceStream->currentIndex();
}

void DeviceStreamSelectionDialog::accept()
{
    m_streamIndex = ui->deviceStream->currentIndex();
    m_hasChanged = true;
     QDialog::accept();
}