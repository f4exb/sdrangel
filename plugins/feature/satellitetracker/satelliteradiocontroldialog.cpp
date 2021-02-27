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
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>

#include "device/deviceset.h"
#include "settings/mainsettings.h"
#include "settings/preset.h"
#include "maincore.h"
#include "util/messagequeue.h"
#include "satelliteradiocontroldialog.h"

SatelliteRadioControlDialog::SatelliteRadioControlDialog(SatelliteTrackerSettings *settings,
        const QHash<QString, SatNogsSatellite *>& satellites,
        QWidget* parent) :
    QDialog(parent),
    m_settings(settings),
    m_satellites(satellites),
    ui(new Ui::SatelliteRadioControlDialog)
{
    ui->setupUi(this);

    // Must resize before setting m_deviceSettings
    resizeTable();

    m_deviceSettings = m_settings->m_deviceSettings;

    for (int i = 0; i < settings->m_satellites.size(); i++)
        ui->satelliteSelect->addItem(settings->m_satellites[i]);
}

SatelliteRadioControlDialog::~SatelliteRadioControlDialog()
{
    delete ui;
}

void SatelliteRadioControlDialog::accept()
{
    for (int i = 0; i < m_devSettingsGUIs.size(); i++)
        m_devSettingsGUIs[i]->accept();
    QDialog::accept();
    m_settings->m_deviceSettings = m_deviceSettings;
}

void SatelliteRadioControlDialog::resizeTable()
{
    on_add_clicked();
    ui->table->resizeColumnsToContents();
    ui->table->selectRow(0);
    on_remove_clicked();
    ui->table->selectRow(-1);
}

void SatelliteRadioControlDialog::on_add_clicked()
{
    QString name = ui->satelliteSelect->currentText();
    if (!name.isEmpty())
    {
        SatelliteTrackerSettings::SatelliteDeviceSettings *devSettings = new SatelliteTrackerSettings::SatelliteDeviceSettings();
        SatelliteDeviceSettingsGUI *devSettingsGUI = new SatelliteDeviceSettingsGUI(devSettings, ui->table);

        m_devSettingsGUIs.append(devSettingsGUI);
        QList<SatelliteTrackerSettings::SatelliteDeviceSettings *> *devSettingsList = m_deviceSettings.value(name);
        devSettingsList->append(devSettings);
    }
}

// Remove selected row
void SatelliteRadioControlDialog::on_remove_clicked()
{
    // Selection mode is single, so only a single row should be returned
    QModelIndexList indexList = ui->table->selectionModel()->selectedRows();
    if (!indexList.isEmpty())
    {
        int row = indexList.at(0).row();
        ui->table->removeRow(row);
        delete m_devSettingsGUIs.takeAt(row);

        QString name = ui->satelliteSelect->currentText();
        QList<SatelliteTrackerSettings::SatelliteDeviceSettings *> *devSettingsList = m_deviceSettings.value(name);
        delete devSettingsList->takeAt(row);
    }
}

void SatelliteRadioControlDialog::on_satelliteSelect_currentIndexChanged(int index)
{
    (void) index;

    // Save details from current GUI elements
    for (int i = 0; i < m_devSettingsGUIs.size(); i++)
        m_devSettingsGUIs[i]->accept();
    // Clear GUI
    ui->table->setRowCount(0);
    qDeleteAll(m_devSettingsGUIs);
    m_devSettingsGUIs.clear();

    // Create settings list for newly selected satellite, if one doesn't already exist
    QString name = ui->satelliteSelect->currentText();
    if (!m_deviceSettings.contains(name))
         m_deviceSettings.insert(name, new QList<SatelliteTrackerSettings::SatelliteDeviceSettings *>());

    // Add existing settings to GUI
    QList<SatelliteTrackerSettings::SatelliteDeviceSettings *> *devSettingsList = m_deviceSettings.value(name);
    for (int i = 0; i < devSettingsList->size(); i++)
    {
        SatelliteDeviceSettingsGUI *devSettingsGUI = new SatelliteDeviceSettingsGUI(devSettingsList->at(i), ui->table);
        m_devSettingsGUIs.append(devSettingsGUI);
    }

    // Display modes for the satellite, to help user select appropriate presets
    SatNogsSatellite *sat = m_satellites[name];
    QStringList info;
    for (int i = 0; i < sat->m_transmitters.size(); i++)
    {
        if (sat->m_transmitters[i]->m_status != "invalid")
        {
            QStringList mode;
            mode.append("  ");
            mode.append(sat->m_transmitters[i]->m_description);
            if (sat->m_transmitters[i]->m_downlinkHigh > 0)
                mode.append(QString("D: %1").arg(SatNogsTransmitter::getFrequencyRangeText(sat->m_transmitters[i]->m_downlinkLow, sat->m_transmitters[i]->m_downlinkHigh)));
            else if (sat->m_transmitters[i]->m_downlinkLow > 0)
                mode.append(QString("D: %1").arg(SatNogsTransmitter::getFrequencyText(sat->m_transmitters[i]->m_downlinkLow)));
            if (sat->m_transmitters[i]->m_uplinkHigh > 0)
                mode.append(QString("U: %1").arg(SatNogsTransmitter::getFrequencyRangeText(sat->m_transmitters[i]->m_uplinkLow, sat->m_transmitters[i]->m_uplinkHigh)));
            else if (sat->m_transmitters[i]->m_uplinkLow > 0)
                mode.append(QString("U: %1").arg(SatNogsTransmitter::getFrequencyText(sat->m_transmitters[i]->m_uplinkLow)));
            info.append(mode.join(" "));
        }
    }
    ui->satelliteModes->setText(info.join("\n"));
}
