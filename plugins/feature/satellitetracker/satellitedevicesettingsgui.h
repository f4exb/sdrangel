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

#ifndef INCLUDE_FEATURE_SATELLITEDEVICESETTINGSGUI_H
#define INCLUDE_FEATURE_SATELLITEDEVICESETTINGSGUI_H

#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QStandardItemModel>
#include <QList>

#include "settings/preset.h"
#include "satellitetrackersettings.h"

class SatelliteRadioControlDialog;

class SatelliteDeviceSettingsGUI : public QObject
{
    Q_OBJECT

public:

    SatelliteDeviceSettingsGUI(SatelliteTrackerSettings::SatelliteDeviceSettings *devSettings,
                               QTableWidget *table);
    void accept();

protected:

    void layout(QWidget *parent, QWidget *child);
    void addDeviceSets();
    void addPresets(const QString& deviceSet);
    void addChannels();
    const Preset *getSelectedPreset();

private slots:

    void on_m_deviceSetWidget_currentTextChanged(const QString &text);
    void on_m_presetWidget_currentIndexChanged(int index);

protected:

    friend SatelliteRadioControlDialog;
    QWidget *m_deviceSetItem;
    QComboBox *m_deviceSetWidget;
    QWidget *m_presetItem;
    QComboBox *m_presetWidget;
    QWidget *m_dopplerItem;
    QComboBox *m_dopplerWidget;
    QWidget *m_startOnAOSItem;
    QCheckBox *m_startOnAOSWidget;
    QWidget *m_stopOnLOSItem;
    QCheckBox *m_stopOnLOSWidget;
    QWidget *m_startStopFileSinkItem;
    QCheckBox *m_startStopFileSinkWidget;
    QTableWidgetItem *m_frequencyItem;
    QTableWidgetItem *m_aosCommandItem;
    QTableWidgetItem *m_losCommandItem;
    QChar m_currentPresets;

    QStandardItemModel m_dopplerModel;
    QList<QStandardItem *> m_dopplerItems;

    SatelliteTrackerSettings::SatelliteDeviceSettings *m_devSettings;

    enum SatDeviceCol {
        SAT_DEVICE_COL_DEVICESET,
        SAT_DEVICE_COL_PRESET,
        SAT_DEVICE_COL_DOPPLER,
        SAT_DEVICE_COL_START,
        SAT_DEVICE_COL_STOP,
        SAT_DEVICE_COL_START_FILE_SINK,
        SAT_DEVICE_COL_FREQUENCY,
        SAT_DEVICE_COL_AOS_COMMAND,
        SAT_DEVICE_COL_LOS_COMMAND
    };
};

#endif // INCLUDE_FEATURE_SATELLITEDEVICESETTINGSGUI_H
