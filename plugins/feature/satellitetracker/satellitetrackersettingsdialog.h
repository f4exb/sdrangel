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

#ifndef INCLUDE_SATELLITETRACKERSETTINGSDIALOG_H
#define INCLUDE_SATELLITETRACKERSETTINGSDIALOG_H

#include "ui_satellitetrackersettingsdialog.h"
#include "satellitetrackersettings.h"

class SatelliteTrackerSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SatelliteTrackerSettingsDialog(SatelliteTrackerSettings* settings, QWidget* parent = 0);
    ~SatelliteTrackerSettingsDialog();

   SatelliteTrackerSettings *m_settings;

private slots:
    void on_addTle_clicked();
    void on_removeTle_clicked();
    void accept();

private:
    Ui::SatelliteTrackerSettingsDialog* ui;
};

#endif // INCLUDE_SATELLITETRACKERSETTINGSDIALOG_H
