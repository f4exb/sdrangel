///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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
#include "util/messagequeue.h"
#include "util/export.h"
#include "mainparser.h"

class QLabel;
class QTreeWidgetItem;
class QDir;
class SamplingDeviceControl;

class AudioDeviceInfo;
class DSPEngine;
class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;
class Indicator;
class SpectrumVis;
class GLSpectrum;
class GLSpectrumGUI;
class ChannelWindow;
class PluginAPI;
class PluginInstanceGUI;
class ChannelMarker;
class PluginManager;
class DeviceSourceAPI;
class DeviceSinkAPI;
class DeviceUISet;
class PluginInterface;
class QWidget;
class WebAPIRequestMapper;
class WebAPIServer;
class WebAPIAdapterGUI;

namespace qtwebapp {
    class LoggerWithFile;
}

namespace Ui {
	class MainWindow;
}

class SDRANGEL_API MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QWidget* parent = 0);
	~MainWindow();
	static MainWindow *getInstance() { return m_instance; } // Main Window is de facto a singleton so this just returns its reference

	MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }

	void addViewAction(QAction* action);

    void addChannelRollup(int deviceTabIndex, QWidget* widget);
	void setDeviceGUI(int deviceTabIndex, QWidget* gui, const QString& deviceDisplayName, bool sourceDevice = true);

	const QTimer& getMasterTimer() const { return m_masterTimer; }
	const MainSettings& getMainSettings() const { return m_settings; }

	friend class WebAPIAdapterGUI;

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
	AudioDeviceInfo m_audioDeviceInfo;
	MessageQueue m_inputMessageQueue;
	MainSettings m_settings;
	std::vector<DeviceUISet*> m_deviceUIs;
	QList<DeviceWidgetTabData> m_deviceWidgetTabs;
	int m_masterTabIndex;

	DSPEngine* m_dspEngine;
	PluginManager* m_pluginManager;

	QTimer m_masterTimer;
	QTimer m_statusTimer;
	int m_lastEngineState;

	QLabel* m_dateTimeWidget;
	QLabel* m_showSystemWidget;

	QWidget* m_inputGUI;

	int m_sampleRate;
	quint64 m_centerFrequency;
	std::string m_sampleFileName;

	qtwebapp::LoggerWithFile *m_logger;

	WebAPIRequestMapper *m_requestMapper;
	WebAPIServer *m_apiServer;
	WebAPIAdapterGUI *m_apiAdapter;

	void loadSettings();
	void loadPresetSettings(const Preset* preset, int tabIndex);
	void savePresetSettings(Preset* preset, int tabIndex);

	void createStatusBar();
	void closeEvent(QCloseEvent*);
	void updatePresetControls();
	QTreeWidgetItem* addPresetToTree(const Preset* preset);
	void applySettings();

	void addSourceDevice();
	void addSinkDevice();
    void removeLastDevice();

    void setLoggingOpions();

private slots:
	void handleMessages();
	void updateStatus();
	void on_action_View_Fullscreen_toggled(bool checked);
	void on_presetSave_clicked();
	void on_presetUpdate_clicked();
	void on_presetExport_clicked();
	void on_presetImport_clicked();
	void on_settingsSave_clicked();
	void on_presetLoad_clicked();
	void on_presetDelete_clicked();
	void on_presetTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void on_presetTree_itemActivated(QTreeWidgetItem *item, int column);
	void on_action_Audio_triggered();
    void on_action_Logging_triggered();
	void on_action_DV_Serial_triggered(bool checked);
	void on_action_My_Position_triggered();
	void on_sampleSource_changed();
	void on_sampleSink_changed();
    void on_channel_addClicked(bool checked);
	void on_action_Loaded_Plugins_triggered();
	void on_action_About_triggered();
	void on_action_addSourceDevice_triggered();
	void on_action_addSinkDevice_triggered();
	void on_action_removeLastDevice_triggered();
	void on_action_reloadDevices_triggered();
	void on_action_Exit_triggered();
	void tabInputViewIndexChanged();
};

#endif // INCLUDE_MAINWINDOW_H
