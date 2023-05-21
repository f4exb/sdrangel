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

#include "satellitetrackersettingsdialog.h"
#include <QDebug>

SatelliteTrackerSettingsDialog::SatelliteTrackerSettingsDialog(SatelliteTrackerSettings *settings,
        QWidget* parent) :
    QDialog(parent),
    m_settings(settings),
    ui(new Ui::SatelliteTrackerSettingsDialog)
{
    ui->setupUi(this);
    ui->height->setValue(settings->m_heightAboveSeaLevel);
    ui->predictionPeriod->setValue(settings->m_predictionPeriod);
    ui->passStartTime->setTime(settings->m_passStartTime);
    ui->passFinishTime->setTime(settings->m_passFinishTime);
    ui->minimumAOSElevation->setValue(settings->m_minAOSElevation);
    ui->minimumPassElevation->setValue(settings->m_minPassElevation);
    ui->aosSpeech->setText(settings->m_aosSpeech);
    ui->losSpeech->setText(settings->m_losSpeech);
    ui->rotatorMaximumAzimuth->setValue(settings->m_rotatorMaxAzimuth);
    ui->rotatorMaximumElevation->setValue(settings->m_rotatorMaxElevation);
    ui->azimuthOffset->setValue(settings->m_azimuthOffset);
    ui->elevationOffset->setValue(settings->m_elevationOffset);
    ui->aosCommand->setText(settings->m_aosCommand);
    ui->losCommand->setText(settings->m_losCommand);
    ui->updatePeriod->setValue(settings->m_updatePeriod);
    ui->dopplerPeriod->setValue(settings->m_dopplerPeriod);
    ui->defaultFrequency->setValue(settings->m_defaultFrequency / 1000000.0);
    ui->azElUnits->setCurrentIndex((int)settings->m_azElUnits);
    ui->groundTrackPoints->setValue(settings->m_groundTrackPoints);
    ui->drawRotators->setCurrentIndex((int)settings->m_drawRotators);
    ui->dateFormat->setText(settings->m_dateFormat);
    ui->utc->setChecked(settings->m_utc);
    ui->drawOnMap->setChecked(settings->m_drawOnMap);
    for (int i = 0; i < settings->m_tles.size(); i++)
    {
        QListWidgetItem *item = new QListWidgetItem(settings->m_tles[i]);
        item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEditable|Qt::ItemIsEnabled);
        ui->tles->addItem(item);
    }
    ui->replayEnabled->setChecked(settings->m_replayEnabled);
    ui->replayDateTime->setDateTime(settings->m_replayStartDateTime);
    ui->sendTimeToMap->setChecked(settings->m_sendTimeToMap);
}

SatelliteTrackerSettingsDialog::~SatelliteTrackerSettingsDialog()
{
    delete ui;
}

void SatelliteTrackerSettingsDialog::on_addTle_clicked()
{
    QListWidgetItem *item = new QListWidgetItem("http://");
    item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEditable|Qt::ItemIsEnabled);
    ui->tles->addItem(item);
}

void SatelliteTrackerSettingsDialog::on_removeTle_clicked()
{
    QList<QListWidgetItem *> items = ui->tles->selectedItems();
    for (int i = 0; i < items.size(); i++)
        delete items[i];
}

void SatelliteTrackerSettingsDialog::accept()
{
    m_settings->m_heightAboveSeaLevel = ui->height->value();
    m_settings->m_predictionPeriod = ui->predictionPeriod->value();
    m_settings->m_passStartTime = ui->passStartTime->time();
    m_settings->m_passFinishTime = ui->passFinishTime->time();
    m_settings->m_minAOSElevation = ui->minimumAOSElevation->value();
    m_settings->m_minPassElevation = ui->minimumPassElevation->value();
    m_settings->m_rotatorMaxAzimuth = ui->rotatorMaximumAzimuth->value();
    m_settings->m_rotatorMaxElevation = ui->rotatorMaximumElevation->value();
    m_settings->m_azimuthOffset = ui->azimuthOffset->value();
    m_settings->m_elevationOffset = ui->elevationOffset->value();
    m_settings->m_aosSpeech = ui->aosSpeech->text();
    m_settings->m_losSpeech = ui->losSpeech->text();
    m_settings->m_aosCommand = ui->aosCommand->text();
    m_settings->m_losCommand = ui->losCommand->text();
    m_settings->m_updatePeriod = (float)ui->updatePeriod->value();
    m_settings->m_dopplerPeriod = (float)ui->dopplerPeriod->value();
    m_settings->m_defaultFrequency = (float)(ui->defaultFrequency->value() * 1000000.0);
    m_settings->m_azElUnits = (SatelliteTrackerSettings::AzElUnits)ui->azElUnits->currentIndex();
    m_settings->m_groundTrackPoints = ui->groundTrackPoints->value();
    m_settings->m_drawRotators = (SatelliteTrackerSettings::Rotators)ui->drawRotators->currentIndex();
    m_settings->m_dateFormat = ui->dateFormat->text();
    m_settings->m_utc = ui->utc->isChecked();
    m_settings->m_drawOnMap = ui->drawOnMap->isChecked();
    m_settings->m_tles.clear();
    for (int i = 0; i < ui->tles->count(); i++) {
        m_settings->m_tles.append(ui->tles->item(i)->text());
    }
    m_settings->m_replayEnabled = ui->replayEnabled->isChecked();
    m_settings->m_replayStartDateTime = ui->replayDateTime->dateTime();
    m_settings->m_sendTimeToMap = ui->sendTimeToMap->isChecked();
    QDialog::accept();
}
