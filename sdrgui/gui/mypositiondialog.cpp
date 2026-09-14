///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2017, 2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2020, 2022 Jon Beniston, M7RCE <jon@beniston.com>               //
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

#include "gui/mypositiondialog.h"
#include "ui_myposdialog.h"
#include "maincore.h"

#include <QGeoCoordinate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

MyPositionDialog::MyPositionDialog(MainSettings& mainSettings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::MyPositionDialog),
	m_mainSettings(mainSettings)
{
	ui->setupUi(this);
    ui->name->setText(m_mainSettings.getStationName());
    ui->latitudeSpinBox->setValue(m_mainSettings.getLatitude());
    ui->longitudeSpinBox->setValue(m_mainSettings.getLongitude());
    ui->altitudeSpinBox->setValue(m_mainSettings.getAltitude());
    ui->autoUpdatePosition->setChecked(m_mainSettings.getAutoUpdatePosition());
}

MyPositionDialog::~MyPositionDialog()
{
	delete ui;
}

void MyPositionDialog::accept()
{
    m_mainSettings.setStationName(ui->name->text());
    m_mainSettings.setLatitude(ui->latitudeSpinBox->value());
    m_mainSettings.setLongitude(ui->longitudeSpinBox->value());
    m_mainSettings.setAltitude(ui->altitudeSpinBox->value());
    m_mainSettings.setAutoUpdatePosition(ui->autoUpdatePosition->isChecked());
	QDialog::accept();
}

void MyPositionDialog::on_gps_clicked()
{
    const QGeoPositionInfo& position = MainCore::instance()->getPosition();
    if (position.isValid())
    {
        QGeoCoordinate coord = position.coordinate();
        ui->latitudeSpinBox->setValue(coord.latitude());
        ui->longitudeSpinBox->setValue(coord.longitude());
        ui->altitudeSpinBox->setValue(coord.altitude());
        return;
    }

    // Only guess when both are zero.
    if (ui->latitudeSpinBox->value() != 0.0 || ui->longitudeSpinBox->value() != 0.0)
    {
       return;
    }

    qWarning() << "SDRangel uses IP2Location.io <a href=\"https://www.ip2location.io\">IP geolocation</a>"
               " web service as fall back when GPS is not available";

    // Look up the public IP to get an approximate latitude and longitude.
    QNetworkAccessManager *networkManager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl(QStringLiteral("https://api.ip2location.io/?format=json")));
    QNetworkReply *reply = networkManager->get(request);

    auto cleanup = [networkManager](QNetworkReply *reply) {
        reply->deleteLater();
        networkManager->deleteLater();
    };

    connect(reply, &QNetworkReply::finished, this, [this, reply, networkManager, cleanup]() {
        if (reply->error() != QNetworkReply::NoError)
        {
            qWarning() << "MyPositionDialog::on_gps_clicked:"
                       << "IP geolocation failed:" << reply->errorString();
            cleanup(reply);
            return;
        }

        const QJsonDocument document =
            QJsonDocument::fromJson(reply->readAll());

        if (!document.isObject())
        {
            qWarning() << "MyPositionDialog::on_gps_clicked:"
                       << "Invalid IP geolocation response.";
            cleanup(reply);
            return;
        }

        const QJsonObject object = document.object();
        const QJsonValue latitude = object.value(QStringLiteral("latitude"));
        const QJsonValue longitude = object.value(QStringLiteral("longitude"));

        if (!latitude.isDouble() || !longitude.isDouble())
        {
            qWarning() << "MyPositionDialog::on_gps_clicked:"
                       << "IP geolocation response has no valid coordinates.";
            cleanup(reply);
            return;
        }

        // Look up elevation for the IP-derived coordinates.
        const QUrl elevationUrl(
            QStringLiteral("https://api.open-elevation.com/api/v1/lookup?locations=%1,%2")
                .arg(latitude.toDouble())
                .arg(longitude.toDouble()));

        QNetworkReply *elevationReply =
            networkManager->get(QNetworkRequest(elevationUrl));

        reply->deleteLater();

        connect(elevationReply, &QNetworkReply::finished, this,
                [this, elevationReply, cleanup, latitude, longitude]() {
            if (elevationReply->error() != QNetworkReply::NoError)
            {
                qWarning() << "MyPositionDialog::on_gps_clicked:"
                           << "Elevation lookup failed:"
                           << elevationReply->errorString();
                cleanup(elevationReply);
                return;
            }

            const QJsonDocument document =
                QJsonDocument::fromJson(elevationReply->readAll());

            if (!document.isObject())
            {
                qWarning() << "MyPositionDialog::on_gps_clicked:"
                           << "Invalid elevation response.";
                cleanup(elevationReply);
                return;
            }

            const QJsonArray results =
                document.object().value(QStringLiteral("results")).toArray();

            if (results.isEmpty() || !results.first().isObject())
            {
                qWarning() << "MyPositionDialog::on_gps_clicked:"
                           << "Elevation response has no results.";
                cleanup(elevationReply);
                return;
            }

            const QJsonValue elevation =
                results.first().toObject().value(QStringLiteral("elevation"));

            if (!elevation.isDouble())
            {
                qWarning() << "MyPositionDialog::on_gps_clicked:"
                           << "Elevation response has no valid elevation.";
                cleanup(elevationReply);
                return;
            }

            // Use the IP-derived (estimated) coordinates and elevation.
            ui->latitudeSpinBox->setValue(latitude.toDouble());
            ui->longitudeSpinBox->setValue(longitude.toDouble());
            ui->altitudeSpinBox->setValue(elevation.toDouble());

            qDebug() << "MyPositionDialog::on_gps_clicked:"
                     << "Using approximate position from public IP:"
                     << latitude.toDouble()
                     << longitude.toDouble()
                     << "elevation:" << elevation.toDouble();

            cleanup(elevationReply);
        });
    });
}
