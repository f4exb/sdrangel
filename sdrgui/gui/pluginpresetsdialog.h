///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef SDRGUI_GUI_PLUGINPRESETSDIALOG_H_
#define SDRGUI_GUI_PLUGINPRESETSDIALOG_H_

#include <QDialog>
#include <QList>
#include <QTreeWidgetItem>

#include "export.h"

class PluginPreset;
class SerializableInterface;
class WebAPIAdapterInterface;
class PluginAPI;
class Workspace;

namespace Ui {
    class PluginPresetsDialog;
}

class SDRGUI_API PluginPresetsDialog : public QDialog {
    Q_OBJECT
public:
    explicit PluginPresetsDialog(const QString& pluginId, QWidget* parent = nullptr);
    ~PluginPresetsDialog();
    void setPresets(QList<PluginPreset*>* presets) { m_pluginPresets = presets; }
    void setSerializableInterface(SerializableInterface *serializableInterface) { m_serializableInterface = serializableInterface; }
    void populateTree();
    bool wasPresetLoaded() const { return m_presetLoaded; }

private:
    enum {
        PGroup,
        PItem
    };

    Ui::PluginPresetsDialog* ui;
    QString m_pluginId;
    QList<PluginPreset*> *m_pluginPresets;
    SerializableInterface *m_serializableInterface;
    bool m_presetLoaded;

    QTreeWidgetItem* addPresetToTree(const PluginPreset* preset);
    void updatePresetControls();
    PluginPreset* newPreset(const QString& group, const QString& description);
    void addPreset(PluginPreset *preset);
    void savePresetSettings(PluginPreset* preset);
    void loadPresetSettings(const PluginPreset* preset);
    void sortPresets();
    void renamePresetGroup(const QString& oldGroupName, const QString& newGroupName);
    void deletePreset(const PluginPreset* preset);
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

#endif // SDRGUI_GUI_PLUGINPRESETSDIALOG_H_
