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
#include <QResource>
#include <QFontDatabase>

#include <plugin/plugininstancegui.h>
#include <plugin/plugininstancegui.h>
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "device/deviceenumerator.h"
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
#include "gui/deviceuserargsdialog.h"
#include "gui/samplingdevicecontrol.h"
#include "gui/sdrangelsplash.h"
#include "gui/mypositiondialog.h"
#include "gui/ambedevicesdialog.h"
#include "dsp/dspengine.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "dsp/devicesamplemimo.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "plugin/pluginapi.h"
#include "gui/glspectrum.h"
#include "gui/glspectrumgui.h"
#include "loggerwithfile.h"
#include "webapi/webapirequestmapper.h"
#include "webapi/webapiserver.h"
#include "webapi/webapiadaptergui.h"
#include "commands/command.h"

#include "mainwindow.h"

#include <audio/audiodevicemanager.h>

#include "ui_mainwindow.h"

#include <string>
#include <QDebug>
#include <QSplashScreen>

#if defined(HAS_LIMERFEUSB)
#include "limerfegui/limerfeusbdialog.h"
#endif

MESSAGE_CLASS_DEFINITION(MainWindow::MsgLoadPreset, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgSavePreset, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgDeletePreset, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgAddDeviceSet, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgRemoveLastDeviceSet, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgSetDevice, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgAddChannel, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgDeleteChannel, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgDeviceSetFocus, Message)
MESSAGE_CLASS_DEFINITION(MainWindow::MsgApplySettings, Message)

MainWindow *MainWindow::m_instance = 0;

MainWindow::MainWindow(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QWidget* parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow),
	m_settings(),
    m_masterTabIndex(0),
	m_dspEngine(DSPEngine::instance()),
	m_lastEngineState(DeviceAPI::StNotStarted),
	m_inputGUI(0),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_sampleFileName(std::string("./test.sdriq")),
	m_logger(logger)
{
	qDebug() << "MainWindow::MainWindow: start";

    m_instance = this;

    QFontDatabase::addApplicationFont(":/LiberationSans-Regular.ttf");
    QFontDatabase::addApplicationFont(":/LiberationMono-Regular.ttf");

    QFont font("Liberation Sans");
    font.setPointSize(9);
    qApp->setFont(font);

    QPixmap logoPixmap(":/sdrangel_logo.png");
    SDRangelSplash *splash = new SDRangelSplash(logoPixmap);
    splash->setMessageRect(QRect(10, 80, 350, 16));
    splash->show();
    splash->showStatusMessage("starting...", Qt::white);
    splash->showStatusMessage("starting...", Qt::white);

	m_settings.setAudioDeviceManager(m_dspEngine->getAudioDeviceManager());
    m_settings.setAMBEEngine(m_dspEngine->getAMBEEngine());

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

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()), Qt::QueuedConnection);

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(1000);

	m_masterTimer.setTimerType(Qt::PreciseTimer);
	m_masterTimer.start(50);

    splash->showStatusMessage("allocate FFTs...", Qt::white);
    splash->showStatusMessage("allocate FFTs...", Qt::white);
    m_dspEngine->createFFTFactory(parser.getFFTWFWisdomFileName());
    m_dspEngine->preAllocateFFTs();

    splash->showStatusMessage("load settings...", Qt::white);
    qDebug() << "MainWindow::MainWindow: load settings...";

	loadSettings();

    splash->showStatusMessage("load plugins...", Qt::white);
    qDebug() << "MainWindow::MainWindow: load plugins...";

    m_pluginManager = new PluginManager(this);
    m_pluginManager->loadPlugins(QString("plugins"));
    m_pluginManager->loadPluginsNonDiscoverable(m_settings.getDeviceUserArgs());

    splash->showStatusMessage("load file input...", Qt::white);
    qDebug() << "MainWindow::MainWindow: select SampleSource from settings or default (file input)...";

	int deviceIndex = DeviceEnumerator::instance()->getRxSamplingDeviceIndex(m_settings.getSourceDeviceId(), m_settings.getSourceIndex());
	addSourceDevice(deviceIndex);  // add the first device set with file input device as default if device in settings is not enumerated
	m_deviceUIs.back()->m_deviceAPI->setBuddyLeader(true); // the first device is always the leader

    splash->showStatusMessage("load current preset settings...", Qt::white);
	qDebug() << "MainWindow::MainWindow: load current preset settings...";

	loadPresetSettings(m_settings.getWorkingPreset(), 0);

    splash->showStatusMessage("update preset controls...", Qt::white);
	qDebug() << "MainWindow::MainWindow: update preset controls...";

	updatePresetControls();

    splash->showStatusMessage("finishing...", Qt::white);
	connect(ui->tabInputsView, SIGNAL(currentChanged(int)), this, SLOT(tabInputViewIndexChanged()));

	QString applicationDirPath = qApp->applicationDirPath();

#ifdef _MSC_VER
    if (QResource::registerResource(applicationDirPath + "/sdrbase.rcc")) {
        qDebug("MainWindow::MainWindow: registered resource file %s/%s", qPrintable(applicationDirPath), "sdrbase.rcc");
    } else {
        qWarning("MainWindow::MainWindow: could not register resource file %s/%s", qPrintable(applicationDirPath), "sdrbase.rcc");
    }
#endif

	m_apiAdapter = new WebAPIAdapterGUI(*this);
	m_requestMapper = new WebAPIRequestMapper(this);
	m_requestMapper->setAdapter(m_apiAdapter);
	m_apiHost = parser.getServerAddress();
	m_apiPort = parser.getServerPort();
	m_apiServer = new WebAPIServer(m_apiHost, m_apiPort, m_requestMapper);
	m_apiServer->start();

	m_commandKeyReceiver = new CommandKeyReceiver();
	m_commandKeyReceiver->setRelease(true);
	this->installEventFilter(m_commandKeyReceiver);

    m_dspEngine->setMIMOSupport(parser.getMIMOSupport());

    if (!parser.getMIMOSupport()) {
        ui->menu_Devices->removeAction(ui->action_addMIMODevice);
    }

#ifdef __APPLE__
    ui->menuPreferences->removeAction(ui->action_AMBE);
#endif
#if not defined(HAS_LIMERFEUSB)
    ui->menuPreferences->removeAction(ui->action_LimeRFE);
#endif

    delete splash;

    qDebug() << "MainWindow::MainWindow: end";
}

MainWindow::~MainWindow()
{
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
	delete m_commandKeyReceiver;
}

void MainWindow::addSourceDevice(int deviceIndex)
{
    DSPDeviceSourceEngine *dspDeviceSourceEngine = m_dspEngine->addDeviceSourceEngine();
    dspDeviceSourceEngine->start();

    uint dspDeviceSourceEngineUID =  dspDeviceSourceEngine->getUID();
    char uidCStr[16];
    sprintf(uidCStr, "UID:%d", dspDeviceSourceEngineUID);

    int deviceTabIndex = m_deviceUIs.size();
    m_deviceUIs.push_back(new DeviceUISet(deviceTabIndex, 0, m_masterTimer));
    m_deviceUIs.back()->m_deviceSourceEngine = dspDeviceSourceEngine;
    m_deviceUIs.back()->m_deviceSinkEngine = nullptr;
    m_deviceUIs.back()->m_deviceMIMOEngine = nullptr;

    char tabNameCStr[16];
    sprintf(tabNameCStr, "R%d", deviceTabIndex);

    DeviceAPI *deviceAPI = new DeviceAPI(DeviceAPI::StreamSingleRx, deviceTabIndex, dspDeviceSourceEngine, nullptr, nullptr);

    m_deviceUIs.back()->m_deviceAPI = deviceAPI;
    m_deviceUIs.back()->m_samplingDeviceControl->setPluginManager(m_pluginManager);
    QList<QString> channelNames;
    m_pluginManager->listRxChannels(channelNames);
    QStringList channelNamesList(channelNames);
    m_deviceUIs.back()->m_samplingDeviceControl->getChannelSelector()->addItems(channelNamesList);
    m_deviceUIs.back()->setNumberOfAvailableRxChannels(channelNamesList.size());

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
        deviceIndex = DeviceEnumerator::instance()->getFileInputDeviceIndex();
    }

    const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(deviceIndex);
    m_deviceUIs.back()->m_deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
    m_deviceUIs.back()->m_deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
    m_deviceUIs.back()->m_deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
    m_deviceUIs.back()->m_deviceAPI->setHardwareId(samplingDevice->hardwareId);
    m_deviceUIs.back()->m_deviceAPI->setSamplingDeviceId(samplingDevice->id);
    m_deviceUIs.back()->m_deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
    m_deviceUIs.back()->m_deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
    m_deviceUIs.back()->m_deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getRxPluginInterface(deviceIndex));

    QString userArgs = m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

    if (userArgs.size() > 0) {
        m_deviceUIs.back()->m_deviceAPI->setHardwareUserArguments(userArgs);
    }

    m_deviceUIs.back()->m_samplingDeviceControl->setSelectedDeviceIndex(deviceIndex);

    // delete previous plugin GUI
    m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceGUI(
            m_deviceUIs.back()->m_deviceAPI->getSamplingDevicePluginInstanceGUI());


    DeviceSampleSource *source = m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->createSampleSourcePluginInstance(
            m_deviceUIs.back()->m_deviceAPI->getSamplingDeviceId(), m_deviceUIs.back()->m_deviceAPI);
    m_deviceUIs.back()->m_deviceAPI->setSampleSource(source);
    QWidget *gui;
    PluginInstanceGUI *pluginGUI = m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->createSampleSourcePluginInstanceGUI(
            m_deviceUIs.back()->m_deviceAPI->getSamplingDeviceId(),
            &gui,
            m_deviceUIs.back());
    m_deviceUIs.back()->m_deviceAPI->getSampleSource()->setMessageQueueToGUI(pluginGUI->getInputMessageQueue());
    m_deviceUIs.back()->m_deviceAPI->setSamplingDevicePluginInstanceGUI(pluginGUI);
    m_deviceUIs.back()->m_deviceAPI->getSampleSource()->init();
    setDeviceGUI(deviceTabIndex, gui, m_deviceUIs.back()->m_deviceAPI->getSamplingDeviceDisplayName());
}

void MainWindow::addSinkDevice()
{
    DSPDeviceSinkEngine *dspDeviceSinkEngine = m_dspEngine->addDeviceSinkEngine();
    dspDeviceSinkEngine->start();

    uint dspDeviceSinkEngineUID =  dspDeviceSinkEngine->getUID();
    char uidCStr[16];
    sprintf(uidCStr, "UID:%d", dspDeviceSinkEngineUID);

    int deviceTabIndex = m_deviceUIs.size();
    m_deviceUIs.push_back(new DeviceUISet(deviceTabIndex, 1, m_masterTimer));
    m_deviceUIs.back()->m_deviceSourceEngine = nullptr;
    m_deviceUIs.back()->m_deviceSinkEngine = dspDeviceSinkEngine;
    m_deviceUIs.back()->m_deviceMIMOEngine = nullptr;

    char tabNameCStr[16];
    sprintf(tabNameCStr, "T%d", deviceTabIndex);

    DeviceAPI *deviceAPI = new DeviceAPI(DeviceAPI::StreamSingleTx, deviceTabIndex, nullptr, dspDeviceSinkEngine, nullptr);

    m_deviceUIs.back()->m_deviceAPI = deviceAPI;
    m_deviceUIs.back()->m_samplingDeviceControl->setPluginManager(m_pluginManager);
    QList<QString> channelNames;
    m_pluginManager->listTxChannels(channelNames);
    QStringList channelNamesList(channelNames);
    m_deviceUIs.back()->m_samplingDeviceControl->getChannelSelector()->addItems(channelNamesList);
    m_deviceUIs.back()->setNumberOfAvailableTxChannels(channelNamesList.size());

    connect(m_deviceUIs.back()->m_samplingDeviceControl->getAddChannelButton(), SIGNAL(clicked(bool)), this, SLOT(channelAddClicked(bool)));

    dspDeviceSinkEngine->addSpectrumSink(m_deviceUIs.back()->m_spectrumVis);
    m_deviceUIs.back()->m_spectrum->setDisplayedStream(false, 0);
    ui->tabSpectra->addTab(m_deviceUIs.back()->m_spectrum, tabNameCStr);
    ui->tabSpectraGUI->addTab(m_deviceUIs.back()->m_spectrumGUI, tabNameCStr);
    ui->tabChannels->addTab(m_deviceUIs.back()->m_channelWindow, tabNameCStr);

    connect(m_deviceUIs.back()->m_samplingDeviceControl, SIGNAL(changed()), this, SLOT(sampleSinkChanged()));

    ui->tabInputsSelect->addTab(m_deviceUIs.back()->m_samplingDeviceControl, tabNameCStr);
    ui->tabInputsSelect->setTabToolTip(deviceTabIndex, QString(uidCStr));

    // create a file sink by default
    int fileSinkDeviceIndex = DeviceEnumerator::instance()->getFileSinkDeviceIndex();
    const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(fileSinkDeviceIndex);
    m_deviceUIs.back()->m_deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
    m_deviceUIs.back()->m_deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
    m_deviceUIs.back()->m_deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
    m_deviceUIs.back()->m_deviceAPI->setHardwareId(samplingDevice->hardwareId);
    m_deviceUIs.back()->m_deviceAPI->setSamplingDeviceId(samplingDevice->id);
    m_deviceUIs.back()->m_deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
    m_deviceUIs.back()->m_deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
    m_deviceUIs.back()->m_deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getTxPluginInterface(fileSinkDeviceIndex));

    QString userArgs = m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

    if (userArgs.size() > 0) {
        m_deviceUIs.back()->m_deviceAPI->setHardwareUserArguments(userArgs);
    }

    m_deviceUIs.back()->m_samplingDeviceControl->setSelectedDeviceIndex(fileSinkDeviceIndex);

    // delete previous plugin GUI if it exists
    m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceGUI(
            m_deviceUIs.back()->m_deviceAPI->getSamplingDevicePluginInstanceGUI());

    DeviceSampleSink *sink = m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->createSampleSinkPluginInstance(
            m_deviceUIs.back()->m_deviceAPI->getSamplingDeviceId(), m_deviceUIs.back()->m_deviceAPI);
    m_deviceUIs.back()->m_deviceAPI->setSampleSink(sink);
    QWidget *gui;
    PluginInstanceGUI *pluginUI = m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->createSampleSinkPluginInstanceGUI(
            m_deviceUIs.back()->m_deviceAPI->getSamplingDeviceId(),
            &gui,
            m_deviceUIs.back());
    m_deviceUIs.back()->m_deviceAPI->getSampleSink()->setMessageQueueToGUI(pluginUI->getInputMessageQueue());
    m_deviceUIs.back()->m_deviceAPI->setSamplingDevicePluginInstanceGUI(pluginUI);
    m_deviceUIs.back()->m_deviceAPI->getSampleSink()->init();
    setDeviceGUI(deviceTabIndex, gui, m_deviceUIs.back()->m_deviceAPI->getSamplingDeviceDisplayName(), 1);
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
        m_deviceUIs.back()->freeChannels();      // destroys the channel instances
        m_deviceUIs.back()->m_deviceAPI->getSampleSource()->setMessageQueueToGUI(0); // have source stop sending messages to the GUI
        m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceGUI(
                m_deviceUIs.back()->m_deviceAPI->getSamplingDevicePluginInstanceGUI());
        m_deviceUIs.back()->m_deviceAPI->resetSamplingDeviceId();
        m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceInput(
                m_deviceUIs.back()->m_deviceAPI->getSampleSource());
        m_deviceUIs.back()->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists
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

	    DeviceAPI *sourceAPI = m_deviceUIs.back()->m_deviceAPI;
	    delete m_deviceUIs.back();

	    lastDeviceEngine->stop();
	    m_dspEngine->removeLastDeviceSourceEngine();

	    delete sourceAPI;
	}
	else if (m_deviceUIs.back()->m_deviceSinkEngine) // sink tab
	{
	    DSPDeviceSinkEngine *lastDeviceEngine = m_deviceUIs.back()->m_deviceSinkEngine;
	    lastDeviceEngine->stopGeneration();
	    lastDeviceEngine->removeSpectrumSink(m_deviceUIs.back()->m_spectrumVis);

	    ui->tabSpectraGUI->removeTab(ui->tabSpectraGUI->count() - 1);
	    ui->tabSpectra->removeTab(ui->tabSpectra->count() - 1);

        // deletes old UI and output object
        m_deviceUIs.back()->freeChannels();
        m_deviceUIs.back()->m_deviceAPI->getSampleSink()->setMessageQueueToGUI(0); // have sink stop sending messages to the GUI
	    m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceGUI(
	            m_deviceUIs.back()->m_deviceAPI->getSamplingDevicePluginInstanceGUI());
	    m_deviceUIs.back()->m_deviceAPI->resetSamplingDeviceId();
	    m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput(
	            m_deviceUIs.back()->m_deviceAPI->getSampleSink());
        m_deviceUIs.back()->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists
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

	    DeviceAPI *sinkAPI = m_deviceUIs.back()->m_deviceAPI;
	    delete m_deviceUIs.back();

	    lastDeviceEngine->stop();
	    m_dspEngine->removeLastDeviceSinkEngine();

	    delete sinkAPI;
	}
	else if (m_deviceUIs.back()->m_deviceMIMOEngine) // MIMO tab
	{
	    DSPDeviceMIMOEngine *lastDeviceEngine = m_deviceUIs.back()->m_deviceMIMOEngine;
	    lastDeviceEngine->stopProcess(1); // Tx side
        lastDeviceEngine->stopProcess(0); // Rx side
	    lastDeviceEngine->removeSpectrumSink(m_deviceUIs.back()->m_spectrumVis);

	    ui->tabSpectraGUI->removeTab(ui->tabSpectraGUI->count() - 1);
	    ui->tabSpectra->removeTab(ui->tabSpectra->count() - 1);

        // deletes old UI and output object
        m_deviceUIs.back()->freeChannels();
        m_deviceUIs.back()->m_deviceAPI->getSampleMIMO()->setMessageQueueToGUI(nullptr); // have sink stop sending messages to the GUI
	    m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->deleteSampleMIMOPluginInstanceGUI(
	            m_deviceUIs.back()->m_deviceAPI->getSamplingDevicePluginInstanceGUI());
	    m_deviceUIs.back()->m_deviceAPI->resetSamplingDeviceId();
	    m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->deleteSampleMIMOPluginInstanceMIMO(
	            m_deviceUIs.back()->m_deviceAPI->getSampleMIMO());
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

	    DeviceAPI *mimoAPI = m_deviceUIs.back()->m_deviceAPI;
	    delete m_deviceUIs.back();

	    lastDeviceEngine->stop();
	    m_dspEngine->removeLastDeviceMIMOEngine();

	    delete mimoAPI;
	}

    m_deviceUIs.pop_back();
}

void MainWindow::deleteChannel(int deviceSetIndex, int channelIndex)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_deviceUIs[deviceSetIndex];
        deviceSet->deleteChannel(channelIndex);
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

void MainWindow::setDeviceGUI(int deviceTabIndex, QWidget* gui, const QString& deviceDisplayName, int deviceType)
{
    char tabNameCStr[16];

    if (deviceType == 0) {
        sprintf(tabNameCStr, "R%d", deviceTabIndex);
    } else if (deviceType == 1) {
        sprintf(tabNameCStr, "T%d", deviceTabIndex);
    } else if (deviceType == 2) {
        sprintf(tabNameCStr, "M%d", deviceTabIndex);
    }

    qDebug("MainWindow::setDeviceGUI: insert device type %d tab at %d", deviceType, deviceTabIndex);

    if (deviceTabIndex < m_deviceWidgetTabs.size()) {
        m_deviceWidgetTabs[deviceTabIndex] = {gui, deviceDisplayName, QString(tabNameCStr)};
    } else {
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
        deviceUI->m_spectrumGUI->deserialize(preset->getSpectrumConfig());
        deviceUI->m_deviceAPI->loadSamplingDeviceSettings(preset);

        if (deviceUI->m_deviceSourceEngine) { // source device
            deviceUI->loadRxChannelSettings(preset, m_pluginManager->getPluginAPI());
        } else if (deviceUI->m_deviceSinkEngine) { // sink device
            deviceUI->loadTxChannelSettings(preset, m_pluginManager->getPluginAPI());
        } else if (deviceUI->m_deviceMIMOEngine) { // MIMO device
            deviceUI->loadMIMOChannelSettings(preset, m_pluginManager->getPluginAPI());
        }
	}

	// has to be last step
    if (!preset->getLayout().isEmpty()) {
	    restoreState(preset->getLayout());
    }

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
        preset->setSourcePreset();
        deviceUI->saveRxChannelSettings(preset);
        deviceUI->m_deviceAPI->saveSamplingDeviceSettings(preset);
    }
    else if (deviceUI->m_deviceSinkEngine) // sink device
    {
        preset->setSpectrumConfig(deviceUI->m_spectrumGUI->serialize());
        preset->clearChannels();
        preset->setSinkPreset();
        deviceUI->saveTxChannelSettings(preset);
        deviceUI->m_deviceAPI->saveSamplingDeviceSettings(preset);
    }
    else if (deviceUI->m_deviceMIMOEngine) // MIMO device
    {
        preset->setSpectrumConfig(deviceUI->m_spectrumGUI->serialize());
        preset->clearChannels();
        preset->setMIMOPreset();
        deviceUI->saveMIMOChannelSettings(preset);
        deviceUI->m_deviceAPI->saveSamplingDeviceSettings(preset);
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
    m_showSystemWidget = new QLabel("SDRangel " + qApp->applicationVersion() + " " + qtVersionStr
            + QSysInfo::currentCpuArchitecture() + " " + QSysInfo::prettyProductName(), this);
#else
    m_showSystemWidget = new QLabel("SDRangel " + qApp->applicationVersion() + " " + qtVersionStr, this);
#endif
    statusBar()->addPermanentWidget(m_showSystemWidget);

	m_dateTimeWidget = new QLabel(tr("Date"), this);
	m_dateTimeWidget->setToolTip(tr("Current date/time"));
	statusBar()->addPermanentWidget(m_dateTimeWidget);
}

void MainWindow::closeEvent(QCloseEvent *closeEvent)
{
    qDebug("MainWindow::closeEvent");

    savePresetSettings(m_settings.getWorkingPreset(), 0);
    m_settings.save();

    while (m_deviceUIs.size() > 0)
    {
        removeLastDevice();
    }

    closeEvent->accept();
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
	sl.append(QString("%1").arg(preset->isSourcePreset() ? 'R' : preset->isSinkPreset() ? 'T' : preset->isMIMOPreset() ? 'M' : 'X'));           // mode column
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
 	loadPresetSettings(m_settings.getWorkingPreset(), 0);

    m_settings.sortPresets();
    int middleIndex = m_settings.getPresetCount() / 2;
    QTreeWidgetItem *treeItem;
    ui->presetTree->clear();

    for (int i = 0; i < m_settings.getPresetCount(); ++i)
    {
        treeItem = addPresetToTree(m_settings.getPreset(i));

        if (i == middleIndex) {
            ui->presetTree->setCurrentItem(treeItem);
        }
    }

    m_settings.sortCommands();
    ui->commandTree->clear();

    for (int i = 0; i < m_settings.getCommandCount(); ++i) {
        treeItem = addCommandToTree(m_settings.getCommand(i));
    }

    setLoggingOptions();
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
                        (preset->getPresetType() == presetToDelete->getPresetType()))
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
        int direction = notif.getDirection();

        if (direction == 1) { // Single stream Tx
            addSinkDevice();
        } else if (direction == 0) { // Single stream Rx
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

        if (notif.getDeviceType() == 1) {
            sampleSinkChanged();
        } else if (notif.getDeviceType() == 0) {
            sampleSourceChanged();
        } else if (notif.getDeviceType() == 2) {
            sampleMIMOChanged();
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
    else if (MsgApplySettings::match(cmd))
    {
        applySettings();
        return true;
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

void MainWindow::commandKeysConnect(QObject *object, const char *slot)
{
    setFocus();
    connect(m_commandKeyReceiver, SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
            object, slot);
}

void MainWindow::commandKeysDisconnect(QObject *object, const char *slot)
{
    disconnect(m_commandKeyReceiver, SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
            object, slot);
}

void MainWindow::on_commandKeyboardConnect_toggled(bool checked)
{
    qDebug("on_commandKeyboardConnect_toggled: %s", checked ? "true" : "false");

    if (checked)
    {
        commandKeysConnect(this, SLOT(commandKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
    }
    else
    {
        commandKeysDisconnect(this, SLOT(commandKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
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
			    tr("Open preset export file"), ".", tr("Preset export files (*.prex)"), 0, QFileDialog::DontUseNativeDialog);

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
		    tr("Open preset export file"), ".", tr("Preset export files (*.prex)"), 0, QFileDialog::DontUseNativeDialog);

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

                ui->presetTree->clear();

                for (int i = 0; i < m_settings.getPresetCount(); ++i) {
                    addPresetToTree(m_settings.getPreset(i));
                }
            }
        }
	}
}

void MainWindow::on_presetTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    (void) current;
    (void) previous;
	updatePresetControls();
}

void MainWindow::on_presetTree_itemActivated(QTreeWidgetItem *item, int column)
{
    (void) item;
    (void) column;
	on_presetLoad_clicked();
}

void MainWindow::on_action_Loaded_Plugins_triggered()
{
    PluginsDialog pluginsDialog(m_pluginManager, this);
    pluginsDialog.exec();
}

void MainWindow::on_action_Audio_triggered()
{
	AudioDialogX audioDialog(m_dspEngine->getAudioDeviceManager(), this);
	audioDialog.exec();
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

void MainWindow::on_action_DeviceUserArguments_triggered()
{
    qDebug("MainWindow::on_action_DeviceUserArguments_triggered");
    DeviceUserArgsDialog deviceUserArgsDialog(DeviceEnumerator::instance(), m_settings.getDeviceUserArgs(), this);
    deviceUserArgsDialog.exec();
}

void MainWindow::on_action_AMBE_triggered()
{
    qDebug("MainWindow::on_action_AMBE_triggered");
#ifndef __APPLE__
    AMBEDevicesDialog ambeDevicesDialog(m_dspEngine->getAMBEEngine(), this);
    ambeDevicesDialog.exec();
#endif
}

void MainWindow::on_action_LimeRFE_triggered()
{
    qDebug("MainWindow::on_action_LimeRFE_triggered");
#if defined(HAS_LIMERFEUSB)
    qDebug("MainWindow::on_action_LimeRFE_triggered: activated");
    LimeRFEUSBDialog *limeRFEUSBDialog = new LimeRFEUSBDialog(m_settings.getLimeRFEUSBCalib(), this);
    limeRFEUSBDialog->setModal(false);
    limeRFEUSBDialog->show();
#endif
}

void MainWindow::sampleSourceChanged()
{
    // Do it in the currently selected source tab
    int currentSourceTabIndex = ui->tabInputsSelect->currentIndex();

    if (currentSourceTabIndex >= 0)
    {
        qDebug("MainWindow::sampleSourceChanged: tab at %d", currentSourceTabIndex);
        DeviceUISet *deviceUI = m_deviceUIs[currentSourceTabIndex];
        deviceUI->m_deviceAPI->saveSamplingDeviceSettings(m_settings.getWorkingPreset()); // save old API settings
        deviceUI->m_deviceAPI->stopDeviceEngine();

        // deletes old UI and input object
        deviceUI->m_deviceAPI->getSampleSource()->setMessageQueueToGUI(nullptr); // have source stop sending messages to the GUI
        deviceUI->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceGUI(
                deviceUI->m_deviceAPI->getSamplingDevicePluginInstanceGUI());
        deviceUI->m_deviceAPI->resetSamplingDeviceId();
        deviceUI->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceInput(
                deviceUI->m_deviceAPI->getSampleSource());
        deviceUI->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

        const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(
            deviceUI->m_samplingDeviceControl->getSelectedDeviceIndex());
        deviceUI->m_deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
        deviceUI->m_deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
        deviceUI->m_deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
        deviceUI->m_deviceAPI->setHardwareId(samplingDevice->hardwareId);
        deviceUI->m_deviceAPI->setSamplingDeviceId(samplingDevice->id);
        deviceUI->m_deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
        deviceUI->m_deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
        deviceUI->m_deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getRxPluginInterface(deviceUI->m_samplingDeviceControl->getSelectedDeviceIndex()));

        QString userArgs = m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

        if (userArgs.size() > 0) {
            deviceUI->m_deviceAPI->setHardwareUserArguments(userArgs);
        }

        // add to buddies list
        std::vector<DeviceUISet*>::iterator it = m_deviceUIs.begin();
        int nbOfBuddies = 0;

        for (; it != m_deviceUIs.end(); ++it)
        {
            if (*it != deviceUI) // do not add to itself
            {
                if ((*it)->m_deviceSourceEngine) // it is a source device
                {
                    if ((deviceUI->m_deviceAPI->getHardwareId() == (*it)->m_deviceAPI->getHardwareId()) &&
                        (deviceUI->m_deviceAPI->getSamplingDeviceSerial() == (*it)->m_deviceAPI->getSamplingDeviceSerial()))
                    {
                        (*it)->m_deviceAPI->addSourceBuddy(deviceUI->m_deviceAPI);
                        nbOfBuddies++;
                    }
                }

                if ((*it)->m_deviceSinkEngine) // it is a sink device
                {
                    if ((deviceUI->m_deviceAPI->getHardwareId() == (*it)->m_deviceAPI->getHardwareId()) &&
                        (deviceUI->m_deviceAPI->getSamplingDeviceSerial() == (*it)->m_deviceAPI->getSamplingDeviceSerial()))
                    {
                        (*it)->m_deviceAPI->addSourceBuddy(deviceUI->m_deviceAPI);
                        nbOfBuddies++;
                    }
                }
            }
        }

        if (nbOfBuddies == 0) {
            deviceUI->m_deviceAPI->setBuddyLeader(true);
        }

        // constructs new GUI and input object
        DeviceSampleSource *source = deviceUI->m_deviceAPI->getPluginInterface()->createSampleSourcePluginInstance(
                deviceUI->m_deviceAPI->getSamplingDeviceId(), deviceUI->m_deviceAPI);
        deviceUI->m_deviceAPI->setSampleSource(source);
        QWidget *gui;
        PluginInstanceGUI *pluginUI = deviceUI->m_deviceAPI->getPluginInterface()->createSampleSourcePluginInstanceGUI(
                deviceUI->m_deviceAPI->getSamplingDeviceId(),
                &gui,
                deviceUI);
        deviceUI->m_deviceAPI->getSampleSource()->setMessageQueueToGUI(pluginUI->getInputMessageQueue());
        deviceUI->m_deviceAPI->setSamplingDevicePluginInstanceGUI(pluginUI);
        setDeviceGUI(currentSourceTabIndex, gui, deviceUI->m_deviceAPI->getSamplingDeviceDisplayName());
        deviceUI->m_deviceAPI->getSampleSource()->init();

        deviceUI->m_deviceAPI->loadSamplingDeviceSettings(m_settings.getWorkingPreset()); // load new API settings

        if (currentSourceTabIndex == 0) // save as default starting device
        {
            m_settings.setSourceIndex(samplingDevice->sequence);
            m_settings.setSourceDeviceId(samplingDevice->id);
        }
    }
}

void MainWindow::sampleSinkChanged()
{
    // Do it in the currently selected source tab
    int currentSinkTabIndex = ui->tabInputsSelect->currentIndex();

    if (currentSinkTabIndex >= 0)
    {
        qDebug("MainWindow::sampleSinkChanged: tab at %d", currentSinkTabIndex);
        DeviceUISet *deviceUI = m_deviceUIs[currentSinkTabIndex];
        deviceUI->m_deviceAPI->saveSamplingDeviceSettings(m_settings.getWorkingPreset()); // save old API settings
        deviceUI->m_deviceAPI->stopDeviceEngine();

        // deletes old UI and output object
        deviceUI->m_deviceAPI->getSampleSink()->setMessageQueueToGUI(0); // have sink stop sending messages to the GUI
        deviceUI->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceGUI(
                deviceUI->m_deviceAPI->getSamplingDevicePluginInstanceGUI());
        deviceUI->m_deviceAPI->resetSamplingDeviceId();
        deviceUI->m_deviceAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput(
                deviceUI->m_deviceAPI->getSampleSink());
        deviceUI->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

        const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(deviceUI->m_samplingDeviceControl->getSelectedDeviceIndex());
        deviceUI->m_deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
        deviceUI->m_deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
        deviceUI->m_deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
        deviceUI->m_deviceAPI->setHardwareId(samplingDevice->hardwareId);
        deviceUI->m_deviceAPI->setSamplingDeviceId(samplingDevice->id);
        deviceUI->m_deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
        deviceUI->m_deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
        deviceUI->m_deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getTxPluginInterface(deviceUI->m_samplingDeviceControl->getSelectedDeviceIndex()));

        QString userArgs = m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

        if (userArgs.size() > 0) {
            deviceUI->m_deviceAPI->setHardwareUserArguments(userArgs);
        }

        // add to buddies list
        std::vector<DeviceUISet*>::iterator it = m_deviceUIs.begin();
        int nbOfBuddies = 0;

        for (; it != m_deviceUIs.end(); ++it)
        {
            if (*it != deviceUI) // do not add to itself
            {
                if ((*it)->m_deviceSourceEngine) // it is a source device
                {
                    if ((deviceUI->m_deviceAPI->getHardwareId() == (*it)->m_deviceAPI->getHardwareId()) &&
                        (deviceUI->m_deviceAPI->getSamplingDeviceSerial() == (*it)->m_deviceAPI->getSamplingDeviceSerial()))
                    {
                        (*it)->m_deviceAPI->addSinkBuddy(deviceUI->m_deviceAPI);
                        nbOfBuddies++;
                    }
                }

                if ((*it)->m_deviceSinkEngine) // it is a sink device
                {
                    if ((deviceUI->m_deviceAPI->getHardwareId() == (*it)->m_deviceAPI->getHardwareId()) &&
                        (deviceUI->m_deviceAPI->getSamplingDeviceSerial() == (*it)->m_deviceAPI->getSamplingDeviceSerial()))
                    {
                        (*it)->m_deviceAPI->addSinkBuddy(deviceUI->m_deviceAPI);
                        nbOfBuddies++;
                    }
                }
            }
        }

        if (nbOfBuddies == 0) {
            deviceUI->m_deviceAPI->setBuddyLeader(true);
        }

        // constructs new GUI and output object
        DeviceSampleSink *sink = deviceUI->m_deviceAPI->getPluginInterface()->createSampleSinkPluginInstance(
                deviceUI->m_deviceAPI->getSamplingDeviceId(), deviceUI->m_deviceAPI);
        deviceUI->m_deviceAPI->setSampleSink(sink);
        QWidget *gui;
        PluginInstanceGUI *pluginUI = deviceUI->m_deviceAPI->getPluginInterface()->createSampleSinkPluginInstanceGUI(
                deviceUI->m_deviceAPI->getSamplingDeviceId(),
                &gui,
                deviceUI);
        deviceUI->m_deviceAPI->getSampleSink()->setMessageQueueToGUI(pluginUI->getInputMessageQueue());
        deviceUI->m_deviceAPI->setSamplingDevicePluginInstanceGUI(pluginUI);
        setDeviceGUI(currentSinkTabIndex, gui, deviceUI->m_deviceAPI->getSamplingDeviceDisplayName(), 1);
        m_deviceUIs.back()->m_deviceAPI->getSampleSink()->init();

        deviceUI->m_deviceAPI->loadSamplingDeviceSettings(m_settings.getWorkingPreset()); // load new API settings
    }
}

void MainWindow::sampleMIMOChanged()
{
    // Do it in the currently selected source tab
    int currentMIMOTabIndex = ui->tabInputsSelect->currentIndex();

    if (currentMIMOTabIndex >= 0)
    {
        qDebug("MainWindow::sampleMIMOChanged: tab at %d", currentMIMOTabIndex);
        DeviceUISet *deviceUI = m_deviceUIs[currentMIMOTabIndex];
        deviceUI->m_deviceAPI->saveSamplingDeviceSettings(m_settings.getWorkingPreset()); // save old API settings
        deviceUI->m_deviceAPI->stopDeviceEngine();

        // deletes old UI and output object
        deviceUI->m_deviceAPI->getSampleMIMO()->setMessageQueueToGUI(nullptr); // have sink stop sending messages to the GUI
        deviceUI->m_deviceAPI->getPluginInterface()->deleteSampleMIMOPluginInstanceGUI(
                deviceUI->m_deviceAPI->getSamplingDevicePluginInstanceGUI());
        deviceUI->m_deviceAPI->resetSamplingDeviceId();
        deviceUI->m_deviceAPI->getPluginInterface()->deleteSampleMIMOPluginInstanceMIMO(
                deviceUI->m_deviceAPI->getSampleMIMO());

        const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getMIMOSamplingDevice(
            deviceUI->m_samplingDeviceControl->getSelectedDeviceIndex());
        deviceUI->m_deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
        deviceUI->m_deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
        deviceUI->m_deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
        deviceUI->m_deviceAPI->setHardwareId(samplingDevice->hardwareId);
        deviceUI->m_deviceAPI->setSamplingDeviceId(samplingDevice->id);
        deviceUI->m_deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
        deviceUI->m_deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
        deviceUI->m_deviceAPI->setSamplingDevicePluginInterface(
            DeviceEnumerator::instance()->getMIMOPluginInterface(deviceUI->m_samplingDeviceControl->getSelectedDeviceIndex()));

        QString userArgs = m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

        if (userArgs.size() > 0) {
            deviceUI->m_deviceAPI->setHardwareUserArguments(userArgs);
        }

        // constructs new GUI and MIMO object
        DeviceSampleMIMO *mimo = deviceUI->m_deviceAPI->getPluginInterface()->createSampleMIMOPluginInstance(
                deviceUI->m_deviceAPI->getSamplingDeviceId(), deviceUI->m_deviceAPI);
        deviceUI->m_deviceAPI->setSampleMIMO(mimo);
        QWidget *gui;
        PluginInstanceGUI *pluginUI = deviceUI->m_deviceAPI->getPluginInterface()->createSampleMIMOPluginInstanceGUI(
                deviceUI->m_deviceAPI->getSamplingDeviceId(),
                &gui,
                deviceUI);
        deviceUI->m_deviceAPI->getSampleMIMO()->setMessageQueueToGUI(pluginUI->getInputMessageQueue());
        deviceUI->m_deviceAPI->setSamplingDevicePluginInstanceGUI(pluginUI);
        setDeviceGUI(currentMIMOTabIndex, gui, deviceUI->m_deviceAPI->getSamplingDeviceDisplayName(), 2);
        m_deviceUIs.back()->m_deviceAPI->getSampleMIMO()->init();

        deviceUI->m_deviceAPI->loadSamplingDeviceSettings(m_settings.getWorkingPreset()); // load new API settings
    }
}

void MainWindow::channelAddClicked(bool checked)
{
    (void) checked;
    // Do it in the currently selected source tab
    int currentSourceTabIndex = ui->tabInputsSelect->currentIndex();

    if (currentSourceTabIndex >= 0)
    {
        DeviceUISet *deviceUI = m_deviceUIs[currentSourceTabIndex];

        if (deviceUI->m_deviceSourceEngine) // source device => Rx channels
        {
            m_pluginManager->createRxChannelInstance(
                deviceUI->m_samplingDeviceControl->getChannelSelector()->currentIndex(), deviceUI, deviceUI->m_deviceAPI);
        }
        else if (deviceUI->m_deviceSinkEngine) // sink device => Tx channels
        {
            m_pluginManager->createTxChannelInstance(
                deviceUI->m_samplingDeviceControl->getChannelSelector()->currentIndex(), deviceUI, deviceUI->m_deviceAPI);
        }
        else if (deviceUI->m_deviceMIMOEngine) // MIMO device => all possible channels. Depends on index range
        {
            int nbRxChannels = deviceUI->getNumberOfAvailableRxChannels();
            int nbTxChannels = deviceUI->getNumberOfAvailableTxChannels();
            int selectedIndex = deviceUI->m_samplingDeviceControl->getChannelSelector()->currentIndex();
            qDebug("MainWindow::channelAddClicked: MIMO: tab: %d nbRx: %d nbTx: %d selected: %d",
                currentSourceTabIndex, nbRxChannels, nbTxChannels, selectedIndex);

            if (selectedIndex < nbRxChannels) {
                m_pluginManager->createRxChannelInstance(
                    selectedIndex, deviceUI, deviceUI->m_deviceAPI);
            } else if (selectedIndex < nbRxChannels + nbTxChannels) {
                m_pluginManager->createTxChannelInstance(
                    selectedIndex - nbRxChannels, deviceUI, deviceUI->m_deviceAPI);
            }
        }
    }
}

void MainWindow::on_action_About_triggered()
{
	AboutDialog dlg(m_apiHost, m_apiPort, m_settings, this);
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

void MainWindow::on_action_addMIMODevice_triggered()
{
    QMessageBox::information(this, tr("Message"), tr("MIMO not supported in this version"));
}

void MainWindow::on_action_removeLastDevice_triggered()
{
    if (m_deviceUIs.size() > 1)
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
    m_dateTimeWidget->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss t"));
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
