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

#include <QProgressDialog>

#include "util/httpdownloadmanager.h"
#include "beacon.h"

class MapGUI;

class MapBeaconDialog : public QDialog {
    Q_OBJECT

public:
    explicit MapBeaconDialog(MapGUI *gui, QWidget* parent = 0);
    ~MapBeaconDialog();
    void updateTable();

private:
    qint64 fileAgeInDays(QString filename);
    bool confirmDownload(QString filename);
    void downloadFinished(const QString& filename, bool success);

private slots:
    void accept();
    void on_downloadIARU_clicked();
    void updateDownloadProgress(qint64 bytesRead, qint64 totalBytes);
    void on_beacons_cellDoubleClicked(int row, int column);
    void on_filter_currentIndexChanged(int index);

private:
    MapGUI *m_gui;
    Ui::MapBeaconDialog* ui;
    HttpDownloadManager m_dlm;
    QProgressDialog *m_progressDialog;

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
