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
#include <QStandardPaths>
#include <QDesktopServices>
#include <QProcess>

#include <QAction>
#include <QMenuBar>
#include <QStatusBar>

#include "device/devicegui.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "device/deviceset.h"
#include "device/deviceenumerator.h"
#include "channel/channelapi.h"
#include "channel/channelgui.h"
#include "feature/featureuiset.h"
#include "feature/featureset.h"
#include "feature/feature.h"
#include "feature/featuregui.h"
#include "mainspectrum/mainspectrumgui.h"
#include "commands/commandkeyreceiver.h"
#include "gui/indicator.h"
#include "gui/presetitem.h"
#include "gui/addpresetdialog.h"
#include "gui/pluginsdialog.h"
#include "gui/aboutdialog.h"
#include "gui/rollupwidget.h"
#include "gui/channelwindow.h"
#include "gui/featurewindow.h"
#include "gui/audiodialog.h"
#include "gui/loggingdialog.h"
#include "gui/deviceuserargsdialog.h"
#include "gui/sdrangelsplash.h"
#include "gui/mypositiondialog.h"
#include "gui/fftwisdomdialog.h"
#include "gui/ambedevicesdialog.h"
#include "gui/workspace.h"
#include "gui/featurepresetsdialog.h"
#include "gui/devicesetpresetsdialog.h"
#include "gui/commandsdialog.h"
#include "gui/configurationsdialog.h"
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
#include "webapi/webapiadapter.h"
#include "commands/command.h"

#include "mainwindow.h"

#include <audio/audiodevicemanager.h>

//#include "ui_mainwindow.h"
#include <QtWidgets/QApplication>

#include <string>
#include <QDebug>
#include <QSplashScreen>
#include <QProgressDialog>

#if defined(HAS_LIMERFEUSB)
#include "limerfegui/limerfeusbdialog.h"
#endif

MainWindow *MainWindow::m_instance = 0;

MainWindow::MainWindow(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QWidget* parent) :
	QMainWindow(parent),
	// ui(new Ui::MainWindow),
    m_currentWorkspace(nullptr),
    m_mainCore(MainCore::instance()),
	m_dspEngine(DSPEngine::instance()),
	m_lastEngineState(DeviceAPI::StNotStarted),
    m_fftWisdomProcess(nullptr)
{
	qDebug() << "MainWindow::MainWindow: start";
    setWindowTitle("SDRangel v7");

    m_instance = this;
    m_mainCore->m_logger = logger;
    m_mainCore->m_masterTabIndex = 0;
    m_mainCore->m_mainMessageQueue = &m_inputMessageQueue;
	m_mainCore->m_settings.setAudioDeviceManager(m_dspEngine->getAudioDeviceManager());
    m_mainCore->m_settings.setAMBEEngine(m_dspEngine->getAMBEEngine());

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

	// ui->setupUi(this);
    setWindowIcon(QIcon(":/sdrangel_icon.png"));
    createMenuBar();
	createStatusBar();

    setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::West);
    setTabPosition(Qt::RightDockWidgetArea, QTabWidget::East);
	setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

	// work around broken Qt dock widget ordering
    // removeDockWidget(ui->inputViewDock);
	// removeDockWidget(ui->spectraControlDock);
	// removeDockWidget(ui->presetDock);
    // removeDockWidget(ui->commandsDock);
	// removeDockWidget(ui->channelDock);
    // removeDockWidget(ui->featureDock);
    // addDockWidget(Qt::LeftDockWidgetArea, ui->inputViewDock);
	// addDockWidget(Qt::LeftDockWidgetArea, ui->spectraControlDock);
	// addDockWidget(Qt::LeftDockWidgetArea, ui->presetDock);
    // addDockWidget(Qt::LeftDockWidgetArea, ui->commandsDock);
    // tabifyDockWidget(ui->presetDock, ui->commandsDock);
	// addDockWidget(Qt::RightDockWidgetArea, ui->channelDock);
	// addDockWidget(Qt::RightDockWidgetArea, ui->featureDock);

	// ui->inputViewDock->show();
	// ui->spectraControlDock->show();
	// ui->presetDock->show();
	// ui->commandsDock->show();
	// ui->channelDock->show();
    // ui->featureDock->show();

    // m_spectrumToggleViewAction = new QAction(tr("Spectrum display"));
    // m_spectrumToggleViewAction->setCheckable(true);
    // m_spectrumToggleViewAction->setChecked(true);
    // connect(m_spectrumToggleViewAction, SIGNAL(toggled(bool)), this, SLOT(toggleSpectrumView(bool)));

    // ui->menu_Window->addAction(ui->inputViewDock->toggleViewAction());
	// ui->menu_Window->addAction(ui->spectraControlDock->toggleViewAction());
    // ui->menu_Window->addAction(m_spectrumToggleViewAction);
	// ui->menu_Window->addAction(ui->presetDock->toggleViewAction());
    // ui->menu_Window->addAction(ui->commandsDock->toggleViewAction());
	// ui->menu_Window->addAction(ui->channelDock->toggleViewAction());
    // ui->menu_Window->addAction(ui->featureDock->toggleViewAction());

    // ui->spectraControlDock->setStyleSheet("QAbstractButton#qt_dockwidget_closebutton{qproperty-toolTip: \"Close\";}");
    // ui->spectraControlDock->setStyleSheet("QAbstractButton#qt_dockwidget_floatbutton{qproperty-toolTip: \"Dock/undock\";}");
    // ui->presetDock->setStyleSheet("QAbstractButton#qt_dockwidget_closebutton{qproperty-toolTip: \"Close\";}");
    // ui->presetDock->setStyleSheet("QAbstractButton#qt_dockwidget_floatbutton{qproperty-toolTip: \"Dock/undock\";}");

    // ui->tabInputsView->setStyleSheet("QWidget { background: rgb(50,50,50); } "
    //         "QToolButton::checked { background: rgb(128,70,0); } "
    //         "QComboBox::item:selected { color: rgb(255,140,0); } "
    //         "QTabWidget::pane { border: 1px solid #C06900; } "
    //         "QTabBar::tab:selected { background: rgb(128,70,0); }");

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()), Qt::QueuedConnection);

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(1000);

    splash->showStatusMessage("allocate FFTs...", Qt::white);
    splash->showStatusMessage("allocate FFTs...", Qt::white);

    if (parser.getFFTWFWisdomFileName().length() != 0)
    {
        m_dspEngine->createFFTFactory(parser.getFFTWFWisdomFileName());
    }
    else
    {
        QString filePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        filePath += QDir::separator();
        filePath += "fftw-wisdom";
        QFileInfo fileInfo = QFileInfo(filePath);

        if (fileInfo.exists()) {
            m_dspEngine->createFFTFactory(filePath);
        } else {
            m_dspEngine->createFFTFactory("");
        }
    }

    m_dspEngine->preAllocateFFTs();

    splash->showStatusMessage("load settings...", Qt::white);
    qDebug() << "MainWindow::MainWindow: load settings...";

    loadSettings();

    splash->showStatusMessage("load plugins...", Qt::white);
    qDebug() << "MainWindow::MainWindow: load plugins...";

    m_pluginManager = new PluginManager(this);
    m_mainCore->m_pluginManager = m_pluginManager;
    m_pluginManager->loadPlugins(QString("plugins"));
    m_pluginManager->loadPluginsNonDiscoverable(m_mainCore->m_settings.getDeviceUserArgs());

    splash->showStatusMessage("Add unique feature set...", Qt::white);
    // QStringList featureNames;
    // m_pluginManager->listFeatures(featureNames);
    // ui->featureDock->addAvailableFeatures(featureNames);
    addFeatureSet();
    // ui->featureDock->setFeatureUISet(m_featureUIs[0]);
    // ui->featureDock->setPresets(m_mainCore->m_settings.getFeatureSetPresets());
    // ui->featureDock->setPluginAPI(m_pluginManager->getPluginAPI());

    // splash->showStatusMessage("load last device or file input...", Qt::white);
    // qDebug() << "MainWindow::MainWindow: select SampleSource from settings or default (file input)...";
    // qDebug() << "MainWindow::MainWindow: look for"
    //     << m_mainCore->m_settings.getSourceDevice()
    //     << "at index" << m_mainCore->m_settings.getSourceIndex()
    //     << "and item index" << m_mainCore->m_settings.getSourceItemIndex();

	// int deviceIndex = DeviceEnumerator::instance()->getRxSamplingDeviceIndex(
    //     m_mainCore->m_settings.getSourceDevice(),
    //     m_mainCore->m_settings.getSourceIndex(),
    //     m_mainCore->m_settings.getSourceItemIndex());
	// sampleSourceAdd(deviceIndex);  // add the first device set with file input device as default if device in settings is not enumerated
	// m_deviceUIs.back()->m_deviceAPI->setBuddyLeader(true); // the first device is always the leader
    tabChannelsIndexChanged(); // force channel selection list update

	m_apiAdapter = new WebAPIAdapter();

    if (!parser.getScratch())
    {
        splash->showStatusMessage("load current configuration...", Qt::white);
        qDebug() << "MainWindow::MainWindow: load current configuration...";

        // loadDeviceSetPresetSettings(m_mainCore->m_settings.getWorkingPreset(), 0);
        // loadFeatureSetPresetSettings(m_mainCore->m_settings.getWorkingFeatureSetPreset(), 0);
        loadConfiguration(m_mainCore->m_settings.getWorkingConfiguration());
    }
    else
    {
        qDebug() << "MainWindow::MainWindow: scratch mode: do not load current configuration";
    }

    // splash->showStatusMessage("update preset controls...", Qt::white);
	// qDebug() << "MainWindow::MainWindow: update preset controls...";

	// updatePresetControls();

    splash->showStatusMessage("finishing...", Qt::white);
	// connect(ui->tabInputsView, SIGNAL(currentChanged(int)), this, SLOT(tabInputViewIndexChanged()));
    // connect(ui->tabChannels, SIGNAL(currentChanged(int)), this, SLOT(tabChannelsIndexChanged()));
    // connect(ui->channelDock, SIGNAL(addChannel(int)), this, SLOT(channelAddClicked(int)));
    // connect(ui->inputViewDock, SIGNAL(deviceChanged(int, int, int)), this, SLOT(samplingDeviceChanged(int, int, int)));
    // connect(ui->tabFeatures, SIGNAL(currentChanged(int)), this, SLOT(tabFeaturesIndexChanged()));
    // connect(ui->featureDock, SIGNAL(addFeature(int)), this, SLOT(featureAddClicked(int)));

	QString applicationDirPath = qApp->applicationDirPath();

#ifdef _MSC_VER
    if (QResource::registerResource(applicationDirPath + "/sdrbase.rcc")) {
        qDebug("MainWindow::MainWindow: registered resource file %s/%s", qPrintable(applicationDirPath), "sdrbase.rcc");
    } else {
        qWarning("MainWindow::MainWindow: could not register resource file %s/%s", qPrintable(applicationDirPath), "sdrbase.rcc");
    }
#endif

    // ui->featureDock->setWebAPIAdapter(m_apiAdapter);
	m_requestMapper = new WebAPIRequestMapper(this);
	m_requestMapper->setAdapter(m_apiAdapter);
	m_apiHost = parser.getServerAddress();
	m_apiPort = parser.getServerPort();
	m_apiServer = new WebAPIServer(m_apiHost, m_apiPort, m_requestMapper);
	m_apiServer->start();

	m_commandKeyReceiver = new CommandKeyReceiver();
	m_commandKeyReceiver->setRelease(true);
	this->installEventFilter(m_commandKeyReceiver);

    m_dspEngine->setMIMOSupport(true);

    // if (!parser.getMIMOSupport()) {
    //     ui->menu_Devices->removeAction(ui->action_sampleMIMOAdd);
    // }

#ifdef __APPLE__
    ui->menuPreferences->removeAction(ui->action_AMBE);
#endif
#if not defined(HAS_LIMERFEUSB)
    ui->menuPreferences->removeAction(ui->action_LimeRFE);
#endif

    delete splash;

    // Restore window size and position
    QSettings s;
    restoreGeometry(qUncompress(QByteArray::fromBase64(s.value("mainWindowGeometry").toByteArray())));
    restoreState(qUncompress(QByteArray::fromBase64(s.value("mainWindowState").toByteArray())));

    qDebug() << "MainWindow::MainWindow: end";
}

MainWindow::~MainWindow()
{
	qDebug() << "MainWindow::~MainWindow";

    m_mainCore->m_settings.save();
    m_apiServer->stop();
    delete m_apiServer;
    delete m_requestMapper;
    delete m_apiAdapter;

    delete m_pluginManager;
	delete m_dateTimeWidget;
	delete m_showSystemWidget;

    // disconnect(ui->tabFeatures, SIGNAL(currentChanged(int)), this, SLOT(tabFeaturesIndexChanged()));

    removeAllFeatureSets();

	// delete ui;
    // delete m_spectrumToggleViewAction;

	delete m_commandKeyReceiver;

    for (const auto& workspace : m_workspaces) {
        delete workspace;
    }

	qDebug() << "MainWindow::~MainWindow: end";
}

void MainWindow::sampleSourceAdd(Workspace *deviceWorkspace, Workspace *spectrumWorkspace, int deviceIndex)
{
    DSPDeviceSourceEngine *dspDeviceSourceEngine = m_dspEngine->addDeviceSourceEngine();
    dspDeviceSourceEngine->start();

    uint dspDeviceSourceEngineUID =  dspDeviceSourceEngine->getUID();
    char uidCStr[16];
    sprintf(uidCStr, "UID:%d", dspDeviceSourceEngineUID);

    int deviceSetIndex = m_deviceUIs.size();

    m_mainCore->appendDeviceSet(0);
    m_deviceUIs.push_back(new DeviceUISet(deviceSetIndex, m_mainCore->m_deviceSets.back()));
    m_deviceUIs.back()->m_deviceSourceEngine = dspDeviceSourceEngine;
    m_mainCore->m_deviceSets.back()->m_deviceSourceEngine = dspDeviceSourceEngine;
    m_deviceUIs.back()->m_deviceSinkEngine = nullptr;
    m_mainCore->m_deviceSets.back()->m_deviceSinkEngine = nullptr;
    m_deviceUIs.back()->m_deviceMIMOEngine = nullptr;
    m_mainCore->m_deviceSets.back()->m_deviceMIMOEngine = nullptr;

    DeviceAPI *deviceAPI = new DeviceAPI(DeviceAPI::StreamSingleRx, deviceSetIndex, dspDeviceSourceEngine, nullptr, nullptr);

    m_deviceUIs.back()->m_deviceAPI = deviceAPI;
    m_mainCore->m_deviceSets.back()->m_deviceAPI = deviceAPI;
    QList<QString> channelNames;
    m_pluginManager->listRxChannels(channelNames);
    m_deviceUIs.back()->setNumberOfAvailableRxChannels(channelNames.size());

    dspDeviceSourceEngine->addSink(m_deviceUIs.back()->m_spectrumVis);

    // Create a file source instance by default if requested device was not enumerated (index = -1)
    if (deviceIndex < 0) {
        deviceIndex = DeviceEnumerator::instance()->getFileInputDeviceIndex();
    }

    sampleSourceCreate(deviceSetIndex, deviceIndex, deviceAPI, m_deviceUIs.back());
    m_deviceUIs.back()->m_deviceGUI->setWorkspaceIndex(deviceWorkspace->getIndex());
    m_deviceUIs.back()->m_mainSpectrumGUI->setWorkspaceIndex(spectrumWorkspace->getIndex());
    MainSpectrumGUI *mainSpectrumGUI = m_deviceUIs.back()->m_mainSpectrumGUI;

    QObject::connect(
        mainSpectrumGUI,
        &MainSpectrumGUI::moveToWorkspace,
        this,
        [=](int wsIndexDest){ this->mainSpectrumMove(mainSpectrumGUI, wsIndexDest); }
    );

    QObject::connect(
        m_deviceUIs.back()->m_deviceGUI,
        &DeviceGUI::addChannelEmitted,
        this,
        [=](int channelIndex){ this->channelAddClicked(deviceWorkspace, deviceSetIndex, channelIndex); }
    );

    deviceWorkspace->addToMdiArea(m_deviceUIs.back()->m_deviceGUI);
    spectrumWorkspace->addToMdiArea(m_deviceUIs.back()->m_mainSpectrumGUI);
    emit m_mainCore->deviceSetAdded(deviceSetIndex, deviceAPI);
}

void MainWindow::sampleSourceCreate(
    int deviceSetIndex,
    int deviceIndex,
    DeviceAPI *deviceAPI,
    DeviceUISet *deviceUISet
)
{
    int selectedDeviceIndex = deviceIndex;
    DeviceEnumerator::instance()->changeRxSelection(deviceSetIndex, deviceIndex);
    const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(deviceIndex);
    deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
    deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
    deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
    deviceAPI->setHardwareId(samplingDevice->hardwareId);
    deviceAPI->setSamplingDeviceId(samplingDevice->id);
    deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
    deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
    deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getRxPluginInterface(deviceIndex));

    qDebug() << "MainWindow::sampleSourceImplement:"
        << "deviceIndex:" << deviceIndex
        << "hardwareId:" << samplingDevice->hardwareId
        << "sequence:" << samplingDevice->sequence
        << "id:" << samplingDevice->id
        << "serial:" << samplingDevice->serial
        << "displayedName:" << samplingDevice->displayedName;

    if (deviceAPI->getSamplingDeviceId().size() == 0) // non existent device => replace by default
    {
        qDebug("MainWindow::sampleSourceAdd: non existent device replaced by File Input");
        int fileInputDeviceIndex = DeviceEnumerator::instance()->getFileInputDeviceIndex();
        selectedDeviceIndex = fileInputDeviceIndex;
        samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(fileInputDeviceIndex);
        deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
        deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
        deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
        deviceAPI->setHardwareId(samplingDevice->hardwareId);
        deviceAPI->setSamplingDeviceId(samplingDevice->id);
        deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
        deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
        deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getRxPluginInterface(fileInputDeviceIndex));
    }

    QString userArgs = m_mainCore->m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

    if (userArgs.size() > 0) {
        deviceAPI->setHardwareUserArguments(userArgs);
    }

    // add to buddies list
    std::vector<DeviceUISet*>::iterator it = m_deviceUIs.begin();
    int nbOfBuddies = 0;

    for (; it != m_deviceUIs.end(); ++it)
    {
        if (*it != deviceUISet) // do not add to itself
        {
            if ((*it)->m_deviceSourceEngine) // it is a source device
            {
                if ((deviceUISet->m_deviceAPI->getHardwareId() == (*it)->m_deviceAPI->getHardwareId()) &&
                    (deviceUISet->m_deviceAPI->getSamplingDeviceSerial() == (*it)->m_deviceAPI->getSamplingDeviceSerial()))
                {
                    (*it)->m_deviceAPI->addSourceBuddy(deviceUISet->m_deviceAPI);
                    nbOfBuddies++;
                }
            }

            if ((*it)->m_deviceSinkEngine) // it is a sink device
            {
                if ((deviceUISet->m_deviceAPI->getHardwareId() == (*it)->m_deviceAPI->getHardwareId()) &&
                    (deviceUISet->m_deviceAPI->getSamplingDeviceSerial() == (*it)->m_deviceAPI->getSamplingDeviceSerial()))
                {
                    (*it)->m_deviceAPI->addSourceBuddy(deviceUISet->m_deviceAPI);
                    nbOfBuddies++;
                }
            }
        }
    }

    if (nbOfBuddies == 0) {
        deviceUISet->m_deviceAPI->setBuddyLeader(true);
    }

    // delete previous GUI if it exists
    if (deviceUISet->m_deviceGUI) {
        emit deviceUISet->m_deviceGUI->destroy();
    }

    // constructs new GUI and input object
    DeviceSampleSource *source = deviceAPI->getPluginInterface()->createSampleSourcePluginInstance(
            deviceAPI->getSamplingDeviceId(), deviceAPI);
    deviceAPI->setSampleSource(source);
    QWidget *gui;
    DeviceGUI *deviceGUI = deviceAPI->getPluginInterface()->createSampleSourcePluginInstanceGUI(
            deviceAPI->getSamplingDeviceId(),
            &gui,
            deviceUISet
        );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::moveToWorkspace,
        this,
        [=](int wsIndexDest){ this->deviceMove(deviceGUI, wsIndexDest); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::deviceChange,
        this,
        [=](int newDeviceIndex){ this->sampleDeviceChangeHandler(deviceGUI, newDeviceIndex); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::showSpectrum,
        this,
        [=](int deviceSetIndex){ this->mainSpectrumShow(this->m_deviceUIs[deviceSetIndex]->m_mainSpectrumGUI); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::showAllChannels,
        this,
        &MainWindow::showAllChannels
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::closing,
        this,
        [=](){ this->removeDeviceSet(deviceGUI->getIndex()); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::deviceSetPresetsDialogRequested,
        this,
        &MainWindow::openDeviceSetPresetsDialog
    );

    deviceAPI->getSampleSource()->setMessageQueueToGUI(deviceGUI->getInputMessageQueue());
    deviceUISet->m_deviceGUI = deviceGUI;
    const PluginInterface::SamplingDevice *selectedDevice = DeviceEnumerator::instance()->getRxSamplingDevice(selectedDeviceIndex);
    deviceUISet->m_selectedDeviceId = selectedDevice->id;
    deviceUISet->m_selectedDeviceSerial = selectedDevice->serial;
    deviceUISet->m_selectedDeviceSequence = selectedDevice->sequence;
    deviceUISet->m_selectedDeviceItemImdex = selectedDevice->deviceItemIndex;
    deviceUISet->m_deviceAPI->getSampleSource()->init();
    // Finalize GUI setup and add it to workspace MDI
    deviceGUI->setDeviceType(DeviceGUI::DeviceRx);
    deviceGUI->setIndex(deviceSetIndex);
    deviceGUI->setToolTip(samplingDevice->displayedName);
    deviceGUI->setTitle(samplingDevice->displayedName.split(" ")[0]);
    deviceGUI->setCurrentDeviceIndex(selectedDeviceIndex);
    QStringList channelNames;
    m_pluginManager->listRxChannels(channelNames);
    deviceGUI->setChannelNames(channelNames);
    MainSpectrumGUI *mainSpectrumGUI = deviceUISet->m_mainSpectrumGUI;
    mainSpectrumGUI->setDeviceType(MainSpectrumGUI::DeviceRx);
    mainSpectrumGUI->setIndex(deviceSetIndex);
    mainSpectrumGUI->setToolTip(samplingDevice->displayedName);
    mainSpectrumGUI->setTitle(samplingDevice->displayedName.split(" ")[0]);
}

void MainWindow::sampleSinkAdd(Workspace *deviceWorkspace, Workspace *spectrumWorkspace, int deviceIndex)
{
    DSPDeviceSinkEngine *dspDeviceSinkEngine = m_dspEngine->addDeviceSinkEngine();
    dspDeviceSinkEngine->start();

    uint dspDeviceSinkEngineUID =  dspDeviceSinkEngine->getUID();
    char uidCStr[16];
    sprintf(uidCStr, "UID:%d", dspDeviceSinkEngineUID);

    int deviceSetIndex = m_deviceUIs.size();

    m_mainCore->appendDeviceSet(1);
    m_deviceUIs.push_back(new DeviceUISet(deviceSetIndex, m_mainCore->m_deviceSets.back()));
    m_deviceUIs.back()->m_deviceSourceEngine = nullptr;
    m_mainCore->m_deviceSets.back()->m_deviceSourceEngine = nullptr;
    m_deviceUIs.back()->m_deviceSinkEngine = dspDeviceSinkEngine;
    m_mainCore->m_deviceSets.back()->m_deviceSinkEngine = dspDeviceSinkEngine;
    m_deviceUIs.back()->m_deviceMIMOEngine = nullptr;
    m_mainCore->m_deviceSets.back()->m_deviceMIMOEngine = nullptr;

    DeviceAPI *deviceAPI = new DeviceAPI(DeviceAPI::StreamSingleTx, deviceSetIndex, nullptr, dspDeviceSinkEngine, nullptr);

    m_deviceUIs.back()->m_deviceAPI = deviceAPI;
    m_mainCore->m_deviceSets.back()->m_deviceAPI = deviceAPI;
    QList<QString> channelNames;
    m_pluginManager->listTxChannels(channelNames);
    m_deviceUIs.back()->setNumberOfAvailableTxChannels(channelNames.size());

    dspDeviceSinkEngine->addSpectrumSink(m_deviceUIs.back()->m_spectrumVis);
    m_deviceUIs.back()->m_spectrum->setDisplayedStream(false, 0);

    if (deviceIndex < 0) {
        deviceIndex = DeviceEnumerator::instance()->getFileOutputDeviceIndex(); // create a file output by default
    }

    sampleSinkCreate(deviceSetIndex, deviceIndex, deviceAPI, m_deviceUIs.back());
    m_deviceUIs.back()->m_deviceGUI->setWorkspaceIndex(deviceWorkspace->getIndex());
    m_deviceUIs.back()->m_mainSpectrumGUI->setWorkspaceIndex(spectrumWorkspace->getIndex());
    MainSpectrumGUI *mainSpectrumGUI = m_deviceUIs.back()->m_mainSpectrumGUI;

    QObject::connect(
        mainSpectrumGUI,
        &MainSpectrumGUI::moveToWorkspace,
        this,
        [=](int wsIndexDest){ this->mainSpectrumMove(mainSpectrumGUI, wsIndexDest); }
    );

    QObject::connect(
        m_deviceUIs.back()->m_deviceGUI,
        &DeviceGUI::addChannelEmitted,
        this,
        [=](int channelIndex){ this->channelAddClicked(deviceWorkspace, deviceSetIndex, channelIndex); }
    );

    deviceWorkspace->addToMdiArea(m_deviceUIs.back()->m_deviceGUI);
    spectrumWorkspace->addToMdiArea(m_deviceUIs.back()->m_mainSpectrumGUI);
    emit m_mainCore->deviceSetAdded(deviceSetIndex, deviceAPI);
}

void MainWindow::sampleSinkCreate(
        int deviceSetIndex,
        int deviceIndex,
        DeviceAPI *deviceAPI,
        DeviceUISet *deviceUISet
)
{
    int selectedDeviceIndex = deviceIndex;
    DeviceEnumerator::instance()->changeTxSelection(deviceSetIndex, deviceIndex);
    const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(deviceIndex);
    deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
    deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
    deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
    deviceAPI->setHardwareId(samplingDevice->hardwareId);
    deviceAPI->setSamplingDeviceId(samplingDevice->id);
    deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
    deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
    deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getTxPluginInterface(deviceIndex));

    qDebug() << "MainWindow::sampleSinkImplement:"
        << "newDeviceIndex:" << deviceIndex
        << "hardwareId:" << samplingDevice->hardwareId
        << "sequence:" << samplingDevice->sequence
        << "id:" << samplingDevice->id
        << "serial:" << samplingDevice->serial
        << "displayedName:" << samplingDevice->displayedName;

    if (deviceAPI->getSamplingDeviceId().size() == 0) // non existent device => replace by default
    {
        qDebug("MainWindow::sampleSinkImplement: non existent device replaced by File Sink");
        int fileSinkDeviceIndex = DeviceEnumerator::instance()->getFileOutputDeviceIndex();
        selectedDeviceIndex = fileSinkDeviceIndex;
        const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(fileSinkDeviceIndex);
        deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
        deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
        deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
        deviceAPI->setHardwareId(samplingDevice->hardwareId);
        deviceAPI->setSamplingDeviceId(samplingDevice->id);
        deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
        deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
        deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getTxPluginInterface(fileSinkDeviceIndex));
    }

    QString userArgs = m_mainCore->m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

    if (userArgs.size() > 0) {
        deviceAPI->setHardwareUserArguments(userArgs);
    }

    // add to buddies list
    std::vector<DeviceUISet*>::iterator it = m_deviceUIs.begin();
    int nbOfBuddies = 0;

    for (; it != m_deviceUIs.end(); ++it)
    {
        if (*it != deviceUISet) // do not add to itself
        {
            if ((*it)->m_deviceSourceEngine) // it is a source device
            {
                if ((deviceAPI->getHardwareId() == (*it)->m_deviceAPI->getHardwareId()) &&
                    (deviceAPI->getSamplingDeviceSerial() == (*it)->m_deviceAPI->getSamplingDeviceSerial()))
                {
                    (*it)->m_deviceAPI->addSinkBuddy(deviceAPI);
                    nbOfBuddies++;
                }
            }

            if ((*it)->m_deviceSinkEngine) // it is a sink device
            {
                if ((deviceAPI->getHardwareId() == (*it)->m_deviceAPI->getHardwareId()) &&
                    (deviceAPI->getSamplingDeviceSerial() == (*it)->m_deviceAPI->getSamplingDeviceSerial()))
                {
                    (*it)->m_deviceAPI->addSinkBuddy(deviceAPI);
                    nbOfBuddies++;
                }
            }
        }
    }

    if (nbOfBuddies == 0) {
        deviceAPI->setBuddyLeader(true);
    }

    // delete previous plugin GUI if it exists
    if (deviceUISet->m_deviceGUI) {
        emit deviceUISet->m_deviceGUI->destroy();
    }

    // constructs new GUI and output object
    DeviceSampleSink *sink = deviceAPI->getPluginInterface()->createSampleSinkPluginInstance(
            deviceAPI->getSamplingDeviceId(), deviceAPI);
    deviceAPI->setSampleSink(sink);
    QWidget *gui;
    DeviceGUI *deviceGUI = deviceAPI->getPluginInterface()->createSampleSinkPluginInstanceGUI(
            deviceAPI->getSamplingDeviceId(),
            &gui,
            deviceUISet
        );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::moveToWorkspace,
        this,
        [=](int wsIndexDest){ this->deviceMove(deviceGUI, wsIndexDest); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::deviceChange,
        this,
        [=](int newDeviceIndex){ this->sampleDeviceChangeHandler(deviceGUI, newDeviceIndex); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::showSpectrum,
        this,
        [=](int deviceSetIndex){ this->mainSpectrumShow(this->m_deviceUIs[deviceSetIndex]->m_mainSpectrumGUI); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::showAllChannels,
        this,
        &MainWindow::showAllChannels
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::closing,
        this,
        [=](){ this->removeDeviceSet(deviceGUI->getIndex()); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::deviceSetPresetsDialogRequested,
        this,
        &MainWindow::openDeviceSetPresetsDialog
    );

    deviceAPI->getSampleSink()->setMessageQueueToGUI(deviceGUI->getInputMessageQueue());
    deviceUISet->m_deviceGUI = deviceGUI;
    const PluginInterface::SamplingDevice *selectedDevice = DeviceEnumerator::instance()->getRxSamplingDevice(selectedDeviceIndex);
    deviceUISet->m_selectedDeviceId = selectedDevice->id;
    deviceUISet->m_selectedDeviceSerial = selectedDevice->serial;
    deviceUISet->m_selectedDeviceSequence = selectedDevice->sequence;
    deviceUISet->m_selectedDeviceItemImdex = selectedDevice->deviceItemIndex;
    deviceUISet->m_deviceAPI->getSampleSink()->init();
    // Finalize GUI setup and add it to workspace MDI
    deviceGUI->setDeviceType(DeviceGUI::DeviceTx);
    deviceGUI->setIndex(deviceSetIndex);
    deviceGUI->setToolTip(samplingDevice->displayedName);
    deviceGUI->setTitle(samplingDevice->displayedName.split(" ")[0]);
    deviceGUI->setCurrentDeviceIndex(selectedDeviceIndex);
    QStringList channelNames;
    m_pluginManager->listTxChannels(channelNames);
    deviceGUI->setChannelNames(channelNames);
    MainSpectrumGUI *spectrumGUI = deviceUISet->m_mainSpectrumGUI;
    spectrumGUI->setDeviceType(MainSpectrumGUI::DeviceTx);
    spectrumGUI->setIndex(deviceSetIndex);
    spectrumGUI->setToolTip(samplingDevice->displayedName);
    spectrumGUI->setTitle(samplingDevice->displayedName.split(" ")[0]);
}

void MainWindow::sampleMIMOAdd(Workspace *deviceWorkspace, Workspace *spectrumWorkspace, int deviceIndex)
{
    DSPDeviceMIMOEngine *dspDeviceMIMOEngine = m_dspEngine->addDeviceMIMOEngine();
    dspDeviceMIMOEngine->start();

    uint dspDeviceMIMOEngineUID =  dspDeviceMIMOEngine->getUID();
    char uidCStr[16];
    sprintf(uidCStr, "UID:%d", dspDeviceMIMOEngineUID);

    int deviceSetIndex = m_deviceUIs.size();

    m_mainCore->appendDeviceSet(2);
    m_deviceUIs.push_back(new DeviceUISet(deviceSetIndex, m_mainCore->m_deviceSets.back()));
    m_deviceUIs.back()->m_deviceSourceEngine = nullptr;
    m_mainCore->m_deviceSets.back()->m_deviceSourceEngine = nullptr;
    m_deviceUIs.back()->m_deviceSinkEngine = nullptr;
    m_mainCore->m_deviceSets.back()->m_deviceSinkEngine = nullptr;
    m_deviceUIs.back()->m_deviceMIMOEngine = dspDeviceMIMOEngine;
    m_mainCore->m_deviceSets.back()->m_deviceMIMOEngine = dspDeviceMIMOEngine;

    DeviceAPI *deviceAPI = new DeviceAPI(DeviceAPI::StreamMIMO, deviceSetIndex, nullptr, nullptr, dspDeviceMIMOEngine);

    m_deviceUIs.back()->m_deviceAPI = deviceAPI;
    m_mainCore->m_deviceSets.back()->m_deviceAPI = deviceAPI;
    // add MIMO channels
    QList<QString> mimoChannelNames;
    m_pluginManager->listMIMOChannels(mimoChannelNames);
    m_deviceUIs.back()->setNumberOfAvailableMIMOChannels(mimoChannelNames.size());
    // Add Rx channels
    QList<QString> rxChannelNames;
    m_pluginManager->listRxChannels(rxChannelNames);
    m_deviceUIs.back()->setNumberOfAvailableRxChannels(rxChannelNames.size());
    // Add Tx channels
    QList<QString> txChannelNames;
    m_pluginManager->listTxChannels(txChannelNames);
    m_deviceUIs.back()->setNumberOfAvailableTxChannels(txChannelNames.size());

    dspDeviceMIMOEngine->addSpectrumSink(m_deviceUIs.back()->m_spectrumVis);

    if (deviceIndex < 0) {
        deviceIndex = DeviceEnumerator::instance()->getTestMIMODeviceIndex(); // create a test MIMO by default
    }

    sampleMIMOCreate(deviceSetIndex, deviceIndex, deviceAPI, m_deviceUIs.back());
    m_deviceUIs.back()->m_deviceGUI->setWorkspaceIndex(deviceWorkspace->getIndex());
    m_deviceUIs.back()->m_mainSpectrumGUI->setWorkspaceIndex(spectrumWorkspace->getIndex());
    MainSpectrumGUI *mainSpectrumGUI = m_deviceUIs.back()->m_mainSpectrumGUI;

    QObject::connect(
        mainSpectrumGUI,
        &MainSpectrumGUI::moveToWorkspace,
        this,
        [=](int wsIndexDest){ this->mainSpectrumMove(mainSpectrumGUI, wsIndexDest); }
    );

    QObject::connect(
        m_deviceUIs.back()->m_deviceGUI,
        &DeviceGUI::addChannelEmitted,
        this,
        [=](int channelIndex){ this->channelAddClicked(deviceWorkspace, deviceSetIndex, channelIndex); }
    );

    deviceWorkspace->addToMdiArea(m_deviceUIs.back()->m_deviceGUI);
    spectrumWorkspace->addToMdiArea(m_deviceUIs.back()->m_mainSpectrumGUI);
    emit m_mainCore->deviceSetAdded(deviceSetIndex, deviceAPI);
}

void MainWindow::sampleMIMOCreate(
    int deviceSetIndex,
    int deviceIndex,
    DeviceAPI *deviceAPI,
    DeviceUISet *deviceUISet
)
{
    int selectedDeviceIndex = deviceIndex;
    DeviceEnumerator::instance()->changeMIMOSelection(deviceSetIndex, deviceIndex);
    const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getMIMOSamplingDevice(deviceIndex);
    deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
    deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
    deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
    deviceAPI->setHardwareId(samplingDevice->hardwareId);
    deviceAPI->setSamplingDeviceId(samplingDevice->id);
    deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
    deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
    deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getMIMOPluginInterface(deviceIndex));

    qDebug() << "MainWindow::sampleMIMOImplement:"
        << "newDeviceIndex:" << deviceIndex
        << "hardwareId:" << samplingDevice->hardwareId
        << "sequence:" << samplingDevice->sequence
        << "id:" << samplingDevice->id
        << "serial:" << samplingDevice->serial
        << "displayedName:" << samplingDevice->displayedName;

    if (deviceAPI->getSamplingDeviceId().size() == 0) // non existent device => replace by default
    {
        qDebug("MainWindow::sampleMIMOImplement: non existent device replaced by Test MIMO");
        int testMIMODeviceIndex = DeviceEnumerator::instance()->getTestMIMODeviceIndex();
        selectedDeviceIndex = testMIMODeviceIndex;
        const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getMIMOSamplingDevice(testMIMODeviceIndex);
        deviceAPI->setSamplingDeviceSequence(samplingDevice->sequence);
        deviceAPI->setDeviceNbItems(samplingDevice->deviceNbItems);
        deviceAPI->setDeviceItemIndex(samplingDevice->deviceItemIndex);
        deviceAPI->setHardwareId(samplingDevice->hardwareId);
        deviceAPI->setSamplingDeviceId(samplingDevice->id);
        deviceAPI->setSamplingDeviceSerial(samplingDevice->serial);
        deviceAPI->setSamplingDeviceDisplayName(samplingDevice->displayedName);
        deviceAPI->setSamplingDevicePluginInterface(DeviceEnumerator::instance()->getMIMOPluginInterface(testMIMODeviceIndex));
    }

    QString userArgs = m_mainCore->m_settings.getDeviceUserArgs().findUserArgs(samplingDevice->hardwareId, samplingDevice->sequence);

    if (userArgs.size() > 0) {
        deviceAPI->setHardwareUserArguments(userArgs);
    }

    // delete previous plugin GUI if it exists
    if (deviceUISet->m_deviceGUI) {
        emit deviceUISet->m_deviceGUI->destroy();
    }

    // constructs new GUI and output object
    DeviceSampleMIMO *mimo = deviceAPI->getPluginInterface()->createSampleMIMOPluginInstance(
            deviceAPI->getSamplingDeviceId(), deviceAPI);
    deviceAPI->setSampleMIMO(mimo);
    QWidget *gui;
    DeviceGUI *deviceGUI = deviceAPI->getPluginInterface()->createSampleMIMOPluginInstanceGUI(
            deviceAPI->getSamplingDeviceId(),
            &gui,
            deviceUISet
        );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::moveToWorkspace,
        this,
        [=](int wsIndexDest){ this->deviceMove(deviceGUI, wsIndexDest); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::deviceChange,
        this,
        [=](int newDeviceIndex){ this->sampleDeviceChangeHandler(deviceGUI, newDeviceIndex); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::showSpectrum,
        this,
        [=](int deviceSetIndex){ this->mainSpectrumShow(this->m_deviceUIs[deviceSetIndex]->m_mainSpectrumGUI); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::showAllChannels,
        this,
        &MainWindow::showAllChannels
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::closing,
        this,
        [=](){ this->removeDeviceSet(deviceGUI->getIndex()); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::deviceSetPresetsDialogRequested,
        this,
        &MainWindow::openDeviceSetPresetsDialog
    );

    deviceAPI->getSampleMIMO()->setMessageQueueToGUI(deviceGUI->getInputMessageQueue());
    deviceUISet->m_deviceGUI = deviceGUI;
    const PluginInterface::SamplingDevice *selectedDevice = DeviceEnumerator::instance()->getRxSamplingDevice(selectedDeviceIndex);
    deviceUISet->m_selectedDeviceId = selectedDevice->id;
    deviceUISet->m_selectedDeviceSerial = selectedDevice->serial;
    deviceUISet->m_selectedDeviceSequence = selectedDevice->sequence;
    deviceUISet->m_selectedDeviceItemImdex = selectedDevice->deviceItemIndex;
    deviceUISet->m_deviceAPI->getSampleMIMO()->init();
    // Finalize GUI setup and add it to workspace MDI
    deviceGUI->setDeviceType(DeviceGUI::DeviceMIMO);
    deviceGUI->setIndex(deviceSetIndex);
    deviceGUI->setToolTip(samplingDevice->displayedName);
    deviceGUI->setTitle(samplingDevice->displayedName.split(" ")[0]);
    deviceGUI->setCurrentDeviceIndex(selectedDeviceIndex);
    QStringList channelNames;
    m_pluginManager->listMIMOChannels(channelNames);
    deviceGUI->setChannelNames(channelNames);
    MainSpectrumGUI *spectrumGUI = deviceUISet->m_mainSpectrumGUI;
    spectrumGUI->setDeviceType(MainSpectrumGUI::DeviceMIMO);
    spectrumGUI->setIndex(deviceSetIndex);
    spectrumGUI->setToolTip(samplingDevice->displayedName);
    spectrumGUI->setTitle(samplingDevice->displayedName.split(" ")[0]);
}

void MainWindow::removeDeviceSet(int deviceSetIndex)
{
    if (deviceSetIndex >= (int) m_deviceUIs.size()) {
        return;
    }

    DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];

    if (deviceUISet->m_deviceSourceEngine) // source device
    {
        DSPDeviceSourceEngine *deviceEngine = deviceUISet->m_deviceSourceEngine;
        deviceEngine->stopAcquistion();
        deviceEngine->removeSink(deviceUISet->m_spectrumVis);

        // deletes old UI and core object
        deviceUISet->freeChannels();      // destroys the channel instances
        deviceUISet->m_deviceAPI->getSampleSource()->setMessageQueueToGUI(nullptr); // have source stop sending messages to the GUI
        deviceUISet->m_deviceGUI->destroy();
        deviceUISet->m_deviceAPI->resetSamplingDeviceId();
        deviceUISet->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceInput(
            deviceUISet->m_deviceAPI->getSampleSource());
        deviceUISet->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

	    DeviceAPI *sourceAPI = deviceUISet->m_deviceAPI;
	    delete deviceUISet;

	    deviceEngine->stop();
	    m_dspEngine->removeDeviceEngineAt(deviceSetIndex);

	    delete sourceAPI;
    }
    else if (deviceUISet->m_deviceSinkEngine) // sink device
    {
	    DSPDeviceSinkEngine *deviceEngine = deviceUISet->m_deviceSinkEngine;
	    deviceEngine->stopGeneration();
	    deviceEngine->removeSpectrumSink(deviceUISet->m_spectrumVis);

        // deletes old UI and output object
        deviceUISet->freeChannels();
        deviceUISet->m_deviceAPI->getSampleSink()->setMessageQueueToGUI(nullptr); // have sink stop sending messages to the GUI
        deviceUISet->m_deviceGUI->destroy();
	    deviceUISet->m_deviceAPI->resetSamplingDeviceId();
	    deviceUISet->m_deviceAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput(
	        deviceUISet->m_deviceAPI->getSampleSink());
        deviceUISet->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

	    DeviceAPI *sinkAPI = deviceUISet->m_deviceAPI;
	    delete deviceUISet;

	    deviceEngine->stop();
	    m_dspEngine->removeDeviceEngineAt(deviceSetIndex);

	    delete sinkAPI;
    }
    else if (deviceUISet->m_deviceMIMOEngine) // MIMO device
    {
	    DSPDeviceMIMOEngine *deviceEngine = deviceUISet->m_deviceMIMOEngine;
	    deviceEngine->stopProcess(1); // Tx side
        deviceEngine->stopProcess(0); // Rx side
	    deviceEngine->removeSpectrumSink(deviceUISet->m_spectrumVis);

        // deletes old UI and output object
        deviceUISet->freeChannels();
        deviceUISet->m_deviceAPI->getSampleMIMO()->setMessageQueueToGUI(nullptr); // have sink stop sending messages to the GUI
        deviceUISet->m_deviceGUI->destroy();
	    deviceUISet->m_deviceAPI->resetSamplingDeviceId();
	    deviceUISet->m_deviceAPI->getPluginInterface()->deleteSampleMIMOPluginInstanceMIMO(
	        deviceUISet->m_deviceAPI->getSampleMIMO());

	    DeviceAPI *mimoAPI = deviceUISet->m_deviceAPI;
	    delete deviceUISet;

	    deviceEngine->stop();
	    m_dspEngine->removeDeviceEngineAt(deviceSetIndex);

	    delete mimoAPI;
    }

    m_deviceUIs.erase(m_deviceUIs.begin() + deviceSetIndex);
    m_mainCore->removeDeviceSet(deviceSetIndex);

    // Renumerate
    for (int i = 0; i < (int) m_deviceUIs.size(); i++)
    {
        DeviceUISet *deviceUISet = m_deviceUIs[i];
        deviceUISet->setIndex(i);
    }

    emit m_mainCore->deviceSetRemoved(deviceSetIndex);
}

void MainWindow::removeLastDevice()
{
    int removedDeviceSetIndex = m_deviceUIs.size() - 1;

    if (m_deviceUIs.back()->m_deviceSourceEngine) // source tab
	{
	    DSPDeviceSourceEngine *lastDeviceEngine = m_deviceUIs.back()->m_deviceSourceEngine;
	    lastDeviceEngine->stopAcquistion();
	    lastDeviceEngine->removeSink(m_deviceUIs.back()->m_spectrumVis);

        // deletes old UI and input object
        m_deviceUIs.back()->freeChannels();      // destroys the channel instances
        m_deviceUIs.back()->m_deviceAPI->getSampleSource()->setMessageQueueToGUI(nullptr); // have source stop sending messages to the GUI
        m_deviceUIs.back()->m_deviceGUI->destroy();
        m_deviceUIs.back()->m_deviceAPI->resetSamplingDeviceId();
        m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceInput(
            m_deviceUIs.back()->m_deviceAPI->getSampleSource());
        m_deviceUIs.back()->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

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

        // deletes old UI and output object
        m_deviceUIs.back()->freeChannels();
        m_deviceUIs.back()->m_deviceAPI->getSampleSink()->setMessageQueueToGUI(nullptr); // have sink stop sending messages to the GUI
        m_deviceUIs.back()->m_deviceGUI->destroy();
	    m_deviceUIs.back()->m_deviceAPI->resetSamplingDeviceId();
	    m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput(
	        m_deviceUIs.back()->m_deviceAPI->getSampleSink());
        m_deviceUIs.back()->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

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

        // deletes old UI and output object
        m_deviceUIs.back()->freeChannels();
        m_deviceUIs.back()->m_deviceAPI->getSampleMIMO()->setMessageQueueToGUI(nullptr); // have sink stop sending messages to the GUI
        m_deviceUIs.back()->m_deviceGUI->destroy();
	    m_deviceUIs.back()->m_deviceAPI->resetSamplingDeviceId();
	    m_deviceUIs.back()->m_deviceAPI->getPluginInterface()->deleteSampleMIMOPluginInstanceMIMO(
	        m_deviceUIs.back()->m_deviceAPI->getSampleMIMO());

	    DeviceAPI *mimoAPI = m_deviceUIs.back()->m_deviceAPI;
	    delete m_deviceUIs.back();

	    lastDeviceEngine->stop();
	    m_dspEngine->removeLastDeviceMIMOEngine();

	    delete mimoAPI;
	}

    m_deviceUIs.pop_back();
    m_mainCore->removeLastDeviceSet();
    emit m_mainCore->deviceSetRemoved(removedDeviceSetIndex);
}

void MainWindow::addFeatureSet()
{
    int tabIndex = m_featureUIs.size();
    m_mainCore->appendFeatureSet();
    m_featureUIs.push_back(new FeatureUISet(tabIndex, m_mainCore->m_featureSets[tabIndex]));
    // ui->tabFeatures->addTab(m_featureUIs.back()->m_featureWindow, QString("F%1").arg(tabIndex));
    emit m_mainCore->featureSetAdded(tabIndex);
}

void MainWindow::removeFeatureSet(unsigned int tabIndex)
{
    if (tabIndex < m_featureUIs.size())
    {
        delete m_featureUIs[tabIndex];
        m_featureUIs.pop_back();
        m_mainCore->removeFeatureSet(tabIndex);
        emit m_mainCore->featureSetRemoved(tabIndex);
    }
}

void MainWindow::removeAllFeatureSets()
{
    while (m_featureUIs.size() > 0)
    {
        delete m_featureUIs.back();
        m_featureUIs.pop_back();
        m_mainCore->removeLastFeatureSet();
    }
}

void MainWindow::deleteChannel(int deviceSetIndex, int channelIndex)
{
    if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_deviceUIs.size()))
    {
        DeviceUISet *deviceSet = m_deviceUIs[deviceSetIndex];
        deviceSet->deleteChannel(channelIndex);
    }
}

void MainWindow::loadSettings()
{
	qDebug() << "MainWindow::loadSettings";

    m_mainCore->m_settings.load();
    m_mainCore->m_settings.sortPresets();
    // int middleIndex = m_mainCore->m_settings.getPresetCount() / 2;
    // QTreeWidgetItem *treeItem;

    // for (int i = 0; i < m_mainCore->m_settings.getPresetCount(); ++i)
    // {
    //     treeItem = addPresetToTree(m_mainCore->m_settings.getPreset(i));

    //     if (i == middleIndex) {
    //         ui->presetTree->setCurrentItem(treeItem);
    //     }
    // }

    m_mainCore->m_settings.sortCommands();

    // for (int i = 0; i < m_mainCore->m_settings.getCommandCount(); ++i)
    // {
    //     treeItem = addCommandToTree(m_mainCore->m_settings.getCommand(i));
    // }

    m_mainCore->setLoggingOptions();
}

void MainWindow::loadDeviceSetPresetSettings(const Preset* preset, int deviceSetIndex)
{
	qDebug("MainWindow::loadDeviceSetPresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

	if (deviceSetIndex >= 0)
	{
        DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];
        deviceUISet->loadDeviceSetSettings(preset, m_pluginManager->getPluginAPI(), &m_workspaces, nullptr);
	}

    // m_spectrumToggleViewAction->setChecked(preset->getShowSpectrum());

	// // has to be last step
    // if (!preset->getLayout().isEmpty()) {
	//     restoreState(preset->getLayout());
    // }

    // tabifyDockWidget(ui->presetDock, ui->commandsDock); // override this setting
    // ui->presetDock->raise();
}

void MainWindow::saveDeviceSetPresetSettings(Preset* preset, int deviceSetIndex)
{
	qDebug("MainWindow::saveDeviceSetPresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

    // Save from currently selected source tab
    //int currentSourceTabIndex = ui->tabInputsView->currentIndex();
    DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];
    deviceUISet->saveDeviceSetSettings(preset);
    // preset->setShowSpectrum(m_spectrumToggleViewAction->isChecked());
    // preset->setLayout(saveState());
}

void MainWindow::loadFeatureSetPresetSettings(const FeatureSetPreset* preset, int featureSetIndex, Workspace *workspace)
{
	qDebug("MainWindow::loadFeatureSetPresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

	if (featureSetIndex >= 0)
	{
        FeatureUISet *featureSetUI = m_featureUIs[featureSetIndex];
        qDebug("MainWindow::loadFeatureSetPresetSettings: m_apiAdapter: %p", m_apiAdapter);
        featureSetUI->loadFeatureSetSettings(preset, m_pluginManager->getPluginAPI(), m_apiAdapter, &m_workspaces, workspace);
	}
}

void MainWindow::saveFeatureSetPresetSettings(FeatureSetPreset* preset, int featureSetIndex)
{
	qDebug("MainWindow::saveFeatureSetPresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

    // Save from currently selected source tab
    //int currentSourceTabIndex = ui->tabInputsView->currentIndex();
    FeatureUISet *featureUI = m_featureUIs[featureSetIndex];

    preset->clearFeatures();
    featureUI->saveFeatureSetSettings(preset);
}

void MainWindow::loadConfiguration(const Configuration *configuration, bool fromDialog)
{
	qDebug("MainWindow::loadConfiguration: configuration [%s | %s] %d workspace(s) - %d device set(s) - %d feature(s)",
		qPrintable(configuration->getGroup()),
		qPrintable(configuration->getDescription()),
        configuration->getNumberOfWorkspaces(),
        configuration->getDeviceSetPresets().size(),
        configuration->getFeatureSetPreset().getFeatureCount()
    );

    QMessageBox *waitBox = nullptr;

    if (fromDialog)
    {
        waitBox = new QMessageBox(this);
        waitBox->setStandardButtons(QMessageBox::NoButton);
        waitBox->setWindowModality(Qt::NonModal);
        waitBox->setIcon(QMessageBox::Information);
        waitBox->setText("Loading configuration                  ");
        waitBox->setInformativeText("Deleting existing...");
        waitBox->setWindowTitle("Please wait");
        waitBox->show();
        waitBox->raise();
    }

    // Wipe out everything first

    // Device sets
    while (m_deviceUIs.size() > 0) {
        removeLastDevice();
    }
    // Features
    m_featureUIs[0]->freeFeatures();
    // Workspaces
    for (const auto& workspace : m_workspaces) {
        delete workspace;
    }
    m_workspaces.clear();

    // Reconstruct

    // Workspaces
    for (int i = 0; i < configuration->getNumberOfWorkspaces(); i++) {
        addWorkspace();
    }

    if (m_workspaces.size() <= 0) { // cannot go further if there are no workspaces
        return;
    }

    // Device sets
    if (waitBox) {
        waitBox->setInformativeText("Loading device sets...");
    }

    const QList<Preset>& deviceSetPresets = configuration->getDeviceSetPresets();

    for (const auto& deviceSetPreset : deviceSetPresets)
    {
        if (deviceSetPreset.isSourcePreset())
        {
            int bestDeviceIndex = DeviceEnumerator::instance()->getBestRxSamplingDeviceIndex(
                deviceSetPreset.getSelectedDevice().m_deviceId,
                deviceSetPreset.getSelectedDevice().m_deviceSerial,
                deviceSetPreset.getSelectedDevice().m_deviceSequence,
                deviceSetPreset.getSelectedDevice().m_deviceItemIndex
            );
            qDebug("MainWindow::loadConfiguration: add source %s in workspace %d spectrum in %d",
                qPrintable(deviceSetPreset.getSelectedDevice().m_deviceId),
                deviceSetPreset.getDeviceWorkspaceIndex(),
                deviceSetPreset.getSpectrumWorkspaceIndex());
            int deviceWorkspaceIndex = deviceSetPreset.getDeviceWorkspaceIndex() < m_workspaces.size() ?
                deviceSetPreset.getDeviceWorkspaceIndex() :
                0;
            int spectrumWorkspaceIndex = deviceSetPreset.getSpectrumWorkspaceIndex() < m_workspaces.size() ?
                deviceSetPreset.getSpectrumWorkspaceIndex() :
                deviceWorkspaceIndex;
            sampleSourceAdd(m_workspaces[deviceWorkspaceIndex], m_workspaces[spectrumWorkspaceIndex], bestDeviceIndex);
        }
        else if (deviceSetPreset.isSinkPreset())
        {
            int bestDeviceIndex = DeviceEnumerator::instance()->getBestTxSamplingDeviceIndex(
                deviceSetPreset.getSelectedDevice().m_deviceId,
                deviceSetPreset.getSelectedDevice().m_deviceSerial,
                deviceSetPreset.getSelectedDevice().m_deviceSequence,
                deviceSetPreset.getSelectedDevice().m_deviceItemIndex
            );
            qDebug("MainWindow::loadConfiguration: add sink %s in workspace %d spectrum in %d",
                qPrintable(deviceSetPreset.getSelectedDevice().m_deviceId),
                deviceSetPreset.getDeviceWorkspaceIndex(),
                deviceSetPreset.getSpectrumWorkspaceIndex());
            int deviceWorkspaceIndex = deviceSetPreset.getDeviceWorkspaceIndex() < m_workspaces.size() ?
                deviceSetPreset.getDeviceWorkspaceIndex() :
                0;
            int spectrumWorkspaceIndex = deviceSetPreset.getSpectrumWorkspaceIndex() < m_workspaces.size() ?
                deviceSetPreset.getSpectrumWorkspaceIndex() :
                deviceWorkspaceIndex;
            sampleSinkAdd(m_workspaces[deviceWorkspaceIndex], m_workspaces[spectrumWorkspaceIndex], bestDeviceIndex);
        } else if (deviceSetPreset.isMIMOPreset())
        {
            int bestDeviceIndex = DeviceEnumerator::instance()->getBestMIMOSamplingDeviceIndex(
                deviceSetPreset.getSelectedDevice().m_deviceId,
                deviceSetPreset.getSelectedDevice().m_deviceSerial,
                deviceSetPreset.getSelectedDevice().m_deviceSequence
            );
            qDebug("MainWindow::loadConfiguration: add MIMO %s in workspace %d spectrum in %d",
                qPrintable(deviceSetPreset.getSelectedDevice().m_deviceId),
                deviceSetPreset.getDeviceWorkspaceIndex(),
                deviceSetPreset.getSpectrumWorkspaceIndex());
            int deviceWorkspaceIndex = deviceSetPreset.getDeviceWorkspaceIndex() < m_workspaces.size() ?
                deviceSetPreset.getDeviceWorkspaceIndex() :
                0;
            int spectrumWorkspaceIndex = deviceSetPreset.getSpectrumWorkspaceIndex() < m_workspaces.size() ?
                deviceSetPreset.getSpectrumWorkspaceIndex() :
                deviceWorkspaceIndex;
            sampleMIMOAdd(m_workspaces[deviceWorkspaceIndex], m_workspaces[spectrumWorkspaceIndex], bestDeviceIndex);
        }

        m_deviceUIs.back()->m_deviceGUI->restoreGeometry(deviceSetPreset.getDeviceGeometry());
        m_deviceUIs.back()->m_mainSpectrumGUI->restoreGeometry(deviceSetPreset.getSpectrumGeometry());
        m_deviceUIs.back()->loadDeviceSetSettings(&deviceSetPreset, m_pluginManager->getPluginAPI(), &m_workspaces, nullptr);
    }

    // Features
    if (waitBox) {
        waitBox->setInformativeText("Loading device sets...");
    }

    m_featureUIs[0]->loadFeatureSetSettings(
        &configuration->getFeatureSetPreset(),
        m_pluginManager->getPluginAPI(),
        m_apiAdapter,
        &m_workspaces,
        nullptr
    );

    for (int i = 0; i < m_featureUIs[0]->getNumberOfFeatures(); i++)
    {
        FeatureGUI *gui = m_featureUIs[0]->getFeatureGuiAt(i);
        QObject::connect(
            gui,
            &FeatureGUI::moveToWorkspace,
            this,
            [=](int wsIndexDest){ this->featureMove(gui, wsIndexDest); }
        );
    }

    // Lastly restore workspaces geometry
    if (waitBox) {
        waitBox->setInformativeText("Finalizing...");
    }

    for (int i = 0; i < configuration->getNumberOfWorkspaces(); i++) {
        m_workspaces[i]->restoreGeometry(configuration->getWorkspaceGeometries()[i]);
    }

    if (waitBox)
    {
        waitBox->close();
        delete waitBox;
    }
}

void MainWindow::saveConfiguration(Configuration *configuration)
{
	qDebug("MainWindow::saveConfiguration: configuration [%s | %s] %d workspaces",
		qPrintable(configuration->getGroup()),
		qPrintable(configuration->getDescription()),
        m_workspaces.size()
    );

    configuration->clearData();
    QList<Preset>& deviceSetPresets = configuration->getDeviceSetPresets();

    for (const auto& deviceUISet : m_deviceUIs)
    {
        deviceSetPresets.push_back(Preset());
        deviceUISet->saveDeviceSetSettings(&deviceSetPresets.back());
        deviceSetPresets.back().setSpectrumGeometry(deviceUISet->m_mainSpectrumGUI->saveGeometry());
        deviceSetPresets.back().setSpectrumWorkspaceIndex(deviceUISet->m_mainSpectrumGUI->getWorkspaceIndex());
        deviceSetPresets.back().setDeviceGeometry(deviceUISet->m_deviceGUI->saveGeometry());
        deviceSetPresets.back().setDeviceWorkspaceIndex(deviceUISet->m_deviceGUI->getWorkspaceIndex());
        qDebug("MainWindow::saveConfiguration: %s device in workspace %d spectrun in %d",
            qPrintable(deviceUISet->m_deviceAPI->getSamplingDeviceId()),
            deviceUISet->m_deviceGUI->getWorkspaceIndex(),
            deviceUISet->m_mainSpectrumGUI->getWorkspaceIndex());
    }

    m_featureUIs[0]->saveFeatureSetSettings(&configuration->getFeatureSetPreset());

    for (const auto& workspace : m_workspaces) {
        configuration->getWorkspaceGeometries().push_back(workspace->saveGeometry());
    }
}

QString MainWindow::openGLVersion()
{
    QOpenGLContext *glCurrentContext = QOpenGLContext::globalShareContext();
    if (glCurrentContext)
    {
        if (glCurrentContext->isValid())
        {
            int major = glCurrentContext->format().majorVersion();
            int minor = glCurrentContext->format().minorVersion();
            bool es = glCurrentContext->isOpenGLES();
            QString version = QString("%1.%2%3").arg(major).arg(minor).arg(es ? " ES" : "");
            // Waterfall doesn't work if major version is less than 3, so display in red
            if (major < 3) {
                version = "<span style=\"color:red\">" + version + "</span>";
            }
            return version;
        }
        else
        {
            return "N/A";
        }
    }
    else
    {
        return "N/A";
    }
}

void MainWindow::createMenuBar()
{
    QMenuBar *menuBar = this->menuBar();

    QMenu *fileMenu = menuBar->addMenu("File");
    QAction *exitAction = fileMenu->addAction("Exit");
    exitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    exitAction->setToolTip("Exit");
    QObject::connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    QMenu *viewMenu = menuBar->addMenu("View");
    QAction *fullscreenAction = viewMenu->addAction("Fullscreen");
    fullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    fullscreenAction->setToolTip("Toggle fullscreen view");
    fullscreenAction->setCheckable(true);
    QObject::connect(fullscreenAction, &QAction::triggered, this, &MainWindow::on_action_View_Fullscreen_toggled);

    QMenu *workspacesMenu = menuBar->addMenu("Workspaces");
    QAction *newWorkspaceAction = workspacesMenu->addAction("New");
    newWorkspaceAction->setToolTip("Add a new workspace");
    QObject::connect(newWorkspaceAction, &QAction::triggered, this, &MainWindow::addWorkspace);
    QAction *viewAllWorkspacesAction = workspacesMenu->addAction("View all");
    viewAllWorkspacesAction->setToolTip("View all workspaces");
    QObject::connect(viewAllWorkspacesAction, &QAction::triggered, this, &MainWindow::viewAllWorkspaces);
    QAction *removeEmptyWorkspacesAction = workspacesMenu->addAction("Remove empty");
    removeEmptyWorkspacesAction->setToolTip("Remove empty workspaces");
    QObject::connect(removeEmptyWorkspacesAction, &QAction::triggered, this, &MainWindow::removeEmptyWorkspaces);

    QMenu *preferencesMenu = menuBar->addMenu("Preferences");
    QAction *configurationsAction = preferencesMenu->addAction("Configurations");
    configurationsAction->setToolTip("Manage configurations");
    QObject::connect(configurationsAction, &QAction::triggered, this, &MainWindow::on_action_Configurations_triggered);
    QAction *audioAction = preferencesMenu->addAction("Audio");
    audioAction->setToolTip("Audio preferences");
    QObject::connect(audioAction, &QAction::triggered, this, &MainWindow::on_action_Audio_triggered);
    QAction *loggingAction = preferencesMenu->addAction("Logging");
    loggingAction->setToolTip("Logging preferences");
    QObject::connect(loggingAction, &QAction::triggered, this, &MainWindow::on_action_Logging_triggered);
    QAction *myPositionAction = preferencesMenu->addAction("My Position");
    myPositionAction->setToolTip("Set station position");
    QObject::connect(myPositionAction, &QAction::triggered, this, &MainWindow::on_action_My_Position_triggered);
    QAction *fftAction = preferencesMenu->addAction("FFT");
    fftAction->setToolTip("Set FFT cache");
    QObject::connect(fftAction, &QAction::triggered, this, &MainWindow::on_action_FFT_triggered);
    QAction *ambeAction = preferencesMenu->addAction("AMBE");
    ambeAction->setToolTip("AMBE options");
    QObject::connect(ambeAction, &QAction::triggered, this, &MainWindow::on_action_AMBE_triggered);
    QAction *limeRFEAction = preferencesMenu->addAction("Lime RFE");
    limeRFEAction->setToolTip("Lime RFE options");
    QObject::connect(limeRFEAction, &QAction::triggered, this, &MainWindow::on_action_LimeRFE_triggered);
    QMenu *devicesMenu = preferencesMenu->addMenu("Devices");
    QAction *userArgumentsAction = devicesMenu->addAction("User arguments");
    userArgumentsAction->setToolTip("Device custom user arguments");
    QObject::connect(userArgumentsAction, &QAction::triggered, this, &MainWindow::on_action_DeviceUserArguments_triggered);
    QAction *commandsAction = preferencesMenu->addAction("Commands");
    commandsAction->setToolTip("External commands dialog");
    QObject::connect(commandsAction, &QAction::triggered, this, &MainWindow::on_action_commands_triggered);
    QAction *saveAllAction = preferencesMenu->addAction("Save all");
    saveAllAction->setToolTip("Save all current settings");
    QObject::connect(saveAllAction, &QAction::triggered, this, &MainWindow::on_action_saveAll_triggered);

    QMenu *helpMenu = menuBar->addMenu("Help");
    QAction *quickStartAction = helpMenu->addAction("Quick start");
    quickStartAction->setToolTip("Instructions for quick start");
    QObject::connect(quickStartAction, &QAction::triggered, this, &MainWindow::on_action_Quick_Start_triggered);
    QAction *mainWindowAction = helpMenu->addAction("Main Window");
    mainWindowAction->setToolTip("Help on main window details");
    QObject::connect(mainWindowAction, &QAction::triggered, this, &MainWindow::on_action_Main_Window_triggered);
    QAction *loadedPluginsAction = helpMenu->addAction("Loaded plugins");
    loadedPluginsAction->setToolTip("List available plugins");
    QObject::connect(loadedPluginsAction, &QAction::triggered, this, &MainWindow::on_action_Loaded_Plugins_triggered);
    QAction *aboutAction = helpMenu->addAction("ABout SDRangel");
    aboutAction->setToolTip("SDRangel application details");
    QObject::connect(aboutAction, &QAction::triggered, this, &MainWindow::on_action_About_triggered);
}

void MainWindow::createStatusBar()
{
    QString qtVersionStr = QString("Qt %1 ").arg(QT_VERSION_STR);
    QString openGLVersionStr = QString("OpenGL %1 ").arg(openGLVersion());
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
    m_showSystemWidget = new QLabel("SDRangel " + qApp->applicationVersion() + " " + qtVersionStr + openGLVersionStr
            + QSysInfo::currentCpuArchitecture() + " " + QSysInfo::prettyProductName(), this);
#else
    m_showSystemWidget = new QLabel("SDRangel " + qApp->applicationVersion() + " " + qtVersionStr + openGLVersionStr, this);
#endif
    statusBar()->addPermanentWidget(m_showSystemWidget);

	m_dateTimeWidget = new QLabel(tr("Date"), this);
	m_dateTimeWidget->setToolTip(tr("Current date/time"));
	statusBar()->addPermanentWidget(m_dateTimeWidget);
}

void MainWindow::closeEvent(QCloseEvent *closeEvent)
{
    qDebug("MainWindow::closeEvent");

    // Save window size and position
    QSettings s;
    s.setValue("mainWindowGeometry", qCompress(saveGeometry()).toBase64());
    s.setValue("mainWindowState", qCompress(saveState()).toBase64());

    // saveDeviceSetPresetSettings(m_mainCore->m_settings.getWorkingPreset(), 0);
    // saveFeatureSetPresetSettings(m_mainCore->m_settings.getWorkingFeatureSetPreset(), 0);
    saveConfiguration(m_mainCore->m_settings.getWorkingConfiguration());
    m_mainCore->m_settings.save();

    while (m_deviceUIs.size() > 0) {
        removeLastDevice();
    }

    closeEvent->accept();
}

void MainWindow::updatePresetControls()
{
	// ui->presetTree->resizeColumnToContents(0);

	// if(ui->presetTree->currentItem() != 0)
	// {
	// 	ui->presetDelete->setEnabled(true);
	// 	ui->presetLoad->setEnabled(true);
	// }
	// else
	// {
	// 	ui->presetDelete->setEnabled(false);
	// 	ui->presetLoad->setEnabled(false);
	// }
}

void MainWindow::applySettings()
{
 	// loadDeviceSetPresetSettings(m_mainCore->m_settings.getWorkingPreset(), 0);
    // loadFeatureSetPresetSettings(m_mainCore->m_settings.getWorkingFeatureSetPreset(), 0);
    loadConfiguration(m_mainCore->m_settings.getWorkingConfiguration());

    m_mainCore->m_settings.sortPresets();
    // int middleIndex = m_mainCore->m_settings.getPresetCount() / 2;
    // QTreeWidgetItem *treeItem;
    // ui->presetTree->clear();

    // for (int i = 0; i < m_mainCore->m_settings.getPresetCount(); ++i)
    // {
    //     treeItem = addPresetToTree(m_mainCore->m_settings.getPreset(i));

    //     if (i == middleIndex) {
    //         ui->presetTree->setCurrentItem(treeItem);
    //     }
    // }

    // m_mainCore->m_settings.sortCommands();
    // ui->commandTree->clear();

    // for (int i = 0; i < m_mainCore->m_settings.getCommandCount(); ++i) {
    //     treeItem = addCommandToTree(m_mainCore->m_settings.getCommand(i));
    // }

    m_mainCore->setLoggingOptions();
}

bool MainWindow::handleMessage(const Message& cmd)
{
    if (MainCore::MsgLoadPreset::match(cmd))
    {
        MainCore::MsgLoadPreset& notif = (MainCore::MsgLoadPreset&) cmd;
        int deviceSetIndex =  notif.getDeviceSetIndex();
        const Preset *preset = notif.getPreset();

        if (deviceSetIndex < (int) m_deviceUIs.size())
        {
            DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];
            deviceUISet->loadDeviceSetSettings(preset, m_pluginManager->getPluginAPI(), &m_workspaces, nullptr);
        }

        return true;
    }
    else if (MainCore::MsgSavePreset::match(cmd))
    {
        // MainCore::MsgSavePreset& notif = (MainCore::MsgSavePreset&) cmd;
        // saveDeviceSetPresetSettings(notif.getPreset(), notif.getDeviceSetIndex());
        // if (notif.isNewPreset()) { ui->presetTree->setCurrentItem(addPresetToTree(notif.getPreset())); }
        // m_mainCore->m_settings.sortPresets();
        // m_mainCore->m_settings.save();
        return true;
    }
    else if (MainCore::MsgLoadFeatureSetPreset::match(cmd))
    {
        if (m_workspaces.size() > 0)
        {
            MainCore::MsgLoadFeatureSetPreset& notif = (MainCore::MsgLoadFeatureSetPreset&) cmd;
            loadFeatureSetPresetSettings(notif.getPreset(), notif.getFeatureSetIndex(), m_workspaces[0]);
        }

        return true;
    }
    else if (MainCore::MsgSaveFeatureSetPreset::match(cmd))
    {
        MainCore::MsgSaveFeatureSetPreset& notif = (MainCore::MsgSaveFeatureSetPreset&) cmd;
        saveFeatureSetPresetSettings(notif.getPreset(), notif.getFeatureSetIndex());
        m_mainCore->m_settings.sortFeatureSetPresets();
        m_mainCore->m_settings.save();
        return true;
    }
    else if (MainCore::MsgDeletePreset::match(cmd))
    {
        MainCore::MsgDeletePreset& notif = (MainCore::MsgDeletePreset&) cmd;
        const Preset *presetToDelete = notif.getPreset();

        // remove preset from tree
        // for (int ig = 0; ig < ui->presetTree->topLevelItemCount(); ig++)
        // {
        //     QTreeWidgetItem *groupItem = ui->presetTree->topLevelItem(ig);
        //     if (groupItem->text(0) == presetToDelete->getGroup())
        //     {
        //         for (int ip = 0; ip < groupItem->childCount(); ip++)
        //         {
        //             QTreeWidgetItem *presetItem = groupItem->child(ip);
        //             const Preset* preset = qvariant_cast<const Preset*>(presetItem->data(0, Qt::UserRole));

        //             if ((preset->getGroup() == presetToDelete->getGroup()) &&
        //                 (preset->getCenterFrequency() == presetToDelete->getCenterFrequency()) &&
        //                 (preset->getDescription() == presetToDelete->getDescription()) &&
        //                 (preset->getPresetType() == presetToDelete->getPresetType()))
        //             {
        //                 groupItem->takeChild(ip);
        //             }
        //         }
        //     }
        // }

        // remove preset from settings
        m_mainCore->m_settings.deletePreset(presetToDelete);
        return true;
    }
    else if (MainCore::MsgDeleteFeatureSetPreset::match(cmd))
    {
        MainCore::MsgDeleteFeatureSetPreset& notif = (MainCore::MsgDeleteFeatureSetPreset&) cmd;
        const FeatureSetPreset *presetToDelete = notif.getPreset();
        // remove preset from settings
        m_mainCore->m_settings.deleteFeatureSetPreset(presetToDelete);
        return true;
    }
    else if (MainCore::MsgAddDeviceSet::match(cmd))
    {
        MainCore::MsgAddDeviceSet& notif = (MainCore::MsgAddDeviceSet&) cmd;
        int direction = notif.getDirection();

        // TODO: implement add workspace API. Will have to give the workspace index that will be ignored
        // in Server flavor. Set nullptr for workspace if index is out of bonds

        if (direction == 1) { // Single stream Tx
            sampleSinkAdd(nullptr, nullptr, -1); // create with file output device by default
        } else if (direction == 0) { // Single stream Rx
            sampleSourceAdd(nullptr, nullptr, -1); // create with file input device by default
        } else if (direction == 2) { // MIMO
            sampleMIMOAdd(nullptr, nullptr, -1); // create with testMI MIMO device y default
        }

        return true;
    }
    else if (MainCore::MsgRemoveLastDeviceSet::match(cmd))
    {
        if (m_deviceUIs.size() > 1) {
            removeLastDevice();
        }

        return true;
    }
    else if (MainCore::MsgSetDevice::match(cmd))
    {
        MainCore::MsgSetDevice& notif = (MainCore::MsgSetDevice&) cmd;

        // TODO: implement add workspace API. Will have to give the workspace index that will be ignored
        // in Server flavor. Set nullptr for workspace if index is out of bonds

        sampleDeviceChange(notif.getDeviceType(), notif.getDeviceSetIndex(), notif.getDeviceIndex(), nullptr);

        return true;
    }
    else if (MainCore::MsgAddFeatureSet::match(cmd))
    {
        addFeatureSet();
        return true;
    }
    else if (MainCore::MsgRemoveLastFeatureSet::match(cmd))
    {
        if (m_mainCore->m_featureSets.size() != 0) {
            removeFeatureSet(m_mainCore->m_featureSets.size() - 1);
        }

        return true;
    }
    else if (MainCore::MsgAddChannel::match(cmd))
    {
        // MainCore::MsgAddChannel& notif = (MainCore::MsgAddChannel&) cmd;
        // ui->tabInputsView->setCurrentIndex(notif.getDeviceSetIndex());
        // int currentChannelTabIndex = ui->tabChannels->currentIndex();

        // if (currentChannelTabIndex >= 0)
        // {
        //     DeviceUISet *deviceUI = m_deviceUIs[currentChannelTabIndex];
        //     int channelRegistrationIndex;

        //     if (deviceUI->m_deviceMIMOEngine)
        //     {
        //         int nbMIMOChannels = deviceUI->getNumberOfAvailableMIMOChannels();
        //         int nbRxChannels = deviceUI->getNumberOfAvailableRxChannels();
        //         int direction = notif.getDirection();

        //         if (direction == 2) {
        //             channelRegistrationIndex = notif.getChannelRegistrationIndex();
        //         } else if (direction == 0) {
        //             channelRegistrationIndex = nbMIMOChannels + notif.getChannelRegistrationIndex();
        //         } else {
        //             channelRegistrationIndex = nbMIMOChannels + nbRxChannels + notif.getChannelRegistrationIndex();
        //         }
        //     }
        //     else
        //     {
        //         channelRegistrationIndex = notif.getChannelRegistrationIndex();
        //     }

        //     channelAddClicked(channelRegistrationIndex);
        // }

        return true;
    }
    else if (MainCore::MsgDeleteChannel::match(cmd))
    {
        MainCore::MsgDeleteChannel& notif = (MainCore::MsgDeleteChannel&) cmd;
        deleteChannel(notif.getDeviceSetIndex(), notif.getChannelIndex());
        return true;
    }
    else if (MainCore::MsgDeviceSetFocus::match(cmd))
    {
        // MainCore::MsgDeviceSetFocus& notif = (MainCore::MsgDeviceSetFocus&) cmd;
        // int index = notif.getDeviceSetIndex();
        // if ((index >= 0) && (index < (int) m_deviceUIs.size())) {
        //     ui->tabInputsView->setCurrentIndex(index);
        // }

        return true;
    }
    else if (MainCore::MsgAddFeature::match(cmd))
    {
        // MainCore::MsgAddFeature& notif = (MainCore::MsgAddFeature&) cmd;
        // ui->tabFeatures->setCurrentIndex(notif.getFeatureSetIndex());
        // featureAddClicked(notif.getFeatureRegistrationIndex());

        return true;
    }
    else if (MainCore::MsgDeleteFeature::match(cmd))
    {
        MainCore::MsgDeleteFeature& notif = (MainCore::MsgDeleteFeature&) cmd;
        deleteFeature(notif.getFeatureSetIndex(), notif.getFeatureIndex());
        return true;
    }
    else if (MainCore::MsgApplySettings::match(cmd))
    {
        applySettings();
        return true;
    }
    else if (MainCore::MsgDVSerial::match(cmd))
    {
        // MainCore::MsgDVSerial& notif = (MainCore::MsgDVSerial&) cmd;
        // ui->action_DV_Serial->setChecked(notif.getActive());
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

void MainWindow::handleWorkspaceVisibility(Workspace *workspace, bool visibility)
{
    qDebug("MainWindow::handleWorkspaceHasFocus: index: %d %s",
        workspace->getIndex(), visibility ? "visible" : "non visible");
    m_currentWorkspace = workspace;
}

void MainWindow::addWorkspace()
{
    int workspaceIndex = m_workspaces.size();
    m_workspaces.push_back(new Workspace(workspaceIndex));
    QStringList featureNames;
    m_pluginManager->listFeatures(featureNames);
    m_workspaces.back()->addAvailableFeatures(featureNames);
    this->addDockWidget(Qt::LeftDockWidgetArea, m_workspaces.back());

    QObject::connect(
        m_workspaces.back(),
        &Workspace::addRxDevice,
        this,
        [=](Workspace *inWorkspace, int deviceIndex) { this->sampleSourceAdd(inWorkspace, inWorkspace, deviceIndex); }
    );

    QObject::connect(
        m_workspaces.back(),
        &Workspace::addTxDevice,
        this,
        [=](Workspace *inWorkspace, int deviceIndex) { this->sampleSourceAdd(inWorkspace, inWorkspace, deviceIndex); }
    );

    QObject::connect(
        m_workspaces.back(),
        &Workspace::addMIMODevice,
        this,
        [=](Workspace *inWorkspace, int deviceIndex) { this->sampleSourceAdd(inWorkspace, inWorkspace, deviceIndex); }
    );

    QObject::connect(
        m_workspaces.back(),
        &Workspace::addFeature,
        this,
        &MainWindow::featureAddClicked
    );

    QObject::connect(
        m_workspaces.back(),
        &Workspace::featurePresetsDialogRequested,
        this,
        &MainWindow::openFeaturePresetsDialog
    );

    if (m_workspaces.size() > 1)
    {
        for (int i = 1; i < m_workspaces.size(); i++) {
            tabifyDockWidget(m_workspaces[0], m_workspaces[i]);
        }

        m_workspaces.back()->show();
        m_workspaces.back()->raise();
        // QList<QTabBar *> tabBars = findChildren<QTabBar *>();

        // if (tabBars.size() > 0) {
        //     tabBars.back()->setStyleSheet("QTabBar::tab:selected { background: rgb(128,70,0); }"); // change text color so it is visible
        // }
    }
}

void MainWindow::viewAllWorkspaces()
{
    for (const auto& workspace : m_workspaces)
    {
        if (workspace->isHidden()) {
            workspace->show();
        }
    }
}

void MainWindow::removeEmptyWorkspaces()
{
    QList<Workspace *>::iterator it = m_workspaces.begin();

    while (it != m_workspaces.end())
    {
        if ((*it)->getNumberOfSubWindows() == 0)
        {
            removeDockWidget(*it);
            it = m_workspaces.erase(it);
        }
        else
        {
            ++it;
        }
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

void MainWindow::commandKeysConnect(QObject *object, const char *slot)
{
    setFocus();
    connect(
        m_commandKeyReceiver,
        SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
        object,
        slot
    );
}

void MainWindow::commandKeysDisconnect(QObject *object, const char *slot)
{
    disconnect(
        m_commandKeyReceiver,
        SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
        object,
        slot
    );
}

void MainWindow::on_presetSave_clicked()
{
    // QStringList groups;
    // QString group;
    // QString description = "";

    // for(int i = 0; i < ui->presetTree->topLevelItemCount(); i++) {
    //     groups.append(ui->presetTree->topLevelItem(i)->text(0));
    // }

    // QTreeWidgetItem* item = ui->presetTree->currentItem();

    // if(item != 0)
    // {
    //     if(item->type() == PGroup) {
    //         group = item->text(0);
    //     } else if(item->type() == PItem) {
    //         group = item->parent()->text(0);
    //         description = item->text(0);
    //     }
    // }

    // AddPresetDialog dlg(groups, group, this);

    // if (description.length() > 0) {
    //     dlg.setDescription(description);
    // }

    // if(dlg.exec() == QDialog::Accepted) {
    //     Preset* preset = m_mainCore->m_settings.newPreset(dlg.group(), dlg.description());
    //     saveDeviceSetPresetSettings(preset, ui->tabInputsView->currentIndex());

    //     ui->presetTree->setCurrentItem(addPresetToTree(preset));
    // }

    // m_mainCore->m_settings.sortPresets();
}

void MainWindow::on_presetUpdate_clicked()
{
	// QTreeWidgetItem* item = ui->presetTree->currentItem();
	// const Preset* changedPreset = 0;

	// if(item != 0)
	// {
	// 	if(item->type() == PItem)
	// 	{
	// 		const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));

	// 		if (preset != 0)
	// 		{
	// 			Preset* preset_mod = const_cast<Preset*>(preset);
	// 			saveDeviceSetPresetSettings(preset_mod, ui->tabInputsView->currentIndex());
	// 			changedPreset = preset;
	// 		}
	// 	}
	// }

	// m_mainCore->m_settings.sortPresets();
    // ui->presetTree->clear();

    // for (int i = 0; i < m_mainCore->m_settings.getPresetCount(); ++i)
    // {
    //     QTreeWidgetItem *item_x = addPresetToTree(m_mainCore->m_settings.getPreset(i));
    //     const Preset* preset_x = qvariant_cast<const Preset*>(item_x->data(0, Qt::UserRole));
    //     if (changedPreset &&  (preset_x == changedPreset)) { // set cursor on changed preset
    //         ui->presetTree->setCurrentItem(item_x);
    //     }
    // }
}

void MainWindow::on_presetEdit_clicked()
{
    // QTreeWidgetItem* item = ui->presetTree->currentItem();
    // QStringList groups;
    // bool change = false;
    // const Preset *changedPreset = 0;
    // QString newGroupName;

    // for(int i = 0; i < ui->presetTree->topLevelItemCount(); i++) {
    //     groups.append(ui->presetTree->topLevelItem(i)->text(0));
    // }

    // if(item != 0)
    // {
    //     if (item->type() == PItem)
    //     {
    //         const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));
    //         AddPresetDialog dlg(groups, preset->getGroup(), this);
    //         dlg.setDescription(preset->getDescription());

    //         if (dlg.exec() == QDialog::Accepted)
    //         {
    //             Preset* preset_mod = const_cast<Preset*>(preset);
    //             preset_mod->setGroup(dlg.group());
    //             preset_mod->setDescription(dlg.description());
    //             change = true;
    //             changedPreset = preset;
    //         }
    //     }
    //     else if (item->type() == PGroup)
    //     {
    //         AddPresetDialog dlg(groups, item->text(0), this);
    //         dlg.showGroupOnly();
    //         dlg.setDialogTitle("Edit preset group");

    //         if (dlg.exec() == QDialog::Accepted)
    //         {
    //             m_mainCore->m_settings.renamePresetGroup(item->text(0), dlg.group());
    //             newGroupName = dlg.group();
    //             change = true;
    //         }
    //     }
    // }

    // if (change)
    // {
    //     m_mainCore->m_settings.sortPresets();
    //     ui->presetTree->clear();

    //     for (int i = 0; i < m_mainCore->m_settings.getPresetCount(); ++i)
    //     {
    //         QTreeWidgetItem *item_x = addPresetToTree(m_mainCore->m_settings.getPreset(i));
    //         const Preset* preset_x = qvariant_cast<const Preset*>(item_x->data(0, Qt::UserRole));
    //         if (changedPreset &&  (preset_x == changedPreset)) { // set cursor on changed preset
    //             ui->presetTree->setCurrentItem(item_x);
    //         }
    //     }

    //     if (!changedPreset) // on group name change set cursor on the group that has been changed
    //     {
    //         for(int i = 0; i < ui->presetTree->topLevelItemCount(); i++)
    //         {
    //             QTreeWidgetItem* item = ui->presetTree->topLevelItem(i);

    //             if (item->text(0) == newGroupName) {
    //                 ui->presetTree->setCurrentItem(item);
    //             }
    //         }
    //     }
    // }
}

void MainWindow::on_presetExport_clicked()
{
	// QTreeWidgetItem* item = ui->presetTree->currentItem();

	// if(item != 0) {
	// 	if(item->type() == PItem)
	// 	{
	// 		const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));
	// 		QString base64Str = preset->serialize().toBase64();
	// 		QString fileName = QFileDialog::getSaveFileName(this,
	// 		    tr("Open preset export file"), ".", tr("Preset export files (*.prex)"), 0, QFileDialog::DontUseNativeDialog);

	// 		if (fileName != "")
	// 		{
	// 			QFileInfo fileInfo(fileName);

	// 			if (fileInfo.suffix() != "prex") {
	// 				fileName += ".prex";
	// 			}

	// 			QFile exportFile(fileName);

	// 			if (exportFile.open(QIODevice::WriteOnly | QIODevice::Text))
	// 			{
	// 				QTextStream outstream(&exportFile);
	// 				outstream << base64Str;
	// 				exportFile.close();
	// 			}
	// 			else
	// 			{
	// 		    	QMessageBox::information(this, tr("Message"), tr("Cannot open file for writing"));
	// 			}
	// 		}
	// 	}
	// }
}

void MainWindow::on_presetImport_clicked()
{
	// QTreeWidgetItem* item = ui->presetTree->currentItem();

	// if(item != 0)
	// {
	// 	QString group;

	// 	if (item->type() == PGroup)	{
	// 		group = item->text(0);
	// 	} else if (item->type() == PItem) {
	// 		group = item->parent()->text(0);
	// 	} else {
	// 		return;
	// 	}

	// 	QString fileName = QFileDialog::getOpenFileName(this,
	// 	    tr("Open preset export file"), ".", tr("Preset export files (*.prex)"), 0, QFileDialog::DontUseNativeDialog);

	// 	if (fileName != "")
	// 	{
	// 		QFile exportFile(fileName);

	// 		if (exportFile.open(QIODevice::ReadOnly | QIODevice::Text))
	// 		{
	// 			QByteArray base64Str;
	// 			QTextStream instream(&exportFile);
	// 			instream >> base64Str;
	// 			exportFile.close();

	// 			Preset* preset = m_mainCore->m_settings.newPreset("", "");
	// 			preset->deserialize(QByteArray::fromBase64(base64Str));
	// 			preset->setGroup(group); // override with current group

	// 			ui->presetTree->setCurrentItem(addPresetToTree(preset));
	// 		}
	// 		else
	// 		{
	// 			QMessageBox::information(this, tr("Message"), tr("Cannot open file for reading"));
	// 		}
	// 	}
	// }
}

void MainWindow::on_action_saveAll_triggered()
{
    // saveDeviceSetPresetSettings(m_mainCore->m_settings.getWorkingPreset(), ui->tabInputsView->currentIndex());
    // saveFeatureSetPresetSettings(m_mainCore->m_settings.getWorkingFeatureSetPreset(), ui->tabFeatures->currentIndex());
    saveConfiguration(m_mainCore->m_settings.getWorkingConfiguration());
    m_mainCore->m_settings.save();
    QMessageBox::information(this, tr("Done"), tr("All curent settings saved"));
}

void MainWindow::on_presetLoad_clicked()
{
	// qDebug() << "MainWindow::on_presetLoad_clicked";

	// QTreeWidgetItem* item = ui->presetTree->currentItem();

	// if(item == 0)
	// {
	// 	qDebug("MainWindow::on_presetLoad_clicked: item null");
	// 	updatePresetControls();
	// 	return;
	// }

	// const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));

	// if(preset == 0)
	// {
	// 	qDebug("MainWindow::on_presetLoad_clicked: preset null");
	// 	return;
	// }

	// loadDeviceSetPresetSettings(preset, ui->tabInputsView->currentIndex());
}

void MainWindow::on_presetDelete_clicked()
{
	// QTreeWidgetItem* item = ui->presetTree->currentItem();

	// if (item == 0)
	// {
	// 	updatePresetControls();
	// 	return;
	// }
	// else
	// {
    //     if (item->type() == PItem)
    //     {
    //         const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));

    //         if (preset)
    //         {
    //             if(QMessageBox::question(this, tr("Delete Preset"), tr("Do you want to delete preset '%1'?").arg(preset->getDescription()), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
    //                 delete item;
    //                 m_mainCore->m_settings.deletePreset(preset);
    //             }
    //         }
    //     }
    //     else if (item->type() == PGroup)
    //     {
    //         if (QMessageBox::question(this,
    //                 tr("Delete preset group"),
    //                 tr("Do you want to delete preset group '%1'?")
    //                     .arg(item->text(0)), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
    //         {
    //             m_mainCore->m_settings.deletePresetGroup(item->text(0));

    //             ui->presetTree->clear();

    //             for (int i = 0; i < m_mainCore->m_settings.getPresetCount(); ++i) {
    //                 addPresetToTree(m_mainCore->m_settings.getPreset(i));
    //             }
    //         }
    //     }
	// }
}

void MainWindow::on_presetTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    (void) current;
    (void) previous;
	// updatePresetControls();
}

void MainWindow::on_presetTree_itemActivated(QTreeWidgetItem *item, int column)
{
    (void) item;
    (void) column;
	// on_presetLoad_clicked();
}

void MainWindow::on_action_Quick_Start_triggered()
{
    QDesktopServices::openUrl(QUrl("https://github.com/f4exb/sdrangel/wiki/Quick-start"));
}

void MainWindow::on_action_Main_Window_triggered()
{
    QDesktopServices::openUrl(QUrl("https://github.com/f4exb/sdrangel/blob/master/sdrgui/readme.md"));
}

void MainWindow::on_action_Loaded_Plugins_triggered()
{
    PluginsDialog pluginsDialog(m_pluginManager, this);
    pluginsDialog.exec();
}

void MainWindow::on_action_Configurations_triggered()
{
    ConfigurationsDialog dialog(this);
    dialog.setConfigurations(m_mainCore->m_settings.getConfigurations());
    dialog.populateTree();
    QObject::connect(
        &dialog,
        &ConfigurationsDialog::saveConfiguration,
        this,
        &MainWindow::saveConfiguration
    );
    QObject::connect(
        &dialog,
        &ConfigurationsDialog::loadConfiguration,
        this,
        [=](const Configuration* configuration) { this->loadConfiguration(configuration, true); }
    );
    dialog.exec();
}

void MainWindow::on_action_Audio_triggered()
{
	AudioDialogX audioDialog(m_dspEngine->getAudioDeviceManager(), this);
	audioDialog.exec();
}

void MainWindow::on_action_Logging_triggered()
{
    LoggingDialog loggingDialog(m_mainCore->m_settings, this);
    loggingDialog.exec();
    m_mainCore->setLoggingOptions();
}

void MainWindow::on_action_My_Position_triggered()
{
	MyPositionDialog myPositionDialog(m_mainCore->m_settings, this);
	myPositionDialog.exec();
}

void MainWindow::on_action_DeviceUserArguments_triggered()
{
    qDebug("MainWindow::on_action_DeviceUserArguments_triggered");
    DeviceUserArgsDialog deviceUserArgsDialog(DeviceEnumerator::instance(), m_mainCore->m_settings.getDeviceUserArgs(), this);
    deviceUserArgsDialog.exec();
}

void MainWindow::on_action_commands_triggered()
{
    qDebug("MainWindow::on_action_commands_triggered");
    CommandsDialog commandsDialog(this);
    commandsDialog.setApiHost(m_apiServer->getHost());
    commandsDialog.setApiPort(m_apiServer->getPort());
    commandsDialog.setCommandKeyReceiver(m_commandKeyReceiver);
    commandsDialog.populateTree();
    commandsDialog.exec();
}

void MainWindow::on_action_FFT_triggered()
{
    qDebug("MainWindow::on_action_FFT_triggered");

    if (m_fftWisdomProcess)
    {
        QMessageBox::information(this, "FFTW Wisdom", QString("Process %1 is already running").arg(m_fftWisdomProcess->processId()));
        return;
    }

    m_fftWisdomProcess = new QProcess(this);
    connect(m_fftWisdomProcess,
        SIGNAL(finished(int, QProcess::ExitStatus)),
        this,
        SLOT(fftWisdomProcessFinished(int, QProcess::ExitStatus)));
    FFTWisdomDialog fftWisdomDialog(m_fftWisdomProcess, this);

    if (fftWisdomDialog.exec() == QDialog::Rejected)
    {
        disconnect(m_fftWisdomProcess,
            SIGNAL(finished(int, QProcess::ExitStatus)),
            this,
            SLOT(fftWisdomProcessFinished(int, QProcess::ExitStatus)));
        delete m_fftWisdomProcess;
        m_fftWisdomProcess = nullptr;
    }
    else
    {
        QMessageBox::information(this, "FFTW Wisdom", QString("Process %1 started").arg(m_fftWisdomProcess->processId()));
    }
}

void MainWindow::fftWisdomProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug("MainWindow::fftWisdomProcessFinished: process finished rc=%d (%d)", exitCode, (int) exitStatus);

    if ((exitCode != 0) || (exitStatus != QProcess::NormalExit))
    {
        QMessageBox::critical(this, "FFTW Wisdom", "fftwf-widdsom program failed");
    }
    else
    {
        QString log = m_fftWisdomProcess->readAllStandardOutput();
        QMessageBox::information(this, "FFTW Wisdom", QString("Success\n%1").arg(log));

    }

    delete m_fftWisdomProcess;
    m_fftWisdomProcess = nullptr;
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
    LimeRFEUSBDialog *limeRFEUSBDialog = new LimeRFEUSBDialog(m_mainCore->m_settings.getLimeRFEUSBCalib(), this);
    limeRFEUSBDialog->setModal(false);
    limeRFEUSBDialog->show();
#endif
}

void MainWindow::sampleDeviceChangeHandler(DeviceGUI *deviceGUI, int newDeviceIndex)
{
    int deviceType = (int) deviceGUI->getDeviceType();
    int deviceSetIndex = deviceGUI->getIndex();
    Workspace *workspace = m_workspaces[deviceGUI->getWorkspaceIndex()];
    sampleDeviceChange(deviceType, deviceSetIndex, newDeviceIndex, workspace);
}

void MainWindow::sampleDeviceChange(int deviceType, int deviceSetIndex, int newDeviceIndex, Workspace *workspace)
{
    qDebug("MainWindow::sampleDeviceChange: deviceType: %d deviceSetIndex: %d newDeviceIndex: %d",
        deviceType, deviceSetIndex, newDeviceIndex);
    if (deviceType == 0) {
        sampleSourceChange(deviceSetIndex, newDeviceIndex, workspace);
    } else if (deviceType == 1) {
        sampleSinkChange(deviceSetIndex, newDeviceIndex, workspace);
    } else if (deviceType == 2) {
        sampleMIMOChange(deviceSetIndex, newDeviceIndex, workspace);
    }

    emit MainCore::instance()->deviceChanged(deviceSetIndex);
}

void MainWindow::sampleSourceChange(int deviceSetIndex, int newDeviceIndex, Workspace *workspace)
{
    if (deviceSetIndex >= 0)
    {
        qDebug("MainWindow::sampleSourceChange: deviceSet %d workspace: %d", deviceSetIndex, workspace->getIndex());
        DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];
        DeviceGUI *gui = deviceUISet->m_deviceGUI;
        workspace->removeFromMdiArea(gui);
        // deviceUI->m_deviceAPI->saveSamplingDeviceSettings(m_mainCore->m_settings.getWorkingPreset()); // save old API settings
        deviceUISet->m_deviceAPI->stopDeviceEngine();

        // deletes old UI and input object
        deviceUISet->m_deviceAPI->getSampleSource()->setMessageQueueToGUI(nullptr); // have source stop sending messages to the GUI
        //deviceUISet->m_deviceGUI->destroy();
        deviceUISet->m_deviceAPI->resetSamplingDeviceId();
        deviceUISet->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceInput(deviceUISet->m_deviceAPI->getSampleSource());
        deviceUISet->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

        sampleSourceCreate(deviceSetIndex, newDeviceIndex, deviceUISet->m_deviceAPI, deviceUISet);
        deviceUISet->m_deviceGUI->setWorkspaceIndex(workspace->getIndex());
        workspace->addToMdiArea(deviceUISet->m_deviceGUI);

        QObject::connect(
            deviceUISet->m_deviceGUI,
            &DeviceGUI::addChannelEmitted,
            this,
            [=](int channelIndex){ this->channelAddClicked(workspace, deviceSetIndex, channelIndex); }
        );
    }
}

void MainWindow::sampleSinkChange(int deviceSetIndex, int newDeviceIndex, Workspace *workspace)
{
    if (deviceSetIndex >= 0)
    {
        qDebug("MainWindow::sampleSinkChange: deviceSet %d workspace: %d", deviceSetIndex, workspace->getIndex());
        DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];
        deviceUISet->m_deviceAPI->saveSamplingDeviceSettings(m_mainCore->m_settings.getWorkingPreset()); // save old API settings
        deviceUISet->m_deviceAPI->stopDeviceEngine();

        // deletes old UI and output object
        deviceUISet->m_deviceAPI->getSampleSink()->setMessageQueueToGUI(nullptr); // have sink stop sending messages to the GUI
        // m_deviceUIs[deviceSetIndex]->m_deviceGUI->destroy();
        deviceUISet->m_deviceAPI->resetSamplingDeviceId();
        deviceUISet->m_deviceAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput(deviceUISet->m_deviceAPI->getSampleSink());
        deviceUISet->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

        sampleSourceCreate(deviceSetIndex, newDeviceIndex, deviceUISet->m_deviceAPI, deviceUISet);
        deviceUISet->m_deviceGUI->setWorkspaceIndex(workspace->getIndex());
        workspace->addToMdiArea(deviceUISet->m_deviceGUI);

        QObject::connect(
            deviceUISet->m_deviceGUI,
            &DeviceGUI::addChannelEmitted,
            this,
            [=](int channelIndex){ this->channelAddClicked(workspace, deviceSetIndex, channelIndex); }
        );
    }
}

void MainWindow::sampleMIMOChange(int deviceSetIndex, int newDeviceIndex, Workspace *workspace)
{
    if (deviceSetIndex >= 0)
    {
        qDebug("MainWindow::sampleSinkChange: deviceSet %d workspace: %d", deviceSetIndex, workspace->getIndex());
        DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];
        deviceUISet->m_deviceAPI->saveSamplingDeviceSettings(m_mainCore->m_settings.getWorkingPreset()); // save old API settings
        deviceUISet->m_deviceAPI->stopDeviceEngine();

        // deletes old UI and output object
        deviceUISet->m_deviceAPI->getSampleMIMO()->setMessageQueueToGUI(nullptr); // have sink stop sending messages to the GUI
        // m_deviceUIs[tabIndex]->m_deviceGUI->destroy();
        deviceUISet->m_deviceAPI->resetSamplingDeviceId();
        deviceUISet->m_deviceAPI->getPluginInterface()->deleteSampleMIMOPluginInstanceMIMO(deviceUISet->m_deviceAPI->getSampleMIMO());

        sampleSourceCreate(deviceSetIndex, newDeviceIndex, deviceUISet->m_deviceAPI, deviceUISet);
        deviceUISet->m_deviceGUI->setWorkspaceIndex(workspace->getIndex());
        workspace->addToMdiArea(deviceUISet->m_deviceGUI);

        QObject::connect(
            deviceUISet->m_deviceGUI,
            &DeviceGUI::addChannelEmitted,
            this,
            [=](int channelIndex){ this->channelAddClicked(workspace, deviceSetIndex, channelIndex); }
        );
    }
}

void MainWindow::channelAddClicked(Workspace *workspace, int deviceSetIndex, int channelIndex)
{
    if (deviceSetIndex >= 0)
    {
        DeviceUISet *deviceUI = m_deviceUIs[deviceSetIndex];
        ChannelGUI *gui = nullptr;
        DeviceAPI *deviceAPI = deviceUI->m_deviceAPI;

        if (deviceUI->m_deviceSourceEngine) // source device => Rx channels
        {
            PluginAPI::ChannelRegistrations *channelRegistrations = m_pluginManager->getRxChannelRegistrations(); // Available channel plugins
            PluginInterface *pluginInterface = (*channelRegistrations)[channelIndex].m_plugin;
            ChannelAPI *channelAPI;
            BasebandSampleSink *rxChannel;
            pluginInterface->createRxChannel(deviceUI->m_deviceAPI, &rxChannel, &channelAPI);
            gui = pluginInterface->createRxChannelGUI(deviceUI, rxChannel);
            deviceUI->registerRxChannelInstance(channelAPI, gui);
            gui->setDeviceType(ChannelGUI::DeviceRx);
            gui->setIndex(channelAPI->getIndexInDeviceSet());
        }
        else if (deviceUI->m_deviceSinkEngine) // sink device => Tx channels
        {
            PluginAPI::ChannelRegistrations *channelRegistrations = m_pluginManager->getTxChannelRegistrations(); // Available channel plugins
            PluginInterface *pluginInterface = (*channelRegistrations)[channelIndex].m_plugin;
            ChannelAPI *channelAPI;
            BasebandSampleSource *txChannel;
            pluginInterface->createTxChannel(deviceUI->m_deviceAPI, &txChannel, &channelAPI);
            gui = pluginInterface->createTxChannelGUI(deviceUI, txChannel);
            deviceUI->registerTxChannelInstance(channelAPI, gui);
            gui->setDeviceType(ChannelGUI::DeviceTx);
            gui->setIndex(channelAPI->getIndexInDeviceSet());
        }
        else if (deviceUI->m_deviceMIMOEngine) // MIMO device => all possible channels. Depends on index range
        {
            int nbMIMOChannels = deviceUI->getNumberOfAvailableMIMOChannels();
            int nbRxChannels = deviceUI->getNumberOfAvailableRxChannels();
            int nbTxChannels = deviceUI->getNumberOfAvailableTxChannels();
            qDebug("MainWindow::channelAddClicked: MIMO: dev %d : nbMIMO: %d nbRx: %d nbTx: %d selected: %d",
                deviceSetIndex, nbMIMOChannels, nbRxChannels, nbTxChannels, channelIndex);

            if (channelIndex < nbMIMOChannels)
            {
                PluginAPI::ChannelRegistrations *channelRegistrations = m_pluginManager->getMIMOChannelRegistrations(); // Available channel plugins
                PluginInterface *pluginInterface = (*channelRegistrations)[channelIndex].m_plugin;
                ChannelAPI *channelAPI;
                MIMOChannel *mimoChannel;
                pluginInterface->createMIMOChannel(deviceUI->m_deviceAPI, &mimoChannel, &channelAPI);
                gui = pluginInterface->createMIMOChannelGUI(deviceUI, mimoChannel);
                deviceUI->registerChannelInstance(channelAPI, gui);
                gui->setIndex(channelAPI->getIndexInDeviceSet());
            }
            else if (channelIndex < nbMIMOChannels + nbRxChannels) // Rx
            {
                PluginAPI::ChannelRegistrations *channelRegistrations = m_pluginManager->getRxChannelRegistrations(); // Available channel plugins
                PluginInterface *pluginInterface = (*channelRegistrations)[channelIndex - nbMIMOChannels].m_plugin;
                ChannelAPI *channelAPI;
                BasebandSampleSink *rxChannel;
                pluginInterface->createRxChannel(deviceUI->m_deviceAPI, &rxChannel, &channelAPI);
                gui = pluginInterface->createRxChannelGUI(deviceUI, rxChannel);
                deviceUI->registerRxChannelInstance(channelAPI, gui);
                gui->setIndex(channelAPI->getIndexInDeviceSet());
            }
            else if (channelIndex < nbMIMOChannels + nbRxChannels + nbTxChannels)
            {
                PluginAPI::ChannelRegistrations *channelRegistrations = m_pluginManager->getTxChannelRegistrations(); // Available channel plugins
                PluginInterface *pluginInterface = (*channelRegistrations)[channelIndex - nbMIMOChannels - nbRxChannels].m_plugin;
                ChannelAPI *channelAPI;
                BasebandSampleSource *txChannel;
                pluginInterface->createTxChannel(deviceUI->m_deviceAPI, &txChannel, &channelAPI);
                gui = pluginInterface->createTxChannelGUI(deviceUI, txChannel);
                deviceUI->registerTxChannelInstance(channelAPI, gui);
                gui->setIndex(channelAPI->getIndexInDeviceSet());
            }

            gui->setDeviceType(ChannelGUI::DeviceMIMO);
        }

        if (gui)
        {
            QObject::connect(
                gui,
                &ChannelGUI::moveToWorkspace,
                this,
                [=](int wsIndexDest){ this->channelMove(gui, wsIndexDest); }
            );

            gui->setDeviceSetIndex(deviceSetIndex);
            gui->setToolTip(deviceAPI->getSamplingDeviceDisplayName());
            gui->setWorkspaceIndex(workspace->getIndex());
            qDebug("MainWindow::channelAddClicked: adding %s to workspace #%d",
                qPrintable(gui->getTitle()), workspace->getIndex());
            workspace->addToMdiArea((QMdiSubWindow*) gui);
            //gui->restoreGeometry(gui->getGeometryBytes());
        }
    }
}

void MainWindow::featureAddClicked(Workspace *workspace, int featureIndex)
{
    qDebug("MainWindow::featureAddClicked: W%d feature at %d", workspace->getIndex(), featureIndex);
    // // Do it in the currently selected source tab
    int currentFeatureTabIndex = 0;

    // if (currentFeatureTabIndex >= 0)
    // {
    FeatureUISet *featureUISet = m_featureUIs[currentFeatureTabIndex];
    qDebug("MainWindow::featureAddClicked: m_apiAdapter: %p", m_apiAdapter);
    PluginAPI::FeatureRegistrations *featureRegistrations = m_pluginManager->getFeatureRegistrations(); // Available feature plugins
    PluginInterface *pluginInterface = (*featureRegistrations)[featureIndex].m_plugin;
    Feature *feature = pluginInterface->createFeature(m_apiAdapter);
    FeatureGUI *gui = pluginInterface->createFeatureGUI(featureUISet, feature);
    featureUISet->registerFeatureInstance(gui, feature);
    gui->setIndex(feature->getIndexInFeatureSet());
    gui->setWorkspaceIndex(workspace->getIndex());
    workspace->addToMdiArea((QMdiSubWindow*) gui);

    QObject::connect(
        gui,
        &FeatureGUI::moveToWorkspace,
        this,
        [=](int wsIndexDest){ this->featureMove(gui, wsIndexDest); }
    );
    // }
}

void MainWindow::featureMove(FeatureGUI *gui, int wsIndexDestnation)
{
    int wsIndexOrigin = gui->getWorkspaceIndex();

    if (wsIndexOrigin == wsIndexDestnation) {
        return;
    }

    m_workspaces[wsIndexOrigin]->removeFromMdiArea(gui);
    gui->setWorkspaceIndex(wsIndexDestnation);
    m_workspaces[wsIndexDestnation]->addToMdiArea(gui);
}

void MainWindow::deviceMove(DeviceGUI *gui, int wsIndexDestnation)
{
    int wsIndexOrigin = gui->getWorkspaceIndex();
    qDebug("MainWindow::deviceMove: %s from %d to %d",
        qPrintable(gui->getTitle()), wsIndexOrigin, wsIndexDestnation);

    if (wsIndexOrigin == wsIndexDestnation) {
        return;
    }

    m_workspaces[wsIndexOrigin]->removeFromMdiArea(gui);
    gui->setWorkspaceIndex(wsIndexDestnation);
    m_workspaces[wsIndexDestnation]->addToMdiArea(gui);
}

void MainWindow::channelMove(ChannelGUI *gui, int wsIndexDestnation)
{
    int wsIndexOrigin = gui->getWorkspaceIndex();

    if (wsIndexOrigin == wsIndexDestnation) {
        return;
    }

    m_workspaces[wsIndexOrigin]->removeFromMdiArea(gui);
    gui->setWorkspaceIndex(wsIndexDestnation);
    m_workspaces[wsIndexDestnation]->addToMdiArea(gui);
}

void MainWindow::mainSpectrumMove(MainSpectrumGUI *gui, int wsIndexDestnation)
{
    int wsIndexOrigin = gui->getWorkspaceIndex();
    qDebug("MainWindow::mainSpectrumMove: %s from %d to %d",
        qPrintable(gui->getTitle()), wsIndexOrigin, wsIndexDestnation);

    if (wsIndexOrigin == wsIndexDestnation) {
        return;
    }

    m_workspaces[wsIndexOrigin]->removeFromMdiArea(gui);
    gui->setWorkspaceIndex(wsIndexDestnation);
    m_workspaces[wsIndexDestnation]->addToMdiArea(gui);
}

void MainWindow::mainSpectrumShow(MainSpectrumGUI *gui)
{
    gui->show();
}

void MainWindow::showAllChannels(int deviceSetIndex)
{
    DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];

    for (int i = 0; i < deviceUISet->getNumberOfChannels(); i++) {
        deviceUISet->getChannelGUIAt(i)->show();
    }
}

void MainWindow::openFeaturePresetsDialog(QPoint p, Workspace *workspace)
{
    FeaturePresetsDialog dialog;
    dialog.setFeatureUISet(m_featureUIs[0]);
    dialog.setPresets(m_mainCore->m_settings.getFeatureSetPresets());
    dialog.setPluginAPI(m_pluginManager->getPluginAPI());
    dialog.setWebAPIAdapter(m_apiAdapter);
    dialog.setCurrentWorkspace(workspace);
    dialog.setWorkspaces(&m_workspaces);
    dialog.populateTree();
    dialog.move(p);
    dialog.exec();

    if (dialog.wasPresetLoaded())
    {
        for (int i = 0; i < m_featureUIs[0]->getNumberOfFeatures(); i++)
        {
            FeatureGUI *gui = m_featureUIs[0]->getFeatureGuiAt(i);
            QObject::connect(
                gui,
                &FeatureGUI::moveToWorkspace,
                this,
                [=](int wsIndexDest){ this->featureMove(gui, wsIndexDest); }
            );
        }
    }
}

void MainWindow::openDeviceSetPresetsDialog(QPoint p, DeviceGUI *deviceGUI)
{
    Workspace *workspace = m_workspaces[deviceGUI->getWorkspaceIndex()];
    DeviceUISet *deviceUISet = m_deviceUIs[deviceGUI->getIndex()];

    DeviceSetPresetsDialog dialog;
    dialog.setDeviceUISet(deviceUISet);
    dialog.setPresets(m_mainCore->m_settings.getPresets());
    dialog.setPluginAPI(m_pluginManager->getPluginAPI());
    dialog.setCurrentWorkspace(workspace);
    dialog.setWorkspaces(&m_workspaces);
    dialog.populateTree((int) deviceGUI->getDeviceType());
    dialog.move(p);
    dialog.exec();

    // if (dialog.wasPresetLoaded())
    // {
    //     for (int i = 0; i < m_featureUIs[0]->getNumberOfFeatures(); i++)
    //     {
    //         FeatureGUI *gui = m_featureUIs[0]->getFeatureGuiAt(i);
    //         QObject::connect(
    //             gui,
    //             &FeatureGUI::moveToWorkspace,
    //             this,
    //             [=](int wsIndexDest){ this->featureMove(gui, wsIndexDest); }
    //         );
    //     }
    // }
}

void MainWindow::deleteFeature(int featureSetIndex, int featureIndex)
{
    if ((featureSetIndex >= 0) && (featureSetIndex < (int) m_featureUIs.size()))
    {
        FeatureUISet *featureUISet = m_featureUIs[featureSetIndex];
        featureUISet->deleteFeature(featureIndex);
    }
}

void MainWindow::on_action_About_triggered()
{
	AboutDialog dlg(m_apiHost.isEmpty() ? "127.0.0.1" : m_apiHost, m_apiPort, m_mainCore->m_settings, this);
	dlg.exec();
}

void MainWindow::on_action_removeLastDevice_triggered()
{
    if (m_deviceUIs.size() > 1)
    {
        removeLastDevice();
    }
}

void MainWindow::on_action_addFeatureSet_triggered()
{
    addFeatureSet();
}

void MainWindow::on_action_removeLastFeatureSet_triggered()
{
    if (m_featureUIs.size() > 1) {
        removeFeatureSet(m_featureUIs.size() - 1);
    }
}

void MainWindow::tabInputViewIndexChanged()
{
    // int inputViewIndex = ui->tabInputsView->currentIndex();

    // if (inputViewIndex >= 0) {
    //     ui->inputViewDock->setCurrentTabIndex(inputViewIndex);
    // }

    // if ((inputViewIndex >= 0) && (m_mainCore->m_masterTabIndex >= 0) && (inputViewIndex != m_mainCore->m_masterTabIndex))
    // {
    //     DeviceUISet *deviceUI = m_deviceUIs[inputViewIndex];
    //     DeviceUISet *lastdeviceUI = m_deviceUIs[m_mainCore->m_masterTabIndex];
    //     lastdeviceUI->m_mainWindowState = saveState();
    //     restoreState(deviceUI->m_mainWindowState);
    //     m_mainCore->m_masterTabIndex = inputViewIndex;
    // }

    // ui->tabSpectra->setCurrentIndex(inputViewIndex);
    // ui->tabChannels->setCurrentIndex(inputViewIndex);
    // ui->tabSpectraGUI->setCurrentIndex(inputViewIndex);
}

void MainWindow::tabChannelsIndexChanged()
{
    // int channelsTabIndex = ui->tabChannels->currentIndex();

    // if (channelsTabIndex >= 0)
    // {
    //     DeviceUISet *deviceUI = m_deviceUIs[channelsTabIndex];
    //     QList<QString> channelNames;
    //     ui->channelDock->resetAvailableChannels();

    //     if (deviceUI->m_deviceSourceEngine) // source device
    //     {
    //         m_pluginManager->listRxChannels(channelNames);
    //         ui->channelDock->addAvailableChannels(channelNames);
    //     }
    //     else if (deviceUI->m_deviceSinkEngine) // sink device
    //     {
    //         m_pluginManager->listTxChannels(channelNames);
    //         ui->channelDock->addAvailableChannels(channelNames);
    //     }
    //     else if (deviceUI->m_deviceMIMOEngine) // MIMO device
    //     {
    //         m_pluginManager->listMIMOChannels(channelNames);
    //         ui->channelDock->addAvailableChannels(channelNames);
    //         m_pluginManager->listRxChannels(channelNames);
    //         ui->channelDock->addAvailableChannels(channelNames);
    //         m_pluginManager->listTxChannels(channelNames);
    //         ui->channelDock->addAvailableChannels(channelNames);
    //     }
    // }
}

void MainWindow::updateStatus()
{
    m_dateTimeWidget->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss t"));
}

void MainWindow::commandKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release)
{
    qDebug("MainWindow::commandKeyPressed: key: %x mod: %x %s", (int) key, (int) keyModifiers, release ? "release" : "press");
    int currentDeviceSetIndex = 0;

    for (int i = 0; i < m_mainCore->m_settings.getCommandCount(); ++i)
    {
        const Command* command = m_mainCore->m_settings.getCommand(i);

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
