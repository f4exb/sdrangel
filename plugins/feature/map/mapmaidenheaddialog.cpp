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
#include <QGeoCoordinate>
#include <QGeoCodingManager>
#include <QGeoServiceProvider>

#include "util/units.h"
#include "util/maidenhead.h"

#include "mapmaidenheaddialog.h"
#include "maplocationdialog.h"

MapMaidenheadDialog::MapMaidenheadDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::MapMaidenheadDialog)
{
    ui->setupUi(this);
}

MapMaidenheadDialog::~MapMaidenheadDialog()
{
    delete ui;
}

void MapMaidenheadDialog::on_address_returnPressed()
{
    QString address = ui->address->text().trimmed();
    if (!address.isEmpty())
    {
        ui->latAndLong->setText("");
        ui->error->setText("");
        QGeoServiceProvider* geoSrv = new QGeoServiceProvider("osm");
        if (geoSrv != nullptr)
        {
            QLocale qLocaleC(QLocale::C, QLocale::AnyCountry);
            geoSrv->setLocale(qLocaleC);
            QGeoCodeReply *pQGeoCode = geoSrv->geocodingManager()->geocode(address);
            if (pQGeoCode)
                 QObject::connect(pQGeoCode, &QGeoCodeReply::finished, this, &MapMaidenheadDialog::geoReply);
            else
                ui->error->setText("GeoCoding failed");
        }
        else
            ui->error->setText("OpenStreetMap Location services not available");
    }
}

void MapMaidenheadDialog::on_latAndLong_returnPressed()
{
    float latitude, longitude;
    QString coords = ui->latAndLong->text();

    if (Units::stringToLatitudeAndLongitude(coords, latitude, longitude))
    {
        ui->error->setText("");
        ui->maidenhead->setText(Maidenhead::toMaidenhead(latitude, longitude));
    }
    else
    {
        ui->error->setText("Not a valid latitude and longitude");
        ui->maidenhead->setText("");
        QApplication::beep();
    }
    ui->address->setText("");
}

void MapMaidenheadDialog::on_maidenhead_returnPressed()
{
    float latitude, longitude;
    QString locator = ui->maidenhead->text();

    if (Maidenhead::fromMaidenhead(locator, latitude, longitude))
    {
        ui->error->setText("");
        ui->latAndLong->setText(QString("%1,%2").arg(latitude).arg(longitude));
    }
    else
    {
        ui->error->setText("Not a valid Maidenhead locator");
        ui->latAndLong->setText("");
        QApplication::beep();
    }
    ui->address->setText("");
}

void MapMaidenheadDialog::geoReply()
{
    QGeoCodeReply *pQGeoCode = dynamic_cast<QGeoCodeReply*>(sender());

    if ((pQGeoCode != nullptr) && (pQGeoCode->error() == QGeoCodeReply::NoError))
    {
        QList<QGeoLocation> qGeoLocs = pQGeoCode->locations();
        if (qGeoLocs.size() == 0)
        {
            ui->error->setText("No location found for address");
            QApplication::beep();
        }
        else
        {
            if (qGeoLocs.size() == 1)
            {
                QGeoCoordinate c = qGeoLocs.at(0).coordinate();
                ui->latAndLong->setText(QString("%1,%2").arg(c.latitude()).arg(c.longitude()));
                ui->maidenhead->setText(Maidenhead::toMaidenhead(c.latitude(), c.longitude()));
            }
            else
            {
                // Show dialog allowing user to select from the results
                MapLocationDialog dialog(qGeoLocs, this);
                if (dialog.exec() == QDialog::Accepted)
                {
                    QGeoCoordinate c = dialog.m_selectedLocation.coordinate();
                    ui->latAndLong->setText(QString("%1,%2").arg(c.latitude()).arg(c.longitude()));
                    ui->maidenhead->setText(Maidenhead::toMaidenhead(c.latitude(), c.longitude()));
                }
            }
        }
    }
    else
        ui->error->setText(QString("GeoCode error: %1").arg(pQGeoCode->error()));
    pQGeoCode->deleteLater();
}

// Use a custom close button, so pressing enter doesn't close the dialog
void MapMaidenheadDialog::on_close_clicked()
{
    QDialog::accept();
}
