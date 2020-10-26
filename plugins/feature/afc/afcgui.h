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
#include "afcsettings.h"

class PluginAPI;
class FeatureUISet;
class AFC;

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

private:
	Ui::AFCGUI* ui;
	PluginAPI* m_pluginAPI;
	FeatureUISet* m_featureUISet;
	AFCSettings m_settings;
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
    void updateDeviceSetLists();
	bool handleMessage(const Message& message);

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);

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
