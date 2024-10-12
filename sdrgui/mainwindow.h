///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2019 Stefan Biereigel <stefan@biereigel.de>                     //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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
#include <QStateMachine>
#include <QState>
#include <QFinalState>
#include <QSignalTransition>
#include <QProgressDialog>

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
class DSPDeviceMIMOEngine;
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
class SDRangelSplash;

class QMenuBar;
class Workspace;
class MainWindow;

// Would preferablly have these FSM classes as nested classes of MainWindow
// However, as they inherit from QObject, they should have Q_OBJECT macro which isn't supported for nested classes
// Instead we have to declare them as friend classes to MainWindow

class MainWindowFSM : public QStateMachine {
    Q_OBJECT

public:

    MainWindowFSM(MainWindow *mainWindow, QObject *parent=nullptr);

protected:

    void createStates(int states); // number of states to create, including final state

    MainWindow *m_mainWindow;
    QList<QState *> m_states;
    QFinalState *m_finalState;

};

class AddSampleSourceFSM : public MainWindowFSM {
    Q_OBJECT

public:

    AddSampleSourceFSM(MainWindow *mainWindow, Workspace *deviceWorkspace, Workspace *spectrumWorkspace, int deviceIndex, bool loadDefaults, QObject *parent=nullptr);

private:

    Workspace *m_deviceWorkspace;
    Workspace *m_spectrumWorkspace;
    int m_deviceIndex;
    bool m_loadDefaults;

    int m_deviceSetIndex;
    DeviceAPI *m_deviceAPI;
    DeviceUISet *m_deviceUISet;

    DSPDeviceSourceEngine *m_dspDeviceSourceEngine;

    void addEngine();
    void addDevice();
    void addDeviceUI();
};

class AddSampleSinkFSM : public MainWindowFSM {
    Q_OBJECT

public:

    AddSampleSinkFSM(MainWindow *mainWindow, Workspace *deviceWorkspace, Workspace *spectrumWorkspace, int deviceIndex, bool loadDefaults, QObject *parent=nullptr);

private:

    Workspace *m_deviceWorkspace;
    Workspace *m_spectrumWorkspace;
    int m_deviceIndex;
    bool m_loadDefaults;

    int m_deviceSetIndex;
    DeviceAPI *m_deviceAPI;
    DeviceUISet *m_deviceUISet;

    DSPDeviceSinkEngine *m_dspDeviceSinkEngine;

    void addEngine();
    void addDevice();
    void addDeviceUI();
};

class AddSampleMIMOFSM : public MainWindowFSM {
    Q_OBJECT

public:

    AddSampleMIMOFSM(MainWindow *mainWindow, Workspace *deviceWorkspace, Workspace *spectrumWorkspace, int deviceIndex, bool loadDefaults, QObject *parent=nullptr);

private:

    Workspace *m_deviceWorkspace;
    Workspace *m_spectrumWorkspace;
    int m_deviceIndex;
    bool m_loadDefaults;

    int m_deviceSetIndex;
    DeviceAPI *m_deviceAPI;
    DeviceUISet *m_deviceUISet;

    DSPDeviceMIMOEngine *m_dspDeviceMIMOEngine;

    void addEngine();
    void addDevice();
    void addDeviceUI();
};

class RemoveDeviceSetFSM : public MainWindowFSM {
    Q_OBJECT

public:
    RemoveDeviceSetFSM(MainWindow *mainWindow, int deviceSetIndex, QObject *parent=nullptr);

private:
    int m_deviceSetIndex;
    DeviceUISet *m_deviceUISet;
    DSPDeviceSourceEngine *m_deviceSourceEngine;
    DSPDeviceSinkEngine *m_deviceSinkEngine;
    DSPDeviceMIMOEngine *m_deviceMIMOEngine;
    QSignalTransition *m_t1;
    QSignalTransition *m_t2;

    void stopAcquisition();
    void removeSink();
    void removeUI();
    void stopEngine();
    void removeDeviceSet();
};

class RemoveAllDeviceSetsFSM : public MainWindowFSM {
    Q_OBJECT

public:
    RemoveAllDeviceSetsFSM(MainWindow *mainWindow, QObject *parent=nullptr);

private:

    void removeNext();

};

class RemoveAllWorkspacesFSM : public MainWindowFSM {
    Q_OBJECT

public:
    RemoveAllWorkspacesFSM(MainWindow *mainWindow, QObject *parent=nullptr);

private:

    RemoveAllDeviceSetsFSM *m_removeAllDeviceSetsFSM;

    void removeDeviceSets();
    void removeWorkspaces();

};

class LoadConfigurationFSM : public MainWindowFSM {
    Q_OBJECT

public:
    LoadConfigurationFSM(MainWindow *mainWindow, const Configuration *configuration, QProgressDialog *waitBox, QObject *parent=nullptr);

private:

    const Configuration *m_configuration;
    QProgressDialog *m_waitBox;

    RemoveAllWorkspacesFSM *m_removeAllWorkspacesFSM;

    void clearWorkspace();
    void createWorkspaces();
    void loadDeviceSets();
    void loadDeviceSetSettings();
    void loadFeatureSets();
    void restoreGeometry();
};

class CloseFSM : public MainWindowFSM {
    Q_OBJECT

public:
    CloseFSM(MainWindow *mainWindow, QObject *parent=nullptr);

private:
    void on_started();
    void on_finished();
};

class InitFSM : public MainWindowFSM {
    Q_OBJECT

public:
    InitFSM(MainWindow *mainWindow, SDRangelSplash *splash, bool loadDefault, QObject *parent=nullptr);

private:
    SDRangelSplash *m_splash;
    LoadConfigurationFSM *m_loadConfigurationFSM;

    void loadDefaultConfiguration();
    void showDefaultConfigurations();
};


class SDRGUI_API MainWindow : public QMainWindow {
	Q_OBJECT

    friend InitFSM;
    friend AddSampleSourceFSM;
    friend AddSampleSinkFSM;
    friend AddSampleMIMOFSM;
    friend LoadConfigurationFSM;
    friend RemoveDeviceSetFSM;
    friend RemoveAllDeviceSetsFSM;
    friend RemoveAllWorkspacesFSM;
    friend CloseFSM;

public:
	explicit MainWindow(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QWidget* parent = nullptr);
	~MainWindow() final;
	static MainWindow *getInstance() { return m_instance; } // Main Window is de facto a singleton so this just returns its reference
	MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }
    const PluginManager *getPluginManager() const { return m_pluginManager; }
    std::vector<DeviceUISet*>& getDeviceUISets() { return m_deviceUIs; }
    void commandKeysConnect(const QObject *object, const char *slot);
    void commandKeysDisconnect(const QObject *object, const char *slot) const;
    int getNumberOfWorkspaces() const { return m_workspaces.size(); }

public slots:
    void channelMove(ChannelGUI *gui, int wsIndexDestination);
    void channelDuplicate(const ChannelGUI *gui);
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

#if QT_CONFIG(process)
	QProcess *m_fftWisdomProcess;
#endif

    bool m_settingsSaved;               // Records if settings have already been saved in response to a QCloseEvent

	void loadSettings();
	void loadDeviceSetPresetSettings(const Preset* preset, int deviceSetIndex);
	void saveDeviceSetPresetSettings(Preset* preset, int deviceSetIndex);
	void loadFeatureSetPresetSettings(const FeatureSetPreset* preset, int featureSetIndex, Workspace *workspace);
	void saveFeatureSetPresetSettings(FeatureSetPreset* preset, int featureSetIndex);

	QString openGLVersion() const;
    void createMenuBar(QToolButton *button) const;
	void createStatusBar();
	void closeEvent(QCloseEvent*) final;
	void applySettings();

    void removeDeviceSet(int deviceSetIndex);
    void removeLastDeviceSet();
    void removeAllDeviceSets();
    void addFeatureSet();
    void removeFeatureSet(unsigned int featureSetIndex);
    void removeAllFeatureSets();
    void deleteChannel(int deviceSetIndex, int channelIndex);
    void channelDuplicateToDeviceSet(const ChannelGUI *sourceChannelGUI, int dsIndexDestination);
    void sampleDeviceChange(int deviceType, int deviceSetIndex, int newDeviceIndex, Workspace *workspace);
    void sampleSourceChange(int deviceSetIndex, int newDeviceIndex, Workspace *workspace);
	void sampleSinkChange(int deviceSetIndex, int newDeviceIndex, Workspace *workspace);
	void sampleMIMOChange(int deviceSetIndex, int newDeviceIndex, Workspace *workspace);
    void sampleSourceCreate(
        int deviceSetIndex,
        int& deviceIndex,
        DeviceUISet *deviceUISet
    );
    void sampleSourceCreateUI(
        int deviceSetIndex,
        int deviceIndex,
        DeviceUISet *deviceUISet
    );
    void sampleSinkCreate(
        int deviceSetIndex,
        int& deviceIndex,
        DeviceUISet *deviceUISet
    );
    void sampleSinkCreateUI(
        int deviceSetIndex,
        int deviceIndex,
        DeviceUISet *deviceUISet
    );
    void sampleMIMOCreate(
        int deviceSetIndex,
        int& deviceIndex,
        DeviceUISet *deviceUISet
    );
     void sampleMIMOCreateUI(
        int deviceSetIndex,
        int deviceIndex,
        DeviceUISet *deviceUISet
    );
    void deleteFeature(int featureSetIndex, int featureIndex);
    void loadDefaultPreset(const QString& pluginId, SerializableInterface *serializableInterface);

    bool handleMessage(const Message& cmd);

protected:
    void keyPressEvent(QKeyEvent* event) override;

signals:
    // For internal FSM usage
    void allDeviceSetsRemoved();
    void allDeviceSetsAdded();
    void engineStopped();

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
#if QT_CONFIG(process)
	void on_action_FFTWisdom_triggered();
    void on_action_commands_triggered();
#endif
	void on_action_My_Position_triggered();
    void on_action_DeviceUserArguments_triggered();
    void on_action_Welcome_triggered();
    void on_action_Quick_Start_triggered() const;
    void on_action_Main_Window_triggered() const;
	void on_action_Loaded_Plugins_triggered();
	void on_action_About_triggered();

	void updateStatus();
    void addWorkspace();
    void viewAllWorkspaces() const;
    void removeEmptyWorkspaces();
    void openConfigurationDialog(bool openOnly);
    void loadDefaultConfigurations() const;
	void loadConfiguration(const Configuration *configuration, bool fromDialog = false);
    void saveConfiguration(Configuration *configuration);
	void sampleSourceAdd(Workspace *deviceWorkspace, Workspace *spectrumWorkspace, int deviceIndex);
	void sampleSinkAdd(Workspace *workspace, Workspace *spectrumWorkspace, int deviceIndex);
	void sampleMIMOAdd(Workspace *workspace, Workspace *spectrumWorkspace, int deviceIndex);
    void samplingDeviceChangeHandler(const DeviceGUI *deviceGUI, int newDeviceIndex);
    void channelAddClicked(Workspace *workspace, int deviceSetIndex, int channelPluginIndex);
    void featureAddClicked(Workspace *workspace, int featureIndex);
    void featureMove(FeatureGUI *gui, int wsIndexDestnation);
    void deviceStateChanged(DeviceAPI *deviceAPI);
    void openFeaturePresetsDialog(QPoint p, Workspace *workspace);
    void startAllDevices(const Workspace *workspace) const;
    void stopAllDevices(const Workspace *workspace) const;
    void deviceMove(DeviceGUI *gui, int wsIndexDestnation);
    void mainSpectrumMove(MainSpectrumGUI *gui, int wsIndexDestnation);
    void mainSpectrumShow(int deviceSetIndex);
    void mainSpectrumRequestDeviceCenterFrequency(int deviceSetIndex, qint64 deviceCenterFrequency);
    void showAllChannels(int deviceSetIndex);
    void openDeviceSetPresetsDialog(QPoint p, const DeviceGUI *deviceGUI);
	void commandKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release) const;
#if QT_CONFIG(process)
	void fftWisdomProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
#endif
    void orientationChanged(Qt::ScreenOrientation orientation);
};

#endif // INCLUDE_MAINWINDOW_H
