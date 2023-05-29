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

#ifndef INCLUDE_FEATURE_SIMPLEPTTGUI_H_
#define INCLUDE_FEATURE_SIMPLEPTTGUI_H_

#include <QTimer>
#include <QDateTime>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "simplepttsettings.h"

class PluginAPI;
class FeatureUISet;
class SimplePTT;

namespace Ui {
	class SimplePTTGUI;
}

class SimplePTTGUI : public FeatureGUI {
	Q_OBJECT
public:
	static SimplePTTGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
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
	Ui::SimplePTTGUI* ui;
	PluginAPI* m_pluginAPI;
	FeatureUISet* m_featureUISet;
	SimplePTTSettings m_settings;
    QList<QString> m_settingsKeys;
	RollupState m_rollupState;
	bool m_doApplySettings;

	SimplePTT* m_simplePTT;
	MessageQueue m_inputMessageQueue;
	QTimer m_statusTimer;
	int m_lastFeatureState;
	std::vector<QString> m_statusColors;
	std::vector<QString> m_statusTooltips;

    bool m_lastCommandResult;
    int m_lastCommandExitCode;
    int m_lastCommandExitStatus;
    int m_lastCommandError;
    bool m_lastCommandErrorReported;
    QDateTime m_lastCommandEndTime;
    QString m_lastCommandLog;

	explicit SimplePTTGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
	virtual ~SimplePTTGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void applyPTT(bool tx);
	void displaySettings();
    void updateDeviceSetLists();
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
	void onMenuDialogCalled(const QPoint &p);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
	void on_devicesRefresh_clicked();
	void on_rxDevice_currentIndexChanged(int index);
	void on_txDevice_currentIndexChanged(int index);
	void on_rxtxDelay_valueChanged(int value);
	void on_txrxDelay_valueChanged(int value);
	void on_ptt_toggled(bool checked);
	void on_vox_toggled(bool checked);
	void on_voxEnable_clicked(bool checked);
	void on_voxLevel_valueChanged(int value);
	void on_voxHold_valueChanged(int value);
    void on_commandRxTxEnable_toggled(bool checked);
    void on_commandTxRxEnable_toggled(bool checked);
    void on_commandRxTxFileDialog_clicked();
    void on_commandTxRxFileDialog_clicked();
    void on_gpioRxTxControlEnable_toggled(bool checked);
    void on_gpioTxRxControlEnable_toggled(bool checked);
    void on_gpioRxTxMask_editingFinished();
    void on_gpioRxTxValue_editingFinished();
    void on_gpioTxRxMask_editingFinished();
    void on_gpioTxRxValue_editingFinished();
    void on_gpioControl_clicked();
    void on_lastCommandLog_clicked();

	void updateStatus();
	void audioSelect(const QPoint& p);
};


#endif // INCLUDE_FEATURE_SIMPLEPTTGUI_H_
