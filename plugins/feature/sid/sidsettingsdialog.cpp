///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023-2024 Jon Beniston, M7RCE                                   //
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

#include "util/units.h"
#include "gui/colordialog.h"
#include "gui/tablecolorchooser.h"

#include "sidsettingsdialog.h"

SIDSettingsDialog::SIDSettingsDialog(SIDSettings *settings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::SIDSettingsDialog),
    m_settings(settings),
    m_fileDialog(nullptr, "Select file to write autosave CSV data to", "", "*.csv")
{
    ui->setupUi(this);
    ui->period->setValue(m_settings->m_period);
    ui->autosave->setChecked(m_settings->m_autosave);
    ui->autoload->setChecked(m_settings->m_autoload);
    ui->filename->setText(m_settings->m_filename);
    ui->autosavePeriod->setValue(m_settings->m_autosavePeriod);
    switch (m_settings->m_legendAlignment) {
    case Qt::AlignTop:
        ui->legendAlignment->setCurrentIndex(0);
        break;
    case Qt::AlignRight:
        ui->legendAlignment->setCurrentIndex(1);
        break;
    case Qt::AlignBottom:
        ui->legendAlignment->setCurrentIndex(2);
        break;
    case Qt::AlignLeft:
        ui->legendAlignment->setCurrentIndex(3);
        break;
    }
    ui->displayAxisTitles->setChecked(m_settings->m_displayAxisTitles);
    ui->displaySecondaryAxis->setChecked(m_settings->m_displaySecondaryAxis);

    m_settings->createChannelSettings();

    // Add settings to table
    for (int i = 0; i < m_settings->m_channelSettings.size(); i++)
    {
        SIDSettings::ChannelSettings *channelSettings = &m_settings->m_channelSettings[i];

        int row = ui->channels->rowCount();
        ui->channels->setRowCount(row+1);

        ui->channels->setItem(row, CHANNELS_COL_ID, new QTableWidgetItem(channelSettings->m_id));

        QTableWidgetItem *enableItem = new QTableWidgetItem();
        enableItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        enableItem->setCheckState(channelSettings->m_enabled ? Qt::Checked : Qt::Unchecked);
        ui->channels->setItem(row, CHANNELS_COL_ENABLED, enableItem);

        ui->channels->setItem(row, CHANNELS_COL_LABEL, new QTableWidgetItem(channelSettings->m_label));

        TableColorChooser *colorGUI = new TableColorChooser(ui->channels, row, CHANNELS_COL_COLOR, false, channelSettings->m_color.rgba());
        m_channelColorGUIs.append(colorGUI);
    }
    ui->channels->resizeColumnsToContents();

    addColor("Primary Long X-Ray", m_settings->m_xrayLongColors[0]);
    addColor("Secondary Long X-Ray", m_settings->m_xrayLongColors[1]);
    addColor("Primary Short X-Ray ", m_settings->m_xrayShortColors[0]);
    addColor("Secondary Short X-Ray", m_settings->m_xrayShortColors[1]);
    addColor("GRB", m_settings->m_grbColor);
    addColor("STIX", m_settings->m_stixColor);
    addColor("10 MeV Proton", m_settings->m_protonColors[0]);
    addColor("100 MeV Proton", m_settings->m_protonColors[2]);
    ui->colors->resizeColumnsToContents();
}

void SIDSettingsDialog::addColor(const QString& name, QRgb rgb)
{
    int row = ui->colors->rowCount();
    ui->colors->setRowCount(row+1);

    ui->colors->setItem(row, COLORS_COL_NAME, new QTableWidgetItem(name));

    TableColorChooser *colorGUI = new TableColorChooser(ui->colors, row, COLORS_COL_COLOR, false, rgb);
    m_colorGUIs.append(colorGUI);
}

SIDSettingsDialog::~SIDSettingsDialog()
{
    delete ui;
    qDeleteAll(m_channelColorGUIs);
    qDeleteAll(m_colorGUIs);
}

void SIDSettingsDialog::accept()
{
    m_settings->m_period = ui->period->value();
    m_settings->m_autosave = ui->autosave->isChecked();
    m_settings->m_autoload = ui->autoload->isChecked();
    m_settings->m_filename = ui->filename->text();
    m_settings->m_autosavePeriod = ui->autosavePeriod->value();

    switch (ui->legendAlignment->currentIndex() ) {
    case 0:
        m_settings->m_legendAlignment = Qt::AlignTop;
        break;
    case 1:
        m_settings->m_legendAlignment = Qt::AlignRight;
        break;
    case 2:
        m_settings->m_legendAlignment = Qt::AlignBottom;
        break;
    case 3:
        m_settings->m_legendAlignment = Qt::AlignLeft;
        break;
    }
    m_settings->m_displayAxisTitles = ui->displayAxisTitles->isChecked();
    m_settings->m_displaySecondaryAxis = ui->displaySecondaryAxis->isChecked();

    m_settings->m_xrayLongColors[0] = m_colorGUIs[0]->m_color;
    m_settings->m_xrayLongColors[1] = m_colorGUIs[1]->m_color;
    m_settings->m_xrayShortColors[0] = m_colorGUIs[2]->m_color;
    m_settings->m_xrayShortColors[1] = m_colorGUIs[3]->m_color;
    m_settings->m_grbColor = m_colorGUIs[4]->m_color;
    m_settings->m_stixColor = m_colorGUIs[5]->m_color;
    m_settings->m_protonColors[0] = m_colorGUIs[6]->m_color;
    m_settings->m_protonColors[2] = m_colorGUIs[7]->m_color;

    if (m_removeIds.size() > 0) {
        emit removeChannels(m_removeIds);
    }
    for (int i = 0; i < m_settings->m_channelSettings.size(); i++)
    {
        SIDSettings::ChannelSettings *channelSettings = &m_settings->m_channelSettings[i];

        channelSettings->m_id = ui->channels->item(i, CHANNELS_COL_ID)->text();
        channelSettings->m_enabled = ui->channels->item(i, CHANNELS_COL_ENABLED)->checkState() == Qt::Checked;
        channelSettings->m_label = ui->channels->item(i, CHANNELS_COL_LABEL)->text();
        channelSettings->m_color = m_channelColorGUIs[i]->m_color;
    }

    QDialog::accept();
}

void SIDSettingsDialog::on_browse_clicked()
{
    m_fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (m_fileDialog.exec())
    {
        QStringList fileNames = m_fileDialog.selectedFiles();
        if (fileNames.size() > 0) {
            ui->filename->setText(fileNames[0]);
        }
    }
}

void SIDSettingsDialog::on_remove_clicked()
{
    QItemSelectionModel *select = ui->channels->selectionModel();
    while (select->hasSelection())
    {
        QModelIndexList list = select->selectedRows();
        int row = list[0].row();
        m_removeIds.append(ui->channels->item(row, CHANNELS_COL_ID)->text());
        ui->channels->removeRow(row);
    }
}
