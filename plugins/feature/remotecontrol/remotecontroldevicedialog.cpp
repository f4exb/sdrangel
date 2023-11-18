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

#include "remotecontroldevicedialog.h"
#include "remotecontrolvisasensordialog.h"
#include "remotecontrolvisacontroldialog.h"

#include <QDebug>
#include <QUrl>
#include <QMessageBox>

RemoteControlDeviceDialog::RemoteControlDeviceDialog(RemoteControlSettings *settings, RemoteControlDevice *rcDevice, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::RemoteControlDeviceDialog),
    m_settings(settings),
    m_rcDevice(rcDevice),
    m_discoverer(nullptr),
    m_setDeviceWhenAvailable(false)
{
    ui->setupUi(this);
    connect(ui->controls->selectionModel(), &QItemSelectionModel::selectionChanged, this, &RemoteControlDeviceDialog::controlSelectionChanged);
    connect(ui->sensors->selectionModel(), &QItemSelectionModel::selectionChanged, this, &RemoteControlDeviceDialog::sensorSelectionChanged);
    enableWidgets();
    resizeTables();
    if (!m_rcDevice->m_info.m_id.isEmpty())
    {
        ui->controlsLayout->setCurrentIndex((int)m_rcDevice->m_verticalControls);
        ui->sensorsLayout->setCurrentIndex((int)m_rcDevice->m_verticalSensors);
        ui->yAxis->setCurrentIndex((int)m_rcDevice->m_commonYAxis);
        m_setDeviceWhenAvailable = true;
        // Set protocol last, as that triggers discovery
        ui->protocol->setCurrentText(m_rcDevice->m_protocol);
    }
}

RemoteControlDeviceDialog::~RemoteControlDeviceDialog()
{
    delete ui;
    delete m_discoverer;
}

void RemoteControlDeviceDialog::resizeTables()
{
    // Fill table with a row of dummy data that will size the columns nicely
    int row = ui->controls->rowCount();
    ui->controls->setRowCount(row + 1);
    ui->controls->setItem(row, COL_ENABLE, new QTableWidgetItem("C"));
    ui->controls->setItem(row, COL_UNITS, new QTableWidgetItem("Units"));
    ui->controls->setItem(row, COL_NAME, new QTableWidgetItem("A reasonably long control name"));
    ui->controls->setItem(row, COL_ID, new QTableWidgetItem("An identifier"));
    ui->controls->setItem(row, COL_LABEL_LEFT, new QTableWidgetItem("A reasonably long control name"));
    ui->controls->setItem(row, COL_LABEL_RIGHT, new QTableWidgetItem("Units"));
    ui->controls->resizeColumnsToContents();
    ui->controls->removeRow(row);
    row = ui->sensors->rowCount();
    ui->sensors->setRowCount(row + 1);
    ui->sensors->setItem(row, COL_ENABLE, new QTableWidgetItem("C"));
    ui->sensors->setItem(row, COL_NAME, new QTableWidgetItem("A reasonably long sensor name"));
    ui->sensors->setItem(row, COL_UNITS, new QTableWidgetItem("Units"));
    ui->sensors->setItem(row, COL_ID, new QTableWidgetItem("An identifier"));
    ui->sensors->setItem(row, COL_LABEL_LEFT, new QTableWidgetItem("A reasonably long sensor name"));
    ui->sensors->setItem(row, COL_LABEL_RIGHT, new QTableWidgetItem("Units"));
    ui->sensors->setItem(row, COL_FORMAT, new QTableWidgetItem("Format"));
    ui->sensors->setItem(row, COL_PLOT, new QTableWidgetItem("C"));
    ui->sensors->resizeColumnsToContents();
    ui->sensors->removeRow(row);
}

void RemoteControlDeviceDialog::accept()
{
    QDialog::accept();
    if ((ui->protocol->currentIndex() > 0) && (!ui->device->currentText().isEmpty()))
    {
        int deviceIndex = ui->device->currentIndex();
        m_rcDevice->m_protocol = ui->protocol->currentText();
        m_rcDevice->m_label = ui->label->text();
        m_rcDevice->m_verticalControls = ui->controlsLayout->currentIndex() == 1;
        m_rcDevice->m_verticalSensors = ui->sensorsLayout->currentIndex() == 1;
        m_rcDevice->m_commonYAxis = ui->yAxis->currentIndex() == 1;
        m_rcDevice->m_info = m_deviceInfo[deviceIndex];
        m_rcDevice->m_controls.clear();
        for (int row = 0; row < ui->controls->rowCount(); row++)
        {
            if (ui->controls->item(row, COL_ENABLE)->checkState() == Qt::Checked)
            {
                RemoteControlControl control;
                control.m_id = ui->controls->item(row, COL_ID)->text();
                control.m_labelLeft = ui->controls->item(row, COL_LABEL_LEFT)->text();
                control.m_labelRight = ui->controls->item(row, COL_LABEL_RIGHT)->text();
                m_rcDevice->m_controls.append(control);
            }
        }
        m_rcDevice->m_sensors.clear();
        for (int row = 0; row < ui->sensors->rowCount(); row++)
        {
            if (ui->sensors->item(row, COL_ENABLE)->checkState() == Qt::Checked)
            {
                RemoteControlSensor sensor;
                sensor.m_id = ui->sensors->item(row, COL_ID)->text();
                sensor.m_labelLeft = ui->sensors->item(row, COL_LABEL_LEFT)->text();
                sensor.m_labelRight = ui->sensors->item(row, COL_LABEL_RIGHT)->text();
                sensor.m_format = ui->sensors->item(row, COL_FORMAT)->text();
                sensor.m_plot = ui->sensors->item(row, COL_PLOT)->checkState() == Qt::Checked;
                m_rcDevice->m_sensors.append(sensor);
            }
        }
    }
}

void RemoteControlDeviceDialog::enableWidgets()
{
    bool allEnabled = false;
    bool visible = false;
    bool editControlsEnabled = false;
    bool editSensorsEnabled = false;

    QString protocol = ui->protocol->currentText();
    if (protocol != "Select a protocol...") {
        allEnabled = true;
    }
    if (protocol == "VISA")
    {
        visible = true;
        editControlsEnabled = ui->controls->selectedItems().size() > 0;
        editSensorsEnabled = ui->sensors->selectedItems().size() > 0;
    }

    ui->device->setEnabled(allEnabled);
    ui->deviceLabel->setEnabled(allEnabled);
    ui->label->setEnabled(allEnabled);
    ui->labelLabel->setEnabled(allEnabled);
    ui->model->setEnabled(allEnabled);
    ui->modelLabel->setEnabled(allEnabled);
    ui->controlsGroup->setEnabled(allEnabled);
    ui->sensorsGroup->setEnabled(allEnabled);

    ui->controlAdd->setVisible(visible);
    ui->controlRemove->setVisible(visible);
    ui->controlEdit->setVisible(visible);
    ui->controlRemove->setEnabled(editControlsEnabled);
    ui->controlEdit->setEnabled(editControlsEnabled);
    ui->sensorAdd->setVisible(visible);
    ui->sensorRemove->setVisible(visible);
    ui->sensorEdit->setVisible(visible);
    ui->sensorRemove->setEnabled(editSensorsEnabled);
    ui->sensorEdit->setEnabled(editSensorsEnabled);
}

void RemoteControlDeviceDialog::controlSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    (void)deselected;
    bool arrowsEnabled = (selected.indexes().size() > 0);
    bool editEnabled = arrowsEnabled && (ui->protocol->currentText() == "VISA");

    ui->controlRemove->setEnabled(editEnabled);
    ui->controlEdit->setEnabled(editEnabled);
    ui->controlUp->setEnabled(arrowsEnabled);
    ui->controlDown->setEnabled(arrowsEnabled);
}

void RemoteControlDeviceDialog::sensorSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    (void)deselected;
    bool arrowsEnabled = (selected.indexes().size() > 0);
    bool editEnabled = arrowsEnabled && (ui->protocol->currentText() == "VISA");

    ui->sensorRemove->setEnabled(editEnabled);
    ui->sensorEdit->setEnabled(editEnabled);
    ui->sensorUp->setEnabled(arrowsEnabled);
    ui->sensorDown->setEnabled(arrowsEnabled);
}

void RemoteControlDeviceDialog::on_protocol_currentTextChanged(const QString &protocol)
{
    QHash<QString, QVariant> settings;

    // Clear current values in all widgets
    ui->device->setCurrentIndex(-1);

    if (protocol != "Select a protocol...")
    {
        if (protocol == "TPLink")
        {
            settings.insert("username", m_settings->m_tpLinkUsername);
            settings.insert("password", m_settings->m_tpLinkPassword);
        }
        else if (protocol == "HomeAssistant")
        {
            settings.insert("apiKey", m_settings->m_homeAssistantToken);
            settings.insert("url", m_settings->m_homeAssistantHost);
        }
        else if (protocol == "VISA")
        {
            settings.insert("resourceFilter", m_settings->m_visaResourceFilter);
        }

        delete m_discoverer;
        m_discoverer = DeviceDiscoverer::getDiscoverer(settings, protocol);
        if (m_discoverer)
        {
            connect(m_discoverer, &DeviceDiscoverer::deviceList, this, &RemoteControlDeviceDialog::deviceList);
            connect(m_discoverer, &DeviceDiscoverer::error, this, &RemoteControlDeviceDialog::deviceError);
            m_discoverer->getDevices();
        }
        else
        {
            QMessageBox::critical(this, "Remote Control Error", QString("Failed to discover %1 devices").arg(protocol));
        }
    }
    enableWidgets();
}

int RemoteControlDeviceDialog::addControlRow(const QString &name, const QString &id, const QString &units)
{
    QTableWidgetItem *item;
    int row = ui->controls->rowCount();
    ui->controls->setRowCount(row + 1);

    item = new QTableWidgetItem();
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    item->setCheckState(Qt::Checked);
    ui->controls->setItem(row, COL_ENABLE, item);

    item = new QTableWidgetItem(name);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->controls->setItem(row, COL_NAME, item);

    item = new QTableWidgetItem(units);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->controls->setItem(row, COL_UNITS, item);

    item = new QTableWidgetItem(id);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->controls->setItem(row, COL_ID, item);

    item = new QTableWidgetItem(name);
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->controls->setItem(row, COL_LABEL_LEFT, item);

    item = new QTableWidgetItem(units);
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->controls->setItem(row, COL_LABEL_RIGHT, item);

    return row;
}

int RemoteControlDeviceDialog::addSensorRow(const QString &name, const QString &id, const QString &units)
{
    QTableWidgetItem *item;
    int row = ui->sensors->rowCount();
    ui->sensors->setRowCount(row + 1);

    item = new QTableWidgetItem();
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    item->setCheckState(Qt::Checked);
    ui->sensors->setItem(row, COL_ENABLE, item);

    item = new QTableWidgetItem(name);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->sensors->setItem(row, COL_NAME, item);

    item = new QTableWidgetItem(units);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->sensors->setItem(row, COL_UNITS, item);

    item = new QTableWidgetItem(id);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->sensors->setItem(row, COL_ID, item);

    item = new QTableWidgetItem(name);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
    ui->sensors->setItem(row, COL_LABEL_LEFT, item);

    item = new QTableWidgetItem(units);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
    ui->sensors->setItem(row, COL_LABEL_RIGHT, item);

    item = new QTableWidgetItem();
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
    ui->sensors->setItem(row, COL_FORMAT, item);

    item = new QTableWidgetItem();
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    item->setCheckState(Qt::Unchecked);
    ui->sensors->setItem(row, COL_PLOT, item);

    return row;
}

void RemoteControlDeviceDialog::on_device_currentIndexChanged(int index)
{
    ui->model->setText("");
    ui->label->setText("");
    ui->controls->setRowCount(0);
    ui->sensors->setRowCount(0);
    if ((index < m_deviceInfo.size()) && (index >= 0))
    {
        DeviceDiscoverer::DeviceInfo *deviceInfo = &m_deviceInfo[index];

        ui->model->setText(deviceInfo->m_model);
        if (m_rcDevice->m_info.m_id == deviceInfo->m_id) {
            ui->label->setText(m_rcDevice->m_label);
        } else {
            ui->label->setText(deviceInfo->m_name);
        }

        for (auto c : deviceInfo->m_controls) {
            addControlRow(c->m_name, c->m_id, c->m_units);
        }
        for (auto s : deviceInfo->m_sensors) {
            addSensorRow(s->m_name, s->m_id, s->m_units);
        }
    }
}

void RemoteControlDeviceDialog::updateTable()
{
    for (int row = 0; row < ui->controls->rowCount(); row++)
    {
        QString controlId = ui->controls->item(row, COL_ID)->text();
        RemoteControlControl *control = m_rcDevice->getControl(controlId);
        if (control != nullptr)
        {
            ui->controls->item(row, COL_ENABLE)->setCheckState(Qt::Checked);
            ui->controls->item(row, COL_LABEL_LEFT)->setText(control->m_labelLeft);
            ui->controls->item(row, COL_LABEL_RIGHT)->setText(control->m_labelRight);
        }
        else
        {
            ui->controls->item(row, COL_ENABLE)->setCheckState(Qt::Unchecked);
        }
    }
    for (int row = 0; row < ui->sensors->rowCount(); row++)
    {
        QString sensorId = ui->sensors->item(row, COL_ID)->text();
        RemoteControlSensor *sensor = m_rcDevice->getSensor(sensorId);
        if (sensor != nullptr)
        {
            ui->sensors->item(row, COL_ENABLE)->setCheckState(Qt::Checked);
            ui->sensors->item(row, COL_LABEL_LEFT)->setText(sensor->m_labelLeft);
            ui->sensors->item(row, COL_LABEL_RIGHT)->setText(sensor->m_labelRight);
            ui->sensors->item(row, COL_FORMAT)->setText(sensor->m_format);
            ui->sensors->item(row, COL_PLOT)->setCheckState(sensor->m_plot ? Qt::Checked : Qt::Unchecked);
        }
        else
        {
            ui->sensors->item(row, COL_ENABLE)->setCheckState(Qt::Unchecked);
        }
    }
}

void RemoteControlDeviceDialog::deviceList(const QList<DeviceDiscoverer::DeviceInfo> &devices)
{
    ui->device->clear();
    m_deviceInfo = devices;  // Take a deep copy
    int i = 0;
    for (auto const &device : m_deviceInfo)
    {
        // Update default device info, with info for device we are editing
        if (m_setDeviceWhenAvailable && (device.m_id == m_rcDevice->m_info.m_id)) {
            m_deviceInfo[i] = m_rcDevice->m_info;
        }
        // Add device to list
        ui->device->addItem(device.m_name);
        i++;
    }
    if (m_setDeviceWhenAvailable)
    {
        ui->device->setCurrentText(m_rcDevice->m_info.m_name);
        m_setDeviceWhenAvailable = false;
        updateTable();
    }
}

void RemoteControlDeviceDialog::deviceError(const QString &error)
{
    QMessageBox::critical(this, "Remote Control Error", error);
}

void RemoteControlDeviceDialog::on_controlAdd_clicked()
{
    VISADevice::VISAControl *control = new VISADevice::VISAControl();
    RemoteControlVISAControlDialog dialog(m_settings, m_rcDevice, control, true);
    if (dialog.exec() == QDialog::Accepted)
    {
        DeviceDiscoverer::DeviceInfo *info = &m_deviceInfo[ui->device->currentIndex()];
        info->m_controls.append(reinterpret_cast<DeviceDiscoverer::ControlInfo *>(control));

        addControlRow(control->m_name, control->m_id, control->m_units);
    }
    else
    {
        delete control;
    }
}

void RemoteControlDeviceDialog::on_controlEdit_clicked()
{
    QList<QTableWidgetItem *> items = ui->controls->selectedItems();
    if (items.size() > 0)
    {
        int row = items[0]->row();
        QString id = ui->controls->item(row, COL_ID)->text();
        DeviceDiscoverer::DeviceInfo *info = &m_deviceInfo[ui->device->currentIndex()];
        VISADevice::VISAControl *control = reinterpret_cast<VISADevice::VISAControl *>(info->getControl(id));

        RemoteControlVISAControlDialog dialog(m_settings, m_rcDevice, control, false);
        if (dialog.exec() == QDialog::Accepted)
        {
            ui->controls->item(row, COL_NAME)->setText(control->m_name);
            ui->controls->item(row, COL_UNITS)->setText(control->m_units);
            ui->controls->item(row, COL_ID)->setText(control->m_id);
        }
    }
}

void RemoteControlDeviceDialog::on_controls_cellDoubleClicked(int row, int column)
{
    (void)row;
    if ((ui->protocol->currentText() == "VISA") && (column <= COL_ID)) {
        on_controlEdit_clicked();
    }
}

void RemoteControlDeviceDialog::on_controlRemove_clicked()
{
    QList<QTableWidgetItem *> items = ui->controls->selectedItems();
    if (items.size() > 0)
    {
        int row = items[0]->row();
        QString id = ui->controls->item(row, COL_ID)->text();
        ui->controls->removeRow(row);
        DeviceDiscoverer::DeviceInfo *info = &m_deviceInfo[ui->device->currentIndex()];
        info->deleteControl(id);
    }
}

void RemoteControlDeviceDialog::on_controlUp_clicked()
{
    QList<QTableWidgetItem *> items = ui->controls->selectedItems();
    for (int i = 0; i < items.size(); i++)
    {
        int row = items[i]->row();
        int col = items[i]->column();
        if (row > 0)
        {
            // Swap rows in table
            QTableWidgetItem *item1 = ui->controls->takeItem(row, col);
            QTableWidgetItem *item2 = ui->controls->takeItem(row - 1, col);
            ui->controls->setItem(row - 1, col, item1);
            ui->controls->setItem(row, col, item2);
        }
        if (i == items.size() - 1)
        {
            ui->controls->setCurrentItem(items[i]);
            if (row > 0)
            {
                // Swap device info
                DeviceDiscoverer::DeviceInfo *info = &m_deviceInfo[ui->device->currentIndex()];
#if QT_VERSION < QT_VERSION_CHECK(5, 13, 0)
                info->m_controls.swap(row, row - 1);
#else
                info->m_controls.swapItemsAt(row, row - 1);
#endif
            }
        }
    }
}

void RemoteControlDeviceDialog::on_controlDown_clicked()
{
    QList<QTableWidgetItem *> items = ui->controls->selectedItems();
    for (int i = 0; i < items.size(); i++)
    {
        int row = items[i]->row();
        int col = items[i]->column();
        if (row < ui->controls->rowCount() - 1)
        {
            // Swap rows in table
            QTableWidgetItem *item1 = ui->controls->takeItem(row, col);
            QTableWidgetItem *item2 = ui->controls->takeItem(row + 1, col);
            ui->controls->setItem(row + 1, col, item1);
            ui->controls->setItem(row, col, item2);
        }
        if (i == items.size() - 1)
        {
            ui->controls->setCurrentItem(items[i]);
            if (row < ui->controls->rowCount() - 1)
            {
                // Swap device info
                DeviceDiscoverer::DeviceInfo *info = &m_deviceInfo[ui->device->currentIndex()];
#if QT_VERSION < QT_VERSION_CHECK(5, 13, 0)
                info->m_controls.swap(row, row + 1);
#else
                info->m_controls.swapItemsAt(row, row + 1);
#endif
            }
        }
    }
}

void RemoteControlDeviceDialog::on_sensorAdd_clicked()
{
    VISADevice::VISASensor *sensor = new VISADevice::VISASensor();
    RemoteControlVISASensorDialog dialog(m_settings, m_rcDevice, sensor, true);
    if (dialog.exec() == QDialog::Accepted)
    {
        DeviceDiscoverer::DeviceInfo *info = &m_deviceInfo[ui->device->currentIndex()];
        info->m_sensors.append(reinterpret_cast<DeviceDiscoverer::SensorInfo *>(sensor));

        addSensorRow(sensor->m_name, sensor->m_id, sensor->m_units);
    }
    else
    {
        delete sensor;
    }
}

void RemoteControlDeviceDialog::on_sensorRemove_clicked()
{
    QList<QTableWidgetItem *> items = ui->sensors->selectedItems();
    if (items.size() > 0)
    {
        int row = items[0]->row();
        QString id = ui->sensors->item(row, COL_ID)->text();
        ui->sensors->removeRow(row);
        DeviceDiscoverer::DeviceInfo *info = &m_deviceInfo[ui->device->currentIndex()];
        info->deleteSensor(id);
    }
}

void RemoteControlDeviceDialog::on_sensorEdit_clicked()
{
    QList<QTableWidgetItem *> items = ui->sensors->selectedItems();
    if (items.size() > 0)
    {
        int row = items[0]->row();
        QString id = ui->sensors->item(row, COL_ID)->text();
        DeviceDiscoverer::DeviceInfo *info = &m_deviceInfo[ui->device->currentIndex()];
        VISADevice::VISASensor *sensor = reinterpret_cast<VISADevice::VISASensor *>(info->getSensor(id));

        RemoteControlVISASensorDialog dialog(m_settings, m_rcDevice, sensor, false);
        if (dialog.exec() == QDialog::Accepted)
        {
            ui->sensors->item(row, COL_NAME)->setText(sensor->m_name);
            ui->sensors->item(row, COL_ID)->setText(sensor->m_id);
            ui->sensors->item(row, COL_UNITS)->setText(sensor->m_units);
        }
    }
}

void RemoteControlDeviceDialog::on_sensors_cellDoubleClicked(int row, int column)
{
    (void)row;
    if ((ui->protocol->currentText() == "VISA") && (column <= COL_ID)) {
        on_sensorEdit_clicked();
    }
}

void RemoteControlDeviceDialog::on_sensorUp_clicked()
{
    QList<QTableWidgetItem *> items = ui->sensors->selectedItems();
    for (int i = 0; i < items.size(); i++)
    {
        int row = items[i]->row();
        int col = items[i]->column();
        if (row > 0)
        {
            // Swap rows in table
            QTableWidgetItem *item1 = ui->sensors->takeItem(row, col);
            QTableWidgetItem *item2 = ui->sensors->takeItem(row - 1, col);
            ui->sensors->setItem(row - 1, col, item1);
            ui->sensors->setItem(row, col, item2);
        }
        if (i == items.size() - 1)
        {
            ui->sensors->setCurrentItem(items[i]);
            if (row > 0)
            {
                // Swap device info
                DeviceDiscoverer::DeviceInfo *info = &m_deviceInfo[ui->device->currentIndex()];
#if QT_VERSION < QT_VERSION_CHECK(5, 13, 0)
                info->m_sensors.swap(row, row - 1);
#else
                info->m_sensors.swapItemsAt(row, row - 1);
#endif
            }
        }
    }
}

void RemoteControlDeviceDialog::on_sensorDown_clicked()
{
    QList<QTableWidgetItem *> items = ui->sensors->selectedItems();
    for (int i = 0; i < items.size(); i++)
    {
        int row = items[i]->row();
        int col = items[i]->column();
        if (row < ui->sensors->rowCount() - 1)
        {
            // Swap rows in table
            QTableWidgetItem *item1 = ui->sensors->takeItem(row, col);
            QTableWidgetItem *item2 = ui->sensors->takeItem(row + 1, col);
            ui->sensors->setItem(row + 1, col, item1);
            ui->sensors->setItem(row, col, item2);
        }
        if (i == items.size() - 1)
        {
            ui->sensors->setCurrentItem(items[i]);
            if (row < ui->sensors->rowCount() - 1)
            {
                // Swap device info
                DeviceDiscoverer::DeviceInfo *info = &m_deviceInfo[ui->device->currentIndex()];
#if QT_VERSION < QT_VERSION_CHECK(5, 13, 0)
                info->m_sensors.swap(row, row + 1);
#else
                info->m_sensors.swapItemsAt(row, row + 1);
#endif
            }
        }
    }
}
