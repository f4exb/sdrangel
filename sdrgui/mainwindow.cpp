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

#include <gui/commandkeyreceiver.h>
#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QSysInfo>
#include <QKeyEvent>

#include <plugin/plugininstancegui.h>
#include <plugin/plugininstancegui.h>
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "device/deviceenumerator.h"
#include "audio/audiodeviceinfo.h"
#include "gui/indicator.h"
#include "gui/presetitem.h"
#include "gui/commanditem.h"
#include "gui/addpresetdialog.h"
#include "gui/editcommanddialog.h"
#include "gui/commandoutputdialog.h"
#include "gui/pluginsdialog.h"
#include "gui/aboutdialog.h"
#include "gui/rollupwidget.h"
#include "gui/channelwindow.h"
#include "gui/audiodialog.h"
#include "gui/loggingdialog.h"
#include "gui/samplingdevicecontrol.h"
#include "gui/mypositiondialog.h"
#include "dsp/dspengine.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "plugin/pluginapi.h"
#include "gui/glspectrum.h"
#include "gui/glspectrumgui.h"
#include "loggerwithfile.h"
#include "webapi/webapirequestmapper.h"
#include "webapi/webapiserver.h"
#include "webapi/webapiadaptergui.h"
#include "commands/command.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <string>
#include <QDebug>

MESSAGE_CLASS_DEFINITION(MainWindow::MsgLoadPreset, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgSavePreset, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgDeletePreset, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgAddDeviceSet, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgRemoveLastDeviceSet, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgSetDevice, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgAddChannel, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgDeleteChannel, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgDeviceSetFocus, Message)

MainWindow *MainWindow::m_instance = 0;

MainWindow::MainWindow(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QWidget* parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow),
	m_settings(),
    m_masterTabIndex(0),
	m_dspEngine(DSPEngine::instance()),
	m_lastEngineState((DSPDeviceSourceEngine::State)-1),
	m_inputGUI(0),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_sampleFileName(std::string("./test.sdriq")),
	m_logger(logger)
{
	qDebug() << "MainWindow::MainWindow: start";

    m_instance = this;
	m_settings.setAudioDeviceInfo(&m_audioDeviceInfo);

	ui->setupUi(this);
	createStatusBar();

	setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

	// work around broken Qt dock widget ordering
    removeDockWidget(ui->inputViewDock);
	removeDockWidget(ui->inputSelectDock);
	removeDockWidget(ui->spectraDisplayDock);
	removeDockWidget(ui->presetDock);
    removeDockWidget(ui->commandsDock);
	removeDockWidget(ui->channelDock);
    addDockWidget(Qt::LeftDockWidgetArea, ui->inputViewDock);
	addDockWidget(Qt::LeftDockWidgetArea, ui->inputSelectDock);
	addDockWidget(Qt::LeftDockWidgetArea, ui->spectraDisplayDock);
	addDockWidget(Qt::LeftDockWidgetArea, ui->presetDock);
    addDockWidget(Qt::LeftDockWidgetArea, ui->commandsDock);
    tabifyDockWidget(ui->presetDock, ui->commandsDock);
	addDockWidget(Qt::RightDockWidgetArea, ui->channelDock);

	ui->inputViewDock->show();
    ui->inputSelectDock->show();
	ui->spectraDisplayDock->show();
	ui->presetDock->show();
	ui->commandsDock->show();
	ui->channelDock->show();

    ui->menu_Window->addAction(ui->inputViewDock->toggleViewAction());
	ui->menu_Window->addAction(ui->inputSelectDock->toggleViewAction());
	ui->menu_Window->addAction(ui->spectraDisplayDock->toggleViewAction());
	ui->menu_Window->addAction(ui->presetDock->toggleViewAction());
    ui->menu_Window->addAction(ui->commandsDock->toggleViewAction());
	ui->menu_Window->addAction(ui->channelDock->toggleViewAction());

    ui->tabInputsView->setStyleSheet("QWidget { background: rgb(50,50,50); } "
            "QToolButton::checked { background: rgb(128,70,0); } "
            "QComboBox::item:selected { color: rgb(255,140,0); } "
            "QTabWidget::pane { border: 1px solid #C06900; } "
            "QTabBar::tab:selected { background: rgb(128,70,0); }");
    ui->tabInputsSelect->setStyleSheet("QWidget { background: rgb(50,50,50); } "
            "QToolButton::checked { background: rgb(128,70,0); } "
            "QComboBox::item:selected { color: rgb(255,140,0); } "
            "QTabWidget::pane { border: 1px solid #808080; } "
            "QTabBar::tab:selected { background: rgb(100,100,100); }");

    m_pluginManager = new PluginManager(this);
    m_pluginManager->loadPlugins(QString("plugins"));

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()), Qt::QueuedConnection);

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(1000);

	m_masterTimer.start(50);

    qDebug() << "MainWindow::MainWindow: load settings...";

	loadSettings();

	qDebug() << "MainWindow::MainWindow: select SampleSource from settings or default (file source) ...";

	int deviceIndex = DeviceEnumerator::instance()->getRxSamplingDeviceIndex(m_settings.getSourceDeviceId(), m_settings.getSourceIndex());
	addSourceDevice(deviceIndex);  // add the first device set with file source device as default if device in settings is not enumerated
	m_deviceUIs.back()->m_deviceSourceAPI->setBuddyLeader(true); // the first device is always the leader

	qDebug() << "MainWindow::MainWindow: load current preset settings...";

	loadPresetSettings(m_settings.getWorkingPreset(), 0);

	qDebug() << "MainWindow::MainWindow: apply settings...";

	applySettings();

	qDebug() << "MainWindow::MainWindow: update preset controls...";

	updatePresetControls();

	connect(ui->tabInputsView, SIGNAL(currentChanged(int)), this, SLOT(tabInputViewIndexChanged()));

	m_apiAdapter = new WebAPIAdapterGUI(*this);
	m_requestMapper = new WebAPIRequestMapper(this);
	m_requestMapper->setAdapter(m_apiAdapter);
	m_apiHost = parser.getServerAddress();
	m_apiPort = parser.getServerPort();
	m_apiServer = new WebAPIServer(m_apiHost, m_apiPort, m_requestMapper);
	m_apiServer->start();

	connect(qApp, SIGNAL(focusChanged(QWidget *, QWidget *)), this, SLOT(focusHasChanged(QWidget *, QWidget *)));
	m_commandKeyReceiver = new CommandKeyReceiver();
	m_commandKeyReceiver->setRelease(true);
	this->installEventFilter(m_commandKeyReceiver);

    qDebug() << "MainWindow::MainWindow: end";
}

MainWindow::~MainWindow()
{
    m_commandKeyReceiver->deleteLater();

    m_apiServer->stop();
    delete m_apiServer;
    delete m_requestMapper;
    delete m_apiAdapter;

    delete m_pluginManager;
	delete m_dateTimeWidget;
	delete m_showSystemWidget;

	delete ui;

	qDebug() << "MainWindow::~MainWindow: end";
	delete m_logger;
}

void MainWindow::addSourceDevice(int deviceIndex)
{
    DSPDeviceSourceEngine *dspDeviceSourceEngine = m_dspEngine->addDeviceSourceEngine();
    dspDeviceSourceEngine->start();

    uint dspDeviceSourceEngineUID =  dspDeviceSourceEngine->getUID();
    char uidCStr[16];
    sprintf(uidCStr, "UID:%d", dspDeviceSourceEngineUID);

    int deviceTabIndex = m_deviceUIs.size();
    m_deviceUIs.push_back(new DeviceUISet(deviceTabIndex, true, m_masterTimer));
    m_deviceUIs.back()->m_deviceSourceEngine = dspDeviceSourceEngine;

    char tabNameCStr[16];
    sprintf(tabNameCStr, "R%d", deviceTabIndex);

    DeviceSourceAPI *deviceSourceAPI = new DeviceSourceAPI(deviceTabIndex, dspDeviceSourceEngine);

    m_deviceUIs.back()->m_deviceSourceAPI = deviceSourceAPI;
    m_deviceUIs.back()->m_samplingDeviceControl->setPluginManager(m_pluginManager);
    QList<QString> channelNames;
    m_pluginManager->listRxChannels(channelNames);
    QStringList channelNamesList(channelNames);
    m_deviceUIs.back()->m_samplingDeviceControl->getChannelSelector()->addItems(channelNamesList);

    connect(m_deviceUIs.back()->m_samplingDeviceControl->getAddChannelButton(), SIGNAL(clicked(bool)), this, SLOT(channelAddClicked(bool)));

    dspDeviceSourceEngine->addSink(m_deviceUIs.back()->m_spectrumVis);
    ui->tabSpectra->addTab(m_deviceUIs.back()->m_spectrum, tabNameCStr);
    ui->tabSpectraGUI->addTab(m_deviceUIs.back()->m_spectrumGUI, tabNameCStr);
    ui->tabChannels->addTab(m_deviceUIs.back()->m_channelWindow, tabNameCStr);

    connect(m_deviceUIs.back()->m_samplingDeviceControl, SIGNAL(changed()), this, SLOT(sampleSourceChanged()));

    ui->tabInputsSelect->addTab(m_deviceUIs.back()->m_samplingDeviceControl, tabNameCStr);
    ui->tabInputsSelect->setTabToolTip(deviceTabIndex, QString(uidCStr));

    // Create a file source instance by default if requested device was not enumerated (index = -1)
    if (deviceIndex < 0) {
        deviceIndex = DeviceEnumerator::instance()->getFileSourceDeviceIndex();
    }

    PluginInterface::SamplingDevice samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(deviceIndex);
    m_deviceUIs.back()->m_deviceSourceAPI->setSampleSourceSequence(samplingDevice.sequence);
    m_deviceUIs.back()->m_deviceSourceAPI->setNbItems(samplingDevice.deviceNbItems);
    m_deviceUIs.back()->m_deviceSourceAPI->setItemIndex(samplingDevice.deviceItemIndex);
    m_deviceUIs.back()->m_deviceSourceAPI->setHardwareId(samplingDevice.hardwareId);
    m_deviceUIs.back()->m_deviceSourceAPI->setSampleSourceId(samplingDevice.id);
    m_deviceUIs.back()->m_deviceSourceAPI->setSampleSourceSerial(samplingDevice.serial);
    m_deviceUIs.back()->m_deviceSourceAPI->setSampleSourceDisplayName(samplingDevice.displayedName);
    m_deviceUIs.back()->m_deviceSourceAPI->setSampleSourcePluginInterface(DeviceEnumerator::instance()->getRxPluginInterface(deviceIndex));

    m_deviceUIs.back()->m_samplingDeviceControl->setSelectedDeviceIndex(deviceIndex);

    // delete previous plugin GUI
    m_deviceUIs.back()->m_deviceSourceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceGUI(
            m_deviceUIs.back()->m_deviceSourceAPI->getSampleSourcePluginInstanceGUI());


    DeviceSampleSource *source = m_deviceUIs.back()->m_deviceSourceAPI->getPluginInterface()->createSampleSourcePluginInstanceInput(
            m_deviceUIs.back()->m_deviceSourceAPI->getSampleSourceId(), m_deviceUIs.back()->m_deviceSourceAPI);
    m_deviceUIs.back()->m_deviceSourceAPI->setSampleSource(source);
    QWidget *gui;
    PluginInstanceGUI *pluginGUI = m_deviceUIs.back()->m_deviceSourceAPI->getPluginInterface()->createSampleSourcePluginInstanceGUI(
            m_deviceUIs.back()->m_deviceSourceAPI->getSampleSourceId(),
            &gui,
            m_deviceUIs.back());
    m_deviceUIs.back()->m_deviceSourceAPI->getSampleSource()->setMessageQueueToGUI(pluginGUI->getInputMessageQueue());
    m_deviceUIs.back()->m_deviceSourceAPI->setSampleSourcePluginInstanceGUI(pluginGUI);
    m_deviceUIs.back()->m_deviceSourceAPI->getSampleSource()->init();
    setDeviceGUI(deviceTabIndex, gui, m_deviceUIs.back()->m_deviceSourceAPI->getSampleSourceDisplayName());
}

void MainWindow::addSinkDevice()
{
    DSPDeviceSinkEngine *dspDeviceSinkEngine = m_dspEngine->addDeviceSinkEngine();
    dspDeviceSinkEngine->start();

    uint dspDeviceSinkEngineUID =  dspDeviceSinkEngine->getUID();
    char uidCStr[16];
    sprintf(uidCStr, "UID:%d", dspDeviceSinkEngineUID);

    int deviceTabIndex = m_deviceUIs.size();
    m_deviceUIs.push_back(new DeviceUISet(deviceTabIndex, false, m_masterTimer));
    m_deviceUIs.back()->m_deviceSourceEngine = 0;
    m_deviceUIs.back()->m_deviceSinkEngine = dspDeviceSinkEngine;

    char tabNameCStr[16];
    sprintf(tabNameCStr, "T%d", deviceTabIndex);

    DeviceSinkAPI *deviceSinkAPI = new DeviceSinkAPI(deviceTabIndex, dspDeviceSinkEngine);

    m_deviceUIs.back()->m_deviceSourceAPI = 0;
    m_deviceUIs.back()->m_deviceSinkAPI = deviceSinkAPI;
    m_deviceUIs.back()->m_samplingDeviceControl->setPluginManager(m_pluginManager);
    QList<QString> channelNames;
    m_pluginManager->listTxChannels(channelNames);
    QStringList channelNamesList(channelNames);
    m_deviceUIs.back()->m_samplingDeviceControl->getChannelSelector()->addItems(channelNamesList);

    connect(m_deviceUIs.back()->m_samplingDeviceControl->getAddChannelButton(), SIGNAL(clicked(bool)), this, SLOT(channelAddClicked(bool)));

    dspDeviceSinkEngine->addSpectrumSink(m_deviceUIs.back()->m_spectrumVis);
    ui->tabSpectra->addTab(m_deviceUIs.back()->m_spectrum, tabNameCStr);
    ui->tabSpectraGUI->addTab(m_deviceUIs.back()->m_spectrumGUI, tabNameCStr);
    ui->tabChannels->addTab(m_deviceUIs.back()->m_channelWindow, tabNameCStr);

    connect(m_deviceUIs.back()->m_samplingDeviceControl, SIGNAL(changed()), this, SLOT(sampleSinkChanged()));

    ui->tabInputsSelect->addTab(m_deviceUIs.back()->m_samplingDeviceControl, tabNameCStr);
    ui->tabInputsSelect->setTabToolTip(deviceTabIndex, QString(uidCStr));

    // create a file sink by default
    int fileSinkDeviceIndex = DeviceEnumerator::instance()->getFileSinkDeviceIndex();
    PluginInterface::SamplingDevice samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(fileSinkDeviceIndex);
    m_deviceUIs.back()->m_deviceSinkAPI->setSampleSinkSequence(samplingDevice.sequence);
    m_deviceUIs.back()->m_deviceSinkAPI->setNbItems(samplingDevice.deviceNbItems);
    m_deviceUIs.back()->m_deviceSinkAPI->setItemIndex(samplingDevice.deviceItemIndex);
    m_deviceUIs.back()->m_deviceSinkAPI->setHardwareId(samplingDevice.hardwareId);
    m_deviceUIs.back()->m_deviceSinkAPI->setSampleSinkId(samplingDevice.id);
    m_deviceUIs.back()->m_deviceSinkAPI->setSampleSinkSerial(samplingDevice.serial);
    m_deviceUIs.back()->m_deviceSinkAPI->setSampleSinkDisplayName(samplingDevice.displayedName);
    m_deviceUIs.back()->m_deviceSinkAPI->setSampleSinkPluginInterface(DeviceEnumerator::instance()->getTxPluginInterface(fileSinkDeviceIndex));

    m_deviceUIs.back()->m_samplingDeviceControl->setSelectedDeviceIndex(fileSinkDeviceIndex);

    // delete previous plugin GUI if it exists
    m_deviceUIs.back()->m_deviceSinkAPI->getPluginInterface()->deleteSampleSourcePluginInstanceGUI(
            m_deviceUIs.back()->m_deviceSinkAPI->getSampleSinkPluginInstanceGUI());

    DeviceSampleSink *sink = m_deviceUIs.back()->m_deviceSinkAPI->getPluginInterface()->createSampleSinkPluginInstanceOutput(
            m_deviceUIs.back()->m_deviceSinkAPI->getSampleSinkId(), m_deviceUIs.back()->m_deviceSinkAPI);
    m_deviceUIs.back()->m_deviceSinkAPI->setSampleSink(sink);
    QWidget *gui;
    PluginInstanceGUI *pluginUI = m_deviceUIs.back()->m_deviceSinkAPI->getPluginInterface()->createSampleSinkPluginInstanceGUI(
            m_deviceUIs.back()->m_deviceSinkAPI->getSampleSinkId(),
            &gui,
            m_deviceUIs.back());
    m_deviceUIs.back()->m_deviceSinkAPI->getSampleSink()->setMessageQueueToGUI(pluginUI->getInputMessageQueue());
    m_deviceUIs.back()->m_deviceSinkAPI->setSampleSinkPluginInstanceUI(pluginUI);
    m_deviceUIs.back()->m_deviceSinkAPI->getSampleSink()->init();
    setDeviceGUI(deviceTabIndex, gui, m_deviceUIs.back()->m_deviceSinkAPI->getSampleSinkDisplayName(), false);
}

void MainWindow::removeLastDevice()
{
	if (m_deviceUIs.back()->m_deviceSourceEngine) // source tab
	{
	    DSPDeviceSourceEngine *lastDeviceEngine = m_deviceUIs.back()->m_deviceSourceEngine;
	    lastDeviceEngine->stopAcquistion();
	    lastDeviceEngine->removeSink(m_deviceUIs.back()->m_spectrumVis);

	    ui->tabSpectraGUI->removeTab(ui->tabSpectraGUI->count() - 1);
	    ui->tabSpectra->removeTab(ui->tabSpectra->count() - 1);

        // deletes old UI and input object
        m_deviceUIs.back()->freeRxChannels();      // destroys the channel instances
        m_deviceUIs.back()->m_deviceSourceAPI->getSampleSource()->setMessageQueueToGUI(0); // have source stop sending messages to the GUI
        m_deviceUIs.back()->m_deviceSourceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceGUI(
                m_deviceUIs.back()->m_deviceSourceAPI->getSampleSourcePluginInstanceGUI());
        m_deviceUIs.back()->m_deviceSourceAPI->resetSampleSourceId();
        m_deviceUIs.back()->m_deviceSourceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceInput(
                m_deviceUIs.back()->m_deviceSourceAPI->getSampleSource());
        m_deviceUIs.back()->m_deviceSourceAPI->clearBuddiesLists(); // clear old API buddies lists
        m_deviceUIs.back()->m_samplingDeviceControl->removeSelectedDeviceIndex(); // This releases the device in the device list

	    ui->tabChannels->removeTab(ui->tabChannels->count() - 1);

	    ui->tabInputsSelect->removeTab(ui->tabInputsSelect->count() - 1);

	    m_deviceWidgetTabs.removeLast();
	    ui->tabInputsView->clear();

	    for (int i = 0; i < m_deviceWidgetTabs.size(); i++)
	    {
	        qDebug("MainWindow::removeLastDevice: adding back tab for %s", m_deviceWidgetTabs[i].displayName.toStdString().c_str());
	        ui->tabInputsView->addTab(m_deviceWidgetTabs[i].gui, m_deviceWidgetTabs[i].tabName);
	        ui->tabInputsView->setTabToolTip(i, m_deviceWidgetTabs[i].displayName);
	    }

	    delete m_deviceUIs.back();

	    lastDeviceEngine->stop();
	    m_dspEngine->removeLastDeviceSourceEngine();
	}
	else if (m_deviceUIs.back()->m_deviceSinkEngine) // sink tab
	{
	    DSPDeviceSinkEngine *lastDeviceEngine = m_deviceUIs.back()->m_deviceSinkEngine;
	    lastDeviceEngine->stopGeneration();
	    lastDeviceEngine->removeSpectrumSink(m_deviceUIs.back()->m_spectrumVis);

	    ui->tabSpectraGUI->removeTab(ui->tabSpectraGUI->count() - 1);
	    ui->tabSpectra->removeTab(ui->tabSpectra->count() - 1);

        // deletes old UI and output object
        m_deviceUIs.back()->freeTxChannels();
        m_deviceUIs.back()->m_deviceSinkAPI->getSampleSink()->setMessageQueueToGUI(0); // have sink stop sending messages to the GUI
	    m_deviceUIs.back()->m_deviceSinkAPI->getPluginInterface()->deleteSampleSourcePluginInstanceGUI(
	            m_deviceUIs.back()->m_deviceSinkAPI->getSampleSinkPluginInstanceGUI());
	    m_deviceUIs.back()->m_deviceSinkAPI->resetSampleSinkId();
	    m_deviceUIs.back()->m_deviceSinkAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput(
	            m_deviceUIs.back()->m_deviceSinkAPI->getSampleSink());
        m_deviceUIs.back()->m_deviceSinkAPI->clearBuddiesLists(); // clear old API buddies lists
        m_deviceUIs.back()->m_samplingDeviceControl->removeSelectedDeviceIndex(); // This releases the device in the device list

	    ui->tabChannels->removeTab(ui->tabChannels->count() - 1);

	    ui->tabInputsSelect->removeTab(ui->tabInputsSelect->count() - 1);

	    m_deviceWidgetTabs.removeLast();
	    ui->tabInputsView->clear();

	    for (int i = 0; i < m_deviceWidgetTabs.size(); i++)
	    {
	        qDebug("MainWindow::removeLastDevice: adding back tab for %s", m_deviceWidgetTabs[i].displayName.toStdString().c_str());
	        ui->tabInputsView->addTab(m_deviceWidgetTabs[i].gui, m_deviceWidgetTabs[i].tabName);
	        ui->tabInputsView->setTabToolTip(i, m_deviceWidgetTabs[i].displayName);
	    }

	    delete m_deviceUIs.back();

	    lastDeviceEngine->stop();
	    m_dspEngine->removeLastDeviceSinkEngine();
	}

    m_deviceUIs.pop_back();
}

void MainWindow::deleteChannel(int deviceSetIndex, int channelIndex)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_deviceUIs[deviceSetIndex];

        if (deviceSet->m_deviceSourceEngine) // source device => Rx channels
        {
            deviceSet->deleteRxChannel(channelIndex);
        }
        else if (deviceSet->m_deviceSinkEngine) // sink device => Tx channels
        {
            deviceSet->deleteTxChannel(channelIndex);
        }
    }
}

void MainWindow::addChannelRollup(int deviceTabIndex, QWidget* widget)
{
    if (deviceTabIndex < ui->tabInputsView->count())
    {
        DeviceUISet *deviceUI = m_deviceUIs[deviceTabIndex];
        deviceUI->m_channelWindow->addRollupWidget(widget);
        ui->channelDock->show();
        ui->channelDock->raise();
    }
}

void MainWindow::addViewAction(QAction* action)
{
	ui->menu_Window->addAction(action);
}

void MainWindow::setDeviceGUI(int deviceTabIndex, QWidget* gui, const QString& deviceDisplayName, bool sourceDevice)
{
    char tabNameCStr[16];

    if (sourceDevice)
    {
        sprintf(tabNameCStr, "R%d", deviceTabIndex);
    }
    else
    {
        sprintf(tabNameCStr, "T%d", deviceTabIndex);
    }

    qDebug("MainWindow::setDeviceGUI: insert %s tab at %d", sourceDevice ? "Rx" : "Tx", deviceTabIndex);

    if (deviceTabIndex < m_deviceWidgetTabs.size())
    {
        m_deviceWidgetTabs[deviceTabIndex] = {gui, deviceDisplayName, QString(tabNameCStr)};
    }
    else
    {
        m_deviceWidgetTabs.append({gui, deviceDisplayName, QString(tabNameCStr)});
    }

    ui->tabInputsView->clear();

    for (int i = 0; i < m_deviceWidgetTabs.size(); i++)
    {
        qDebug("MainWindow::setDeviceGUI: adding tab for %s", m_deviceWidgetTabs[i].displayName.toStdString().c_str());
        ui->tabInputsView->addTab(m_deviceWidgetTabs[i].gui, m_deviceWidgetTabs[i].tabName);
        ui->tabInputsView->setTabToolTip(i, m_deviceWidgetTabs[i].displayName);
    }

    ui->tabInputsView->setCurrentIndex(deviceTabIndex);
}

void MainWindow::loadSettings()
{
	qDebug() << "MainWindow::loadSettings";

    m_settings.load();
    m_settings.sortPresets();
    int middleIndex = m_settings.getPresetCount() / 2;
    QTreeWidgetItem *treeItem;

    for (int i = 0; i < m_settings.getPresetCount(); ++i)
    {
        treeItem = addPresetToTree(m_settings.getPreset(i));

        if (i == middleIndex) {
            ui->presetTree->setCurrentItem(treeItem);
        }
    }

    m_settings.sortCommands();

    for (int i = 0; i < m_settings.getCommandCount(); ++i)
    {
        treeItem = addCommandToTree(m_settings.getCommand(i));
    }

    setLoggingOptions();
}

void MainWindow::loadPresetSettings(const Preset* preset, int tabIndex)
{
	qDebug("MainWindow::loadPresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

	if (tabIndex >= 0)
	{
        DeviceUISet *deviceUI = m_deviceUIs[tabIndex];

        if (deviceUI->m_deviceSourceEngine) // source device
        {
            deviceUI->m_spectrumGUI->deserialize(preset->getSpectrumConfig());
            deviceUI->m_deviceSourceAPI->loadSourceSettings(preset);
            deviceUI->loadRxChannelSettings(preset, m_pluginManager->getPluginAPI());
        }
        else if (deviceUI->m_deviceSinkEngine) // sink device
        {
            deviceUI->m_spectrumGUI->deserialize(preset->getSpectrumConfig());
            deviceUI->m_deviceSinkAPI->loadSinkSettings(preset);
            deviceUI->loadTxChannelSettings(preset, m_pluginManager->getPluginAPI());
        }
	}

	// has to be last step
	restoreState(preset->getLayout());
    tabifyDockWidget(ui->presetDock, ui->commandsDock); // override this setting
    ui->presetDock->raise();
}

void MainWindow::savePresetSettings(Preset* preset, int tabIndex)
{
	qDebug("MainWindow::savePresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

    // Save from currently selected source tab
    //int currentSourceTabIndex = ui->tabInputsView->currentIndex();
    DeviceUISet *deviceUI = m_deviceUIs[tabIndex];

    if (deviceUI->m_deviceSourceEngine) // source device
    {
        preset->setSpectrumConfig(deviceUI->m_spectrumGUI->serialize());
        preset->clearChannels();
        deviceUI->saveRxChannelSettings(preset);
        deviceUI->m_deviceSourceAPI->saveSourceSettings(preset);
    }
    else if (deviceUI->m_deviceSinkEngine) // sink device
    {
        preset->setSpectrumConfig(deviceUI->m_spectrumGUI->serialize());
        preset->clearChannels();
        preset->setSourcePreset(false);
        deviceUI->saveTxChannelSettings(preset);
        deviceUI->m_deviceSinkAPI->saveSinkSettings(preset);
    }

    preset->setLayout(saveState());
}

void MainWindow::saveCommandSettings()
{
}

void MainWindow::createStatusBar()
{
    QString qtVersionStr = QString("Qt %1 ").arg(QT_VERSION_STR);
#if QT_VERSION >= 0x050400
    m_showSystemWidget = new QLabel("SDRangel v" + qApp->applicationVersion() + " " + qtVersionStr
            + QSysInfo::currentCpuArchitecture() + " " + QSysInfo::prettyProductName(), this);
#else
    m_showSystemWidget = new QLabel("SDRangel v" + qApp->applicationVersion() + " " + qtVersionStr, this);
#endif
    statusBar()->addPermanentWidget(m_showSystemWidget);

	m_dateTimeWidget = new QLabel(tr("Date"), this);
	m_dateTimeWidget->setToolTip(tr("Current date/time"));
	statusBar()->addPermanentWidget(m_dateTimeWidget);
}

void MainWindow::closeEvent(QCloseEvent*)
{
}

void MainWindow::updatePresetControls()
{
	ui->presetTree->resizeColumnToContents(0);

	if(ui->presetTree->currentItem() != 0)
	{
		ui->presetDelete->setEnabled(true);
		ui->presetLoad->setEnabled(true);
	}
	else
	{
		ui->presetDelete->setEnabled(false);
		ui->presetLoad->setEnabled(false);
	}
}

QTreeWidgetItem* MainWindow::addPresetToTree(const Preset* preset)
{
	QTreeWidgetItem* group = 0;

	for(int i = 0; i < ui->presetTree->topLevelItemCount(); i++)
	{
		if(ui->presetTree->topLevelItem(i)->text(0) == preset->getGroup())
		{
			group = ui->presetTree->topLevelItem(i);
			break;
		}
	}

	if(group == 0)
	{
		QStringList sl;
		sl.append(preset->getGroup());
		group = new QTreeWidgetItem(ui->presetTree, sl, PGroup);
		group->setFirstColumnSpanned(true);
		group->setExpanded(true);
		ui->presetTree->sortByColumn(0, Qt::AscendingOrder);
	}

	QStringList sl;
	sl.append(QString("%1").arg(preset->getCenterFrequency() / 1e6f, 0, 'f', 3)); // frequency column
	sl.append(QString("%1").arg(preset->isSourcePreset() ? 'R' : 'T'));           // mode column
	sl.append(preset->getDescription());                                          // description column
	PresetItem* item = new PresetItem(group, sl, preset->getCenterFrequency(), PItem);
	item->setTextAlignment(0, Qt::AlignRight);
	item->setData(0, Qt::UserRole, qVariantFromValue(preset));
	ui->presetTree->resizeColumnToContents(0); // Resize frequency column to minimum
    ui->presetTree->resizeColumnToContents(1); // Resize mode column to minimum

	updatePresetControls();
	return item;
}

QTreeWidgetItem* MainWindow::addCommandToTree(const Command* command)
{
    QTreeWidgetItem* group = 0;

    for(int i = 0; i < ui->commandTree->topLevelItemCount(); i++)
    {
        if(ui->commandTree->topLevelItem(i)->text(0) == command->getGroup())
        {
            group = ui->commandTree->topLevelItem(i);
            break;
        }
    }

    if(group == 0)
    {
        QStringList sl;
        sl.append(command->getGroup());
        group = new QTreeWidgetItem(ui->commandTree, sl, PGroup);
        group->setFirstColumnSpanned(true);
        group->setExpanded(true);
        ui->commandTree->sortByColumn(0, Qt::AscendingOrder);
    }

    QStringList sl;
    sl.append(QString("%1").arg(command->getDescription())); // Descriptions column
    sl.append(QString("%1").arg(command->getAssociateKey() ? command->getRelease() ? "R" : "P" : "-")); // key press/release column
    sl.append(QString("%1").arg(command->getKeyLabel()));   // key column
    CommandItem* item = new CommandItem(group, sl, command->getDescription(), PItem);
    item->setData(0, Qt::UserRole, qVariantFromValue(command));
    item->setTextAlignment(0, Qt::AlignLeft);
    ui->commandTree->resizeColumnToContents(0); // Resize description column to minimum
    ui->commandTree->resizeColumnToContents(1); // Resize key column to minimum
    ui->commandTree->resizeColumnToContents(2); // Resize key press/release column to minimum

    //updatePresetControls();
    return item;
}

void MainWindow::applySettings()
{
}

bool MainWindow::handleMessage(const Message& cmd)
{
    if (MsgLoadPreset::match(cmd))
    {
        MsgLoadPreset& notif = (MsgLoadPreset&) cmd;
        loadPresetSettings(notif.getPreset(), notif.getDeviceSetIndex());
        return true;
    }
    else if (MsgSavePreset::match(cmd))
    {
        MsgSavePreset& notif = (MsgSavePreset&) cmd;
        savePresetSettings(notif.getPreset(), notif.getDeviceSetIndex());
        if (notif.isNewPreset()) { ui->presetTree->setCurrentItem(addPresetToTree(notif.getPreset())); }
        m_settings.sortPresets();
        m_settings.save();
        return true;
    }
    else if (MsgDeletePreset::match(cmd))
    {
        MsgDeletePreset& notif = (MsgDeletePreset&) cmd;
        const Preset *presetToDelete = notif.getPreset();

        // remove preset from tree
        for (int ig = 0; ig < ui->presetTree->topLevelItemCount(); ig++)
        {
            QTreeWidgetItem *groupItem = ui->presetTree->topLevelItem(ig);
            if (groupItem->text(0) == presetToDelete->getGroup())
            {
                for (int ip = 0; ip < groupItem->childCount(); ip++)
                {
                    QTreeWidgetItem *presetItem = groupItem->child(ip);
                    const Preset* preset = qvariant_cast<const Preset*>(presetItem->data(0, Qt::UserRole));

                    if ((preset->getGroup() == presetToDelete->getGroup()) &&
                        (preset->getCenterFrequency() == presetToDelete->getCenterFrequency()) &&
                        (preset->getDescription() == presetToDelete->getDescription()) &&
                        (preset->isSourcePreset() == presetToDelete->isSourcePreset()))
                    {
                        groupItem->takeChild(ip);
                    }
                }
            }
        }

        // remove preset from settings
        m_settings.deletePreset(presetToDelete);
        return true;
    }
    else if (MsgAddDeviceSet::match(cmd))
    {
        MsgAddDeviceSet& notif = (MsgAddDeviceSet&) cmd;

        if (notif.isTx()) {
            addSinkDevice();
        } else {
            addSourceDevice(-1); // create with file source device by default
        }

        return true;
    }
    else if (MsgRemoveLastDeviceSet::match(cmd))
    {
        if (m_deviceUIs.size() > 1) {
            removeLastDevice();
        }

        return true;
    }
    else if (MsgSetDevice::match(cmd))
    {
        MsgSetDevice& notif = (MsgSetDevice&) cmd;
        ui->tabInputsSelect->setCurrentIndex(notif.getDeviceSetIndex());
        DeviceUISet *deviceUI = m_deviceUIs[notif.getDeviceSetIndex()];
        deviceUI->m_samplingDeviceControl->setSelectedDeviceIndex(notif.getDeviceIndex());

        if (notif.isTx()) {
            sampleSinkChanged();
        } else {
            sampleSourceChanged();
        }

        return true;
    }
    else if (MsgAddChannel::match(cmd))
    {
        MsgAddChannel& notif = (MsgAddChannel&) cmd;
        ui->tabInputsSelect->setCurrentIndex(notif.getDeviceSetIndex());
        DeviceUISet *deviceUI = m_deviceUIs[notif.getDeviceSetIndex()];
        deviceUI->m_samplingDeviceControl->getChannelSelector()->setCurrentIndex(notif.getChannelRegistrationIndex());
        channelAddClicked(true);

        return true;
    }
    else if (MsgDeleteChannel::match(cmd))
    {
        MsgDeleteChannel& notif = (MsgDeleteChannel&) cmd;
        deleteChannel(notif.getDeviceSetIndex(), notif.getChannelIndex());
        return true;
    }
    else if (MsgDeviceSetFocus::match(cmd))
    {
        MsgDeviceSetFocus& notif = (MsgDeviceSetFocus&) cmd;
        int index = notif.getDeviceSetIndex();
        if ((index >= 0) && (index < (int) m_deviceUIs.size())) {
            ui->tabInputsView->setCurrentIndex(index);
        }
    }

    return false;
}

void MainWindow::handleMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		qDebug("MainWindow::handleMessages: message: %s", message->getIdentifier());
		handleMessage(*message);
		delete message;
	}
}

void MainWindow::on_action_View_Fullscreen_toggled(bool checked)
{
	if(checked) {
		showFullScreen();
	} else {
	    showNormal();
	}
}

void MainWindow::on_commandNew_clicked()
{
    QStringList groups;
    QString group = "";
    QString description = "";

    for(int i = 0; i < ui->commandTree->topLevelItemCount(); i++) {
        groups.append(ui->commandTree->topLevelItem(i)->text(0));
    }

    QTreeWidgetItem* item = ui->commandTree->currentItem();

    if(item != 0)
    {
        if(item->type() == PGroup) {
            group = item->text(0);
        } else if(item->type() == PItem) {
            group = item->parent()->text(0);
            description = item->text(0);
        }
    }

    Command *command = new Command();
    command->setGroup(group);
    command->setDescription(description);
    EditCommandDialog editCommandDialog(groups, group, this);
    editCommandDialog.fromCommand(*command);

    if (editCommandDialog.exec() == QDialog::Accepted)
    {
        editCommandDialog.toCommand(*command);
        m_settings.addCommand(command);
        ui->commandTree->setCurrentItem(addCommandToTree(command));
        m_settings.sortCommands();
    }
}

void MainWindow::on_commandDuplicate_clicked()
{
    QTreeWidgetItem* item = ui->commandTree->currentItem();
    const Command* command = qvariant_cast<const Command*>(item->data(0, Qt::UserRole));
    Command *commandCopy = new Command(*command);
    m_settings.addCommand(commandCopy);
    ui->commandTree->setCurrentItem(addCommandToTree(commandCopy));
    m_settings.sortCommands();
}

void MainWindow::on_commandEdit_clicked()
{
    QTreeWidgetItem* item = ui->commandTree->currentItem();
    bool change = false;
    const Command *changedCommand = 0;
    QString newGroupName;

    QStringList groups;

    for(int i = 0; i < ui->commandTree->topLevelItemCount(); i++) {
        groups.append(ui->commandTree->topLevelItem(i)->text(0));
    }

    if(item != 0)
    {
        if (item->type() == PItem)
        {
            const Command* command = qvariant_cast<const Command*>(item->data(0, Qt::UserRole));

            if (command != 0)
            {
                EditCommandDialog editCommandDialog(groups, command->getGroup(), this);
                editCommandDialog.fromCommand(*command);

                if (editCommandDialog.exec() == QDialog::Accepted)
                {
                    Command* command_mod = const_cast<Command*>(command);
                    editCommandDialog.toCommand(*command_mod);
                    change = true;
                    changedCommand = command;
                }
            }
        }
        else if (item->type() == PGroup)
        {
            AddPresetDialog dlg(groups, item->text(0), this);
            dlg.showGroupOnly();
            dlg.setDialogTitle("Edit command group");
            dlg.setDescriptionBoxTitle("Command details");

            if (dlg.exec() == QDialog::Accepted)
            {
                m_settings.renameCommandGroup(item->text(0), dlg.group());
                newGroupName = dlg.group();
                change = true;
            }
        }
    }

    if (change)
    {
        m_settings.sortCommands();
        ui->commandTree->clear();

        for (int i = 0; i < m_settings.getCommandCount(); ++i)
        {
            QTreeWidgetItem *item_x = addCommandToTree(m_settings.getCommand(i));
            const Command* command_x = qvariant_cast<const Command*>(item_x->data(0, Qt::UserRole));
            if (changedCommand &&  (command_x == changedCommand)) { // set cursor on changed command
                ui->commandTree->setCurrentItem(item_x);
            }
        }

        if (!changedCommand) // on group name change set cursor on the group that has been changed
        {
            for(int i = 0; i < ui->commandTree->topLevelItemCount(); i++)
            {
                QTreeWidgetItem* item = ui->commandTree->topLevelItem(i);

                if (item->text(0) == newGroupName) {
                    ui->commandTree->setCurrentItem(item);
                }
            }
        }
    }
}

void MainWindow::on_commandDelete_clicked()
{
    QTreeWidgetItem* item = ui->commandTree->currentItem();

    if (item != 0)
    {
        if (item->type() == PItem) // delete individual command
        {
            const Command* command = qvariant_cast<const Command*>(item->data(0, Qt::UserRole));

            if(command)
            {
                if (QMessageBox::question(this,
                        tr("Delete command"),
                        tr("Do you want to delete command '%1'?")
                            .arg(command->getDescription()), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
                {
                    delete item;
                    m_settings.deleteCommand(command);
                }
            }
        }
        else if (item->type() == PGroup) // delete all commands in this group
        {
            if (QMessageBox::question(this,
                    tr("Delete command group"),
                    tr("Do you want to delete command group '%1'?")
                        .arg(item->text(0)), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
            {
                m_settings.deleteCommandGroup(item->text(0));

                ui->commandTree->clear();

                for (int i = 0; i < m_settings.getCommandCount(); ++i) {
                    addCommandToTree(m_settings.getCommand(i));
                }
            }
        }
    }
}

void MainWindow::on_commandRun_clicked()
{
    QTreeWidgetItem* item = ui->commandTree->currentItem();

    if (item != 0)
    {
        int currentDeviceSetIndex = ui->tabInputsSelect->currentIndex();

        if (item->type() == PItem) // run individual command
        {
            const Command* command = qvariant_cast<const Command*>(item->data(0, Qt::UserRole));
            Command* command_mod = const_cast<Command*>(command);
            command_mod->run(m_apiServer->getHost(), m_apiServer->getPort(), currentDeviceSetIndex);
        }
        else if (item->type() == PGroup) // run all commands in this group
        {
            QString group = item->text(0);

            for (int i = 0; i < m_settings.getCommandCount(); ++i)
            {
                Command *command_mod = const_cast<Command*>(m_settings.getCommand(i));

                if (command_mod->getGroup() == group) {
                    command_mod->run(m_apiServer->getHost(), m_apiServer->getPort(), currentDeviceSetIndex);
                }
            }
        }
    }
}

void MainWindow::on_commandOutput_clicked()
{
    QTreeWidgetItem* item = ui->commandTree->currentItem();

    if ((item != 0) && (item->type() == PItem))
    {
        const Command* command = qvariant_cast<const Command*>(item->data(0, Qt::UserRole));
        Command* command_mod = const_cast<Command*>(command);
        CommandOutputDialog commandOutputDialog(*command_mod);
        commandOutputDialog.exec();
    }
}

void MainWindow::on_commandsSave_clicked()
{
    saveCommandSettings();
    m_settings.save();
}

void MainWindow::on_commandKeyboardConnect_toggled(bool checked)
{
    qDebug("on_commandKeyboardConnect_toggled: %s", checked ? "true" : "false");

    if (checked)
    {
        setFocus();
        connect(m_commandKeyReceiver, SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
                this, SLOT(commandKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
    }
    else
    {
        disconnect(m_commandKeyReceiver, SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
                this, SLOT(commandKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
    }
}

void MainWindow::on_presetSave_clicked()
{
    QStringList groups;
    QString group;
    QString description = "";

    for(int i = 0; i < ui->presetTree->topLevelItemCount(); i++) {
        groups.append(ui->presetTree->topLevelItem(i)->text(0));
    }

    QTreeWidgetItem* item = ui->presetTree->currentItem();

    if(item != 0)
    {
        if(item->type() == PGroup) {
            group = item->text(0);
        } else if(item->type() == PItem) {
            group = item->parent()->text(0);
            description = item->text(0);
        }
    }

    AddPresetDialog dlg(groups, group, this);

    if (description.length() > 0) {
        dlg.setDescription(description);
    }

    if(dlg.exec() == QDialog::Accepted) {
        Preset* preset = m_settings.newPreset(dlg.group(), dlg.description());
        savePresetSettings(preset, ui->tabInputsView->currentIndex());

        ui->presetTree->setCurrentItem(addPresetToTree(preset));
    }

    m_settings.sortPresets();
}

void MainWindow::on_presetUpdate_clicked()
{
	QTreeWidgetItem* item = ui->presetTree->currentItem();
	const Preset* changedPreset = 0;

	if(item != 0)
	{
		if(item->type() == PItem)
		{
			const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));

			if (preset != 0)
			{
				Preset* preset_mod = const_cast<Preset*>(preset);
				savePresetSettings(preset_mod, ui->tabInputsView->currentIndex());
				changedPreset = preset;
			}
		}
	}

	m_settings.sortPresets();
    ui->presetTree->clear();

    for (int i = 0; i < m_settings.getPresetCount(); ++i)
    {
        QTreeWidgetItem *item_x = addPresetToTree(m_settings.getPreset(i));
        const Preset* preset_x = qvariant_cast<const Preset*>(item_x->data(0, Qt::UserRole));
        if (changedPreset &&  (preset_x == changedPreset)) { // set cursor on changed preset
            ui->presetTree->setCurrentItem(item_x);
        }
    }
}

void MainWindow::on_presetEdit_clicked()
{
    QTreeWidgetItem* item = ui->presetTree->currentItem();
    QStringList groups;
    bool change = false;
    const Preset *changedPreset = 0;
    QString newGroupName;

    for(int i = 0; i < ui->presetTree->topLevelItemCount(); i++) {
        groups.append(ui->presetTree->topLevelItem(i)->text(0));
    }

    if(item != 0)
    {
        if (item->type() == PItem)
        {
            const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));
            AddPresetDialog dlg(groups, preset->getGroup(), this);
            dlg.setDescription(preset->getDescription());

            if (dlg.exec() == QDialog::Accepted)
            {
                Preset* preset_mod = const_cast<Preset*>(preset);
                preset_mod->setGroup(dlg.group());
                preset_mod->setDescription(dlg.description());
                change = true;
                changedPreset = preset;
            }
        }
        else if (item->type() == PGroup)
        {
            AddPresetDialog dlg(groups, item->text(0), this);
            dlg.showGroupOnly();
            dlg.setDialogTitle("Edit preset group");

            if (dlg.exec() == QDialog::Accepted)
            {
                m_settings.renamePresetGroup(item->text(0), dlg.group());
                newGroupName = dlg.group();
                change = true;
            }
        }
    }

    if (change)
    {
        m_settings.sortPresets();
        ui->presetTree->clear();

        for (int i = 0; i < m_settings.getPresetCount(); ++i)
        {
            QTreeWidgetItem *item_x = addPresetToTree(m_settings.getPreset(i));
            const Preset* preset_x = qvariant_cast<const Preset*>(item_x->data(0, Qt::UserRole));
            if (changedPreset &&  (preset_x == changedPreset)) { // set cursor on changed preset
                ui->presetTree->setCurrentItem(item_x);
            }
        }

        if (!changedPreset) // on group name change set cursor on the group that has been changed
        {
            for(int i = 0; i < ui->presetTree->topLevelItemCount(); i++)
            {
                QTreeWidgetItem* item = ui->presetTree->topLevelItem(i);

                if (item->text(0) == newGroupName) {
                    ui->presetTree->setCurrentItem(item);
                }
            }
        }
    }
}

void MainWindow::on_presetExport_clicked()
{
	QTreeWidgetItem* item = ui->presetTree->currentItem();

	if(item != 0) {
		if(item->type() == PItem)
		{
			const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));
			QString base64Str = preset->serialize().toBase64();
			QString fileName = QFileDialog::getSaveFileName(this,
			    tr("Open preset export file"), ".", tr("Preset export files (*.prex)"));

			if (fileName != "")
			{
				QFileInfo fileInfo(fileName);

				if (fileInfo.suffix() != "prex") {
					fileName += ".prex";
				}

				QFile exportFile(fileName);

				if (exportFile.open(QIODevice::WriteOnly | QIODevice::Text))
				{
					QTextStream outstream(&exportFile);
					outstream << base64Str;
					exportFile.close();
				}
				else
				{
			    	QMessageBox::information(this, tr("Message"), tr("Cannot open file for writing"));
				}
			}
		}
	}
}

void MainWindow::on_presetImport_clicked()
{
	QTreeWidgetItem* item = ui->presetTree->currentItem();

	if(item != 0)
	{
		QString group;

		if (item->type() == PGroup)	{
			group = item->text(0);
		} else if (item->type() == PItem) {
			group = item->parent()->text(0);
		} else {
			return;
		}

		QString fileName = QFileDialog::getOpenFileName(this,
		    tr("Open preset export file"), ".", tr("Preset export files (*.prex)"));

		if (fileName != "")
		{
			QFile exportFile(fileName);

			if (exportFile.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				QByteArray base64Str;
				QTextStream instream(&exportFile);
				instream >> base64Str;
				exportFile.close();

				Preset* preset = m_settings.newPreset("", "");
				preset->deserialize(QByteArray::fromBase64(base64Str));
				preset->setGroup(group); // override with current group

				ui->presetTree->setCurrentItem(addPresetToTree(preset));
			}
			else
			{
				QMessageBox::information(this, tr("Message"), tr("Cannot open file for reading"));
			}
		}
	}
}

void MainWindow::on_settingsSave_clicked()
{
    savePresetSettings(m_settings.getWorkingPreset(), ui->tabInputsView->currentIndex());
    m_settings.save();
}

void MainWindow::on_presetLoad_clicked()
{
	qDebug() << "MainWindow::on_presetLoad_clicked";

	QTreeWidgetItem* item = ui->presetTree->currentItem();

	if(item == 0)
	{
		qDebug("MainWindow::on_presetLoad_clicked: item null");
		updatePresetControls();
		return;
	}

	const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));

	if(preset == 0)
	{
		qDebug("MainWindow::on_presetLoad_clicked: preset null");
		return;
	}

	loadPresetSettings(preset, ui->tabInputsView->currentIndex());
	applySettings();
}

void MainWindow::on_presetDelete_clicked()
{
	QTreeWidgetItem* item = ui->presetTree->currentItem();

	if (item == 0)
	{
		updatePresetControls();
		return;
	}
	else
	{
        if (item->type() == PItem)
        {
            const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));

            if (preset)
            {
                if(QMessageBox::question(this, tr("Delete Preset"), tr("Do you want to delete preset '%1'?").arg(preset->getDescription()), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
                    delete item;
                    m_settings.deletePreset(preset);
                }
            }
        }
        else if (item->type() == PGroup)
        {
            if (QMessageBox::question(this,
                    tr("Delete preset group"),
                    tr("Do you want to delete preset group '%1'?")
                        .arg(item->text(0)), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
            {
                m_settings.deletePresetGroup(item->text(0));

                ui->commandTree->clear();

                for (int i = 0; i < m_settings.getPresetCount(); ++i) {
                    addPresetToTree(m_settings.getPreset(i));
                }
            }
        }
	}
}

void MainWindow::on_presetTree_currentItemChanged(QTreeWidgetItem *current __attribute__((unused)), QTreeWidgetItem *previous __attribute__((unused)))
{
	updatePresetControls();
}

void MainWindow::on_presetTree_itemActivated(QTreeWidgetItem *item __attribute__((unused)), int column __attribute__((unused)))
{
	on_presetLoad_clicked();
}

void MainWindow::on_action_Loaded_Plugins_triggered()
{
    PluginsDialog pluginsDialog(m_pluginManager, this);
    pluginsDialog.exec();
}

void MainWindow::on_action_Audio_triggered()
{
	AudioDialog audioDialog(&m_audioDeviceInfo, this);
	audioDialog.exec();
	m_dspEngine->setAudioInputVolume(m_audioDeviceInfo.getInputVolume());
	m_dspEngine->setAudioInputDeviceIndex(m_audioDeviceInfo.getInputDeviceIndex());
	m_dspEngine->setAudioOutputDeviceIndex(m_audioDeviceInfo.getOutputDeviceIndex());
}

void MainWindow::on_action_Logging_triggered()
{
    LoggingDialog loggingDialog(m_settings, this);
    loggingDialog.exec();
    setLoggingOptions();
}

void MainWindow::on_action_My_Position_triggered()
{
	MyPositionDialog myPositionDialog(m_settings, this);
	myPositionDialog.exec();
}

void MainWindow::on_action_DV_Serial_triggered(bool checked)
{
    m_dspEngine->setDVSerialSupport(checked);

    if (checked)
    {
        std::vector<std::string> deviceNames;
        m_dspEngine->getDVSerialNames(deviceNames);

        if (deviceNames.size() == 0)
        {
            QMessageBox::information(this, tr("Message"), tr("No DV serial devices found"));
        }
        else
        {
            std::vector<std::string>::iterator it = deviceNames.begin();
            std::string deviceNamesStr = "DV Serial devices found: ";

            while (it != deviceNames.end())
            {
                if (it != deviceNames.begin()) {
                    deviceNamesStr += ",";
                }

                deviceNamesStr += *it;
                ++it;
            }

            QMessageBox::information(this, tr("Message"), tr(deviceNamesStr.c_str()));
        }
    }
}

void MainWindow::sampleSourceChanged()
{
    // Do it in the currently selected source tab
    int currentSourceTabIndex = ui->tabInputsSelect->currentIndex();

    if (currentSourceTabIndex >= 0)
    {
        qDebug("MainWindow::on_sampleSource_confirmClicked: tab at %d", currentSourceTabIndex);
        DeviceUISet *deviceUI = m_deviceUIs[currentSourceTabIndex];
        deviceUI->m_deviceSourceAPI->saveSourceSettings(m_settings.getWorkingPreset()); // save old API settings
        deviceUI->m_deviceSourceAPI->stopAcquisition();

        // deletes old UI and input object
        deviceUI->m_deviceSourceAPI->getSampleSource()->setMessageQueueToGUI(0); // have source stop sending messages to the GUI
        deviceUI->m_deviceSourceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceGUI(
                deviceUI->m_deviceSourceAPI->getSampleSourcePluginInstanceGUI());
        deviceUI->m_deviceSourceAPI->resetSampleSourceId();
        deviceUI->m_deviceSourceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceInput(
                deviceUI->m_deviceSourceAPI->getSampleSource());
        deviceUI->m_deviceSourceAPI->clearBuddiesLists(); // clear old API buddies lists

        PluginInterface::SamplingDevice samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(deviceUI->m_samplingDeviceControl->getSelectedDeviceIndex());
        deviceUI->m_deviceSourceAPI->setSampleSourceSequence(samplingDevice.sequence);
        deviceUI->m_deviceSourceAPI->setNbItems(samplingDevice.deviceNbItems);
        deviceUI->m_deviceSourceAPI->setItemIndex(samplingDevice.deviceItemIndex);
        deviceUI->m_deviceSourceAPI->setHardwareId(samplingDevice.hardwareId);
        deviceUI->m_deviceSourceAPI->setSampleSourceId(samplingDevice.id);
        deviceUI->m_deviceSourceAPI->setSampleSourceSerial(samplingDevice.serial);
        deviceUI->m_deviceSourceAPI->setSampleSourceDisplayName(samplingDevice.displayedName);
        deviceUI->m_deviceSourceAPI->setSampleSourcePluginInterface(DeviceEnumerator::instance()->getRxPluginInterface(deviceUI->m_samplingDeviceControl->getSelectedDeviceIndex()));

        // add to buddies list
        std::vector<DeviceUISet*>::iterator it = m_deviceUIs.begin();
        int nbOfBuddies = 0;

        for (; it != m_deviceUIs.end(); ++it)
        {
            if (*it != deviceUI) // do not add to itself
            {
                if ((*it)->m_deviceSourceEngine) // it is a source device
                {
                    if ((deviceUI->m_deviceSourceAPI->getHardwareId() == (*it)->m_deviceSourceAPI->getHardwareId()) &&
                        (deviceUI->m_deviceSourceAPI->getSampleSourceSerial() == (*it)->m_deviceSourceAPI->getSampleSourceSerial()))
                    {
                        (*it)->m_deviceSourceAPI->addSourceBuddy(deviceUI->m_deviceSourceAPI);
                        nbOfBuddies++;
                    }
                }

                if ((*it)->m_deviceSinkEngine) // it is a sink device
                {
                    if ((deviceUI->m_deviceSourceAPI->getHardwareId() == (*it)->m_deviceSinkAPI->getHardwareId()) &&
                        (deviceUI->m_deviceSourceAPI->getSampleSourceSerial() == (*it)->m_deviceSinkAPI->getSampleSinkSerial()))
                    {
                        (*it)->m_deviceSinkAPI->addSourceBuddy(deviceUI->m_deviceSourceAPI);
                        nbOfBuddies++;
                    }
                }
            }
        }

        if (nbOfBuddies == 0) {
            deviceUI->m_deviceSourceAPI->setBuddyLeader(true);
        }

        // constructs new GUI and input object
        DeviceSampleSource *source = deviceUI->m_deviceSourceAPI->getPluginInterface()->createSampleSourcePluginInstanceInput(
                deviceUI->m_deviceSourceAPI->getSampleSourceId(), deviceUI->m_deviceSourceAPI);
        deviceUI->m_deviceSourceAPI->setSampleSource(source);
        QWidget *gui;
        PluginInstanceGUI *pluginUI = deviceUI->m_deviceSourceAPI->getPluginInterface()->createSampleSourcePluginInstanceGUI(
                deviceUI->m_deviceSourceAPI->getSampleSourceId(),
                &gui,
                deviceUI);
        deviceUI->m_deviceSourceAPI->getSampleSource()->setMessageQueueToGUI(pluginUI->getInputMessageQueue());
        deviceUI->m_deviceSourceAPI->setSampleSourcePluginInstanceGUI(pluginUI);
        setDeviceGUI(currentSourceTabIndex, gui, deviceUI->m_deviceSourceAPI->getSampleSourceDisplayName());
        deviceUI->m_deviceSourceAPI->getSampleSource()->init();

        deviceUI->m_deviceSourceAPI->loadSourceSettings(m_settings.getWorkingPreset()); // load new API settings

        if (currentSourceTabIndex == 0) // save as default starting device
        {
            m_settings.setSourceIndex(samplingDevice.sequence);
            m_settings.setSourceDeviceId(samplingDevice.id);
        }
    }
}

void MainWindow::sampleSinkChanged()
{
    // Do it in the currently selected source tab
    int currentSinkTabIndex = ui->tabInputsSelect->currentIndex();

    if (currentSinkTabIndex >= 0)
    {
        qDebug("MainWindow::on_sampleSink_confirmClicked: tab at %d", currentSinkTabIndex);
        DeviceUISet *deviceUI = m_deviceUIs[currentSinkTabIndex];
        deviceUI->m_deviceSinkAPI->saveSinkSettings(m_settings.getWorkingPreset()); // save old API settings
        deviceUI->m_deviceSinkAPI->stopGeneration();

        // deletes old UI and output object
        deviceUI->m_deviceSinkAPI->getSampleSink()->setMessageQueueToGUI(0); // have sink stop sending messages to the GUI
        deviceUI->m_deviceSinkAPI->getPluginInterface()->deleteSampleSourcePluginInstanceGUI(
                deviceUI->m_deviceSinkAPI->getSampleSinkPluginInstanceGUI());
        deviceUI->m_deviceSinkAPI->resetSampleSinkId();
        deviceUI->m_deviceSinkAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput(
                deviceUI->m_deviceSinkAPI->getSampleSink());
        deviceUI->m_deviceSinkAPI->clearBuddiesLists(); // clear old API buddies lists

        PluginInterface::SamplingDevice samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(deviceUI->m_samplingDeviceControl->getSelectedDeviceIndex());
        deviceUI->m_deviceSinkAPI->setSampleSinkSequence(samplingDevice.sequence);
        deviceUI->m_deviceSinkAPI->setNbItems(samplingDevice.deviceNbItems);
        deviceUI->m_deviceSinkAPI->setItemIndex(samplingDevice.deviceItemIndex);
        deviceUI->m_deviceSinkAPI->setHardwareId(samplingDevice.hardwareId);
        deviceUI->m_deviceSinkAPI->setSampleSinkId(samplingDevice.id);
        deviceUI->m_deviceSinkAPI->setSampleSinkSerial(samplingDevice.serial);
        deviceUI->m_deviceSinkAPI->setSampleSinkDisplayName(samplingDevice.displayedName);
        deviceUI->m_deviceSinkAPI->setSampleSinkPluginInterface(DeviceEnumerator::instance()->getTxPluginInterface(deviceUI->m_samplingDeviceControl->getSelectedDeviceIndex()));

        // add to buddies list
        std::vector<DeviceUISet*>::iterator it = m_deviceUIs.begin();
        int nbOfBuddies = 0;

        for (; it != m_deviceUIs.end(); ++it)
        {
            if (*it != deviceUI) // do not add to itself
            {
                if ((*it)->m_deviceSourceEngine) // it is a source device
                {
                    if ((deviceUI->m_deviceSinkAPI->getHardwareId() == (*it)->m_deviceSourceAPI->getHardwareId()) &&
                        (deviceUI->m_deviceSinkAPI->getSampleSinkSerial() == (*it)->m_deviceSourceAPI->getSampleSourceSerial()))
                    {
                        (*it)->m_deviceSourceAPI->addSinkBuddy(deviceUI->m_deviceSinkAPI);
                        nbOfBuddies++;
                    }
                }

                if ((*it)->m_deviceSinkEngine) // it is a sink device
                {
                    if ((deviceUI->m_deviceSinkAPI->getHardwareId() == (*it)->m_deviceSinkAPI->getHardwareId()) &&
                        (deviceUI->m_deviceSinkAPI->getSampleSinkSerial() == (*it)->m_deviceSinkAPI->getSampleSinkSerial()))
                    {
                        (*it)->m_deviceSinkAPI->addSinkBuddy(deviceUI->m_deviceSinkAPI);
                        nbOfBuddies++;
                    }
                }
            }
        }

        if (nbOfBuddies == 0) {
            deviceUI->m_deviceSinkAPI->setBuddyLeader(true);
        }

        // constructs new GUI and output object
        DeviceSampleSink *sink = deviceUI->m_deviceSinkAPI->getPluginInterface()->createSampleSinkPluginInstanceOutput(
                deviceUI->m_deviceSinkAPI->getSampleSinkId(), deviceUI->m_deviceSinkAPI);
        deviceUI->m_deviceSinkAPI->setSampleSink(sink);
        QWidget *gui;
        PluginInstanceGUI *pluginUI = deviceUI->m_deviceSinkAPI->getPluginInterface()->createSampleSinkPluginInstanceGUI(
                deviceUI->m_deviceSinkAPI->getSampleSinkId(),
                &gui,
                deviceUI);
        deviceUI->m_deviceSinkAPI->getSampleSink()->setMessageQueueToGUI(pluginUI->getInputMessageQueue());
        deviceUI->m_deviceSinkAPI->setSampleSinkPluginInstanceUI(pluginUI);
        setDeviceGUI(currentSinkTabIndex, gui, deviceUI->m_deviceSinkAPI->getSampleSinkDisplayName(), false);
        m_deviceUIs.back()->m_deviceSinkAPI->getSampleSink()->init();

        deviceUI->m_deviceSinkAPI->loadSinkSettings(m_settings.getWorkingPreset()); // load new API settings
    }
}

void MainWindow::channelAddClicked(bool checked __attribute__((unused)))
{
    // Do it in the currently selected source tab
    int currentSourceTabIndex = ui->tabInputsSelect->currentIndex();

    if (currentSourceTabIndex >= 0)
    {
        DeviceUISet *deviceUI = m_deviceUIs[currentSourceTabIndex];

        if (deviceUI->m_deviceSourceEngine) // source device => Rx channels
        {
            m_pluginManager->createRxChannelInstance(deviceUI->m_samplingDeviceControl->getChannelSelector()->currentIndex(), deviceUI, deviceUI->m_deviceSourceAPI);
        }
        else if (deviceUI->m_deviceSinkEngine) // sink device => Tx channels
        {
            uint32_t nbSources = deviceUI->m_deviceSinkAPI->getNumberOfSources();

            if (nbSources > 0) {
                QMessageBox::information(this, tr("Message"), tr("%1 channel(s) already in use. Multiple transmission channels is experimental. You may experience performance problems").arg(nbSources));
            }

            m_pluginManager->createTxChannelInstance(deviceUI->m_samplingDeviceControl->getChannelSelector()->currentIndex(), deviceUI, deviceUI->m_deviceSinkAPI);
        }
    }
}

void MainWindow::on_action_About_triggered()
{
	AboutDialog dlg(m_apiHost, m_apiPort, this);
	dlg.exec();
}

void MainWindow::on_action_addSourceDevice_triggered()
{
    addSourceDevice(-1); // create with file source device by default
}

void MainWindow::on_action_addSinkDevice_triggered()
{
    addSinkDevice();
}

void MainWindow::on_action_removeLastDevice_triggered()
{
    if (m_deviceUIs.size() > 1)
    {
        removeLastDevice();
    }
}

void MainWindow::on_action_Exit_triggered()
{
    savePresetSettings(m_settings.getWorkingPreset(), 0);
    m_settings.save();

    while (m_deviceUIs.size() > 0)
    {
        removeLastDevice();
    }
}

void MainWindow::tabInputViewIndexChanged()
{
    int inputViewIndex = ui->tabInputsView->currentIndex();

    if ((inputViewIndex >= 0) && (m_masterTabIndex >= 0) && (inputViewIndex != m_masterTabIndex))
    {
        DeviceUISet *deviceUI = m_deviceUIs[inputViewIndex];
        DeviceUISet *lastdeviceUI = m_deviceUIs[m_masterTabIndex];
        lastdeviceUI->m_mainWindowState = saveState();
        restoreState(deviceUI->m_mainWindowState);
        m_masterTabIndex = inputViewIndex;
    }

    ui->tabSpectra->setCurrentIndex(inputViewIndex);
    ui->tabChannels->setCurrentIndex(inputViewIndex);
    ui->tabInputsSelect->setCurrentIndex(inputViewIndex);
    ui->tabSpectraGUI->setCurrentIndex(inputViewIndex);
}

void MainWindow::updateStatus()
{
    m_dateTimeWidget->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss t"));
}

void MainWindow::setLoggingOptions()
{
    m_logger->setConsoleMinMessageLevel(m_settings.getConsoleMinLogLevel());

    if (m_settings.getUseLogFile())
    {
        qtwebapp::FileLoggerSettings fileLoggerSettings; // default values

        if (m_logger->hasFileLogger()) {
            fileLoggerSettings = m_logger->getFileLoggerSettings(); // values from file logger if it exists
        }

        fileLoggerSettings.fileName = m_settings.getLogFileName(); // put new values
        m_logger->createOrSetFileLogger(fileLoggerSettings, 2000); // create file logger if it does not exist and apply settings in any case
    }

    if (m_logger->hasFileLogger()) {
        m_logger->setFileMinMessageLevel(m_settings.getFileMinLogLevel());
    }

    m_logger->setUseFileLogger(m_settings.getUseLogFile());

    if (m_settings.getUseLogFile())
    {
#if QT_VERSION >= 0x050400
        QString appInfoStr(tr("%1 %2 Qt %3 %4b %5 %6 DSP Rx:%7b Tx:%8b PID %9")
                .arg(qApp->applicationName())
                .arg(qApp->applicationVersion())
                .arg(QT_VERSION_STR)
                .arg(QT_POINTER_SIZE*8)
                .arg(QSysInfo::currentCpuArchitecture())
                .arg(QSysInfo::prettyProductName())
                .arg(SDR_RX_SAMP_SZ)
                .arg(SDR_TX_SAMP_SZ)
                .arg(qApp->applicationPid()));
#else
        QString appInfoStr(tr("%1 %2 Qt %3 %4b DSP Rx:%5b Tx:%6b PID %7")
                .arg(qApp->applicationName())
                .arg(qApp->applicationVersion())
                .arg(QT_VERSION_STR)
                .arg(QT_POINTER_SIZE*8)
                .arg(SDR_RX_SAMP_SZ)
                .arg(SDR_TX_SAMP_SZ)
                .arg(qApp->applicationPid()));
 #endif
        m_logger->logToFile(QtInfoMsg, appInfoStr);
    }
}

void MainWindow::focusHasChanged(QWidget *oldWidget __attribute__((unused)), QWidget *newWidget)
{
    // this is the hard way:
//    if (ui->commandKeyboardConnect->isChecked() && (newWidget != this)) {
//        setFocus();
//    }
    // this is the soft way:
    if (newWidget != this) {
        ui->commandKeyboardConnect->setChecked(false);
    }
}

void MainWindow::commandKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release)
{
    //qDebug("MainWindow::commandKeyPressed: key: %x mod: %x %s", (int) key, (int) keyModifiers, release ? "release" : "press");
    int currentDeviceSetIndex = ui->tabInputsSelect->currentIndex();

    for (int i = 0; i < m_settings.getCommandCount(); ++i)
    {
        const Command* command = m_settings.getCommand(i);

        if (command->getAssociateKey()
                && (command->getRelease() == release)
                && (command->getKey() == key)
                && (command->getKeyModifiers() == keyModifiers))
        {
            Command* command_mod = const_cast<Command*>(command);
            command_mod->run(m_apiServer->getHost(), m_apiServer->getPort(), currentDeviceSetIndex);
        }
    }
}
