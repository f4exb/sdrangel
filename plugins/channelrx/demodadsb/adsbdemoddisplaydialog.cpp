///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <QFontDialog>
#ifdef QT_LOCATION_FOUND
#include <QGeoServiceProvider>
#endif
#include <QDebug>

#include "adsbdemoddisplaydialog.h"

ADSBDemodDisplayDialog::ADSBDemodDisplayDialog(ADSBDemodSettings *settings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::ADSBDemodDisplayDialog),
    m_settings(settings),
    m_fontName(settings->m_tableFontName),
    m_fontSize(settings->m_tableFontSize)
{
    ui->setupUi(this);

#ifdef QT_LOCATION_FOUND
    QStringList mapProviders = QGeoServiceProvider::availableServiceProviders();
    if (!mapProviders.contains("osm")) {
        ui->mapProvider->removeItem(ui->mapProvider->findText("osm"));
    }
    if (!mapProviders.contains("mapboxgl")) {
        ui->mapProvider->removeItem(ui->mapProvider->findText("mapboxgl"));
    }
#else
    QStringList mapProviders;
#endif

    ui->timeout->setValue(settings->m_removeTimeout);
    ui->aircraftMinZoom->setValue(settings->m_aircraftMinZoom);
    ui->airportRange->setValue(settings->m_airportRange);
    ui->airportSize->setCurrentIndex((int)settings->m_airportMinimumSize);
    ui->heliports->setChecked(settings->m_displayHeliports);
    ui->units->setCurrentIndex((int)settings->m_siUnits);
    ui->autoResizeTableColumns->setChecked(settings->m_autoResizeTableColumns);
    ui->aviationstackAPIKey->setText(settings->m_aviationstackAPIKey);
    ui->checkWXAPIKey->setText(settings->m_checkWXAPIKey);
    for (const auto& airspace: settings->m_airspaces)
    {
        QList<QListWidgetItem *> items = ui->airspaces->findItems(airspace, Qt::MatchExactly);
        for (const auto& item: items) {
            item->setCheckState(Qt::Checked);
        }
    }
    ui->airspaceRange->setValue(settings->m_airspaceRange);
    int idx = ui->mapProvider->findText(settings->m_mapProvider);
    if (idx != -1) {
        ui->mapProvider->setCurrentText(settings->m_mapProvider);
    }
    ui->mapType->setCurrentIndex((int)settings->m_mapType);
    ui->maptilerAPIKey->setText(m_settings->m_maptilerAPIKey);
    ui->navAids->setChecked(settings->m_displayNavAids);
    ui->atcCallsigns->setChecked(settings->m_atcCallsigns);
    ui->photos->setChecked(settings->m_displayPhotos);
    ui->verboseModelMatching->setChecked(settings->m_verboseModelMatching);
    ui->favourLivery->setChecked(settings->m_favourLivery);
    ui->transitionAltitude->setValue(settings->m_transitionAlt);
    for (auto i = ADSBDemodSettings::m_palettes.cbegin(), end = ADSBDemodSettings::m_palettes.cend(); i != end; ++i) {
        ui->flightPathPalette->addItem(i.key());
    }
    ui->flightPathPalette->setCurrentText(settings->m_flightPathPaletteName);
}

ADSBDemodDisplayDialog::~ADSBDemodDisplayDialog()
{
    delete ui;
}

void ADSBDemodDisplayDialog::accept()
{
    if (m_settings->m_removeTimeout != ui->timeout->value())
    {
        m_settings->m_removeTimeout = ui->timeout->value();
        m_settingsKeys.append("removeTimeout");
    }
    if (m_settings->m_aircraftMinZoom != ui->aircraftMinZoom->value())
    {
        m_settings->m_aircraftMinZoom = ui->aircraftMinZoom->value();
        m_settingsKeys.append("aircraftMinZoom");
    }
    if (m_settings->m_airportRange != ui->airportRange->value())
    {
        m_settings->m_airportRange = ui->airportRange->value();
        m_settingsKeys.append("airportRange");
    }
    if (m_settings->m_airportMinimumSize != (ADSBDemodSettings::AirportType)ui->airportSize->currentIndex())
    {
        m_settings->m_airportMinimumSize = (ADSBDemodSettings::AirportType)ui->airportSize->currentIndex();
        m_settingsKeys.append("airportMinimumSize");
    }
    if (m_settings->m_displayHeliports != ui->heliports->isChecked())
    {
        m_settings->m_displayHeliports = ui->heliports->isChecked();
        m_settingsKeys.append("displayHeliports");
    }
    if (m_settings->m_siUnits != (ui->units->currentIndex() == 0 ? false : true))
    {
        m_settings->m_siUnits = ui->units->currentIndex() == 0 ? false : true;
        m_settingsKeys.append("siUnits");
    }
    if (m_settings->m_autoResizeTableColumns != ui->autoResizeTableColumns->isChecked())
    {
        m_settings->m_autoResizeTableColumns = ui->autoResizeTableColumns->isChecked();
        m_settingsKeys.append("autoResizeTableColumns");
    }
    if (m_settings->m_aviationstackAPIKey != ui->aviationstackAPIKey->text())
    {
        m_settings->m_aviationstackAPIKey = ui->aviationstackAPIKey->text();
        m_settingsKeys.append("aviationstackAPIKey");
    }
    if (m_settings->m_checkWXAPIKey != ui->checkWXAPIKey->text())
    {
        m_settings->m_checkWXAPIKey = ui->checkWXAPIKey->text();
        m_settingsKeys.append("checkWXAPIKey");
    }
    QStringList airspaces;
    for (int i = 0; i < ui->airspaces->count(); i++)
    {
        QListWidgetItem *item = ui->airspaces->item(i);
        if (item->checkState() == Qt::Checked) {
            airspaces.append(item->text());
        }
    }
    if (m_settings->m_airspaces != airspaces)
    {
        m_settings->m_airspaces = airspaces;
        m_settingsKeys.append("airspaces");
    }
    if (m_settings->m_airspaceRange != ui->airspaceRange->value())
    {
        m_settings->m_airspaceRange = ui->airspaceRange->value();
        m_settingsKeys.append("airspaceRange");
    }
    if (m_settings->m_mapProvider != ui->mapProvider->currentText())
    {
        m_settings->m_mapProvider = ui->mapProvider->currentText();
        m_settingsKeys.append("mapProvider");
    }
    if (m_settings->m_mapType != (ADSBDemodSettings::MapType)ui->mapType->currentIndex())
    {
        m_settings->m_mapType = (ADSBDemodSettings::MapType)ui->mapType->currentIndex();
        m_settingsKeys.append("mapType");
    }
    if (m_settings->m_maptilerAPIKey != ui->maptilerAPIKey->text())
    {
        m_settings->m_maptilerAPIKey = ui->maptilerAPIKey->text();
        m_settingsKeys.append("maptilerAPIKey");
    }
    if (m_settings->m_displayNavAids != ui->navAids->isChecked())
    {
        m_settings->m_displayNavAids = ui->navAids->isChecked();
        m_settingsKeys.append("displayNavAids");
    }
    if (m_settings->m_atcCallsigns != ui->atcCallsigns->isChecked())
    {
        m_settings->m_atcCallsigns = ui->atcCallsigns->isChecked();
        m_settingsKeys.append("atcCallsigns");
    }
    if (m_settings->m_displayPhotos != ui->photos->isChecked())
    {
        m_settings->m_displayPhotos = ui->photos->isChecked();
        m_settingsKeys.append("displayPhotos");
    }
    if (m_settings->m_verboseModelMatching != ui->verboseModelMatching->isChecked())
    {
        m_settings->m_verboseModelMatching = ui->verboseModelMatching->isChecked();
        m_settingsKeys.append("verboseModelMatching");
    }
    if (m_settings->m_favourLivery != ui->favourLivery->isChecked())
    {
        m_settings->m_favourLivery = ui->favourLivery->isChecked();
        m_settingsKeys.append("favourLivery");
    }
    if (m_settings->m_transitionAlt != ui->transitionAltitude->value())
    {
        m_settings->m_transitionAlt = ui->transitionAltitude->value();
        m_settingsKeys.append("transitionAlt");
    }
    if (m_settings->m_tableFontName != m_fontName)
    {
        m_settings->m_tableFontName = m_fontName;
        m_settingsKeys.append("tableFontName");
    }
    if (m_settings->m_tableFontSize != m_fontSize)
    {
        m_settings->m_tableFontSize = m_fontSize;
        m_settingsKeys.append("tableFontSize");
    }
    if (m_settings->m_flightPathPaletteName != ui->flightPathPalette->currentText())
    {
        m_settings->m_flightPathPaletteName = ui->flightPathPalette->currentText();
        m_settingsKeys.append("flightPathPaletteName");
        m_settings->applyPalette();
    }
    QDialog::accept();
}

void ADSBDemodDisplayDialog::on_font_clicked()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, QFont(m_fontName, m_fontSize), this);
    if (ok)
    {
        m_fontName = font.family();
        m_fontSize = font.pointSize();
    }
}
