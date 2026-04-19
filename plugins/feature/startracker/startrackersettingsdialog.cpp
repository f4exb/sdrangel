///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2026 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include "startrackersettingsdialog.h"
#include "spice.h"

#include <QDebug>
#include <QListWidgetItem>
#include <QInputDialog>

StarTrackerSettingsDialog::StarTrackerSettingsDialog(
    StarTrackerSettings *settings,
    QList<QString>& settingsKeys,
    QWidget* parent
) :
    QDialog(parent),
    m_settings(settings),
    m_settingsKeys(settingsKeys),
    ui(new Ui::StarTrackerSettingsDialog),
    m_spiceEphemerides(this)
{
    ui->setupUi(this);
    ui->epoch->setCurrentIndex(settings->m_jnow ? 1 : 0);
    ui->azElUnits->setCurrentIndex((int)settings->m_azElUnits);
    ui->updatePeriod->setValue(settings->m_updatePeriod);
    ui->serverPort->setValue(settings->m_serverPort);
    ui->enableServer->setChecked(settings->m_enableServer);
    ui->refraction->setCurrentIndex(ui->refraction->findText(settings->m_refraction));
    ui->owmAPIKey->setText(settings->m_owmAPIKey);
    ui->weatherUpdatePeriod->setValue(settings->m_weatherUpdatePeriod);
    ui->pressure->setValue(settings->m_pressure);
    ui->temperature->setValue(settings->m_temperature);
    ui->humidity->setValue(settings->m_humidity);
    ui->height->setValue(settings->m_heightAboveSeaLevel);
    ui->temperatureLapseRate->setValue(settings->m_temperatureLapseRate);
    ui->solarFluxData->setCurrentIndex((int)settings->m_solarFluxData);
    ui->solarFluxUnits->setCurrentIndex((int)settings->m_solarFluxUnits);
    ui->drawRotators->setCurrentIndex((int)settings->m_drawRotators);
    ui->drawSunOnMap->setChecked(settings->m_drawSunOnMap);
    ui->drawMoonOnMap->setChecked(settings->m_drawMoonOnMap);
    ui->drawStarOnMap->setChecked(settings->m_drawStarOnMap);
    ui->ephemerides->clear();
    ui->solarSystemBodies->clear();
    for (const auto& ephemeris : settings->m_spiceEphemerides) {
        ui->ephemerides->addItem(ephemeris);
    }
    for (const auto& body : settings->m_solarSystemBodies) {
        ui->solarSystemBodies->addItem(body);
    }

    connect(&m_spiceEphemerides, &SpiceEphemerides::allDownloadsComplete, this, &StarTrackerSettingsDialog::allDownloadsComplete);
}

StarTrackerSettingsDialog::~StarTrackerSettingsDialog()
{
    delete ui;
}

void StarTrackerSettingsDialog::accept()
{
    m_settings->m_jnow = ui->epoch->currentIndex() == 1;
    m_settings->m_azElUnits = (StarTrackerSettings::AzElUnits)ui->azElUnits->currentIndex();
    m_settings->m_updatePeriod = ui->updatePeriod->value();
    m_settings->m_serverPort = (uint16_t)ui->serverPort->value();
    m_settings->m_enableServer = ui->enableServer->isChecked();
    m_settings->m_refraction = ui->refraction->currentText();
    m_settings->m_owmAPIKey = ui->owmAPIKey->text();
    m_settings->m_weatherUpdatePeriod = ui->weatherUpdatePeriod->value();
    m_settings->m_pressure = ui->pressure->value();
    m_settings->m_temperature = ui->temperature->value();
    m_settings->m_humidity = ui->humidity->value();
    m_settings->m_heightAboveSeaLevel = ui->height->value();
    m_settings->m_temperatureLapseRate = ui->temperatureLapseRate->value();
    m_settings->m_solarFluxData = (StarTrackerSettings::SolarFluxData)ui->solarFluxData->currentIndex();
    m_settings->m_solarFluxUnits = (StarTrackerSettings::SolarFluxUnits)ui->solarFluxUnits->currentIndex();
    m_settings->m_drawRotators = (StarTrackerSettings::Rotators)ui->drawRotators->currentIndex();
    m_settings->m_drawSunOnMap = ui->drawSunOnMap->isChecked();
    m_settings->m_drawMoonOnMap = ui->drawMoonOnMap->isChecked();
    m_settings->m_drawStarOnMap = ui->drawStarOnMap->isChecked();

    m_settingsKeys.append("jnow");
    m_settingsKeys.append("azElUnits");
    m_settingsKeys.append("updatePeriod");
    m_settingsKeys.append("serverPort");
    m_settingsKeys.append("enableServer");
    m_settingsKeys.append("refraction");
    m_settingsKeys.append("owmAPIKey");
    m_settingsKeys.append("weatherUpdatePeriod");
    m_settingsKeys.append("pressure");
    m_settingsKeys.append("temperature");
    m_settingsKeys.append("humidity");
    m_settingsKeys.append("heightAboveSeaLevel");
    m_settingsKeys.append("temperatureLapseRate");
    m_settingsKeys.append("solarFluxData");
    m_settingsKeys.append("solarFluxUnits");
    m_settingsKeys.append("drawRotators");
    m_settingsKeys.append("drawSunOnMap");
    m_settingsKeys.append("drawMoonOnMap");
    m_settingsKeys.append("drawStarOnMap");

    QStringList ephemerides = getEphemerides();;
    if (ephemerides != m_settings->m_spiceEphemerides)
    {
        m_settings->m_spiceEphemerides = ephemerides;
        m_settingsKeys.append("spiceEphemerides");
    }

    QStringList solarSystemBodies;
    for (int i = 0; i < ui->solarSystemBodies->count(); i++) {
        solarSystemBodies.append(ui->solarSystemBodies->item(i)->text());
    }
    if (solarSystemBodies != m_settings->m_solarSystemBodies)
    {
        m_settings->m_solarSystemBodies = solarSystemBodies;
        m_settingsKeys.append("solarSystemBodies");
    }

    QDialog::accept();
}

void StarTrackerSettingsDialog::on_addEphemeris_clicked()
{
    QListWidgetItem *item = new QListWidgetItem("https://");
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
    ui->ephemerides->addItem(item);
}

void StarTrackerSettingsDialog::on_removeEphemeris_clicked()
{
    QList<QListWidgetItem *> items = ui->ephemerides->selectedItems();
    for (int i = 0; i < items.size(); i++) {
        delete items[i];
    }
}

void StarTrackerSettingsDialog::on_useDefaultEphemeridies_clicked()
{
    ui->ephemerides->clear();
    for (const auto& ephemeris : StarTrackerSettings::m_defaultSpiceEphemerides)
    {
        QListWidgetItem *item = new QListWidgetItem(ephemeris);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
        ui->ephemerides->addItem(item);
    }
}

void StarTrackerSettingsDialog::on_addSolarSystemBody_clicked()
{
    if (!downloadEphemerides()) {
        selectSolarSystemBody();
    }
}

void StarTrackerSettingsDialog::on_removeSolarSystemBody_clicked()
{
    QList<QListWidgetItem *> items = ui->solarSystemBodies->selectedItems();
    for (int i = 0; i < items.size(); i++) {
        delete items[i];
    }
}

QStringList StarTrackerSettingsDialog::getEphemerides() const
{
    QStringList ephemerides;
    for (int i = 0; i < ui->ephemerides->count(); i++) {
        ephemerides.append(ui->ephemerides->item(i)->text());
    }
    return ephemerides;
}

bool StarTrackerSettingsDialog::downloadEphemerides()
{
    return m_spiceEphemerides.download(getEphemerides());
}

void StarTrackerSettingsDialog::allDownloadsComplete()
{
    selectSolarSystemBody();
}

void StarTrackerSettingsDialog::selectSolarSystemBody()
{
    QStringList availableSolarSystemBodies = m_spiceEphemerides.getTargets(getEphemerides());
    bool ok;
    QString body = QInputDialog::getItem(this, "Select body to add", "Body", availableSolarSystemBodies, 0, false, &ok);
    if (ok)
    {
        QListWidgetItem *item = new QListWidgetItem(body);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
        ui->solarSystemBodies->addItem(item);
    }
}
