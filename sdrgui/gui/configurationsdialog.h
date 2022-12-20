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

#ifndef SDRGUI_GUI_CONFIGURATIONSDIALOG_H_
#define SDRGUI_GUI_CONFIGURATIONSDIALOG_H_

#include <QDialog>
#include <QList>
#include <QTreeWidgetItem>

#include "export.h"

class Configuration;
class FeatureUISet;
class WebAPIAdapterInterface;
class PluginAPI;
class Workspace;

namespace Ui {
    class ConfigurationsDialog;
}

class SDRGUI_API ConfigurationsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConfigurationsDialog(bool openOnly, QWidget* parent = nullptr);
    ~ConfigurationsDialog();
    void setConfigurations(QList<Configuration*>* configurations) { m_configurations = configurations; }
    void populateTree();

private:
    enum {
		PGroup,
		PItem
	};

    Ui::ConfigurationsDialog* ui;
    QList<Configuration*> *m_configurations;

    QTreeWidgetItem* addConfigurationToTree(const Configuration* configuration);
    void updateConfigurationControls();
    Configuration* newConfiguration(const QString& group, const QString& description);
    void addConfiguration(Configuration *configuration);
    void sortConfigurations();
    void renameConfigurationGroup(const QString& oldGroupName, const QString& newGroupName);
    void deleteConfiguration(const Configuration* configuration);
    void deleteConfigurationGroup(const QString& groupName);

private slots:
	void on_configurationSave_clicked();
	void on_configurationUpdate_clicked();
    void on_configurationEdit_clicked();
	void on_configurationExport_clicked();
	void on_configurationImport_clicked();
	void on_configurationDelete_clicked();
	void on_configurationLoad_clicked();
	void on_configurationTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void on_configurationTree_itemActivated(QTreeWidgetItem *item, int column);
    void accept() override;

signals:
    void saveConfiguration(Configuration*);
    void loadConfiguration(const Configuration*);
};

#endif // SDRGUI_GUI_CONFIGURATIONSDIALOG_H_
