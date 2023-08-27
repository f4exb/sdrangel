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

#include "mapibpbeacondialog.h"

#include <QDebug>

#include "channel/channelwebapiutils.h"
#include "mapgui.h"

MapIBPBeaconDialog::MapIBPBeaconDialog(MapGUI *gui, QWidget* parent) :
    QDialog(parent),
    m_gui(gui),
    m_timer(this),
    ui(new Ui::MapIBPBeaconDialog)
{
    ui->setupUi(this);
    connect(&m_timer, &QTimer::timeout, this, &MapIBPBeaconDialog::updateTime);
    m_timer.setInterval(1000);
    m_timer.start();
    ui->beacons->setRowCount(IBPBeacon::m_frequencies.size());
    for (int row = 0; row < IBPBeacon::m_frequencies.size(); row++)
    {
        ui->beacons->setItem(row, IBP_BEACON_COL_FREQUENCY, new QTableWidgetItem(QString::number(IBPBeacon::m_frequencies[row], 'f', 3)));
        ui->beacons->setItem(row, IBP_BEACON_COL_CALLSIGN, new QTableWidgetItem(""));
        ui->beacons->setItem(row, IBP_BEACON_COL_LOCATION, new QTableWidgetItem(""));
        ui->beacons->setItem(row, IBP_BEACON_COL_DX_ENTITY, new QTableWidgetItem(""));
        ui->beacons->setItem(row, IBP_BEACON_COL_AZIMUTH, new QTableWidgetItem(""));
        ui->beacons->setItem(row, IBP_BEACON_COL_DISTANCE, new QTableWidgetItem(""));
    }
    resizeTable();
    updateTable(QTime::currentTime());
}

MapIBPBeaconDialog::~MapIBPBeaconDialog()
{
    delete ui;
}

// Fill table with a row of dummy data that will size the columns nicely
void MapIBPBeaconDialog::resizeTable(void)
{
    int row = ui->beacons->rowCount();
    ui->beacons->setRowCount(row + 1);
    ui->beacons->setItem(row, IBP_BEACON_COL_FREQUENCY, new QTableWidgetItem("12.345"));
    ui->beacons->setItem(row, IBP_BEACON_COL_CALLSIGN, new QTableWidgetItem("12345"));
    ui->beacons->setItem(row, IBP_BEACON_COL_LOCATION, new QTableWidgetItem("1234567890123456"));
    ui->beacons->setItem(row, IBP_BEACON_COL_DX_ENTITY, new QTableWidgetItem("1234567890123456"));
    ui->beacons->setItem(row, IBP_BEACON_COL_AZIMUTH, new QTableWidgetItem("-123"));
    ui->beacons->setItem(row, IBP_BEACON_COL_DISTANCE, new QTableWidgetItem("12345"));
    ui->beacons->resizeColumnsToContents();
    ui->beacons->removeRow(row);
}

void MapIBPBeaconDialog::updateTable(QTime time)
{
    AzEl azEl = *m_gui->getAzEl();

    // Repeat from begining every 3 minutes
    int index = ((time.minute() * 60 + time.second()) % 180) / IBPBeacon::m_period;

    for (int row = 0; row < IBPBeacon::m_frequencies.size(); row++)
    {
        ui->beacons->item(row, IBP_BEACON_COL_FREQUENCY)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ui->beacons->item(row, IBP_BEACON_COL_CALLSIGN)->setText(IBPBeacon::m_beacons[index].m_callsign);
        ui->beacons->item(row, IBP_BEACON_COL_LOCATION)->setText(IBPBeacon::m_beacons[index].m_location);
        ui->beacons->item(row, IBP_BEACON_COL_DX_ENTITY)->setText(IBPBeacon::m_beacons[index].m_dxEntity);

        // Calculate azimuth and distance to beacon
        azEl.setTarget(IBPBeacon::m_beacons[index].m_latitude, IBPBeacon::m_beacons[index].m_longitude, 0.0);
        azEl.calculate();
        ui->beacons->item(row, IBP_BEACON_COL_AZIMUTH)->setData(Qt::DisplayRole, round(azEl.getAzimuth()));
        ui->beacons->item(row, IBP_BEACON_COL_AZIMUTH)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        int km = round(azEl.getDistance()/1000);
        ui->beacons->item(row, IBP_BEACON_COL_DISTANCE)->setData(Qt::DisplayRole, km);
        ui->beacons->item(row, IBP_BEACON_COL_DISTANCE)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        index--;
        if (index < 0) {
            index = IBPBeacon::m_beacons.size() - 1;
        }
    }
}

void MapIBPBeaconDialog::accept()
{
    QDialog::accept();
}

void MapIBPBeaconDialog::on_beacons_cellDoubleClicked(int row, int column)
{
    if (column == IBP_BEACON_COL_CALLSIGN)
    {
        // Find beacon on map
        QString location = ui->beacons->item(row, column)->text();
        m_gui->find(location);
    }
    else if (column == IBP_BEACON_COL_FREQUENCY)
    {
        // Tune to beacon freq
        ChannelWebAPIUtils::setCenterFrequency(0, ui->beacons->item(row, column)->text().toDouble() * 1e6);
    }
}

void MapIBPBeaconDialog::updateTime()
{
    QTime t = QTime::currentTime();
    ui->time->setTime(t);
    // Beacons rotate every 10 seconds
    if ((t.second() % IBPBeacon::m_period) == 0) {
        updateTable(t);
    }
}
