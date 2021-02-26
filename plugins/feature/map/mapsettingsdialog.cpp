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
#include <QColorDialog>
#include <QColor>

#include "util/units.h"

#include "mapsettingsdialog.h"
#include "maplocationdialog.h"

static QString rgbToColor(quint32 rgb)
{
    QColor color = QColor::fromRgb(rgb);
    return QString("%1,%2,%3").arg(color.red()).arg(color.green()).arg(color.blue());
}

static QString backgroundCSS(quint32 rgb)
{
    return QString("QToolButton { background:rgb(%1); }").arg(rgbToColor(rgb));
}

MapSettingsDialog::MapSettingsDialog(MapSettings *settings, QWidget* parent) :
    QDialog(parent),
    m_settings(settings),
    ui(new Ui::MapSettingsDialog)
{
    ui->setupUi(this);
    ui->mapProvider->setCurrentIndex(MapSettings::m_mapProviders.indexOf(settings->m_mapProvider));
    ui->mapBoxApiKey->setText(settings->m_mapBoxApiKey);
    ui->mapBoxStyles->setText(settings->m_mapBoxStyles);
    for (int i = 0; i < ui->sourceList->count(); i++)
         ui->sourceList->item(i)->setCheckState((m_settings->m_sources & (1 << i)) ? Qt::Checked : Qt::Unchecked);
    ui->groundTrackColor->setStyleSheet(backgroundCSS(m_settings->m_groundTrackColor));
    ui->predictedGroundTrackColor->setStyleSheet(backgroundCSS(m_settings->m_predictedGroundTrackColor));
}

MapSettingsDialog::~MapSettingsDialog()
{
    delete ui;
}

void MapSettingsDialog::accept()
{
    QString mapProvider = MapSettings::m_mapProviders[ui->mapProvider->currentIndex()];
    QString mapBoxApiKey = ui->mapBoxApiKey->text();
    QString mapBoxStyles = ui->mapBoxStyles->text();
    if ((mapProvider != m_settings->m_mapProvider)
        || (mapBoxApiKey != m_settings->m_mapBoxApiKey)
        || (mapBoxStyles != m_settings->m_mapBoxStyles))
    {
        m_settings->m_mapProvider = mapProvider;
        m_settings->m_mapBoxApiKey = mapBoxApiKey;
        m_settings->m_mapBoxStyles = mapBoxStyles;
        m_mapSettingsChanged = true;
    }
    else
        m_mapSettingsChanged = false;
    m_settings->m_sources = 0;
    quint32 sources = MapSettings::SOURCE_STATION;
    for (int i = 0; i < ui->sourceList->count(); i++)
        sources |= (ui->sourceList->item(i)->checkState() == Qt::Checked) << i;
    m_sourcesChanged = sources != m_settings->m_sources;
    m_settings->m_sources = sources;
    QDialog::accept();
}

void MapSettingsDialog::on_groundTrackColor_clicked()
{
    QColorDialog dialog(QColor::fromRgb(m_settings->m_groundTrackColor), this);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings->m_groundTrackColor = dialog.selectedColor().rgb();
        ui->groundTrackColor->setStyleSheet(backgroundCSS(m_settings->m_groundTrackColor));
    }
}

void MapSettingsDialog::on_predictedGroundTrackColor_clicked()
{
    QColorDialog dialog(QColor::fromRgb(m_settings->m_predictedGroundTrackColor), this);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings->m_predictedGroundTrackColor = dialog.selectedColor().rgb();
        ui->predictedGroundTrackColor->setStyleSheet(backgroundCSS(m_settings->m_predictedGroundTrackColor));
    }
}
