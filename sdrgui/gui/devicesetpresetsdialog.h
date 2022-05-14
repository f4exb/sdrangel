///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRGUI_GUI_DEVICESETPRESETDIALOG_H_
#define SDRGUI_GUI_DEVICESETPRESETDIALOG_H_

#include <QDialog>
#include <QList>

#include "export.h"

class Preset;
class DeviceUISet;
class PluginAPI;
class Workspace;

namespace Ui {
    class DeviceSetPresetsDialog;
}

class SDRGUI_API DeviceSetPresetsDialog : public QDialog {
    Q_OBJECT
public:
    explicit DeviceSetPresetsDialog(QWidget* parent = nullptr);
    ~DeviceSetPresetsDialog();

    void setPresets(QList<Preset*>* presets) { m_deviceSetPresets = presets; }
    void setDeviceUISet(DeviceUISet *deviceUISet) { m_deviceUISet = deviceUISet; }
    void setPluginAPI(PluginAPI *pluginAPI) { m_pluginAPI = pluginAPI; }
    void setCurrentWorkspace(Workspace *workspace) { m_currentWorkspace = workspace; }
    void setWorkspaces(QList<Workspace*> *workspaces) { m_workspaces = workspaces; }
    void populateTree(int deviceType);
    bool wasPresetLoaded() const { return m_presetLoaded; }

private:
    enum {
		PGroup,
		PItem
	};

    Ui::DeviceSetPresetsDialog* ui;
    QList<Preset*> *m_deviceSetPresets;
    DeviceUISet *m_deviceUISet;
    PluginAPI *m_pluginAPI;
    Workspace *m_currentWorkspace;
    QList<Workspace*> *m_workspaces;
    bool m_presetLoaded;

    QTreeWidgetItem* addPresetToTree(const Preset* preset);
    void updatePresetControls();
    void loadDeviceSetPresetSettings(const Preset* preset);

private slots:
	void on_presetSave_clicked();
	void on_presetUpdate_clicked();
    void on_presetEdit_clicked();
	void on_presetExport_clicked();
	void on_presetImport_clicked();
    void on_presetLoad_clicked();
    void on_presetDelete_clicked();
	void on_presetTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void on_presetTree_itemActivated(QTreeWidgetItem *item, int column);
};

#endif // SDRGUI_GUI_DEVICESETPRESETDIALOG_H_
