///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <QFileDialog>
#include <QFileInfo>
#include <QListWidgetItem>

#include "aircraftsettingsdialog.h"

AircraftSettingsDialog::AircraftSettingsDialog(AircraftSettings *settings, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AircraftSettingsDialog),
    m_settings(settings)
{
    ui->setupUi(this);

    ui->removal->setValue(m_settings->m_removalMins);
    ui->retention->setValue(m_settings->m_retentionDays);
    ui->flightRetention->setValue(m_settings->m_flightRetentionDays);
    ui->adsbPosition->setValue(m_settings->m_adsbPositionMins);
    ui->acarsPosition->setValue(m_settings->m_acarsPositionMins);
    ui->atcCallsigns->setChecked(m_settings->m_atcCallsigns);
    ui->favourLivery->setChecked(m_settings->m_favourLivery);
    ui->useLiveryIcons->setChecked(m_settings->m_useLiveryIcons);
    ui->maxRangeOnMap->setChecked(m_settings->m_displayMaxRangeOnMap);
    ui->database->setText(m_settings->m_databaseFilename.isEmpty()
        ? AircraftSettings::defaultDatabaseFilename()
        : m_settings->m_databaseFilename);

    // Every demodulator that can feed us, ticked unless it has been turned off
    for (const QString& uri : AircraftSettings::sourceURIs())
    {
        QListWidgetItem *item = new QListWidgetItem(AircraftSettings::sourceName(uri));
        item->setData(Qt::UserRole, uri);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_settings->m_disabledSources.contains(uri) ? Qt::Unchecked : Qt::Checked);
        ui->sources->addItem(item);
    }
}

AircraftSettingsDialog::~AircraftSettingsDialog()
{
    delete ui;
}

void AircraftSettingsDialog::on_browse_clicked()
{
    QString current = ui->database->text().isEmpty()
        ? AircraftSettings::defaultDatabaseFilename()
        : ui->database->text();

    QString filename = QFileDialog::getSaveFileName(this, "Aircraft session database", current,
        "Database files (*.db);;All files (*)", nullptr, QFileDialog::DontConfirmOverwrite);

    if (!filename.isEmpty()) {
        ui->database->setText(filename);
    }
}

void AircraftSettingsDialog::accept()
{
    if (ui->removal->value() != m_settings->m_removalMins)
    {
        m_settings->m_removalMins = ui->removal->value();
        m_changed.append("removalMins");
    }
    if (ui->retention->value() != m_settings->m_retentionDays)
    {
        m_settings->m_retentionDays = ui->retention->value();
        m_changed.append("retentionDays");
    }
    if (ui->flightRetention->value() != m_settings->m_flightRetentionDays)
    {
        m_settings->m_flightRetentionDays = ui->flightRetention->value();
        m_changed.append("flightRetentionDays");
    }
    if (ui->adsbPosition->value() != m_settings->m_adsbPositionMins)
    {
        m_settings->m_adsbPositionMins = ui->adsbPosition->value();
        m_changed.append("adsbPositionMins");
    }
    if (ui->acarsPosition->value() != m_settings->m_acarsPositionMins)
    {
        m_settings->m_acarsPositionMins = ui->acarsPosition->value();
        m_changed.append("acarsPositionMins");
    }
    if (ui->maxRangeOnMap->isChecked() != m_settings->m_displayMaxRangeOnMap)
    {
        m_settings->m_displayMaxRangeOnMap = ui->maxRangeOnMap->isChecked();
        m_changed.append("displayMaxRangeOnMap");
    }
    if (ui->favourLivery->isChecked() != m_settings->m_favourLivery)
    {
        m_settings->m_favourLivery = ui->favourLivery->isChecked();
        m_changed.append("favourLivery");
    }
    if (ui->useLiveryIcons->isChecked() != m_settings->m_useLiveryIcons)
    {
        m_settings->m_useLiveryIcons = ui->useLiveryIcons->isChecked();
        m_changed.append("useLiveryIcons");
    }
    if (ui->atcCallsigns->isChecked() != m_settings->m_atcCallsigns)
    {
        m_settings->m_atcCallsigns = ui->atcCallsigns->isChecked();
        m_changed.append("atcCallsigns");
    }

    // An empty setting means "wherever the default is", so don't store the default path
    QString database = ui->database->text().trimmed();
    if (database == AircraftSettings::defaultDatabaseFilename()) {
        database = "";
    }
    if (database != m_settings->m_databaseFilename)
    {
        m_settings->m_databaseFilename = database;
        m_changed.append("databaseFilename");
    }

    QStringList disabled;
    for (int i = 0; i < ui->sources->count(); i++)
    {
        QListWidgetItem *item = ui->sources->item(i);
        if (item->checkState() != Qt::Checked) {
            disabled.append(item->data(Qt::UserRole).toString());
        }
    }
    if (disabled != m_settings->m_disabledSources)
    {
        m_settings->m_disabledSources = disabled;
        m_changed.append("disabledSources");
    }

    QDialog::accept();
}
