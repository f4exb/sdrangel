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

#include <QString>

#include "ambedevicesdialog.h"
#include "ui_ambedevicesdialog.h"

AMBEDevicesDialog::AMBEDevicesDialog(AMBEEngine& ambeEngine, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::AMBEDevicesDialog),
    m_ambeEngine(ambeEngine)
{
    ui->setupUi(this);
    populateSerialList();
}

AMBEDevicesDialog::~AMBEDevicesDialog()
{
    delete ui;
}

void AMBEDevicesDialog::populateSerialList()
{
    std::vector<QString> ambeSerialDevices;
    m_ambeEngine.scan(ambeSerialDevices);
    std::vector<QString>::const_iterator it = ambeSerialDevices.begin();

    for (; it != ambeSerialDevices.end(); ++it)
    {
        ui->ambeSerialDevices->addItem(QString(*it));
    }
}