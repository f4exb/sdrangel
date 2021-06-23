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

#ifndef INCLUDE_FEATURE_MAPRADIOTIMEDIALOG_H
#define INCLUDE_FEATURE_MAPRADIOTIMEDIALOG_H

#include "ui_mapradiotimedialog.h"

class MapGUI;

class MapRadioTimeDialog : public QDialog {
    Q_OBJECT

public:
    explicit MapRadioTimeDialog(MapGUI *gui, QWidget* parent = 0);
    ~MapRadioTimeDialog();
    void updateTable();

private slots:
    void accept();
    void on_transmitters_cellDoubleClicked(int row, int column);

private:
    MapGUI *m_gui;
    Ui::MapRadioTimeDialog* ui;

    enum TransmitterCol {
        TRANSMITTER_COL_CALLSIGN,
        TRANSMITTER_COL_FREQUENCY,
        TRANSMITTER_COL_LOCATION,
        TRANSMITTER_COL_POWER,
        TRANSMITTER_COL_AZIMUTH,
        TRANSMITTER_COL_ELEVATION,
        TRANSMITTER_COL_DISTANCE
    };
};

#endif // INCLUDE_FEATURE_MAPRADIOTIMEDIALOG_H
