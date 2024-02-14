///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
#include <QToolButton>

#include "skymapsettingsdialog.h"

SkyMapSettingsDialog::SkyMapSettingsDialog(SkyMapSettings *settings, QWidget* parent) :
    QDialog(parent),
    m_settings(settings),
    ui(new Ui::SkyMapSettingsDialog)
{
    ui->setupUi(this);

    ui->hpbw->setValue(m_settings->m_hpbw);
    ui->latitude->setText(QString::number(m_settings->m_latitude, 'f', 6));
    ui->longitude->setText(QString::number(m_settings->m_longitude, 'f', 6));
    ui->altitude->setValue(m_settings->m_altitude);
    ui->useMyPosition->setChecked(m_settings->m_useMyPosition);

    // WWT
    ui->constellationBoundaries->setChecked(m_settings->m_wwtSettings.value("constellationBoundaries").toBool());
    ui->constellationFigures->setChecked(m_settings->m_wwtSettings.value("constellationFigures").toBool());
    ui->constellationLabels->setChecked(m_settings->m_wwtSettings.value("constellationLabels").toBool());
    ui->constellationPictures->setChecked(m_settings->m_wwtSettings.value("constellationPictures").toBool());
    ui->constellationSelection->setChecked(m_settings->m_wwtSettings.value("constellationSelection").toBool());

    ui->ecliptic->setChecked(m_settings->m_wwtSettings.value("ecliptic").toBool());
    ui->eclipticOverviewText->setChecked(m_settings->m_wwtSettings.value("eclipticOverviewText").toBool());
    ui->eclipticGrid->setChecked(m_settings->m_wwtSettings.value("eclipticGrid").toBool());
    ui->eclipticGridText->setChecked(m_settings->m_wwtSettings.value("eclipticGridText").toBool());
    ui->altAzGrid->setChecked(m_settings->m_wwtSettings.value("altAzGrid").toBool());
    ui->altAzGridText->setChecked(m_settings->m_wwtSettings.value("altAzGridText").toBool());
    ui->equatorialGrid->setChecked(m_settings->m_wwtSettings.value("equatorialGrid").toBool());
    ui->equatorialGridText->setChecked(m_settings->m_wwtSettings.value("equatorialGridText").toBool());
    ui->galacticGrid->setChecked(m_settings->m_wwtSettings.value("galacticGrid").toBool());
    ui->galacticGridText->setChecked(m_settings->m_wwtSettings.value("galacticGridText").toBool());
    ui->precessionChart->setChecked(m_settings->m_wwtSettings.value("precessionChart").toBool());

    ui->solarSystemCosmos->setChecked(m_settings->m_wwtSettings.value("solarSystemCosmos").toBool());
    ui->solarSystemLighting->setChecked(m_settings->m_wwtSettings.value("solarSystemLighting").toBool());
    ui->solarSystemMilkyWay->setChecked(m_settings->m_wwtSettings.value("solarSystemMilkyWay").toBool());
    ui->solarSystemMinorOrbits->setChecked(m_settings->m_wwtSettings.value("solarSystemMinorOrbits").toBool());
    ui->solarSystemMinorPlanets->setChecked(m_settings->m_wwtSettings.value("solarSystemMinorPlanets").toBool());
    ui->solarSystemMultiRes->setChecked(m_settings->m_wwtSettings.value("solarSystemMultiRes").toBool());
    ui->solarSystemOrbits->setChecked(m_settings->m_wwtSettings.value("solarSystemOrbits").toBool());
    //ui->solarSystemOverlays->setChecked(m_settings->m_wwtSettings.value("solarSystemOverlays").toBool());
    ui->solarSystemPlanets->setChecked(m_settings->m_wwtSettings.value("solarSystemPlanets").toBool());
    ui->solarSystemStars->setChecked(m_settings->m_wwtSettings.value("solarSystemStars").toBool());

    ui->iss->setChecked(m_settings->m_wwtSettings.value("iss").toBool());
}

SkyMapSettingsDialog::~SkyMapSettingsDialog()
{
    delete ui;
}

void SkyMapSettingsDialog::accept()
{
    QDialog::accept();

    if (m_settings->m_hpbw != ui->hpbw->value())
    {
        m_settings->m_hpbw = ui->hpbw->value();
        m_settingsKeysChanged.append("hpbw");
    }
    if (m_settings->m_latitude != ui->latitude->text().toFloat())
    {
        m_settings->m_latitude = ui->latitude->text().toFloat();
        m_settingsKeysChanged.append("latitude");
    }
    if (m_settings->m_longitude != ui->longitude->text().toFloat())
    {
        m_settings->m_longitude = ui->longitude->text().toFloat();
        m_settingsKeysChanged.append("longitude");
    }
    if (m_settings->m_altitude != ui->altitude->value())
    {
        m_settings->m_altitude = ui->altitude->value();
        m_settingsKeysChanged.append("altitude");
    }
    if (m_settings->m_useMyPosition != ui->useMyPosition->isChecked())
    {
        m_settings->m_useMyPosition = ui->useMyPosition->isChecked();
        m_settingsKeysChanged.append("useMyPosition");
    }

    m_settings->m_wwtSettings.insert("constellationBoundaries", ui->constellationBoundaries->isChecked());
    m_settings->m_wwtSettings.insert("constellationFigures", ui->constellationFigures->isChecked());
    m_settings->m_wwtSettings.insert("constellationLabels", ui->constellationLabels->isChecked());
    m_settings->m_wwtSettings.insert("constellationPictures", ui->constellationPictures->isChecked());
    m_settings->m_wwtSettings.insert("constellationSelection", ui->constellationSelection->isChecked());

    m_settings->m_wwtSettings.insert("ecliptic", ui->ecliptic->isChecked());
    m_settings->m_wwtSettings.insert("eclipticOverviewText", ui->eclipticOverviewText->isChecked());
    m_settings->m_wwtSettings.insert("eclipticGrid", ui->eclipticGrid->isChecked());
    m_settings->m_wwtSettings.insert("eclipticGridText", ui->eclipticGridText->isChecked());
    m_settings->m_wwtSettings.insert("altAzGrid", ui->altAzGrid->isChecked());
    m_settings->m_wwtSettings.insert("altAzGridText", ui->altAzGridText->isChecked());
    m_settings->m_wwtSettings.insert("equatorialGrid", ui->equatorialGrid->isChecked());
    m_settings->m_wwtSettings.insert("equatorialGridText", ui->equatorialGridText->isChecked());
    m_settings->m_wwtSettings.insert("galacticGrid", ui->galacticGrid->isChecked());
    m_settings->m_wwtSettings.insert("galacticGridText", ui->galacticGridText->isChecked());
    m_settings->m_wwtSettings.insert("precessionChart", ui->precessionChart->isChecked());

    m_settings->m_wwtSettings.insert("solarSystemCosmos", ui->solarSystemCosmos->isChecked());
    m_settings->m_wwtSettings.insert("solarSystemLighting", ui->solarSystemLighting->isChecked());
    m_settings->m_wwtSettings.insert("solarSystemMilkyWay", ui->solarSystemMilkyWay->isChecked());
    m_settings->m_wwtSettings.insert("solarSystemMinorOrbits", ui->solarSystemMinorOrbits->isChecked());
    m_settings->m_wwtSettings.insert("solarSystemMinorPlanets", ui->solarSystemMinorPlanets->isChecked());
    m_settings->m_wwtSettings.insert("solarSystemMultiRes", ui->solarSystemMultiRes->isChecked());
    m_settings->m_wwtSettings.insert("solarSystemOrbits", ui->solarSystemOrbits->isChecked());
    //m_settings->m_wwtSettings.insert("solarSystemOverlays", ui->solarSystemOverlays->isChecked());
    m_settings->m_wwtSettings.insert("solarSystemPlanets", ui->solarSystemPlanets->isChecked());
    m_settings->m_wwtSettings.insert("solarSystemStars", ui->solarSystemStars->isChecked());

    m_settings->m_wwtSettings.insert("iss", ui->iss->isChecked());

    m_settingsKeysChanged.append("wwtSettings"); // Being lazy here
}

void SkyMapSettingsDialog::on_constellationBoundaries_toggled(bool checked)
{
    ui->constellationSelection->setEnabled(checked);
}

void SkyMapSettingsDialog::on_ecliptic_toggled(bool checked)
{
    ui->eclipticOverviewText->setEnabled(checked);
}

void SkyMapSettingsDialog::on_eclipticGrid_toggled(bool checked)
{
    ui->eclipticGridText->setEnabled(checked);
}

void SkyMapSettingsDialog::on_altAzGrid_toggled(bool checked)
{
    ui->altAzGridText->setEnabled(checked);
}

void SkyMapSettingsDialog::on_equatorialGrid_toggled(bool checked)
{
    ui->equatorialGridText->setEnabled(checked);
}

void SkyMapSettingsDialog::on_galacticGrid_toggled(bool checked)
{
    ui->galacticGridText->setEnabled(checked);
}

void SkyMapSettingsDialog::on_useMyPosition_toggled(bool checked)
{
    ui->latitude->setEnabled(!checked);
    ui->latitudeLabel->setEnabled(!checked);
    ui->longitude->setEnabled(!checked);
    ui->longitudeLabel->setEnabled(!checked);
    ui->altitude->setEnabled(!checked);
    ui->altitudeLabel->setEnabled(!checked);
}
