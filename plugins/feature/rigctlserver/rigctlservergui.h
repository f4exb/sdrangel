///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_RIGCTLSERVERGUI_H_
#define INCLUDE_FEATURE_RIGCTLSERVERGUI_H_

#include <QTimer>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "rigctlserversettings.h"

class PluginAPI;
class FeatureUISet;
class RigCtlServer;

namespace Ui {
	class RigCtlServerGUI;
}

class RigCtlServerGUI : public FeatureGUI {
	Q_OBJECT
public:
	static RigCtlServerGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
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
	Ui::RigCtlServerGUI* ui;
	PluginAPI* m_pluginAPI;
	FeatureUISet* m_featureUISet;
	RigCtlServerSettings m_settings;
	RollupState m_rollupState;
	bool m_doApplySettings;

	RigCtlServer* m_rigCtlServer;
	MessageQueue m_inputMessageQueue;
	QTimer m_statusTimer;
	int m_lastFeatureState;

	explicit RigCtlServerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
	virtual ~RigCtlServerGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displaySettings();
    void updateDeviceSetList();
	bool updateChannelList(); //!< true if channel index has changed
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
	void onMenuDialogCalled(const QPoint &p);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
	void on_enable_toggled(bool checked);
	void on_devicesRefresh_clicked();
	void on_device_currentIndexChanged(int index);
	void on_channel_currentIndexChanged(int index);
	void on_rigCtrlPort_valueChanged(int value);
	void on_maxFrequencyOffset_valueChanged(int value);
	void updateStatus();
};


#endif // INCLUDE_FEATURE_RIGCTLSERVERGUI_H_
