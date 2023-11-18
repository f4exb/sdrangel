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

#ifndef INCLUDE_FEATURE_REMOTECONTROLDEVICEDIALOG_H
#define INCLUDE_FEATURE_REMOTECONTROLDEVICEDIALOG_H

#include "ui_remotecontroldevicedialog.h"
#include "remotecontrolsettings.h"
#include "util/iot/device.h"

class RemoteControlDeviceDialog : public QDialog {
    Q_OBJECT

public:
    explicit RemoteControlDeviceDialog(RemoteControlSettings *settings, RemoteControlDevice *device, QWidget* parent = 0);
    ~RemoteControlDeviceDialog();

private slots:
    void accept();
    void on_protocol_currentTextChanged(const QString &protocol);
    void on_device_currentIndexChanged(int index);
    void deviceList(const QList<DeviceDiscoverer::DeviceInfo> &devices);
    void deviceError(const QString &error);
    void on_controlAdd_clicked();
    void on_controlRemove_clicked();
    void on_controlEdit_clicked();
    void on_controlUp_clicked();
    void on_controlDown_clicked();
    void on_controls_cellDoubleClicked(int row, int column);
    void on_sensorAdd_clicked();
    void on_sensorRemove_clicked();
    void on_sensorEdit_clicked();
    void on_sensorUp_clicked();
    void on_sensorDown_clicked();
    void on_sensors_cellDoubleClicked(int row, int column);
    void controlSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void sensorSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

private:
    void enableWidgets();
    void resizeTables();
    void updateTable();
    int addControlRow(const QString &name, const QString &id, const QString &units);
    int addSensorRow(const QString &name, const QString &id, const QString &units);

    Ui::RemoteControlDeviceDialog* ui;
    RemoteControlSettings *m_settings;
    RemoteControlDevice *m_rcDevice;
    DeviceDiscoverer *m_discoverer;
    QList<DeviceDiscoverer::DeviceInfo> m_deviceInfo;
    bool m_setDeviceWhenAvailable;

    enum SensorCol {
        COL_ENABLE,
        COL_NAME,
        COL_UNITS,
        COL_ID,
        COL_LABEL_LEFT,
        COL_LABEL_RIGHT,
        COL_FORMAT,
        COL_PLOT
    };
};

#endif // INCLUDE_FEATURE_REMOTECONTROLDEVICEDIALOG_H
