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

#ifndef INCLUDE_FEATURE_AMBEGUI_H_
#define INCLUDE_FEATURE_AMBEGUI_H_

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "util/movingaverage.h"
#include "settings/rollupstate.h"

#include "ambesettings.h"

class PluginAPI;
class FeatureUISet;
class Feature;
class AMBE;
class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;

namespace Ui {
	class AMBEGUI;
}

class AMBEGUI : public FeatureGUI
{
    Q_OBJECT
public:
	static AMBEGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
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
	Ui::AMBEGUI* ui;
    AMBE *m_ambe;
	PluginAPI* m_pluginAPI;
	FeatureUISet* m_featureUISet;
	AMBESettings m_settings;
    QList<QString> m_settingsKeys;
	RollupState m_rollupState;
    bool m_doApplySettings;
	MessageQueue m_inputMessageQueue;

	explicit AMBEGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
	virtual ~AMBEGUI();

    void populateSerialList();
    void refreshInUseList();

    void blockApplySettings(bool block) { m_doApplySettings = !block; }
    void applySettings(bool force = false);
	void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
	void onMenuDialogCalled(const QPoint &p);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_importSerial_clicked();
    void on_importAllSerial_clicked();
    void on_removeAmbeDevice_clicked();
    void on_refreshAmbeList_clicked();
    void on_refreshSerial_clicked();
    void on_clearAmbeList_clicked();
    void on_importAddress_clicked();
};

#endif
