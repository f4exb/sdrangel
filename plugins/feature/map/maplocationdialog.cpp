///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include "maplocationdialog.h"
#include <QDebug>
#include <QGeoAddress>

MapLocationDialog::MapLocationDialog(const QList<QGeoLocation>& locations,
        QWidget* parent) :
    QDialog(parent),
    ui(new Ui::MapLocationDialog)
{
    ui->setupUi(this);
    for (const QGeoLocation& location : locations)
    {
        QGeoAddress address = location.address();
        ui->locations->addItem(address.text());
    }
    ui->locations->setCurrentRow(0);
    m_locations = &locations;
}

MapLocationDialog::~MapLocationDialog()
{
    delete ui;
}

void MapLocationDialog::accept()
{
    int row = ui->locations->currentRow();
    m_selectedLocation = m_locations->at(row);
    QDialog::accept();
}
