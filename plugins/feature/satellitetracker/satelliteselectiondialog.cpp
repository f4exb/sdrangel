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

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDesktopServices>

#include <OrbitalElements.h>
#include <Tle.h>

#include "satelliteselectiondialog.h"
#include "util/units.h"

using namespace libsgp4;

SatelliteSelectionDialog::SatelliteSelectionDialog(SatelliteTrackerSettings *settings,
        const QHash<QString, SatNogsSatellite *>& satellites,
        QWidget* parent) :
    QDialog(parent),
    m_settings(settings),
    m_satellites(satellites),
    m_satInfo(nullptr),
    ui(new Ui::SatelliteSelectionDialog)
{
    ui->setupUi(this);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SatelliteSelectionDialog::networkManagerFinished
    );

    QHashIterator<QString, SatNogsSatellite *> itr(satellites);
    while (itr.hasNext())
    {
        itr.next();
        QString name = itr.key();
        SatNogsSatellite *sat = itr.value();
        // Don't display decayed satellites, or those without TLEs
        if ((sat->m_status == "alive") || (sat->m_status == ""))
        {
            if (sat->m_tle != nullptr)
            {
                if (settings->m_satellites.indexOf(name) == -1)
                    ui->availableSats->addItem(name);
            }
            else
                qDebug() << "SatelliteSelectionDialog::SatelliteSelectionDialog: No TLE for " << name;
        }
    }
    for (int i = 0; i < settings->m_satellites.size(); i++)
        ui->selectedSats->addItem(settings->m_satellites[i]);
}

SatelliteSelectionDialog::~SatelliteSelectionDialog()
{
    delete ui;
}

void SatelliteSelectionDialog::accept()
{
    m_settings->m_satellites.clear();
    for (int i = 0; i < ui->selectedSats->count(); i++)
        m_settings->m_satellites.append(ui->selectedSats->item(i)->text());
    QDialog::accept();
}

void SatelliteSelectionDialog::on_find_textChanged(const QString &text)
{
    QString textTrimmed = text.trimmed();
    QList<QListWidgetItem *> items = ui->availableSats->findItems(textTrimmed, Qt::MatchContains);
    if (items.size() > 0)
        ui->availableSats->setCurrentItem(items[0]);
    else
    {
        // Try alternative names
        QHashIterator<QString, SatNogsSatellite *> itr(m_satellites);
        while (itr.hasNext())
        {
            itr.next();
            SatNogsSatellite *sat = itr.value();
            if (sat->m_names.indexOf(textTrimmed) != -1)
            {
                QList<QListWidgetItem *> items = ui->availableSats->findItems(sat->m_name, Qt::MatchExactly);
                if (items.size() > 0)
                    ui->availableSats->setCurrentItem(items[0]);
                break;
            }
        }
    }
}

void SatelliteSelectionDialog::on_addSat_clicked()
{
    QList<QListWidgetItem *> items = ui->availableSats->selectedItems();
    for (int i = 0; i < items.size(); i++)
    {
        ui->selectedSats->addItem(items[i]->text());
        delete items[i];
    }
}

void SatelliteSelectionDialog::on_removeSat_clicked()
{
    QList<QListWidgetItem *> items = ui->selectedSats->selectedItems();
    for (int i = 0; i < items.size(); i++)
    {
        ui->availableSats->addItem(items[i]->text());
        delete items[i];
    }
}

void SatelliteSelectionDialog::on_moveUp_clicked()
{
    QList<QListWidgetItem *> items = ui->selectedSats->selectedItems();
    for (int i = 0; i < items.size(); i++)
    {
        int row = ui->selectedSats->row(items[i]);
        if (row > 0)
        {
            QListWidgetItem *item = ui->selectedSats->takeItem(row);
            ui->selectedSats->insertItem(row - 1, item);
            ui->selectedSats->setCurrentItem(item);
        }
    }
}

void SatelliteSelectionDialog::on_moveDown_clicked()
{
    QList<QListWidgetItem *> items = ui->selectedSats->selectedItems();
    for (int i = items.size() - 1; i >= 0; i--)
    {
        int row = ui->selectedSats->row(items[i]);
        if (row < ui->selectedSats->count() - 1)
        {
            QListWidgetItem *item = ui->selectedSats->takeItem(row);
            ui->selectedSats->insertItem(row + 1, item);
            ui->selectedSats->setCurrentItem(item);
        }
    }
}

void SatelliteSelectionDialog::on_availableSats_itemDoubleClicked(QListWidgetItem *item)
{
    ui->selectedSats->addItem(item->text());
    delete item;
}

void SatelliteSelectionDialog::on_selectedSats_itemDoubleClicked(QListWidgetItem *item)
{
    ui->availableSats->addItem(item->text());
    delete item;
}

void SatelliteSelectionDialog::on_availableSats_itemSelectionChanged()
{
    QList<QListWidgetItem *> items = ui->availableSats->selectedItems();
    if (items.size() > 0)
    {
        ui->selectedSats->selectionModel()->clear();
        displaySatInfo(items[0]->text());
    }
}

void SatelliteSelectionDialog::on_selectedSats_itemSelectionChanged()
{
    QList<QListWidgetItem *> items = ui->selectedSats->selectedItems();
    if (items.size() > 0)
    {
        ui->availableSats->selectionModel()->clear();
        displaySatInfo(items[0]->text());
    }
}

// Display information about the satellite from the SatNOGS database
void SatelliteSelectionDialog::displaySatInfo(const QString& name)
{
    SatNogsSatellite *sat = m_satellites[name];
    m_satInfo = sat;
    QStringList info;
    info.append(QString("Name: %1").arg(sat->m_name));
    if (sat->m_names.size() > 0)
        info.append(QString("Alternative names: %1").arg(sat->m_names.join(" ")));
    info.append(QString("NORAD ID: %1").arg(sat->m_noradCatId));
    if (sat->m_launched.isValid())
        info.append(QString("Launched: %1").arg(sat->m_launched.toString()));
    if (sat->m_deployed.isValid())
        info.append(QString("Deployed: %1").arg(sat->m_deployed.toString()));
    if (sat->m_decayed.isValid())
        info.append(QString("Decayed: %1").arg(sat->m_decayed.toString()));
    ui->openSatelliteWebsite->setEnabled(!sat->m_website.isEmpty());
    if (!sat->m_operator.isEmpty() && sat->m_operator != "None")
        info.append(QString("Operator: %1").arg(sat->m_operator));
    if (!sat->m_countries.isEmpty())
        info.append(QString("Countries: %1").arg(sat->m_countries));
    if (sat->m_transmitters.size() > 0)
        info.append("Modes:");
    for (int i = 0; i < sat->m_transmitters.size(); i++)
    {
        if (sat->m_transmitters[i]->m_status != "invalid")
        {
            QStringList mode;
            mode.append("  ");
            mode.append(sat->m_transmitters[i]->m_description);
            if (sat->m_transmitters[i]->m_downlinkHigh > 0)
                mode.append(QString("D: %1").arg(SatNogsTransmitter::getFrequencyRangeText(sat->m_transmitters[i]->m_downlinkLow, sat->m_transmitters[i]->m_downlinkHigh)));
            else if (sat->m_transmitters[i]->m_downlinkLow > 0)
                mode.append(QString("D: %1").arg(SatNogsTransmitter::getFrequencyText(sat->m_transmitters[i]->m_downlinkLow)));
            if (sat->m_transmitters[i]->m_uplinkHigh > 0)
                mode.append(QString("U: %1").arg(SatNogsTransmitter::getFrequencyRangeText(sat->m_transmitters[i]->m_uplinkLow, sat->m_transmitters[i]->m_uplinkHigh)));
            else if (sat->m_transmitters[i]->m_uplinkLow > 0)
                mode.append(QString("U: %1").arg(SatNogsTransmitter::getFrequencyText(sat->m_transmitters[i]->m_uplinkLow)));
            info.append(mode.join(" "));
        }
    }
    if (sat->m_tle != nullptr)
    {
        try
        {
            info.append("Orbit:");
            Tle tle = Tle(sat->m_tle->m_tle0.toStdString(),
                            sat->m_tle->m_tle1.toStdString(),
                            sat->m_tle->m_tle2.toStdString());
            OrbitalElements ele(tle);
            info.append(QString("  Period: %1 mins").arg(ele.Period()));
            info.append(QString("  Inclination: %1%2").arg(Units::radiansToDegrees(ele.Inclination())).arg(QChar(0xb0)));
            info.append(QString("  Eccentricity: %1").arg(ele.Eccentricity()));
        }
        catch (TleException& tlee)
        {
            qDebug() << "SatelliteSelectionDialog::displaySatInfo: TleException " << tlee.what();
        }
    }

    ui->satInfo->setText(info.join("\n"));
    if (!sat->m_image.isEmpty())
        m_networkManager->get(QNetworkRequest(QUrl(sat->m_image)));
    else
        ui->satImage->setPixmap(QPixmap());
}

// Open the Satellite's webpage
void SatelliteSelectionDialog::on_openSatelliteWebsite_clicked()
{
    if ((m_satInfo != nullptr) && (!m_satInfo->m_website.isEmpty()))
        QDesktopServices::openUrl(QUrl(m_satInfo->m_website));
}

// Open SatNOGS observations website for the selected satellite
void SatelliteSelectionDialog::on_openSatNogsObservations_clicked()
{
    if (m_satInfo != nullptr)
        QDesktopServices::openUrl(QUrl(QString("https://network.satnogs.org/observations/?norad=%1").arg(m_satInfo->m_noradCatId)));
}

void SatelliteSelectionDialog::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "SatelliteSelectionDialog::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        // Read image data and display it
        QByteArray imageData = reply->readAll();
        QPixmap pixmap;
        if (pixmap.loadFromData(imageData))
            ui->satImage->setPixmap(pixmap.scaled( ui->satImage->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else
            qDebug() << "SatelliteSelectionDialog::networkManagerFinished: Failed to load pixmap from image data";
    }

    reply->deleteLater();
}
