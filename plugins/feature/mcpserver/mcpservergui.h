///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_FEATURE_MCPSERVERGUI_H_
#define INCLUDE_FEATURE_MCPSERVERGUI_H_

#include <QTimer>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "mcpserversettings.h"

class PluginAPI;
class FeatureUISet;
class MCPServer;

namespace Ui {
	class MCPServerGUI;
}

class MCPServerGUI : public FeatureGUI {
	Q_OBJECT
public:
	static MCPServerGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index);
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; }
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; }
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; }

private:
	Ui::MCPServerGUI* ui;
	PluginAPI* m_pluginAPI;
	FeatureUISet* m_featureUISet;
	MCPServerSettings m_settings;
    QList<QString> m_settingsKeys;
	RollupState m_rollupState;
	bool m_doApplySettings;

	MCPServer* m_mcpServer;
	MessageQueue m_inputMessageQueue;
	QTimer m_statisticsTimer;

	explicit MCPServerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
	virtual ~MCPServerGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displaySettings();
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
	void onMenuDialogCalled(const QPoint &p);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
	void on_address_editingFinished();
	void on_port_valueChanged(int value);
	void on_token_editingFinished();
	void on_captureDir_editingFinished();
	void on_captureDirBrowse_clicked();
	void updateFeatureState();
	void updateStatistics();
};

#endif // INCLUDE_FEATURE_MCPSERVERGUI_H_
