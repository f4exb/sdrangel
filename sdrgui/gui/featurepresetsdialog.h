///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 F4EXB                                                      //
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

#ifndef SDRGUI_GUI_FEATUREPRESETDIALOG_H_
#define SDRGUI_GUI_FEATUREPRESETDIALOG_H_

#include <QDialog>
#include <QList>
#include <QTreeWidgetItem>

#include "export.h"

class FeatureSetPreset;
class FeatureUISet;
class WebAPIAdapterInterface;
class PluginAPI;
class Workspace;

namespace Ui {
    class FeaturePresetsDialog;
}

class SDRGUI_API FeaturePresetsDialog : public QDialog {
    Q_OBJECT
public:
    explicit FeaturePresetsDialog(QWidget* parent = nullptr);
    ~FeaturePresetsDialog();
    void setPresets(QList<FeatureSetPreset*>* presets) { m_featureSetPresets = presets; }
    void setFeatureUISet(FeatureUISet *featureUISet) { m_featureUISet = featureUISet; }
    void setPluginAPI(PluginAPI *pluginAPI) { m_pluginAPI = pluginAPI; }
    void setWebAPIAdapter(WebAPIAdapterInterface *apiAdapter) { m_apiAdapter = apiAdapter; }
    void setCurrentWorkspace(Workspace *workspace) { m_currentWorkspace = workspace; }
    void setWorkspaces(QList<Workspace*> *workspaces) { m_workspaces = workspaces; }
    void populateTree();
    bool wasPresetLoaded() const { return m_presetLoaded; }

private:
    enum {
		PGroup,
		PItem
	};

    Ui::FeaturePresetsDialog* ui;
    QList<FeatureSetPreset*> *m_featureSetPresets;
    FeatureUISet *m_featureUISet;
    PluginAPI *m_pluginAPI;
    WebAPIAdapterInterface *m_apiAdapter;
    Workspace *m_currentWorkspace;
    QList<Workspace*> *m_workspaces;
    bool m_presetLoaded;

    QTreeWidgetItem* addPresetToTree(const FeatureSetPreset* preset);
    void updatePresetControls();
    FeatureSetPreset* newFeatureSetPreset(const QString& group, const QString& description);
    void addFeatureSetPreset(FeatureSetPreset *preset);
    void savePresetSettings(FeatureSetPreset* preset);
    void loadPresetSettings(const FeatureSetPreset* preset);
    void sortFeatureSetPresets();
    void renamePresetGroup(const QString& oldGroupName, const QString& newGroupName);
    void deletePreset(const FeatureSetPreset* preset);
    void deletePresetGroup(const QString& groupName);

private slots:
	void on_presetSave_clicked();
	void on_presetUpdate_clicked();
    void on_presetEdit_clicked();
	void on_presetDelete_clicked();
	void on_presetLoad_clicked();
	void on_presetTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void on_presetTree_itemActivated(QTreeWidgetItem *item, int column);
};

#endif // SDRGUI_GUI_FEATUREPRESETDIALOG_H_
