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

#ifndef INCLUDE_FEATURE_SATELLITERADIOCONTROLDIALOG_H
#define INCLUDE_FEATURE_SATELLITERADIOCONTROLDIALOG_H

#include <QHash>

#include "ui_satelliteradiocontroldialog.h"
#include "satellitetrackersettings.h"
#include "satellitedevicesettingsgui.h"
#include "satnogs.h"

class SatelliteRadioControlDialog : public QDialog {
    Q_OBJECT

public:
    explicit SatelliteRadioControlDialog(SatelliteTrackerSettings* settings, const QHash<QString, SatNogsSatellite *>& satellites, QWidget* parent = 0);
    ~SatelliteRadioControlDialog();

   SatelliteTrackerSettings *m_settings;

private:

private slots:
    void accept();
    void on_add_clicked();
    void on_tabCloseRequested(int index);
    void on_satelliteSelect_currentIndexChanged(int index);

private:
    const QHash<QString, SatNogsSatellite *>& m_satellites;
    QHash<QString, QList<SatelliteTrackerSettings::SatelliteDeviceSettings *> *> m_deviceSettings;      // Device settings per sateillite
    QList<SatelliteDeviceSettingsGUI *> m_devSettingsGUIs;                                              // For selected satellite
    Ui::SatelliteRadioControlDialog* ui;
};

#endif // INCLUDE_FEATURE_SATELLITERADIOCONTROLDIALOG_H
