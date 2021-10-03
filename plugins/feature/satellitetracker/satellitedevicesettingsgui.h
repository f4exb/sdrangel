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
#include <QListView>
#include <QLineEdit>
#include <QCheckBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QStandardItemModel>
#include <QList>

#include "settings/preset.h"
#include "satellitetrackersettings.h"

class SatelliteRadioControlDialog;

class SatelliteDeviceSettingsGUI : public QWidget
{
    Q_OBJECT

public:

    explicit SatelliteDeviceSettingsGUI(SatelliteTrackerSettings::SatelliteDeviceSettings *devSettings,
                               QTabWidget *tab, QWidget *parent = nullptr);
    void accept();

protected:

    void addDeviceSets();
    void addPresets(const QString& deviceSet);
    void addChannels();
    const Preset *getSelectedPreset();

private slots:

    void on_m_deviceSetWidget_currentTextChanged(const QString &text);
    void on_m_presetWidget_currentIndexChanged(int index);

protected:

    friend SatelliteRadioControlDialog;
    QTabWidget *m_tab;
    QComboBox *m_deviceSetWidget;
    QComboBox *m_presetWidget;
    QListView *m_dopplerWidget;
    QCheckBox *m_startOnAOSWidget;
    QCheckBox *m_stopOnLOSWidget;
    QCheckBox *m_startStopFileSinkWidget;
    QLineEdit *m_frequencyWidget;
    QLineEdit *m_aosCommandWidget;
    QLineEdit *m_losCommandWidget;
    QChar m_currentPresetType;

    QStandardItemModel m_dopplerModel;
    QList<QStandardItem *> m_dopplerItems;

    SatelliteTrackerSettings::SatelliteDeviceSettings *m_devSettings;

};

#endif // INCLUDE_FEATURE_SATELLITEDEVICESETTINGSGUI_H
