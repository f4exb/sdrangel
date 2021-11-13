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

ADSBDemodDisplayDialog::ADSBDemodDisplayDialog(
        int removeTimeout, float airportRange, ADSBDemodSettings::AirportType airportMinimumSize,
        bool displayHeliports, bool siUnits, QString fontName, int fontSize, bool displayDemodStats,
        bool autoResizeTableColumns, const QString& apiKey, QStringList airspaces, float airspaceRange,
        ADSBDemodSettings::MapType mapType, bool displayNavAids, bool displayPhotos, QWidget* parent) :
    QDialog(parent),
    m_fontName(fontName),
    m_fontSize(fontSize),
    ui(new Ui::ADSBDemodDisplayDialog)
{
    ui->setupUi(this);
    ui->timeout->setValue(removeTimeout);
    ui->airportRange->setValue(airportRange);
    ui->airportSize->setCurrentIndex((int)airportMinimumSize);
    ui->heliports->setChecked(displayHeliports);
    ui->units->setCurrentIndex((int)siUnits);
    ui->displayStats->setChecked(displayDemodStats);
    ui->autoResizeTableColumns->setChecked(autoResizeTableColumns);
    ui->apiKey->setText(apiKey);
    for (const auto& airspace: airspaces)
    {
        QList<QListWidgetItem *> items = ui->airspaces->findItems(airspace, Qt::MatchExactly);
        for (const auto& item: items) {
            item->setCheckState(Qt::Checked);
        }
    }
    ui->airspaceRange->setValue(airspaceRange);
    ui->mapType->setCurrentIndex((int)mapType);
    ui->navAids->setChecked(displayNavAids);
    ui->photos->setChecked(displayPhotos);
}

ADSBDemodDisplayDialog::~ADSBDemodDisplayDialog()
{
    delete ui;
}

void ADSBDemodDisplayDialog::accept()
{
    m_removeTimeout = ui->timeout->value();
    m_airportRange = ui->airportRange->value();
    m_airportMinimumSize = (ADSBDemodSettings::AirportType)ui->airportSize->currentIndex();
    m_displayHeliports = ui->heliports->isChecked();
    m_siUnits = ui->units->currentIndex() == 0 ? false : true;
    m_displayDemodStats = ui->displayStats->isChecked();
    m_autoResizeTableColumns = ui->autoResizeTableColumns->isChecked();
    m_apiKey = ui->apiKey->text();
    m_airspaces = QStringList();
    for (int i = 0; i < ui->airspaces->count(); i++)
    {
        QListWidgetItem *item = ui->airspaces->item(i);
        if (item->checkState() == Qt::Checked) {
            m_airspaces.append(item->text());
        }
    }
    m_airspaceRange = ui->airspaceRange->value();
    m_mapType = (ADSBDemodSettings::MapType)ui->mapType->currentIndex();
    m_displayNavAids = ui->navAids->isChecked();
    m_displayPhotos = ui->photos->isChecked();
    QDialog::accept();
}

void ADSBDemodDisplayDialog::on_font_clicked()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, QFont(m_fontName, m_fontSize), this);
    if (ok)
    {
        qDebug() << font;
        m_fontName = font.family();
        m_fontSize = font.pointSize();
    }
}
