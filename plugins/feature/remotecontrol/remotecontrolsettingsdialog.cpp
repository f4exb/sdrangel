///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "remotecontrolsettingsdialog.h"

#include "channel/channelwebapiutils.h"

RemoteControlSettingsDialog::RemoteControlSettingsDialog(RemoteControlSettings *settings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::RemoteControlSettingsDialog),
    m_settings(settings)
{
    ui->setupUi(this);
    resizeTable();
    ui->tpLinkUsername->setText(m_settings->m_tpLinkUsername);
    ui->tpLinkPassword->setText(m_settings->m_tpLinkPassword);
    ui->homeAssistantToken->setText(m_settings->m_homeAssistantToken);
    ui->homeAssistantHost->setText(m_settings->m_homeAssistantHost);
    ui->visaResourceFilter->setText(m_settings->m_visaResourceFilter);
    ui->visaLogIO->setChecked(m_settings->m_visaLogIO);
    ui->updatePeriod->setValue(m_settings->m_updatePeriod);
    ui->chartVerticalPolicy->setCurrentIndex((int)m_settings->m_chartHeightFixed);
    ui->chartHeight->setValue(m_settings->m_chartHeightPixels);

    connect(ui->devices->selectionModel(), &QItemSelectionModel::selectionChanged, this, &RemoteControlSettingsDialog::devicesSelectionChanged);

    updateTable();
    for (auto device : m_settings->m_devices) {
        m_devices.append(new RemoteControlDevice(*device));
    }
}

RemoteControlSettingsDialog::~RemoteControlSettingsDialog()
{
    qDeleteAll(m_devices);
    m_devices.clear();
    delete ui;
}

void RemoteControlSettingsDialog::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    int row = ui->devices->rowCount();
    ui->devices->setRowCount(row + 1);
    ui->devices->setItem(row, COL_LABEL, new QTableWidgetItem("A short label"));
    ui->devices->setItem(row, COL_NAME, new QTableWidgetItem("A reasonably long name"));
    ui->devices->setItem(row, COL_MODEL, new QTableWidgetItem("A long model name to display"));
    ui->devices->setItem(row, COL_SERVICE, new QTableWidgetItem("Home Assistant"));
    ui->devices->resizeColumnsToContents();
    ui->devices->removeRow(row);
}

void RemoteControlSettingsDialog::addToTable(int row, RemoteControlDevice *device)
{
    QTableWidgetItem *item;

    item = new QTableWidgetItem(device->m_label);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->devices->setItem(row, COL_LABEL, item);

    item = new QTableWidgetItem(device->m_info.m_name);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->devices->setItem(row, COL_NAME, item);

    item = new QTableWidgetItem(device->m_info.m_model);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->devices->setItem(row, COL_MODEL, item);

    item = new QTableWidgetItem(device->m_protocol);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->devices->setItem(row, COL_SERVICE, item);
}

void RemoteControlSettingsDialog::updateTable()
{
    int row = 0;
    ui->devices->setSortingEnabled(false);
    ui->devices->setRowCount(m_settings->m_devices.size());
    for (auto device : m_settings->m_devices)
    {
        addToTable(row, device);
        row++;
    }
    ui->devices->setSortingEnabled(true);
}

void RemoteControlSettingsDialog::accept()
{
    QDialog::accept();
    m_settings->m_tpLinkUsername = ui->tpLinkUsername->text();
    m_settings->m_tpLinkPassword = ui->tpLinkPassword->text();
    m_settings->m_homeAssistantToken = ui->homeAssistantToken->text();
    m_settings->m_homeAssistantHost = ui->homeAssistantHost->text();
    m_settings->m_visaResourceFilter = ui->visaResourceFilter->text();
    m_settings->m_visaLogIO = ui->visaLogIO->isChecked();
    m_settings->m_updatePeriod = ui->updatePeriod->value();
    m_settings->m_chartHeightFixed = ui->chartVerticalPolicy->currentIndex() == 1;
    m_settings->m_chartHeightPixels = ui->chartHeight->value();

    qDeleteAll(m_settings->m_devices);
    m_settings->m_devices.clear();
    m_settings->m_devices = m_devices;
    m_devices.clear();  // So destructor doesn't delete them
}

void RemoteControlSettingsDialog::on_devices_cellDoubleClicked(int row, int column)
{
    (void)row;
    (void)column;
    on_edit_clicked();
}

void RemoteControlSettingsDialog::devicesSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    (void)deselected;
    bool enabled = selected.indexes().size() > 0;
    ui->remove->setEnabled(enabled);
    ui->edit->setEnabled(enabled);
    ui->deviceUp->setEnabled(enabled);
    ui->deviceDown->setEnabled(enabled);
}

void RemoteControlSettingsDialog::on_add_clicked()
{
    RemoteControlDevice *device = new RemoteControlDevice();
    RemoteControlDeviceDialog dialog(m_settings, device);
    if (dialog.exec() == QDialog::Accepted)
    {
        int row = ui->devices->rowCount();
        ui->devices->setRowCount(row + 1);
        addToTable(row, device);
        m_devices.append(device);
    }
    else
    {
        delete device;
    }
}

void RemoteControlSettingsDialog::on_remove_clicked()
{
    QList<QTableWidgetItem *> items = ui->devices->selectedItems();
    if (items.size() > 0)
    {
        int row = items[0]->row();
        if (row >= 0)
        {
            ui->devices->removeRow(row);
            delete m_devices.takeAt(row);
        }
    }
}

void RemoteControlSettingsDialog::on_edit_clicked()
{
    QList<QTableWidgetItem *> items = ui->devices->selectedItems();
    if (items.size() > 0)
    {
        int row = items[0]->row();
        if (row >= 0)
        {
            RemoteControlDevice *device = m_devices[row];
            RemoteControlDeviceDialog dialog(m_settings, device);
            if (dialog.exec() == QDialog::Accepted)
            {
                ui->devices->item(row, COL_LABEL)->setText(device->m_label);
                ui->devices->item(row, COL_NAME)->setText(device->m_info.m_name);
                ui->devices->item(row, COL_MODEL)->setText(device->m_info.m_model);
                ui->devices->item(row, COL_SERVICE)->setText(device->m_protocol);
            }
        }
    }
}

void RemoteControlSettingsDialog::on_deviceUp_clicked()
{
    QList<QTableWidgetItem *> items = ui->devices->selectedItems();
    for (int i = 0; i < items.size(); i++)
    {
        int row = items[i]->row();
        int col = items[i]->column();
        if (row > 0)
        {
            QTableWidgetItem *item1 = ui->devices->takeItem(row, col);
            QTableWidgetItem *item2 = ui->devices->takeItem(row - 1, col);
            ui->devices->setItem(row - 1, col, item1);
            ui->devices->setItem(row, col, item2);
            if (i == items.size() - 1)
            {
                ui->devices->setCurrentItem(items[i]);
#if QT_VERSION < QT_VERSION_CHECK(5, 13, 0)
                m_devices.swap(row, row - 1);
#else
                m_devices.swapItemsAt(row, row - 1);
#endif
            }
        }
    }
}

void RemoteControlSettingsDialog::on_deviceDown_clicked()
{
    QList<QTableWidgetItem *> items = ui->devices->selectedItems();
    for (int i = 0; i < items.size(); i++)
    {
        int row = items[i]->row();
        int col = items[i]->column();
        if (row < ui->devices->rowCount() - 1)
        {
            QTableWidgetItem *item1 = ui->devices->takeItem(row, col);
            QTableWidgetItem *item2 = ui->devices->takeItem(row + 1, col);
            ui->devices->setItem(row + 1, col, item1);
            ui->devices->setItem(row, col, item2);
            if (i == items.size() - 1)
            {
                ui->devices->setCurrentItem(items[i]);
#if QT_VERSION < QT_VERSION_CHECK(5, 13, 0)
                m_devices.swap(row, row + 1);
#else
                m_devices.swapItemsAt(row, row + 1);
#endif
            }
        }
    }
}

void RemoteControlSettingsDialog::on_chartVerticalPolicy_currentIndexChanged(int index)
{
    bool enabled = index == 1;
    ui->chartHeightLabel->setEnabled(enabled);
    ui->chartHeight->setEnabled(enabled);
}
