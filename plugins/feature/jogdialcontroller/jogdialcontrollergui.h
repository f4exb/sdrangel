///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_JOGDIALCONTROLLERGUI_H_
#define INCLUDE_FEATURE_JOGDIALCONTROLLERGUI_H_

#include <QTimer>
#include <QList>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "commands/commandkeyreceiver.h"
#include "settings/rollupstate.h"

#include "jogdialcontrollersettings.h"

class PluginAPI;
class FeatureUISet;
class JogdialController;
class Feature;

namespace Ui {
	class JogdialControllerGUI;
}

class JogdialControllerGUI : public FeatureGUI {
	Q_OBJECT
public:
	static JogdialControllerGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index);
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; }
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; }
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; }

protected:
    void focusInEvent(QFocusEvent* e);
    void focusOutEvent(QFocusEvent *e);

private:
	Ui::JogdialControllerGUI* ui;
	PluginAPI* m_pluginAPI;
	FeatureUISet* m_featureUISet;
	JogdialControllerSettings m_settings;
	RollupState m_rollupState;
	bool m_doApplySettings;

	JogdialController* m_jogdialController;
	MessageQueue m_inputMessageQueue;
	QTimer m_statusTimer;
	int m_lastFeatureState;
	QList<JogdialControllerSettings::AvailableChannel> m_availableChannels;
	ChannelAPI *m_selectedChannel;
	CommandKeyReceiver m_commandKeyReceiver;

	explicit JogdialControllerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
	virtual ~JogdialControllerGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displaySettings();
	void updateChannelList();
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
	void onMenuDialogCalled(const QPoint &p);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
	void on_devicesRefresh_clicked();
	void on_channels_currentIndexChanged(int index);
	void updateStatus();
	void tick();
};


#endif // INCLUDE_FEATURE_JOGDIALCONTROLLERGUI_H_
