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
#include "settings/mainsettings.h"
#include "util/messagequeue.h"
#include "util/export.h"

class QLabel;
class QTreeWidgetItem;
class QDir;

class AudioDeviceInfo;
class DSPEngine;
class Indicator;
class SpectrumVis;
class GLSpectrum;
class GLSpectrumGUI;
class ChannelWindow;
class FileSink;
class SampleSource;
class PluginAPI;
class PluginGUI;
class ChannelMarker;
class PluginManager;
class PluginInterface;

namespace Ui {
	class MainWindow;
}

class SDRANGEL_API MainWindow : public QMainWindow {
	Q_OBJECT

public:
	struct DeviceUISet
	{
		SpectrumVis *m_spectrumVis;
		GLSpectrum *m_spectrum;
		GLSpectrumGUI *m_spectrumGUI;
		ChannelWindow *m_channelWindow;

		DeviceUISet(QTimer& timer);
		~DeviceUISet();
	};

	explicit MainWindow(QWidget* parent = 0);
	~MainWindow();

	MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }

	void addChannelCreateAction(QAction* action);
	void addChannelRollup(QWidget* widget);
	void addViewAction(QAction* action);

	void addChannelMarker(ChannelMarker* channelMarker);
	void removeChannelMarker(ChannelMarker* channelMarker);

	void setInputGUI(QWidget* gui);
	const QTimer& getMasterTimer() const { return m_masterTimer; }

private:
	enum {
		PGroup,
		PItem
	};

	Ui::MainWindow* ui;

	AudioDeviceInfo* m_audioDeviceInfo;

	MessageQueue m_inputMessageQueue;

	MainSettings m_settings;

	SpectrumVis* m_rxSpectrumVis;
	FileSink *m_fileSink;

	std::vector<DeviceUISet*> m_deviceUIs;

	DSPEngine* m_dspEngine;

	QTimer m_masterTimer;
	QTimer m_statusTimer;
	int m_lastEngineState;

	QLabel* m_sampleRateWidget;
	Indicator* m_recording;
	Indicator* m_engineIdle;
	Indicator* m_engineRunning;
	Indicator* m_engineError;

	bool m_startOsmoSDRUpdateAfterStop;

	QWidget* m_inputGUI;

	int m_sampleRate;
	quint64 m_centerFrequency;
	std::string m_sampleFileName;

	PluginManager* m_pluginManager;

	void loadSettings();
	void loadPresetSettings(const Preset* preset);
	void savePresetSettings(Preset* preset);
	void saveSettings();

	void createStatusBar();
	void closeEvent(QCloseEvent*);
	void updateCenterFreqDisplay();
	void updateSampleRate();
	void updatePresetControls();
	QTreeWidgetItem* addPresetToTree(const Preset* preset);
	void applySettings();

private slots:
	void handleDSPMessages();
	void handleMessages();
	void updateStatus();
	void on_action_Start_triggered();
	void on_action_Stop_triggered();
	void on_action_Start_Recording_triggered();
	void on_action_Stop_Recording_triggered();
	void on_action_View_Fullscreen_toggled(bool checked);
	void on_presetSave_clicked();
	void on_presetUpdate_clicked();
	void on_settingsSave_clicked();
	void on_presetLoad_clicked();
	void on_presetDelete_clicked();
	void on_presetTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void on_presetTree_itemActivated(QTreeWidgetItem *item, int column);
	void on_action_Loaded_Plugins_triggered();
	void on_action_Preferences_triggered();
	void on_sampleSource_currentIndexChanged(int index);
	void on_action_About_triggered();
};

#endif // INCLUDE_MAINWINDOW_H
