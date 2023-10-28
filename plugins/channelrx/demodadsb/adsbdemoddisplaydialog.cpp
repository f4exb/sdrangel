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
    ui->timeout->setValue(settings->m_removeTimeout);
    ui->aircraftMinZoom->setValue(settings->m_aircraftMinZoom);
    ui->airportRange->setValue(settings->m_airportRange);
    ui->airportSize->setCurrentIndex((int)settings->m_airportMinimumSize);
    ui->heliports->setChecked(settings->m_displayHeliports);
    ui->units->setCurrentIndex((int)settings->m_siUnits);
    ui->displayStats->setChecked(settings->m_displayDemodStats);
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
    ui->mapProvider->setCurrentText(settings->m_mapProvider);
    ui->mapType->setCurrentIndex((int)settings->m_mapType);
    ui->navAids->setChecked(settings->m_displayNavAids);
    ui->atcCallsigns->setChecked(settings->m_atcCallsigns);
    ui->photos->setChecked(settings->m_displayPhotos);
    ui->verboseModelMatching->setChecked(settings->m_verboseModelMatching);
    ui->airfieldElevation->setValue(settings->m_airfieldElevation);
    ui->transitionAltitude->setValue(settings->m_transitionAlt);
}

ADSBDemodDisplayDialog::~ADSBDemodDisplayDialog()
{
    delete ui;
}

void ADSBDemodDisplayDialog::accept()
{
    m_settings->m_removeTimeout = ui->timeout->value();
    m_settings->m_aircraftMinZoom = ui->aircraftMinZoom->value();
    m_settings->m_airportRange = ui->airportRange->value();
    m_settings->m_airportMinimumSize = (ADSBDemodSettings::AirportType)ui->airportSize->currentIndex();
    m_settings->m_displayHeliports = ui->heliports->isChecked();
    m_settings->m_siUnits = ui->units->currentIndex() == 0 ? false : true;
    m_settings->m_displayDemodStats = ui->displayStats->isChecked();
    m_settings->m_autoResizeTableColumns = ui->autoResizeTableColumns->isChecked();
    m_settings->m_aviationstackAPIKey = ui->aviationstackAPIKey->text();
    m_settings->m_checkWXAPIKey = ui->checkWXAPIKey->text();
    m_settings->m_airspaces = QStringList();
    for (int i = 0; i < ui->airspaces->count(); i++)
    {
        QListWidgetItem *item = ui->airspaces->item(i);
        if (item->checkState() == Qt::Checked) {
            m_settings->m_airspaces.append(item->text());
        }
    }
    m_settings->m_airspaceRange = ui->airspaceRange->value();
    m_settings->m_mapProvider = ui->mapProvider->currentText();
    m_settings->m_mapType = (ADSBDemodSettings::MapType)ui->mapType->currentIndex();
    m_settings->m_displayNavAids = ui->navAids->isChecked();
    m_settings->m_atcCallsigns = ui->atcCallsigns->isChecked();
    m_settings->m_displayPhotos = ui->photos->isChecked();
    m_settings->m_verboseModelMatching = ui->verboseModelMatching->isChecked();
    m_settings->m_airfieldElevation = ui->airfieldElevation->value();
    m_settings->m_transitionAlt = ui->transitionAltitude->value();
    m_settings->m_tableFontName = m_fontName;
    m_settings->m_tableFontSize = m_fontSize;
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
