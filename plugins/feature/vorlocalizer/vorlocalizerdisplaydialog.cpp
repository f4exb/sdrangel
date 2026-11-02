///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifdef QT_LOCATION_FOUND
#include <QGeoServiceProvider>
#endif

#include "vorlocalizerdisplaydialog.h"

VORLocalizerDisplayDialog::VORLocalizerDisplayDialog(VORLocalizerSettings *settings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::VORLocalizerDisplayDialog),
    m_settings(settings)
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
#endif

    int idx = ui->mapProvider->findText(settings->m_mapProvider);
    if (idx != -1) {
        ui->mapProvider->setCurrentIndex(idx);
    }
    ui->mapBoxAPIKey->setText(settings->m_mapBoxAPIKey);
}

VORLocalizerDisplayDialog::~VORLocalizerDisplayDialog()
{
    delete ui;
}

void VORLocalizerDisplayDialog::accept()
{
    if (!ui->mapProvider->currentText().isEmpty() && (m_settings->m_mapProvider != ui->mapProvider->currentText()))
    {
        m_settings->m_mapProvider = ui->mapProvider->currentText();
        m_settingsKeys.append("mapProvider");
    }
    if (m_settings->m_mapBoxAPIKey != ui->mapBoxAPIKey->text())
    {
        m_settings->m_mapBoxAPIKey = ui->mapBoxAPIKey->text();
        m_settingsKeys.append("mapBoxAPIKey");
    }

    QDialog::accept();
}
