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

#include "mapbeacondialog.h"

#include <QDebug>
#include <QUrl>
#include <QMessageBox>

#include "channel/channelwebapiutils.h"
#include "mapgui.h"

MapBeaconDialog::MapBeaconDialog(MapGUI *gui, QWidget* parent) :
    QDialog(parent),
    m_gui(gui),
    ui(new Ui::MapBeaconDialog)
{
    ui->setupUi(this);
    connect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &MapBeaconDialog::downloadFinished);
}

MapBeaconDialog::~MapBeaconDialog()
{
    delete ui;
}

void MapBeaconDialog::updateTable()
{
    AzEl azEl = *m_gui->getAzEl();
    ui->beacons->setSortingEnabled(false);
    ui->beacons->setRowCount(0);
    QList<Beacon *> *beacons = m_gui->getBeacons();
    if (beacons != nullptr)
    {
        ui->beacons->setRowCount(beacons->size());
        QListIterator<Beacon *> i(*beacons);
        int row = 0;
        while (i.hasNext())
        {
            Beacon *beacon = i.next();
            ui->beacons->setItem(row, BEACON_COL_CALLSIGN, new QTableWidgetItem(beacon->m_callsign));
            QTableWidgetItem *freq = new QTableWidgetItem();
            freq->setText(beacon->getFrequencyText());
            freq->setData(Qt::UserRole, beacon->m_frequency);
            ui->beacons->setItem(row, BEACON_COL_FREQUENCY, freq);
            ui->beacons->item(row, BEACON_COL_FREQUENCY)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            ui->beacons->setItem(row, BEACON_COL_LOCATION, new QTableWidgetItem(beacon->m_locator));
            ui->beacons->setItem(row, BEACON_COL_POWER, new QTableWidgetItem(beacon->m_power));
            ui->beacons->item(row, BEACON_COL_POWER)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            ui->beacons->setItem(row, BEACON_COL_POLARIZATION, new QTableWidgetItem(beacon->m_polarization));
            ui->beacons->setItem(row, BEACON_COL_PATTERN, new QTableWidgetItem(beacon->m_pattern));
            ui->beacons->setItem(row, BEACON_COL_KEY, new QTableWidgetItem(beacon->m_key));
            ui->beacons->setItem(row, BEACON_COL_MGM, new QTableWidgetItem(beacon->m_mgm));
            azEl.setTarget(beacon->m_latitude, beacon->m_longitude, beacon->m_altitude);
            azEl.calculate();
            QTableWidgetItem *azymuth = new QTableWidgetItem();
            azymuth->setData(Qt::DisplayRole, round(azEl.getAzimuth()));
            ui->beacons->setItem(row, BEACON_COL_AZIMUTH, azymuth);
            ui->beacons->item(row, BEACON_COL_AZIMUTH)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            QTableWidgetItem *elevation = new QTableWidgetItem();
            elevation->setData(Qt::DisplayRole, round(azEl.getElevation()));
            ui->beacons->setItem(row, BEACON_COL_ELEVATION, elevation);
            ui->beacons->item(row, BEACON_COL_ELEVATION)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            int km = round(azEl.getDistance()/1000);
            QTableWidgetItem *dist = new QTableWidgetItem();
            dist->setData(Qt::DisplayRole, km);
            ui->beacons->setItem(row, BEACON_COL_DISTANCE, dist);
            ui->beacons->item(row, BEACON_COL_DISTANCE)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            row++;
        }
    }
    ui->beacons->setSortingEnabled(true);
    ui->beacons->resizeColumnsToContents();
}

void MapBeaconDialog::accept()
{
    QDialog::accept();
}

void MapBeaconDialog::on_downloadIARU_clicked()
{
    if (!m_dlm.downloading())
    {
        QString beaconFile = MapGUI::getBeaconFilename();
        if (HttpDownloadManagerGUI::confirmDownload(beaconFile, this))
        {
            // Download IARU beacons database to disk
            QUrl dbURL(QString(IARU_BEACONS_URL));
            m_dlm.download(dbURL, beaconFile, this);
        }
    }
}

void MapBeaconDialog::downloadFinished(const QString& filename, bool success, const QString &url, const QString &errorMessage)
{
    if (success)
    {
        if (filename == MapGUI::getBeaconFilename())
        {
            QList<Beacon *> *beacons = Beacon::readIARUCSV(filename);
            if (beacons != nullptr) {
                m_gui->setBeacons(beacons);
            }
        }
        else
        {
            qDebug() << "MapBeaconDialog::downloadFinished: Unexpected filename: " << filename;
        }
    }
    else
    {
        QMessageBox::warning(this, "Download failed", QString("Failed to download %1 to %2\n%3").arg(url).arg(filename).arg(errorMessage));
    }
}

void MapBeaconDialog::on_beacons_cellDoubleClicked(int row, int column)
{
    if ((column == BEACON_COL_CALLSIGN) || (column == BEACON_COL_LOCATION))
    {
        // Find beacon on map
        QString location = ui->beacons->item(row, column)->text();
        m_gui->find(location);
    }
    else if (column == BEACON_COL_FREQUENCY)
    {
        // Tune to beacon freq
        ChannelWebAPIUtils::setCenterFrequency(0, ui->beacons->item(row, column)->data(Qt::UserRole).toDouble());
    }
}

void MapBeaconDialog::on_filter_currentIndexChanged(int index)
{
    for (int row = 0; row < ui->beacons->rowCount(); row++)
    {
        bool hidden = false;
        QTableWidgetItem *item = ui->beacons->item(row, BEACON_COL_FREQUENCY);
        qint64 freq = item->data(Qt::UserRole).toLongLong();
        qint64 band = freq/1000000;
        switch (index)
        {
        case 0: // All
            break;
        case 1:
            hidden = band != 50;
            break;
        case 2:
            hidden = band != 70;
            break;
        case 3:
            hidden = band != 144;
            break;
        case 4:
            hidden = band != 432;
            break;
        case 5:
            hidden = band != 1296;
            break;
        case 6:
            hidden = band <= 1296;
            break;
        }
        ui->beacons->setRowHidden(row, hidden);
    }
}
