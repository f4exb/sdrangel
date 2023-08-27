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

#ifndef INCLUDE_FEATURE_MAPIBPBEACONDIALOG_H
#define INCLUDE_FEATURE_MAPIBPBEACONDIALOG_H

#include "ui_mapibpbeacondialog.h"

#include <QTimer>
#include <QTime>

#include "ibpbeacon.h"

class MapGUI;

class MapIBPBeaconDialog : public QDialog {
    Q_OBJECT

public:
    explicit MapIBPBeaconDialog(MapGUI *gui, QWidget* parent = 0);
    ~MapIBPBeaconDialog();
    void updateTable(QTime time);

private slots:
    void accept();
    void on_beacons_cellDoubleClicked(int row, int column);
    void updateTime();

private:
    MapGUI *m_gui;
    QTimer m_timer;
    Ui::MapIBPBeaconDialog* ui;

    void resizeTable(void);

    enum BeaconCol {
        IBP_BEACON_COL_FREQUENCY,
        IBP_BEACON_COL_CALLSIGN,
        IBP_BEACON_COL_LOCATION,
        IBP_BEACON_COL_DX_ENTITY,
        IBP_BEACON_COL_AZIMUTH,
        IBP_BEACON_COL_DISTANCE
    };
};

#endif // INCLUDE_FEATURE_MAPIBPBEACONDIALOG_H
