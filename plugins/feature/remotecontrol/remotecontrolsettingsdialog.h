///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2021-2022 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_FEATURE_REMOTECONTROLSETTINGSDIALOG_H
#define INCLUDE_FEATURE_REMOTECONTROLSETTINGSDIALOG_H

#include "ui_remotecontrolsettingsdialog.h"
#include "remotecontrolsettings.h"
#include "remotecontroldevicedialog.h"

class RemoteControlSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit RemoteControlSettingsDialog(RemoteControlSettings *settings, QWidget* parent = 0);
    ~RemoteControlSettingsDialog();

private slots:
    void accept();
    void on_devices_cellDoubleClicked(int row, int column);
    void devicesSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void on_add_clicked();
    void on_remove_clicked();
    void on_edit_clicked();
    void on_deviceUp_clicked();
    void on_deviceDown_clicked();
    void on_chartVerticalPolicy_currentIndexChanged(int index);

private:
    void resizeTable();
    void addToTable(int row, RemoteControlDevice *device);
    void updateTable();

    Ui::RemoteControlSettingsDialog* ui;
    RemoteControlSettings *m_settings;
    QList<RemoteControlDevice *> m_devices;

    enum DeviceCol {
        COL_LABEL,
        COL_NAME,
        COL_MODEL,
        COL_SERVICE,
    };
};

#endif // INCLUDE_FEATURE_REMOTECONTROLSETTINGSDIALOG_H
