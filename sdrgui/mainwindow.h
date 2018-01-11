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
#include "util/message.h"
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
class Preset;
class Command;
class CommandKeyReceiver;

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
    class MsgLoadPreset : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const Preset *getPreset() const { return m_preset; }
        int getDeviceSetIndex() const { return m_deviceSetIndex; }

        static MsgLoadPreset* create(const Preset *preset, int deviceSetIndex)
        {
            return new MsgLoadPreset(preset, deviceSetIndex);
        }

    private:
        const Preset *m_preset;
        int m_deviceSetIndex;

        MsgLoadPreset(const Preset *preset, int deviceSetIndex) :
            Message(),
            m_preset(preset),
            m_deviceSetIndex(deviceSetIndex)
        { }
    };

    class MsgSavePreset : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        Preset *getPreset() const { return m_preset; }
        int getDeviceSetIndex() const { return m_deviceSetIndex; }
        bool isNewPreset() const { return m_newPreset; }

        static MsgSavePreset* create(Preset *preset, int deviceSetIndex, bool newPreset)
        {
            return new MsgSavePreset(preset, deviceSetIndex, newPreset);
        }

    private:
        Preset *m_preset;
        int m_deviceSetIndex;
        bool m_newPreset;

        MsgSavePreset(Preset *preset, int deviceSetIndex, bool newPreset) :
            Message(),
            m_preset(preset),
            m_deviceSetIndex(deviceSetIndex),
            m_newPreset(newPreset)
        { }
    };

    class MsgDeletePreset : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const Preset *getPreset() const { return m_preset; }

        static MsgDeletePreset* create(const Preset *preset)
        {
            return new MsgDeletePreset(preset);
        }

    private:
        const Preset *m_preset;

        MsgDeletePreset(const Preset *preset) :
            Message(),
            m_preset(preset)
        { }
    };

    class MsgAddDeviceSet : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool isTx() const { return m_tx; }

        static MsgAddDeviceSet* create(bool tx)
        {
            return new MsgAddDeviceSet(tx);
        }

    private:
        bool m_tx;

        MsgAddDeviceSet(bool tx) :
            Message(),
            m_tx(tx)
        { }
    };

    class MsgRemoveLastDeviceSet : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgRemoveLastDeviceSet* create()
        {
            return new MsgRemoveLastDeviceSet();
        }

    private:
        MsgRemoveLastDeviceSet() :
            Message()
        { }
    };

    class MsgSetDevice : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getDeviceSetIndex() const { return m_deviceSetIndex; }
        int getDeviceIndex() const { return m_deviceIndex; }
        bool isTx() const { return m_tx; }

        static MsgSetDevice* create(int deviceSetIndex, int deviceIndex, bool tx)
        {
            return new MsgSetDevice(deviceSetIndex, deviceIndex, tx);
        }

    private:
        int m_deviceSetIndex;
        int m_deviceIndex;
        bool m_tx;

        MsgSetDevice(int deviceSetIndex, int deviceIndex, bool tx) :
            Message(),
            m_deviceSetIndex(deviceSetIndex),
            m_deviceIndex(deviceIndex),
            m_tx(tx)
        { }
    };

    class MsgAddChannel : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getDeviceSetIndex() const { return m_deviceSetIndex; }
        int getChannelRegistrationIndex() const { return m_channelRegistrationIndex; }
        bool isTx() const { return m_tx; }

        static MsgAddChannel* create(int deviceSetIndex, int channelRegistrationIndex, bool tx)
        {
            return new MsgAddChannel(deviceSetIndex, channelRegistrationIndex, tx);
        }

    private:
        int m_deviceSetIndex;
        int m_channelRegistrationIndex;
        bool m_tx;

        MsgAddChannel(int deviceSetIndex, int channelRegistrationIndex, bool tx) :
            Message(),
            m_deviceSetIndex(deviceSetIndex),
            m_channelRegistrationIndex(channelRegistrationIndex),
            m_tx(tx)
        { }
    };

    class MsgDeleteChannel : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getDeviceSetIndex() const { return m_deviceSetIndex; }
        int getChannelIndex() const { return m_channelIndex; }
        bool isTx() const { return m_tx; }

        static MsgDeleteChannel* create(int deviceSetIndex, int channelIndex, bool tx)
        {
            return new MsgDeleteChannel(deviceSetIndex, channelIndex, tx);
        }

    private:
        int m_deviceSetIndex;
        int m_channelIndex;
        bool m_tx;

        MsgDeleteChannel(int deviceSetIndex, int channelIndex, bool tx) :
            Message(),
            m_deviceSetIndex(deviceSetIndex),
            m_channelIndex(channelIndex),
            m_tx(tx)
        { }
    };

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

	class MsgDeviceSetFocus : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getDeviceSetIndex() const { return m_deviceSetIndex; }

        static MsgDeviceSetFocus* create(int deviceSetIndex)
        {
            return new MsgDeviceSetFocus(deviceSetIndex);
        }

    private:
        int m_deviceSetIndex;

        MsgDeviceSetFocus(int deviceSetIndex) :
            Message(),
            m_deviceSetIndex(deviceSetIndex)
        { }
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
	QString m_apiHost;
	int m_apiPort;

	CommandKeyReceiver *m_commandKeyReceiver;

	void loadSettings();
	void loadPresetSettings(const Preset* preset, int tabIndex);
	void savePresetSettings(Preset* preset, int tabIndex);
	void saveCommandSettings();

	void createStatusBar();
	void closeEvent(QCloseEvent*);
	void updatePresetControls();
	QTreeWidgetItem* addPresetToTree(const Preset* preset);
	QTreeWidgetItem* addCommandToTree(const Command* command);
	void applySettings();

	void addSourceDevice(int deviceIndex);
	void addSinkDevice();
    void removeLastDevice();
    void deleteChannel(int deviceSetIndex, int channelIndex);

    void setLoggingOptions();

    bool handleMessage(const Message& cmd);

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
	void on_action_DV_Serial_triggered(bool checked);
	void on_action_My_Position_triggered();
	void sampleSourceChanged();
	void sampleSinkChanged();
    void channelAddClicked(bool checked);
	void on_action_Loaded_Plugins_triggered();
	void on_action_About_triggered();
	void on_action_addSourceDevice_triggered();
	void on_action_addSinkDevice_triggered();
	void on_action_removeLastDevice_triggered();
	void on_action_Exit_triggered();
	void tabInputViewIndexChanged();
	void focusHasChanged(QWidget *oldWidget, QWidget *newWidget);
	void commandKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release);
};

#endif // INCLUDE_MAINWINDOW_H
