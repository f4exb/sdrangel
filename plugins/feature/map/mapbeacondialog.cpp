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
    ui(new Ui::MapBeaconDialog),
    m_progressDialog(nullptr)
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
            ui->beacons->setItem(row, BEACON_COL_LOCATION, new QTableWidgetItem(beacon->m_locator));
            ui->beacons->setItem(row, BEACON_COL_POWER, new QTableWidgetItem(beacon->m_power));
            ui->beacons->setItem(row, BEACON_COL_POLARIZATION, new QTableWidgetItem(beacon->m_polarization));
            ui->beacons->setItem(row, BEACON_COL_PATTERN, new QTableWidgetItem(beacon->m_pattern));
            ui->beacons->setItem(row, BEACON_COL_KEY, new QTableWidgetItem(beacon->m_key));
            ui->beacons->setItem(row, BEACON_COL_MGM, new QTableWidgetItem(beacon->m_mgm));
            azEl.setTarget(beacon->m_latitude, beacon->m_longitude, beacon->m_altitude);
            azEl.calculate();
            ui->beacons->setItem(row, BEACON_COL_AZIMUTH, new QTableWidgetItem(QString("%1").arg(round(azEl.getAzimuth()))));
            ui->beacons->setItem(row, BEACON_COL_ELEVATION, new QTableWidgetItem(QString("%1").arg(round(azEl.getElevation()))));
            int km = round(azEl.getDistance()/1000);
            QTableWidgetItem *dist = new QTableWidgetItem();
            dist->setData(Qt::DisplayRole, km);
            ui->beacons->setItem(row, BEACON_COL_DISTANCE, dist);
            row++;
        }
    }
    ui->beacons->setSortingEnabled(true);
    ui->beacons->resizeColumnsToContents();
}

qint64 MapBeaconDialog::fileAgeInDays(QString filename)
{
    QFile file(filename);
    if (file.exists())
    {
        QDateTime modified = file.fileTime(QFileDevice::FileModificationTime);
        if (modified.isValid())
            return modified.daysTo(QDateTime::currentDateTime());
        else
            return -1;
    }
    return -1;
}

bool MapBeaconDialog::confirmDownload(QString filename)
{
    qint64 age = fileAgeInDays(filename);
    if ((age == -1) || (age > 100))
        return true;
    else
    {
        QMessageBox::StandardButton reply;
        if (age == 0)
            reply = QMessageBox::question(this, "Confirm download", "This file was last downloaded today. Are you sure you wish to redownload it?", QMessageBox::Yes|QMessageBox::No);
        else if (age == 1)
            reply = QMessageBox::question(this, "Confirm download", "This file was last downloaded yesterday. Are you sure you wish to redownload it?", QMessageBox::Yes|QMessageBox::No);
        else
            reply = QMessageBox::question(this, "Confirm download", QString("This file was last downloaded %1 days ago. Are you sure you wish to redownload this file?").arg(age), QMessageBox::Yes|QMessageBox::No);
        return reply == QMessageBox::Yes;
    }
}

void MapBeaconDialog::updateDownloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    m_progressDialog->setMaximum(totalBytes);
    m_progressDialog->setValue(bytesRead);
}

void MapBeaconDialog::accept()
{
    QDialog::accept();
}

void MapBeaconDialog::on_downloadIARU_clicked()
{
    if (m_progressDialog == nullptr)
    {
        QString beaconFile = MapGUI::getBeaconFilename();
        if (confirmDownload(beaconFile))
        {
            // Download IARU beacons database to disk
            QUrl dbURL(QString(IARU_BEACONS_URL));
            m_progressDialog = new QProgressDialog(this);
            m_progressDialog->setAttribute(Qt::WA_DeleteOnClose);
            m_progressDialog->setCancelButton(nullptr);
            m_progressDialog->setMinimumDuration(500);
            m_progressDialog->setLabelText(QString("Downloading %1.").arg(IARU_BEACONS_URL));
            QNetworkReply *reply = m_dlm.download(dbURL, beaconFile);
            connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(updateDownloadProgress(qint64,qint64)));
        }
    }
}

void MapBeaconDialog::downloadFinished(const QString& filename, bool success)
{
    if (success)
    {
        if (filename == MapGUI::getBeaconFilename())
        {
            QList<Beacon *> *beacons = Beacon::readIARUCSV(filename);
            if (beacons != nullptr)
                m_gui->setBeacons(beacons);
            m_progressDialog->close();
            m_progressDialog = nullptr;
        }
        else
        {
            qDebug() << "MapBeaconDialog::downloadFinished: Unexpected filename: " << filename;
            m_progressDialog->close();
            m_progressDialog = nullptr;
        }
    }
    else
    {
        qDebug() << "MapBeaconDialog::downloadFinished: Failed: " << filename;
        m_progressDialog->close();
        m_progressDialog = nullptr;
        QMessageBox::warning(this, "Download failed", QString("Failed to download %1").arg(filename));
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
