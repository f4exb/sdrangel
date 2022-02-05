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

#ifndef INCLUDE_FEATURE_MAPBEACONDIALOG_H
#define INCLUDE_FEATURE_MAPBEACONDIALOG_H

#include "ui_mapbeacondialog.h"

#include "gui/httpdownloadmanagergui.h"
#include "beacon.h"

class MapGUI;

class MapBeaconDialog : public QDialog {
    Q_OBJECT

public:
    explicit MapBeaconDialog(MapGUI *gui, QWidget* parent = 0);
    ~MapBeaconDialog();
    void updateTable();

private:
    void downloadFinished(const QString& filename, bool success, const QString &url, const QString &errorMessage);

private slots:
    void accept();
    void on_downloadIARU_clicked();
    void on_beacons_cellDoubleClicked(int row, int column);
    void on_filter_currentIndexChanged(int index);

private:
    MapGUI *m_gui;
    Ui::MapBeaconDialog* ui;
    HttpDownloadManagerGUI m_dlm;

    enum BeaconCol {
        BEACON_COL_CALLSIGN,
        BEACON_COL_FREQUENCY,
        BEACON_COL_LOCATION,
        BEACON_COL_POWER,
        BEACON_COL_POLARIZATION,
        BEACON_COL_PATTERN,
        BEACON_COL_KEY,
        BEACON_COL_MGM,
        BEACON_COL_AZIMUTH,
        BEACON_COL_ELEVATION,
        BEACON_COL_DISTANCE
    };
};

#endif // INCLUDE_FEATURE_MAPBEACONDIALOG_H
