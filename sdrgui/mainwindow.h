///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef INCLUDE_MAINWINDOW_H
#define INCLUDE_MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QList>

#include "settings/mainsettings.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "export.h"
#include "mainparser.h"
#include "maincore.h"

class QLabel;
class QTreeWidgetItem;
class QDir;

class DSPEngine;
class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;
class Indicator;
class GLSpectrumGUI;
class PluginAPI;
class ChannelGUI;
class ChannelMarker;
class PluginManager;
class DeviceAPI;
class DeviceUISet;
class FeatureUISet;
class PluginInterface;
class QWidget;
class WebAPIRequestMapper;
class WebAPIServer;
class WebAPIAdapter;
class Preset;
class Command;
class FeatureSetPreset;
class CommandKeyReceiver;

namespace Ui {
	class MainWindow;
}

class SDRGUI_API MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QWidget* parent = 0);
	~MainWindow();
	static MainWindow *getInstance() { return m_instance; } // Main Window is de facto a singleton so this just returns its reference
	MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }
	void addViewAction(QAction* action);
	void setDeviceGUI(int deviceTabIndex, QWidget* gui, const QString& deviceDisplayName, int deviceType = 0);
    const PluginManager *getPluginManager() const { return m_pluginManager; }
    std::vector<DeviceUISet*>& getDeviceUISets() { return m_deviceUIs; }
    void commandKeysConnect(QObject *object, const char *slot);
    void commandKeysDisconnect(QObject *object, const char *slot);

private:
    enum {
		PGroup,
		PItem
	};

	struct DeviceWidgetTabData
	{
	    QWidget *gui;
	    QString displayName;
	    QString tabName;
	};

	static MainWindow *m_instance;
	Ui::MainWindow* ui;
	MessageQueue m_inputMessageQueue;
    MainCore *m_mainCore;
	std::vector<DeviceUISet*> m_deviceUIs;
    std::vector<FeatureUISet*> m_featureUIs;
	QList<DeviceWidgetTabData> m_deviceWidgetTabs;

	DSPEngine* m_dspEngine;
	PluginManager* m_pluginManager;

	QTimer m_statusTimer;
	int m_lastEngineState;

	QLabel* m_dateTimeWidget;
	QLabel* m_showSystemWidget;

	QWidget* m_inputGUI;

	int m_sampleRate;
	quint64 m_centerFrequency;
	std::string m_sampleFileName;

	WebAPIRequestMapper *m_requestMapper;
	WebAPIServer *m_apiServer;
	WebAPIAdapter *m_apiAdapter;
	QString m_apiHost;
	int m_apiPort;

	CommandKeyReceiver *m_commandKeyReceiver;

	void loadSettings();
	void loadPresetSettings(const Preset* preset, int tabIndex);
	void savePresetSettings(Preset* preset, int tabIndex);
	void loadFeatureSetPresetSettings(const FeatureSetPreset* preset, int featureSetIndex);
	void saveFeatureSetPresetSettings(FeatureSetPreset* preset, int featureSetIndex);
	void saveCommandSettings();

	void createStatusBar();
	void closeEvent(QCloseEvent*);
	void updatePresetControls();
	QTreeWidgetItem* addPresetToTree(const Preset* preset);
	QTreeWidgetItem* addCommandToTree(const Command* command);
	void applySettings();

	void addSourceDevice(int deviceIndex);
	void addSinkDevice();
	void addMIMODevice();
    void removeLastDevice();
    void addFeatureSet();
    void removeFeatureSet(unsigned int tabIndex);
    void removeAllFeatureSets();
    void deleteChannel(int deviceSetIndex, int channelIndex);
    void sampleSourceChanged(int tabIndex, int newDeviceIndex);
	void sampleSinkChanged(int tabIndex, int newDeviceIndex);
	void sampleMIMOChanged(int tabIndex, int newDeviceIndex);
    void deleteFeature(int featureSetIndex, int featureIndex);

    bool handleMessage(const Message& cmd);
	void restoreDeviceTabs();

private slots:
	void handleMessages();
	void updateStatus();
	void on_action_View_Fullscreen_toggled(bool checked);
	void on_presetSave_clicked();
	void on_presetUpdate_clicked();
    void on_presetEdit_clicked();
	void on_presetExport_clicked();
	void on_presetImport_clicked();
	void on_settingsSave_clicked();
	void on_presetLoad_clicked();
	void on_presetDelete_clicked();
	void on_presetTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void on_presetTree_itemActivated(QTreeWidgetItem *item, int column);
	void on_commandNew_clicked();
    void on_commandDuplicate_clicked();
    void on_commandEdit_clicked();
    void on_commandDelete_clicked();
    void on_commandRun_clicked();
    void on_commandOutput_clicked();
    void on_commandsSave_clicked();
    void on_commandKeyboardConnect_toggled(bool checked);
	void on_action_Audio_triggered();
    void on_action_Logging_triggered();
    void on_action_AMBE_triggered();
    void on_action_LimeRFE_triggered();
	void on_action_My_Position_triggered();
    void on_action_DeviceUserArguments_triggered();
    void samplingDeviceChanged(int deviceType, int tabIndex, int newDeviceIndex);
    void channelAddClicked(int channelIndex);
    void featureAddClicked(int featureIndex);
	void on_action_Loaded_Plugins_triggered();
	void on_action_About_triggered();
	void on_action_addSourceDevice_triggered();
	void on_action_addSinkDevice_triggered();
    void on_action_addMIMODevice_triggered();
	void on_action_removeLastDevice_triggered();
	void on_action_addFeatureSet_triggered();
	void on_action_removeLastFeatureSet_triggered();
	void tabInputViewIndexChanged();
    void tabChannelsIndexChanged();
	void commandKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release);
};

#endif // INCLUDE_MAINWINDOW_H
