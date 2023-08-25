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
#include <QProcess>

#include "settings/mainsettings.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "export.h"
#include "mainparser.h"
#include "maincore.h"

class QLabel;
class QTreeWidgetItem;
class QDir;
class QToolButton;

class DSPEngine;
class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;
class Indicator;
class GLSpectrumGUI;
class MainSpectrumGUI;
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
class ConfigurationsDialog;
class ProfileDialog;
class SerializableInterface;

class QMenuBar;
class Workspace;

// namespace Ui {
// 	class MainWindow;
// }

class SDRGUI_API MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QWidget* parent = nullptr);
	~MainWindow();
	static MainWindow *getInstance() { return m_instance; } // Main Window is de facto a singleton so this just returns its reference
	MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }
    const PluginManager *getPluginManager() const { return m_pluginManager; }
    std::vector<DeviceUISet*>& getDeviceUISets() { return m_deviceUIs; }
    void commandKeysConnect(QObject *object, const char *slot);
    void commandKeysDisconnect(QObject *object, const char *slot);
    int getNumberOfWorkspaces() const { return m_workspaces.size(); }

public slots:
    void channelMove(ChannelGUI *gui, int wsIndexDestination);
    void channelDuplicate(ChannelGUI *gui);
    void channelMoveToDeviceSet(ChannelGUI *gui, int dsIndexDestination);

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
    QList<Workspace*> m_workspaces;
    Workspace *m_currentWorkspace;
	// Ui::MainWindow* ui;
	MessageQueue m_inputMessageQueue;
    MainCore *m_mainCore;
	std::vector<DeviceUISet*> m_deviceUIs;
    std::vector<FeatureUISet*> m_featureUIs;
	QList<DeviceWidgetTabData> m_deviceWidgetTabs;

	DSPEngine* m_dspEngine;
	PluginManager* m_pluginManager;

	QTimer m_statusTimer;
	int m_lastEngineState;

    QMenuBar *m_menuBar;
	QLabel* m_dateTimeWidget;
	QLabel* m_showSystemWidget;

	WebAPIRequestMapper *m_requestMapper;
	WebAPIServer *m_apiServer;
	WebAPIAdapter *m_apiAdapter;
	QString m_apiHost;
	int m_apiPort;
	QAction *m_spectrumToggleViewAction;

	CommandKeyReceiver *m_commandKeyReceiver;
    ProfileDialog *m_profileDialog;

	QProcess *m_fftWisdomProcess;

	void loadSettings();
	void loadDeviceSetPresetSettings(const Preset* preset, int deviceSetIndex);
	void saveDeviceSetPresetSettings(Preset* preset, int deviceSetIndex);
	void loadFeatureSetPresetSettings(const FeatureSetPreset* preset, int featureSetIndex, Workspace *workspace);
	void saveFeatureSetPresetSettings(FeatureSetPreset* preset, int featureSetIndex);

	QString openGLVersion();
    void createMenuBar(QToolButton *button);
	void createStatusBar();
	void closeEvent(QCloseEvent*);
	void applySettings();

    void removeDeviceSet(int deviceSetIndex);
    void removeLastDeviceSet();
    void addFeatureSet();
    void removeFeatureSet(unsigned int featureSetIndex);
    void removeAllFeatureSets();
    void deleteChannel(int deviceSetIndex, int channelIndex);
    void channelDuplicateToDeviceSet(ChannelGUI *sourceChannelGUI, int dsIndexDestination);
    void sampleDeviceChange(int deviceType, int deviceSetIndex, int newDeviceIndex, Workspace *workspace);
    void sampleSourceChange(int deviceSetIndex, int newDeviceIndex, Workspace *workspace);
	void sampleSinkChange(int deviceSetIndex, int newDeviceIndex, Workspace *workspace);
	void sampleMIMOChange(int deviceSetIndex, int newDeviceIndex, Workspace *workspace);
    void sampleSourceCreate(
        int deviceSetIndex,
        int deviceIndex,
        DeviceUISet *deviceUISet
    );
    void sampleSinkCreate(
        int deviceSetIndex,
        int deviceIndex,
        DeviceUISet *deviceUISet
    );
    void sampleMIMOCreate(
        int deviceSetIndex,
        int deviceIndex,
        DeviceUISet *deviceUISet
    );
    void deleteFeature(int featureSetIndex, int featureIndex);
    void loadDefaultPreset(const QString& pluginId, SerializableInterface *serializableInterface);

    bool handleMessage(const Message& cmd);

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;

private slots:
	void handleMessages();
    void handleWorkspaceVisibility(Workspace *workspace, bool visibility);

#ifdef ANDROID
	void on_action_View_KeepScreenOn_toggled(bool checked);
#endif
	void on_action_View_Fullscreen_toggled(bool checked);
	void on_action_Profile_triggered();
	void on_action_saveAll_triggered();
    void on_action_Configurations_triggered();
	void on_action_Audio_triggered();
	void on_action_Graphics_triggered();
    void on_action_Logging_triggered();
	void on_action_FFT_triggered();
	void on_action_FFTWisdom_triggered();
	void on_action_My_Position_triggered();
    void on_action_DeviceUserArguments_triggered();
    void on_action_commands_triggered();
    void on_action_Quick_Start_triggered();
    void on_action_Main_Window_triggered();
	void on_action_Loaded_Plugins_triggered();
	void on_action_About_triggered();

	void updateStatus();
    void addWorkspace();
    void viewAllWorkspaces();
    void removeEmptyWorkspaces();
    void openConfigurationDialog(bool openOnly);
    void loadDefaultConfigurations();
	void loadConfiguration(const Configuration *configuration, bool fromDialog = false);
    void saveConfiguration(Configuration *configuration);
	void sampleSourceAdd(Workspace *deviceWorkspace, Workspace *spectrumWorkspace, int deviceIndex);
	void sampleSinkAdd(Workspace *workspace, Workspace *spectrumWorkspace, int deviceIndex);
	void sampleMIMOAdd(Workspace *workspace, Workspace *spectrumWorkspace, int deviceIndex);
    void samplingDeviceChangeHandler(DeviceGUI *deviceGUI, int newDeviceIndex);
    void channelAddClicked(Workspace *workspace, int deviceSetIndex, int channelPluginIndex);
    void featureAddClicked(Workspace *workspace, int featureIndex);
    void featureMove(FeatureGUI *gui, int wsIndexDestnation);
    void deviceStateChanged(DeviceAPI *deviceAPI);
    void openFeaturePresetsDialog(QPoint p, Workspace *workspace);
    void startAllDevices(Workspace *workspace);
    void stopAllDevices(Workspace *workspace);
    void deviceMove(DeviceGUI *gui, int wsIndexDestnation);
    void mainSpectrumMove(MainSpectrumGUI *gui, int wsIndexDestnation);
    void mainSpectrumShow(int deviceSetIndex);
    void mainSpectrumRequestDeviceCenterFrequency(int deviceSetIndex, qint64 deviceCenterFrequency);
    void showAllChannels(int deviceSetIndex);
    void openDeviceSetPresetsDialog(QPoint p, DeviceGUI *deviceGUI);
	void commandKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release);
	void fftWisdomProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void orientationChanged(Qt::ScreenOrientation orientation);
};

#endif // INCLUDE_MAINWINDOW_H
