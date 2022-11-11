///////////////////////////////////////////////////////////////////////////////////
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

#ifndef INCLUDE_FEATURE_AFCGUI_H_
#define INCLUDE_FEATURE_AFCGUI_H_

#include <QTimer>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "afcsettings.h"
#include "afc.h"

class PluginAPI;
class FeatureUISet;
namespace Ui {
	class AFCGUI;
}

class AFCGUI : public FeatureGUI {
	Q_OBJECT
public:
	static AFCGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
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
	Ui::AFCGUI* ui;
	PluginAPI* m_pluginAPI;
	FeatureUISet* m_featureUISet;
	AFCSettings m_settings;
	RollupState m_rollupState;
	bool m_doApplySettings;

	AFC* m_afc;
	MessageQueue m_inputMessageQueue;
	QTimer m_statusTimer;
	QTimer m_autoTargetStatusTimer;
	int m_lastFeatureState;

	explicit AFCGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
	virtual ~AFCGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displaySettings();
	void requestDeviceSetLists();
    void updateDeviceSetLists(const AFC::MsgDeviceSetListsReport& report);
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
	void onMenuDialogCalled(const QPoint &p);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
	void on_hasTargetFrequency_toggled(bool checked);
	void on_targetFrequency_changed(quint64 value);
	void on_transverterTarget_toggled(bool checked);
	void on_toleranceFrequency_changed(quint64 value);
	void on_deviceTrack_clicked();
	void on_devicesRefresh_clicked();
	void on_trackerDevice_currentIndexChanged(int index);
	void on_trackedDevice_currentIndexChanged(int index);
	void on_devicesApply_clicked();
	void on_targetPeriod_valueChanged(int value);
	void updateStatus();
	void resetAutoTargetStatus();
};


#endif // INCLUDE_FEATURE_AFCGUI_H_
