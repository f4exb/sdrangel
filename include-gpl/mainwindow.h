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
#include "settings/settings.h"
#include "util/export.h"

class QLabel;
class QTreeWidgetItem;
class QDir;

class AudioDeviceInfo;
class DSPEngine;
class Indicator;
class ScopeWindow;
class SpectrumVis;
class SampleSource;
class PluginAPI;
class PluginGUI;
class ChannelMarker;
class MessageQueue;
class PluginManager;
class PluginInterface;

namespace Ui {
	class MainWindow;
}

class SDRANGELOVE_API MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = NULL);
	~MainWindow();

	MessageQueue* getMessageQueue() { return m_messageQueue; }

	void addChannelCreateAction(QAction* action);
	void addChannelRollup(QWidget* widget);
	void addViewAction(QAction* action);

	void addChannelMarker(ChannelMarker* channelMarker);
	void removeChannelMarker(ChannelMarker* channelMarker);

	void setInputGUI(QWidget* gui);

private:
	enum {
		PGroup,
		PItem
	};

	Ui::MainWindow* ui;

	AudioDeviceInfo* m_audioDeviceInfo;

	MessageQueue* m_messageQueue;

	Settings m_settings;

	SpectrumVis* m_spectrumVis;

	DSPEngine* m_dspEngine;

	QTimer m_statusTimer;
	int m_lastEngineState;

	QLabel* m_sampleRateWidget;
	Indicator* m_engineIdle;
	Indicator* m_engineRunning;
	Indicator* m_engineError;

	bool m_startOsmoSDRUpdateAfterStop;

	ScopeWindow* m_scopeWindow;
	QWidget* m_inputGUI;

	int m_sampleRate;
	quint64 m_centerFrequency;

	PluginManager* m_pluginManager;

	void loadSettings();
	void loadSettings(const Preset* preset);
	void saveSettings(Preset* preset);
	void saveSettings();

	void createStatusBar();
	void closeEvent(QCloseEvent*);
	void updateCenterFreqDisplay();
	void updateSampleRate();
	void updatePresets();
	QTreeWidgetItem* addPresetToTree(const Preset* preset);
	void applySettings();

private slots:
	void handleMessages();
	void updateStatus();
	void scopeWindowDestroyed();
	void on_action_Start_triggered();
	void on_action_Stop_triggered();
	void on_dcOffset_toggled(bool checked);
	void on_iqImbalance_toggled(bool checked);
	void on_action_View_Fullscreen_toggled(bool checked);
	void on_actionOsmoSDR_Firmware_Upgrade_triggered();
	void on_presetSave_clicked();
	void on_presetLoad_clicked();
	void on_presetDelete_clicked();
	void on_presetTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void on_presetTree_itemActivated(QTreeWidgetItem *item, int column);
	void on_action_Oscilloscope_triggered();
	void on_action_Loaded_Plugins_triggered();
	void on_action_Preferences_triggered();
	void on_sampleSource_currentIndexChanged(int index);
	void on_action_About_triggered();
};

#endif // INCLUDE_MAINWINDOW_H
