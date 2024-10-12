///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2017 Ziga S <ziga.svetina@gmail.com>                            //
// Copyright (C) 2018 beta-tester <alpha-beta-release@gmx.net>                   //
// Copyright (C) 2019 Vort <vvort@yandex.ru>                                     //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2019 Stefan Biereigel <stefan@biereigel.de>                     //
// Copyright (C) 2020-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2022 CRD716 <crd716@gmail.com>                                  //
// Copyright (C) 2023 Mohamed <mohamedadlyi@github.com>                          //
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
#include <QProgressDialog>
#include <QLabel>
#include <QToolButton>
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
#include <QDirIterator>
#include <QAction>
#include <QMenuBar>
#include <QStatusBar>
#include <QScreen>

#include "device/devicegui.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "device/deviceset.h"
#include "device/deviceenumerator.h"
#include "channel/channelapi.h"
#include "channel/channelgui.h"
#include "channel/channelwebapiutils.h"
#include "feature/featureuiset.h"
#include "feature/featureset.h"
#include "feature/feature.h"
#include "feature/featuregui.h"
#include "mainspectrum/mainspectrumgui.h"
#include "commands/commandkeyreceiver.h"
#include "gui/presetitem.h"
#include "gui/pluginsdialog.h"
#include "gui/aboutdialog.h"
#include "gui/audiodialog.h"
#include "gui/graphicsdialog.h"
#include "gui/loggingdialog.h"
#include "gui/deviceuserargsdialog.h"
#include "gui/sdrangelsplash.h"
#include "gui/mdiutils.h"
#include "gui/mypositiondialog.h"
#include "gui/fftdialog.h"
#include "gui/fftwisdomdialog.h"
#include "gui/workspace.h"
#include "gui/featurepresetsdialog.h"
#include "gui/devicesetpresetsdialog.h"
#include "gui/commandsdialog.h"
#include "gui/configurationsdialog.h"
#include "gui/dialogpositioner.h"
#include "gui/welcomedialog.h"
#include "gui/profiledialog.h"
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
#include "loggerwithfile.h"
#include "webapi/webapirequestmapper.h"
#include "webapi/webapiserver.h"
#include "webapi/webapiadapter.h"
#include "commands/command.h"
#include "settings/serializableinterface.h"
#ifdef ANDROID
#include "util/android.h"
#endif

#include "mainwindow.h"

#include <audio/audiodevicemanager.h>

//#include "ui_mainwindow.h"
#include <QtWidgets/QApplication>

#include <string>
#include <QDebug>
#include <QSplashScreen>
#include <QProgressDialog>

#include "gui/accessiblevaluedial.h"
#include "gui/accessiblevaluedialz.h"

MainWindow *MainWindow::m_instance = nullptr;

MainWindow::MainWindow(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QWidget* parent) :
	QMainWindow(parent),
	// ui(new Ui::MainWindow),
    m_currentWorkspace(nullptr),
    m_mainCore(MainCore::instance()),
	m_dspEngine(DSPEngine::instance()),
	m_lastEngineState(DeviceAPI::StNotStarted),
    m_dateTimeWidget(nullptr),
    m_showSystemWidget(nullptr),
    m_commandKeyReceiver(nullptr),
    m_profileDialog(nullptr),
#if QT_CONFIG(process)
    m_fftWisdomProcess(nullptr),
#endif
    m_settingsSaved(false)
{
    QAccessible::installFactory(AccessibleValueDial::factory);
    QAccessible::installFactory(AccessibleValueDialZ::factory);

	qDebug() << "MainWindow::MainWindow: start";
    setWindowTitle("SDRangel");

    QApplication::setOverrideCursor(Qt::WaitCursor);

    m_instance = this;
    m_mainCore->m_logger = logger;
    m_mainCore->m_masterTabIndex = 0;
    m_mainCore->m_mainMessageQueue = &m_inputMessageQueue;
	m_mainCore->m_settings.setAudioDeviceManager(m_dspEngine->getAudioDeviceManager());

    QFontDatabase::addApplicationFont(":/LiberationSans-Regular.ttf");
    QFontDatabase::addApplicationFont(":/LiberationMono-Regular.ttf");

    QFont font("Liberation Sans");
    font.setPointSize(9);
    qApp->setFont(font);

    QPixmap logoPixmap(":/sdrangel_logo.png");
    auto *splash = new SDRangelSplash(logoPixmap);
    splash->setMessageRect(QRect(10, 80, 350, 16));
    splash->show();
    splash->showStatusMessage("starting...", Qt::white);
    splash->showStatusMessage("starting...", Qt::white);

    setWindowIcon(QIcon(":/sdrangel_icon.png"));
#ifndef ANDROID
    // To save screen space on Android, don't have menu bar. Instead menus are accessed via toolbar button
    createMenuBar(nullptr);
    createStatusBar();
#endif

#ifdef ANDROID
    if (screen()->isLandscape(screen()->primaryOrientation())) {
        setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::West);
    } else {
        setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::South);
    }
#else
    setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::West);
#endif
    setTabPosition(Qt::RightDockWidgetArea, QTabWidget::East);
	setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()), Qt::QueuedConnection);

    connect(screen(), &QScreen::orientationChanged, this, &MainWindow::orientationChanged);
    #if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    screen()->setOrientationUpdateMask(Qt::PortraitOrientation
        | Qt::LandscapeOrientation
        | Qt::InvertedPortraitOrientation
        | Qt::InvertedLandscapeOrientation);
    #endif

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(1000);

    splash->showStatusMessage("load settings...", Qt::white);
    qDebug() << "MainWindow::MainWindow: load settings...";

    loadSettings();

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
        auto fileInfo = QFileInfo(filePath);

        if (fileInfo.exists()) {
            m_dspEngine->createFFTFactory(filePath);
        } else {
            m_dspEngine->createFFTFactory("");
        }
    }

    m_dspEngine->preAllocateFFTs();

    splash->showStatusMessage("load plugins...", Qt::white);
    qDebug() << "MainWindow::MainWindow: load plugins...";

    m_pluginManager = new PluginManager(this);
    m_mainCore->m_pluginManager = m_pluginManager;
    m_pluginManager->setEnableSoapy(parser.getSoapy());
    m_pluginManager->loadPlugins(QString("plugins"));
    m_pluginManager->loadPluginsNonDiscoverable(m_mainCore->m_settings.getDeviceUserArgs());

    splash->showStatusMessage("Add command key receiver...", Qt::white);
	m_commandKeyReceiver = new CommandKeyReceiver();
	m_commandKeyReceiver->setRelease(true);
	this->installEventFilter(m_commandKeyReceiver);

    splash->showStatusMessage("Add unique feature set...", Qt::white);
    addFeatureSet(); // Create the uniuefeature set
	m_apiAdapter = new WebAPIAdapter();

    splash->showStatusMessage("finishing...", Qt::white);

	QString applicationDirPath = qApp->applicationDirPath();

#ifdef _MSC_VER
    if (QResource::registerResource(applicationDirPath + "/sdrbase.rcc")) {
        qDebug("MainWindow::MainWindow: registered resource file %s/%s", qPrintable(applicationDirPath), "sdrbase.rcc");
    } else {
        qWarning("MainWindow::MainWindow: could not register resource file %s/%s", qPrintable(applicationDirPath), "sdrbase.rcc");
    }
#endif

	m_requestMapper = new WebAPIRequestMapper(this);
	m_requestMapper->setAdapter(m_apiAdapter);
	m_apiHost = parser.getServerAddress();
	m_apiPort = parser.getServerPort();
	m_apiServer = new WebAPIServer(m_apiHost, (uint16_t) m_apiPort, m_requestMapper);
	m_apiServer->start();

    m_dspEngine->setMIMOSupport(true);

    // Restore window size and position
    QSettings s;
    restoreGeometry(qUncompress(QByteArray::fromBase64(s.value("mainWindowGeometry").toByteArray())));
    restoreState(qUncompress(QByteArray::fromBase64(s.value("mainWindowState").toByteArray())));

    // Load initial configuration
    InitFSM *fsm = new InitFSM(this, splash, !parser.getScratch());
    connect(fsm, &InitFSM::finished, fsm, &InitFSM::deleteLater);
    connect(fsm, &InitFSM::finished, splash, &SDRangelSplash::deleteLater);
    fsm->start();

    qDebug() << "MainWindow::MainWindow: end";
}

MainWindow::~MainWindow()
{
	qDebug() << "MainWindow::~MainWindow";

    m_statusTimer.stop();
    m_apiServer->stop();
    delete m_apiServer;
    delete m_requestMapper;
    delete m_apiAdapter;

    delete m_pluginManager;
	delete m_dateTimeWidget;
	delete m_showSystemWidget;

    removeAllFeatureSets();

	delete m_commandKeyReceiver;
    delete m_profileDialog;

    for (const auto& workspace : m_workspaces) {
        delete workspace;
    }

	qDebug() << "MainWindow::~MainWindow: end";
}

MainWindowFSM::MainWindowFSM(MainWindow *mainWindow, QObject *parent) :
    QStateMachine(parent),
    m_mainWindow(mainWindow),
    m_finalState(nullptr)
{
}

void MainWindowFSM::createStates(int states)
{
    for (int i = 0; i < states - 1; i++) {
        m_states.append(new QState());
    }
    m_finalState = new QFinalState();
    for (int i = 0; i < m_states.size(); i++) {
        addState(m_states[i]);
    }
    addState(m_finalState);
    setInitialState(m_states[0]);
}

AddSampleSourceFSM::AddSampleSourceFSM(MainWindow *mainWindow, Workspace *deviceWorkspace, Workspace *spectrumWorkspace, int deviceIndex, bool loadDefaults, QObject *parent) :
    MainWindowFSM(mainWindow, parent),
    m_deviceWorkspace(deviceWorkspace),
    m_spectrumWorkspace(spectrumWorkspace),
    m_deviceIndex(deviceIndex),
    m_loadDefaults(loadDefaults),
    m_deviceSetIndex(-1),
    m_deviceAPI(nullptr),
    m_deviceUISet(nullptr)
{
    // Create source engine
    addEngine();

    // Create FSM
    createStates(3);

    m_states[0]->addTransition(m_dspDeviceSourceEngine, &DSPDeviceSourceEngine::sampleSet, m_states[1]);
    m_states[1]->addTransition(m_finalState);

    connect(m_states[0], &QState::entered, this, &AddSampleSourceFSM::addDevice);
    connect(m_states[1], &QState::entered, this, &AddSampleSourceFSM::addDeviceUI);
}

void AddSampleSourceFSM::addEngine()
{
    // Create the source engine
    m_dspDeviceSourceEngine = m_mainWindow->m_dspEngine->addDeviceSourceEngine();
}

void AddSampleSourceFSM::addDevice()
{
    m_deviceSetIndex = (int) m_mainWindow->m_deviceUIs.size();
    m_mainWindow->m_mainCore->appendDeviceSet(0);

    DeviceSet *deviceSet = m_mainWindow->m_mainCore->getDeviceSets().back();
    m_deviceUISet = new DeviceUISet(m_deviceSetIndex, deviceSet);
    m_mainWindow->m_deviceUIs.push_back(m_deviceUISet);

    m_deviceUISet->m_deviceSourceEngine = m_dspDeviceSourceEngine;
    deviceSet->m_deviceSourceEngine = m_dspDeviceSourceEngine;
    m_deviceUISet->m_deviceSinkEngine = nullptr;
    deviceSet->m_deviceSinkEngine = nullptr;
    m_deviceUISet->m_deviceMIMOEngine = nullptr;
    deviceSet->m_deviceMIMOEngine = nullptr;

    m_deviceAPI = new DeviceAPI(DeviceAPI::StreamSingleRx, m_deviceSetIndex, m_dspDeviceSourceEngine, nullptr, nullptr);

    m_deviceUISet->m_deviceAPI = m_deviceAPI;
    deviceSet->m_deviceAPI = m_deviceAPI;
    QList<QString> channelNames;
    m_mainWindow->m_pluginManager->listRxChannels(channelNames);
    m_deviceUISet->setNumberOfAvailableRxChannels(channelNames.size());

    m_dspDeviceSourceEngine->addSink(m_deviceUISet->m_spectrumVis);

    // Create a file source instance by default if requested device was not enumerated (index = -1)
    if (m_deviceIndex < 0) {
        m_deviceIndex = DeviceEnumerator::instance()->getFileInputDeviceIndex();
    }

    m_mainWindow->sampleSourceCreate(m_deviceSetIndex, m_deviceIndex, m_deviceUISet);
}

void AddSampleSourceFSM::addDeviceUI()
{
    m_mainWindow->sampleSourceCreateUI(m_deviceSetIndex, m_deviceIndex, m_deviceUISet);

    m_deviceUISet->m_deviceGUI->setWorkspaceIndex(m_deviceWorkspace->getIndex());
    m_deviceUISet->m_mainSpectrumGUI->setWorkspaceIndex(m_spectrumWorkspace->getIndex());
    MainSpectrumGUI *mainSpectrumGUI = m_deviceUISet->m_mainSpectrumGUI;

    QObject::connect(
        mainSpectrumGUI,
        &MainSpectrumGUI::moveToWorkspace,
        m_mainWindow,
        [m_mainWindow=m_mainWindow, mainSpectrumGUI](int wsIndexDest){ m_mainWindow->mainSpectrumMove(mainSpectrumGUI, wsIndexDest); }
    );

    QObject::connect(
        m_deviceUISet->m_deviceGUI,
        &DeviceGUI::addChannelEmitted,
        m_mainWindow,
        [m_mainWindow=m_mainWindow, m_deviceWorkspace=m_deviceWorkspace, m_deviceSetIndex=m_deviceSetIndex](int channelPluginIndex){
            m_mainWindow->channelAddClicked(m_deviceWorkspace, m_deviceSetIndex, channelPluginIndex);
        }
    );

    QObject::connect(
        mainSpectrumGUI,
        &MainSpectrumGUI::requestCenterFrequency,
        m_mainWindow,
        &MainWindow::mainSpectrumRequestDeviceCenterFrequency
    );

    QObject::connect(
        m_deviceAPI,
        &DeviceAPI::stateChanged,
        m_mainWindow,
        &MainWindow::deviceStateChanged
    );

    m_deviceWorkspace->addToMdiArea(m_deviceUISet->m_deviceGUI);
    m_spectrumWorkspace->addToMdiArea(m_deviceUISet->m_mainSpectrumGUI);
    if (m_loadDefaults) {
    m_mainWindow->loadDefaultPreset(m_deviceAPI->getSamplingDeviceId(), m_deviceUISet);
    }
    emit m_mainWindow->m_mainCore->deviceSetAdded(m_deviceSetIndex, m_deviceAPI);

#ifdef ANDROID
    // Seemingly needed on some versions of Android, otherwise the new windows aren't always displayed??
    m_deviceWorkspace->repaint();
#endif
}

AddSampleSinkFSM::AddSampleSinkFSM(MainWindow *mainWindow, Workspace *deviceWorkspace, Workspace *spectrumWorkspace, int deviceIndex, bool loadDefaults, QObject *parent) :
    MainWindowFSM(mainWindow, parent),
    m_deviceWorkspace(deviceWorkspace),
    m_spectrumWorkspace(spectrumWorkspace),
    m_deviceIndex(deviceIndex),
    m_loadDefaults(loadDefaults),
    m_deviceSetIndex(-1),
    m_deviceAPI(nullptr),
    m_deviceUISet(nullptr)
{
    // Create source engine
    addEngine();

    // Create FSM
    createStates(3);

    m_states[0]->addTransition(m_dspDeviceSinkEngine, &DSPDeviceSinkEngine::sampleSet, m_states[1]);
    m_states[1]->addTransition(m_finalState);

    connect(m_states[0], &QState::entered, this, &AddSampleSinkFSM::addDevice);
    connect(m_states[1], &QState::entered, this, &AddSampleSinkFSM::addDeviceUI);
}

void AddSampleSinkFSM::addEngine()
{
    // Create the source engine
    m_dspDeviceSinkEngine = m_mainWindow->m_dspEngine->addDeviceSinkEngine();
}

void AddSampleSinkFSM::addDevice()
{
    m_deviceSetIndex = (int) m_mainWindow->m_deviceUIs.size();
    m_mainWindow->m_mainCore->appendDeviceSet(1);

    DeviceSet *deviceSet = m_mainWindow->m_mainCore->getDeviceSets().back();
    m_deviceUISet = new DeviceUISet(m_deviceSetIndex, deviceSet);
    m_mainWindow->m_deviceUIs.push_back(m_deviceUISet);

    m_deviceUISet->m_deviceSourceEngine = nullptr;
    deviceSet->m_deviceSourceEngine = nullptr;
    m_deviceUISet->m_deviceSinkEngine = m_dspDeviceSinkEngine;
    deviceSet->m_deviceSinkEngine = m_dspDeviceSinkEngine;
    m_deviceUISet->m_deviceMIMOEngine = nullptr;
    deviceSet->m_deviceMIMOEngine = nullptr;

    m_deviceAPI = new DeviceAPI(DeviceAPI::StreamSingleTx, m_deviceSetIndex, nullptr, m_dspDeviceSinkEngine, nullptr);

    m_deviceUISet->m_deviceAPI = m_deviceAPI;
    deviceSet->m_deviceAPI = m_deviceAPI;
    QList<QString> channelNames;
    m_mainWindow->m_pluginManager->listTxChannels(channelNames);
    m_deviceUISet->setNumberOfAvailableTxChannels(channelNames.size());

    m_dspDeviceSinkEngine->addSpectrumSink(m_deviceUISet->m_spectrumVis);
    m_deviceUISet->m_spectrum->setDisplayedStream(false, 0);

    // Create a file sink instance by default if requested device was not enumerated (index = -1)
    if (m_deviceIndex < 0) {
        m_deviceIndex = DeviceEnumerator::instance()->getFileOutputDeviceIndex();
    }

    m_mainWindow->sampleSinkCreate(m_deviceSetIndex, m_deviceIndex, m_deviceUISet);
}

void AddSampleSinkFSM::addDeviceUI()
{
    m_mainWindow->sampleSinkCreateUI(m_deviceSetIndex, m_deviceIndex, m_deviceUISet);

    m_deviceUISet->m_deviceGUI->setWorkspaceIndex(m_deviceWorkspace->getIndex());
    m_deviceUISet->m_mainSpectrumGUI->setWorkspaceIndex(m_spectrumWorkspace->getIndex());
    MainSpectrumGUI *mainSpectrumGUI = m_deviceUISet->m_mainSpectrumGUI;

    QObject::connect(
        mainSpectrumGUI,
        &MainSpectrumGUI::moveToWorkspace,
        m_mainWindow,
        [m_mainWindow=m_mainWindow, mainSpectrumGUI](int wsIndexDest){ m_mainWindow->mainSpectrumMove(mainSpectrumGUI, wsIndexDest); }
    );

    QObject::connect(
        m_deviceUISet->m_deviceGUI,
        &DeviceGUI::addChannelEmitted,
        m_mainWindow,
        [m_mainWindow=m_mainWindow, m_deviceWorkspace=m_deviceWorkspace, m_deviceSetIndex=m_deviceSetIndex](int channelPluginIndex){
            m_mainWindow->channelAddClicked(m_deviceWorkspace, m_deviceSetIndex, channelPluginIndex);
        }
    );

    QObject::connect(
        mainSpectrumGUI,
        &MainSpectrumGUI::requestCenterFrequency,
        m_mainWindow,
        &MainWindow::mainSpectrumRequestDeviceCenterFrequency
    );

    QObject::connect(
        m_deviceAPI,
        &DeviceAPI::stateChanged,
        m_mainWindow,
        &MainWindow::deviceStateChanged
    );

    m_deviceWorkspace->addToMdiArea(m_deviceUISet->m_deviceGUI);
    m_spectrumWorkspace->addToMdiArea(m_deviceUISet->m_mainSpectrumGUI);
    if (m_loadDefaults) {
    m_mainWindow->loadDefaultPreset(m_deviceAPI->getSamplingDeviceId(), m_deviceUISet);
    }
    emit m_mainWindow->m_mainCore->deviceSetAdded(m_deviceSetIndex, m_deviceAPI);

#ifdef ANDROID
    // Seemingly needed on some versions of Android, otherwise the new windows aren't always displayed??
    m_deviceWorkspace->repaint();
#endif
}

AddSampleMIMOFSM::AddSampleMIMOFSM(MainWindow *mainWindow, Workspace *deviceWorkspace, Workspace *spectrumWorkspace, int deviceIndex, bool loadDefaults, QObject *parent) :
    MainWindowFSM(mainWindow, parent),
    m_deviceWorkspace(deviceWorkspace),
    m_spectrumWorkspace(spectrumWorkspace),
    m_deviceIndex(deviceIndex),
    m_loadDefaults(loadDefaults),
    m_deviceSetIndex(-1),
    m_deviceAPI(nullptr),
    m_deviceUISet(nullptr)
{
    // Create source engine
    addEngine();

    // Create FSM
    createStates(3);

    m_states[0]->addTransition(m_dspDeviceMIMOEngine, &DSPDeviceMIMOEngine::sampleSet, m_states[1]);
    m_states[1]->addTransition(m_finalState);

    connect(m_states[0], &QState::entered, this, &AddSampleMIMOFSM::addDevice);
    connect(m_states[1], &QState::entered, this, &AddSampleMIMOFSM::addDeviceUI);
}

void AddSampleMIMOFSM::addEngine()
{
    // Create the source engine
    m_dspDeviceMIMOEngine = m_mainWindow->m_dspEngine->addDeviceMIMOEngine();
}

void AddSampleMIMOFSM::addDevice()
{
    m_deviceSetIndex = (int) m_mainWindow->m_deviceUIs.size();
    m_mainWindow->m_mainCore->appendDeviceSet(2);

    DeviceSet *deviceSet = m_mainWindow->m_mainCore->getDeviceSets().back();
    m_deviceUISet = new DeviceUISet(m_deviceSetIndex, deviceSet);
    m_mainWindow->m_deviceUIs.push_back(m_deviceUISet);

    m_deviceUISet->m_deviceSourceEngine = nullptr;
    deviceSet->m_deviceSourceEngine = nullptr;
    m_deviceUISet->m_deviceSinkEngine = nullptr;
    deviceSet->m_deviceSinkEngine = nullptr;
    m_deviceUISet->m_deviceMIMOEngine = m_dspDeviceMIMOEngine;
    deviceSet->m_deviceMIMOEngine = m_dspDeviceMIMOEngine;

    m_deviceAPI = new DeviceAPI(DeviceAPI::StreamMIMO, m_deviceSetIndex, nullptr, nullptr, m_dspDeviceMIMOEngine);

    m_deviceUISet->m_deviceAPI = m_deviceAPI;
    deviceSet->m_deviceAPI = m_deviceAPI;
    QList<QString> mimoChannelNames;
    m_mainWindow->m_pluginManager->listMIMOChannels(mimoChannelNames);
    m_deviceUISet->setNumberOfAvailableMIMOChannels(mimoChannelNames.size());
    // Add Rx channels
    QList<QString> rxChannelNames;
    m_mainWindow->m_pluginManager->listRxChannels(rxChannelNames);
    m_deviceUISet->setNumberOfAvailableRxChannels(rxChannelNames.size());
    // Add Tx channels
    QList<QString> txChannelNames;
    m_mainWindow->m_pluginManager->listTxChannels(txChannelNames);
    m_deviceUISet->setNumberOfAvailableTxChannels(txChannelNames.size());

    m_dspDeviceMIMOEngine->addSpectrumSink(m_deviceUISet->m_spectrumVis);

    // Create a Test MIMO instance by default if requested device was not enumerated (index = -1)
    if (m_deviceIndex < 0) {
        m_deviceIndex = DeviceEnumerator::instance()->getTestMIMODeviceIndex();
    }

    m_mainWindow->sampleMIMOCreate(m_deviceSetIndex, m_deviceIndex, m_deviceUISet);
}

void AddSampleMIMOFSM::addDeviceUI()
{
    m_mainWindow->sampleMIMOCreateUI(m_deviceSetIndex, m_deviceIndex, m_deviceUISet);

    m_deviceUISet->m_deviceGUI->setWorkspaceIndex(m_deviceWorkspace->getIndex());
    m_deviceUISet->m_mainSpectrumGUI->setWorkspaceIndex(m_spectrumWorkspace->getIndex());
    MainSpectrumGUI *mainSpectrumGUI = m_deviceUISet->m_mainSpectrumGUI;

    QObject::connect(
        m_deviceUISet->m_deviceGUI,
        &DeviceGUI::addChannelEmitted,
        m_mainWindow,
        [m_mainWindow=m_mainWindow, m_deviceWorkspace=m_deviceWorkspace, m_deviceSetIndex=m_deviceSetIndex](int channelPluginIndex){
            m_mainWindow->channelAddClicked(m_deviceWorkspace, m_deviceSetIndex, channelPluginIndex);
        }
    );

    QObject::connect(
        mainSpectrumGUI,
        &MainSpectrumGUI::requestCenterFrequency,
        m_mainWindow,
        &MainWindow::mainSpectrumRequestDeviceCenterFrequency
    );

    QObject::connect(
        m_deviceAPI,
        &DeviceAPI::stateChanged,
        m_mainWindow,
        &MainWindow::deviceStateChanged
    );

    m_deviceWorkspace->addToMdiArea(m_deviceUISet->m_deviceGUI);
    m_spectrumWorkspace->addToMdiArea(m_deviceUISet->m_mainSpectrumGUI);
    if (m_loadDefaults) {
    m_mainWindow->loadDefaultPreset(m_deviceAPI->getSamplingDeviceId(), m_deviceUISet);
    }
    emit m_mainWindow->m_mainCore->deviceSetAdded(m_deviceSetIndex, m_deviceAPI);

#ifdef ANDROID
    // Seemingly needed on some versions of Android, otherwise the new windows aren't always displayed??
    m_deviceWorkspace->repaint();
#endif
}

RemoveDeviceSetFSM::RemoveDeviceSetFSM(MainWindow *mainWindow, int deviceSetIndex, QObject *parent) :
    MainWindowFSM(mainWindow, parent),
    m_deviceSetIndex(deviceSetIndex),
    m_deviceSourceEngine(nullptr),
    m_deviceSinkEngine(nullptr),
    m_deviceMIMOEngine(nullptr),
    m_t1(nullptr),
    m_t2(nullptr)
{
    // Create FSM
    createStates(6);

    m_deviceUISet = m_mainWindow->m_deviceUIs[m_deviceSetIndex];
    if (m_deviceUISet->m_deviceSourceEngine)
    {
        m_deviceSourceEngine = m_deviceUISet->m_deviceSourceEngine;
        m_t1 = new QSignalTransition(m_deviceSourceEngine, &DSPDeviceSourceEngine::acquistionStopped, m_states[0]);
        m_t1->setTargetState(m_states[1]);
        m_t2 = new QSignalTransition(m_deviceSourceEngine, &DSPDeviceSourceEngine::sinkRemoved, m_states[1]);
        m_t2->setTargetState(m_states[2]);
    }
    else if (m_deviceUISet->m_deviceSinkEngine)
    {
        m_deviceSinkEngine = m_deviceUISet->m_deviceSinkEngine;
        m_t1 = new QSignalTransition(m_deviceSinkEngine, &DSPDeviceSinkEngine::generationStopped, m_states[0]);
        m_t1->setTargetState(m_states[1]);
        m_t2 = new QSignalTransition(m_deviceSinkEngine, &DSPDeviceSinkEngine::spectrumSinkRemoved, m_states[1]);
        m_t2->setTargetState(m_states[2]);
    }
    else if (m_deviceUISet->m_deviceMIMOEngine)
    {
        m_deviceMIMOEngine = m_deviceUISet->m_deviceMIMOEngine;
        m_t1 = new QSignalTransition(m_deviceMIMOEngine, &DSPDeviceMIMOEngine::acquisitionStopped, m_states[0]);
        m_t1->setTargetState(m_states[1]);
        m_t2 = new QSignalTransition(m_deviceMIMOEngine, &DSPDeviceMIMOEngine::spectrumSinkRemoved, m_states[1]);
        m_t2->setTargetState(m_states[2]);
    }
    m_states[2]->addTransition(m_states[3]);
    m_states[3]->addTransition(m_mainWindow, &MainWindow::engineStopped, m_states[4]);
    m_states[4]->addTransition(m_finalState);

    connect(m_states[0], &QState::entered, this, &RemoveDeviceSetFSM::stopAcquisition);
    connect(m_states[1], &QState::entered, this, &RemoveDeviceSetFSM::removeSink);
    connect(m_states[2], &QState::entered, this, &RemoveDeviceSetFSM::removeUI);
    connect(m_states[3], &QState::entered, this, &RemoveDeviceSetFSM::stopEngine);
    connect(m_states[4], &QState::entered, this, &RemoveDeviceSetFSM::removeDeviceSet);
}

void RemoveDeviceSetFSM::stopAcquisition()
{
    qDebug() << "RemoveDeviceSetFSM::stopAcquisition";
    if (m_deviceSourceEngine)
    {
        m_deviceSourceEngine->stopAcquistion();
    }
    else if (m_deviceSinkEngine)
    {
	    m_deviceSinkEngine->stopGeneration();
    }
    else
    {
        m_deviceMIMOEngine->stopProcess(1); // Tx side
        m_deviceMIMOEngine->stopProcess(0); // Rx side
    }
}

void RemoveDeviceSetFSM::removeSink()
{
    qDebug() << "RemoveDeviceSetFSM::removeSink";
    if (m_deviceSourceEngine) {
        m_deviceSourceEngine->removeSink(m_deviceUISet->m_spectrumVis);
    } else if (m_deviceSinkEngine) {
        m_deviceSinkEngine->removeSpectrumSink(m_deviceUISet->m_spectrumVis);
    } else {
        m_deviceMIMOEngine->removeSpectrumSink(m_deviceUISet->m_spectrumVis);
    }
}

void RemoveDeviceSetFSM::removeUI()
{
    qDebug() << "RemoveDeviceSetFSM::removeUI";

    // Remove transitions, as we will delete the object (DSPDevice*Engine) that is the source of these signals
    m_states[0]->removeTransition(m_t1);
    delete m_t1;
    m_t1 = nullptr;
    m_states[1]->removeTransition(m_t2);
    delete m_t2;
    m_t2 = nullptr;

    m_deviceUISet->freeChannels();      // destroys the channel instances
    if (m_deviceSourceEngine) {
        m_deviceUISet->m_deviceAPI->getSampleSource()->setMessageQueueToGUI(nullptr); // have source stop sending messages to the GUI
    } else if (m_deviceSinkEngine) {
        m_deviceUISet->m_deviceAPI->getSampleSink()->setMessageQueueToGUI(nullptr); // have sink stop sending messages to the GUI
    } else {
        m_deviceUISet->m_deviceAPI->getSampleMIMO()->setMessageQueueToGUI(nullptr); // have sink stop sending messages to the GUI
    }
    delete m_deviceUISet->m_deviceGUI;
    m_deviceUISet->m_deviceAPI->resetSamplingDeviceId();
    if (!m_deviceMIMOEngine) {
        m_deviceUISet->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists
    }
}

void RemoveDeviceSetFSM::stopEngine()
{
    qDebug() << "RemoveDeviceSetFSM::stopEngine";
    QThread *thread = m_mainWindow->m_dspEngine->removeDeviceEngineAt(m_deviceSetIndex);
    if (thread && !thread->isFinished())    // FIXME: Is there a race condition here? We might need to connect before calling thread->exit
    {
        connect(thread, &QThread::finished, m_mainWindow, &MainWindow::engineStopped);
    }
    else
    {
        emit m_mainWindow->engineStopped();
    }
}

void RemoveDeviceSetFSM::removeDeviceSet()
{
    if (m_deviceSourceEngine) {
        DeviceEnumerator::instance()->removeRxSelection(m_deviceSetIndex);
    } else if (m_deviceSinkEngine) {
        DeviceEnumerator::instance()->removeTxSelection(m_deviceSetIndex);
    } else {
        DeviceEnumerator::instance()->removeMIMOSelection(m_deviceSetIndex);
    }

    DeviceAPI *deviceAPI = m_deviceUISet->m_deviceAPI;
    delete m_deviceUISet;
    if (m_deviceSourceEngine) {
        delete deviceAPI->getSampleSource();
    } else if (m_deviceSinkEngine) {
        delete deviceAPI->getSampleSink();
    } else {
        delete deviceAPI->getSampleMIMO();
    }
    delete deviceAPI;

    m_mainWindow->m_deviceUIs.erase(m_mainWindow->m_deviceUIs.begin() + m_deviceSetIndex);
    m_mainWindow->m_mainCore->removeDeviceSet(m_deviceSetIndex);
    DeviceEnumerator::instance()->renumeratetabIndex(m_deviceSetIndex);

    // Renumerate
    for (int i = 0; i < (int) m_mainWindow->m_deviceUIs.size(); i++)
    {
        DeviceUISet *xDeviceUISet = m_mainWindow->m_deviceUIs[i];
        xDeviceUISet->setIndex(i);
        const DeviceGUI *deviceGUI = m_mainWindow->m_deviceUIs[i]->m_deviceGUI;
        Workspace *deviceWorkspace = m_mainWindow->m_workspaces[deviceGUI->getWorkspaceIndex()];

        QObject::disconnect(
            deviceGUI,
            &DeviceGUI::addChannelEmitted,
            this,
            nullptr
        );
        QObject::connect(
            deviceGUI,
            &DeviceGUI::addChannelEmitted,
            this,
            [this, deviceWorkspace, i](int channelPluginIndex){ m_mainWindow->channelAddClicked(deviceWorkspace, i, channelPluginIndex); }
        );
    }

    emit m_mainWindow->m_mainCore->deviceSetRemoved(m_deviceSetIndex);
}

RemoveAllDeviceSetsFSM::RemoveAllDeviceSetsFSM(MainWindow *mainWindow, QObject *parent) :
    MainWindowFSM(mainWindow, parent)
{
    // Create FSM
    createStates(2);

    m_states[0]->addTransition(m_mainWindow->m_mainCore, &MainCore::deviceSetRemoved, m_states[0]);
    m_states[0]->addTransition(m_mainWindow, &MainWindow::allDeviceSetsRemoved, m_finalState);

    connect(m_states[0], &QState::entered, this, &RemoveAllDeviceSetsFSM::removeNext);
}

void RemoveAllDeviceSetsFSM::removeNext()
{
    qDebug() << "RemoveAllDeviceSetsFSM::removeNext";
    if (m_mainWindow->m_deviceUIs.size() > 0) {
        m_mainWindow->removeDeviceSet(m_mainWindow->m_deviceUIs.size() - 1);
    } else {
        emit m_mainWindow->allDeviceSetsRemoved();
    }
}

RemoveAllWorkspacesFSM::RemoveAllWorkspacesFSM(MainWindow *mainWindow, QObject *parent) :
    MainWindowFSM(mainWindow, parent)
{
    // Create FSM
    createStates(3);

    m_removeAllDeviceSetsFSM = new RemoveAllDeviceSetsFSM(m_mainWindow, this);

    m_states[0]->addTransition(m_removeAllDeviceSetsFSM, &RemoveAllDeviceSetsFSM::finished, m_states[1]);
    m_states[1]->addTransition(m_finalState);

    connect(m_states[0], &QState::entered, this, &RemoveAllWorkspacesFSM::removeDeviceSets);
    connect(m_states[1], &QState::entered, this, &RemoveAllWorkspacesFSM::removeWorkspaces);
}

void RemoveAllWorkspacesFSM::removeDeviceSets()
{
    qDebug() << "RemoveAllWorkspacesFSM::removeDeviceSets";
    m_removeAllDeviceSetsFSM->start();
}

void RemoveAllWorkspacesFSM::removeWorkspaces()
{
    qDebug() << "RemoveAllWorkspacesFSM::removeWorkspaces";
    // Features
    m_mainWindow->m_featureUIs[0]->freeFeatures();
    // Workspaces
    for (const auto& workspace : m_mainWindow->m_workspaces) {
        delete workspace;
    }
    m_mainWindow->m_workspaces.clear();
    qDebug() << "RemoveAllWorkspacesFSM::removeWorkspaces done";
}

LoadConfigurationFSM::LoadConfigurationFSM(MainWindow *mainWindow, const Configuration *configuration, QProgressDialog *waitBox, QObject *parent) :
    MainWindowFSM(mainWindow, parent),
    m_configuration(configuration),
    m_waitBox(waitBox)
{
    // Create FSM
    createStates(7);

    m_removeAllWorkspacesFSM = new RemoveAllWorkspacesFSM(m_mainWindow, this);

    m_states[0]->addTransition(m_removeAllWorkspacesFSM, &RemoveAllWorkspacesFSM::finished, m_states[1]);
    m_states[1]->addTransition(m_states[2]);
    m_states[2]->addTransition(m_mainWindow, &MainWindow::allDeviceSetsAdded, m_states[3]);
    m_states[3]->addTransition(m_states[4]);
    m_states[4]->addTransition(m_states[5]);
    m_states[5]->addTransition(m_finalState);

    connect(m_states[0], &QState::entered, this, &LoadConfigurationFSM::clearWorkspace);
    connect(m_states[1], &QState::entered, this, &LoadConfigurationFSM::createWorkspaces);
    connect(m_states[2], &QState::entered, this, &LoadConfigurationFSM::loadDeviceSets);
    connect(m_states[3], &QState::entered, this, &LoadConfigurationFSM::loadDeviceSetSettings);
    connect(m_states[4], &QState::entered, this, &LoadConfigurationFSM::loadFeatureSets);
    connect(m_states[5], &QState::entered, this, &LoadConfigurationFSM::restoreGeometry);
}

void LoadConfigurationFSM::clearWorkspace()
{
    qDebug() << "LoadConfigurationFSM::clearWorkspace";

    if (m_waitBox)
    {
        m_waitBox->setLabelText("Deleting existing...");
        m_waitBox->setValue(5);
    }

    m_removeAllWorkspacesFSM->start();
}

void LoadConfigurationFSM::createWorkspaces()
{
    qDebug() << "LoadConfigurationFSM::createWorkspaces";

    if (m_waitBox)
    {
        m_waitBox->setLabelText("Creating workspaces...");
        m_waitBox->setValue(20);
    }

    // Workspaces
    for (int i = 0; i < m_configuration->getNumberOfWorkspaceGeometries(); i++)
    {
        m_mainWindow->addWorkspace();
        m_mainWindow->m_workspaces[i]->setAutoStackOption(m_configuration->getWorkspaceAutoStackOptions()[i]);
        m_mainWindow->m_workspaces[i]->setTabSubWindowsOption(m_configuration->getWorkspaceTabSubWindowsOptions()[i]);
    }
    }

void LoadConfigurationFSM::loadDeviceSets()
{
    qDebug() << "LoadConfigurationFSM::loadDeviceSets";

    if (m_waitBox)
    {
        m_waitBox->setLabelText("Loading device sets...");
        m_waitBox->setValue(25);
    }

    const QList<Preset>& deviceSetPresets = m_configuration->getDeviceSetPresets();

    // State machine that runs all of the individual AddSampleSourceFSM, AddSampleSinkFSM, AddSampleMIMOFSMs
    QStateMachine *m_addDevicesFSM = new QStateMachine();
    QState *sInitial = nullptr;
    QState *sPrev = nullptr;
    QFinalState *sFinal = new QFinalState();

    connect(m_addDevicesFSM, &QStateMachine::finished, this, [=](){emit m_mainWindow->allDeviceSetsAdded();});
    connect(m_addDevicesFSM, &QStateMachine::finished, m_addDevicesFSM, &QStateMachine::deleteLater);

    for (const auto& deviceSetPreset : deviceSetPresets)
    {
        MainWindowFSM *fsm = nullptr;
        int deviceWorkspaceIndex = deviceSetPreset.getDeviceWorkspaceIndex() < m_mainWindow->m_workspaces.size() ?
            deviceSetPreset.getDeviceWorkspaceIndex() :
            0;
        int spectrumWorkspaceIndex = deviceSetPreset.getSpectrumWorkspaceIndex() < m_mainWindow->m_workspaces.size() ?
            deviceSetPreset.getSpectrumWorkspaceIndex() :
            deviceWorkspaceIndex;

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

            fsm = new AddSampleSourceFSM(m_mainWindow,
                m_mainWindow->m_workspaces[deviceWorkspaceIndex],
                m_mainWindow->m_workspaces[spectrumWorkspaceIndex],
                bestDeviceIndex, false, m_addDevicesFSM);
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

            fsm = new AddSampleSinkFSM(m_mainWindow,
                m_mainWindow->m_workspaces[deviceWorkspaceIndex],
                m_mainWindow->m_workspaces[spectrumWorkspaceIndex],
                bestDeviceIndex, false, m_addDevicesFSM);
        }
        else if (deviceSetPreset.isMIMOPreset())
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

            fsm = new AddSampleMIMOFSM(m_mainWindow,
                m_mainWindow->m_workspaces[deviceWorkspaceIndex],
                m_mainWindow->m_workspaces[spectrumWorkspaceIndex],
                bestDeviceIndex, false, m_addDevicesFSM);
        }
        else
        {
            qDebug() << "MainWindow::loadConfiguration: Unknown preset type: " << deviceSetPreset.getPresetType();
        }

        // Chain the FSMs together
        if (fsm)
        {
            m_addDevicesFSM->addState(fsm);

            if (!sInitial) {
                sInitial = fsm;
            }
            if (sPrev) {
                sPrev->addTransition(sPrev, &QState::finished, fsm);
            }
            sPrev = fsm;
        }
    }

    m_addDevicesFSM->addState(sFinal);
    if (sPrev)
    {
        m_addDevicesFSM->setInitialState(sInitial);
        sPrev->addTransition(sPrev, &QState::finished, sFinal);
    }
    else
    {
        m_addDevicesFSM->setInitialState(sFinal);
    }
    // Run all the FSMs
    m_addDevicesFSM->start();
}

void LoadConfigurationFSM::loadDeviceSetSettings()
{
    qDebug() << "LoadConfigurationFSM::loadDeviceSetSettings";

    if (m_waitBox)
    {
        m_waitBox->setLabelText("Loading device set settings...");
        m_waitBox->setValue(70);
    }

    const QList<Preset>& deviceSetPresets = m_configuration->getDeviceSetPresets();

    int deviceSetIndex = 0;
    for (const auto& deviceSetPreset : deviceSetPresets)
    {
        if (((int) m_mainWindow->m_deviceUIs.size()) > deviceSetIndex)
        {
            MDIUtils::restoreMDIGeometry(m_mainWindow->m_deviceUIs[deviceSetIndex]->m_deviceGUI, deviceSetPreset.getDeviceGeometry());
            MDIUtils::restoreMDIGeometry(m_mainWindow->m_deviceUIs[deviceSetIndex]->m_mainSpectrumGUI, deviceSetPreset.getSpectrumGeometry());
            m_mainWindow->m_deviceUIs[deviceSetIndex]->loadDeviceSetSettings(&deviceSetPreset, m_mainWindow->m_pluginManager->getPluginAPI(), &m_mainWindow->m_workspaces, nullptr);
            deviceSetIndex++;
        }
    }
}

void LoadConfigurationFSM::loadFeatureSets()
{
    qDebug() << "LoadConfigurationFSM::loadFeatureSets";

    if (m_waitBox)
    {
        m_waitBox->setLabelText("Loading feature sets...");
        m_waitBox->setValue(75);
    }

    m_mainWindow->m_featureUIs[0]->loadFeatureSetSettings(
        &m_configuration->getFeatureSetPreset(),
        m_mainWindow->m_pluginManager->getPluginAPI(),
        m_mainWindow->m_apiAdapter,
        &m_mainWindow->m_workspaces,
        nullptr
    );

    for (int i = 0; i < m_mainWindow->m_featureUIs[0]->getNumberOfFeatures(); i++)
    {
        FeatureGUI *gui = m_mainWindow->m_featureUIs[0]->getFeatureGuiAt(i);
        QObject::connect(
            gui,
            &FeatureGUI::moveToWorkspace,
            this,
            [m_mainWindow=m_mainWindow, gui](int wsIndexDest){ m_mainWindow->featureMove(gui, wsIndexDest); }
        );
    }
}

void LoadConfigurationFSM::restoreGeometry()
{
    qDebug() << "LoadConfigurationFSM::restoreGeometry";

    // Lastly restore workspaces geometry
    if (m_waitBox)
    {
        m_waitBox->setValue(90);
        m_waitBox->setLabelText("Finalizing...");
    }

    for (int i = 0; i < m_configuration->getNumberOfWorkspaceGeometries(); i++)
    {
        m_mainWindow->m_workspaces[i]->restoreGeometry(m_configuration->getWorkspaceGeometries()[i]);
        m_mainWindow->m_workspaces[i]->restoreGeometry(m_configuration->getWorkspaceGeometries()[i]);
        m_mainWindow->m_workspaces[i]->adjustSubWindowsAfterRestore();
#ifdef ANDROID
        // On Android, workspaces seem to be restored to 0,20, rather than 0,0
        m_mainWindow->m_workspaces[i]->move(m_workspaces[i]->pos().x(), 0);
        // Need to call updateGeometry, otherwise sometimes the layout is corrupted
        m_mainWindow->m_workspaces[i]->updateGeometry();
#endif
        if (m_mainWindow->m_workspaces[i]->getAutoStackOption()) {
            m_mainWindow->m_workspaces[i]->layoutSubWindows();
        }
    }

    if (m_waitBox) {
        m_waitBox->setValue(100);
    }
}

InitFSM::InitFSM(MainWindow *mainWindow, SDRangelSplash *splash, bool loadDefault, QObject *parent) :
    MainWindowFSM(mainWindow, parent),
    m_splash(splash)
{
    // Create FSM
    createStates(2);

    if (loadDefault)
    {
    m_loadConfigurationFSM = new LoadConfigurationFSM(m_mainWindow, m_mainWindow->m_mainCore->getMutableSettings().getWorkingConfiguration(), nullptr, this);

    m_states[0]->addTransition(m_loadConfigurationFSM, &LoadConfigurationFSM::finished, m_finalState);

    connect(m_states[0], &QState::entered, this, &InitFSM::loadDefaultConfiguration);
    }
    else
    {
        m_states[0]->addTransition(m_finalState);
    }
    connect(m_finalState, &QState::entered, this, &InitFSM::showDefaultConfigurations);
}

void InitFSM::loadDefaultConfiguration()
{
    m_splash->showStatusMessage("load current configuration...", Qt::white);
    qDebug() << "MainWindow::MainWindow: load current configuration...";
    m_loadConfigurationFSM->start();
}

void InitFSM::showDefaultConfigurations()
{
    QApplication::restoreOverrideCursor();
    //m_mainWindow->show();

    if (m_mainWindow->m_workspaces.size() == 0)
    {
        qDebug() << "MainWindow::MainWindow: no or empty current configuration, creating empty workspace...";
        m_mainWindow->addWorkspace();

        // If no configurations, load some basic examples
        if (m_mainWindow->m_mainCore->getMutableSettings().getConfigurations()->size() == 0) {
            m_mainWindow->loadDefaultConfigurations();
        }
#if defined(ANDROID) || defined(__EMSCRIPTEN__)
        // Show welcome dialog
        m_mainWindow->on_action_Welcome_triggered();
#endif
        // Show configurations
        m_mainWindow->openConfigurationDialog(true);
}
}

CloseFSM::CloseFSM(MainWindow *mainWindow, QObject *parent) :
    MainWindowFSM(mainWindow, parent)
{
    // Create FSM
    createStates(2);

    m_states[0]->addTransition(m_mainWindow, &MainWindow::allDeviceSetsRemoved, m_finalState);

    connect(this, &QStateMachine::started, this, &CloseFSM::on_started);
    connect(this, &QStateMachine::finished, this, &CloseFSM::on_finished);
}

void CloseFSM::on_started()
{
    m_mainWindow->removeAllDeviceSets();
}

void CloseFSM::on_finished()
{
    m_mainWindow->close();
}

void MainWindow::sampleSourceAdd(Workspace *deviceWorkspace, Workspace *spectrumWorkspace, int deviceIndex)
{
    AddSampleSourceFSM *fsm = new AddSampleSourceFSM(this, deviceWorkspace, spectrumWorkspace, deviceIndex, true);
    connect(fsm, &AddSampleSourceFSM::finished, fsm, &AddSampleSourceFSM::deleteLater);
    fsm->start();
}

void MainWindow::sampleSourceCreate(
    int deviceSetIndex,
    int& deviceIndex,
    DeviceUISet *deviceUISet
)
{
    DeviceAPI *deviceAPI = deviceUISet->m_deviceAPI;
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

    qDebug() << "MainWindow::sampleSourceCreate:"
        << "deviceSetIndex:" << deviceSetIndex
        << "deviceIndex:" << deviceIndex
        << "hardwareId:" << samplingDevice->hardwareId
        << "sequence:" << samplingDevice->sequence
        << "id:" << samplingDevice->id
        << "serial:" << samplingDevice->serial
        << "displayedName:" << samplingDevice->displayedName;

    if (deviceAPI->getSamplingDeviceId().size() == 0) // non existent device => replace by default
    {
        qDebug("MainWindow::sampleSourceCreate: non existent device replaced by File Input");
        int fileInputDeviceIndex = DeviceEnumerator::instance()->getFileInputDeviceIndex();
        deviceIndex = fileInputDeviceIndex;
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
    auto it = m_deviceUIs.begin();
    int nbOfBuddies = 0;

    for (; it != m_deviceUIs.end(); ++it)
    {
        if ((*it != deviceUISet) && // do not add to itself
            (deviceUISet->m_deviceAPI->getHardwareId() == (*it)->m_deviceAPI->getHardwareId()) &&
            (deviceUISet->m_deviceAPI->getSamplingDeviceSerial() == (*it)->m_deviceAPI->getSamplingDeviceSerial()))
        {
            (*it)->m_deviceAPI->addBuddy(deviceUISet->m_deviceAPI);
            nbOfBuddies++;
        }
    }

    if (nbOfBuddies == 0) {
        deviceUISet->m_deviceAPI->setBuddyLeader(true);
    }

    // constructs new GUI and input object
    DeviceSampleSource *source = deviceAPI->getPluginInterface()->createSampleSourcePluginInstance(
            deviceAPI->getSamplingDeviceId(), deviceAPI);
    deviceAPI->setSampleSource(source);
}

void MainWindow::sampleSourceCreateUI(
    int deviceSetIndex,
    int deviceIndex,
    DeviceUISet *deviceUISet
)
{
    DeviceAPI *deviceAPI = deviceUISet->m_deviceAPI;
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
        [this, deviceGUI](int wsIndexDest){ this->deviceMove(deviceGUI, wsIndexDest); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::deviceChange,
        this,
        [this, deviceGUI](int newDeviceIndex){ this->samplingDeviceChangeHandler(deviceGUI, newDeviceIndex); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::showSpectrum,
        this,
        &MainWindow::mainSpectrumShow
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
        [this, deviceGUI](){ this->removeDeviceSet(deviceGUI->getIndex()); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::deviceSetPresetsDialogRequested,
        this,
        &MainWindow::openDeviceSetPresetsDialog
    );

    deviceAPI->getSampleSource()->setMessageQueueToGUI(deviceGUI->getInputMessageQueue());
    deviceUISet->m_deviceGUI = deviceGUI;
    const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(deviceIndex);
    const PluginInterface::SamplingDevice *selectedDevice = DeviceEnumerator::instance()->getRxSamplingDevice(deviceIndex); // FIXME: Why not use samplingDevice?
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
    deviceGUI->setCurrentDeviceIndex(deviceIndex);
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
    AddSampleSinkFSM *fsm = new AddSampleSinkFSM(this, deviceWorkspace, spectrumWorkspace, deviceIndex, true);
    connect(fsm, &AddSampleSinkFSM::finished, fsm, &AddSampleSinkFSM::deleteLater);
    fsm->start();
    }

void MainWindow::sampleSinkCreate(
        int deviceSetIndex,
        int& deviceIndex,
        DeviceUISet *deviceUISet
)
{
    DeviceAPI *deviceAPI = deviceUISet->m_deviceAPI;
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

    qDebug() << "MainWindow::sampleSinkCreate:"
        << "deviceSetIndex:" << deviceSetIndex
        << "newDeviceIndex:" << deviceIndex
        << "hardwareId:" << samplingDevice->hardwareId
        << "sequence:" << samplingDevice->sequence
        << "id:" << samplingDevice->id
        << "serial:" << samplingDevice->serial
        << "displayedName:" << samplingDevice->displayedName;

    if (deviceAPI->getSamplingDeviceId().size() == 0) // non existent device => replace by default
    {
        qDebug("MainWindow::sampleSinkCreate: non existent device replaced by File Sink");
        int fileSinkDeviceIndex = DeviceEnumerator::instance()->getFileOutputDeviceIndex();
        deviceIndex = fileSinkDeviceIndex;
        samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(fileSinkDeviceIndex);
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
    auto it = m_deviceUIs.begin();
    int nbOfBuddies = 0;

    for (; it != m_deviceUIs.end(); ++it)
    {
        if ((*it != deviceUISet) && // do not add to itself
            (deviceAPI->getHardwareId() == (*it)->m_deviceAPI->getHardwareId()) &&
            (deviceAPI->getSamplingDeviceSerial() == (*it)->m_deviceAPI->getSamplingDeviceSerial()))
        {
            (*it)->m_deviceAPI->addBuddy(deviceAPI);
            nbOfBuddies++;
        }
    }

    if (nbOfBuddies == 0) {
        deviceAPI->setBuddyLeader(true);
    }

    // constructs new GUI and output object
    DeviceSampleSink *sink = deviceAPI->getPluginInterface()->createSampleSinkPluginInstance(
            deviceAPI->getSamplingDeviceId(), deviceAPI);
    deviceAPI->setSampleSink(sink);
}

void MainWindow::sampleSinkCreateUI(
        int deviceSetIndex,
        int deviceIndex,
        DeviceUISet *deviceUISet
)
{
    DeviceAPI *deviceAPI = deviceUISet->m_deviceAPI;
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
        [this, deviceGUI](int wsIndexDest){ this->deviceMove(deviceGUI, wsIndexDest); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::deviceChange,
        this,
        [this, deviceGUI](int newDeviceIndex){ this->samplingDeviceChangeHandler(deviceGUI, newDeviceIndex); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::showSpectrum,
        this,
        &MainWindow::mainSpectrumShow
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
        [this, deviceGUI](){ this->removeDeviceSet(deviceGUI->getIndex()); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::deviceSetPresetsDialogRequested,
        this,
        &MainWindow::openDeviceSetPresetsDialog
    );

    deviceAPI->getSampleSink()->setMessageQueueToGUI(deviceGUI->getInputMessageQueue());
    deviceUISet->m_deviceGUI = deviceGUI;
    const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(deviceIndex);
    const PluginInterface::SamplingDevice *selectedDevice = DeviceEnumerator::instance()->getRxSamplingDevice(deviceIndex); // FIXME: Why getRxSamplingDevice?
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
    deviceGUI->setCurrentDeviceIndex(deviceIndex);
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
    AddSampleMIMOFSM *fsm = new AddSampleMIMOFSM(this, deviceWorkspace, spectrumWorkspace, deviceIndex, true);
    connect(fsm, &AddSampleMIMOFSM::finished, fsm, &AddSampleSinkFSM::deleteLater);
    fsm->start();
    }

void MainWindow::sampleMIMOCreate(
    int deviceSetIndex,
    int& deviceIndex,
    DeviceUISet *deviceUISet
)
{
    DeviceAPI *deviceAPI = deviceUISet->m_deviceAPI;
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

    qDebug() << "MainWindow::sampleMIMOCreate:"
        << "deviceSetIndex" << deviceSetIndex
        << "newDeviceIndex:" << deviceIndex
        << "hardwareId:" << samplingDevice->hardwareId
        << "sequence:" << samplingDevice->sequence
        << "id:" << samplingDevice->id
        << "serial:" << samplingDevice->serial
        << "displayedName:" << samplingDevice->displayedName;

    if (deviceAPI->getSamplingDeviceId().size() == 0) // non existent device => replace by default
    {
        qDebug("MainWindow::sampleMIMOCreate: non existent device replaced by Test MIMO");
        int testMIMODeviceIndex = DeviceEnumerator::instance()->getTestMIMODeviceIndex();
        deviceIndex = testMIMODeviceIndex;
        samplingDevice = DeviceEnumerator::instance()->getMIMOSamplingDevice(testMIMODeviceIndex);
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

    // constructs new GUI and output object
    DeviceSampleMIMO *mimo = deviceAPI->getPluginInterface()->createSampleMIMOPluginInstance(
            deviceAPI->getSamplingDeviceId(), deviceAPI);
    deviceAPI->setSampleMIMO(mimo);
}

void MainWindow::sampleMIMOCreateUI(
    int deviceSetIndex,
    int deviceIndex,
    DeviceUISet *deviceUISet
)
{
    DeviceAPI *deviceAPI = deviceUISet->m_deviceAPI;
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
        [this, deviceGUI](int wsIndexDest){ this->deviceMove(deviceGUI, wsIndexDest); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::deviceChange,
        this,
        [this, deviceGUI](int newDeviceIndex){ this->samplingDeviceChangeHandler(deviceGUI, newDeviceIndex); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::showSpectrum,
        this,
        &MainWindow::mainSpectrumShow
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
        [this, deviceGUI](){ this->removeDeviceSet(deviceGUI->getIndex()); }
    );
    QObject::connect(
        deviceGUI,
        &DeviceGUI::deviceSetPresetsDialogRequested,
        this,
        &MainWindow::openDeviceSetPresetsDialog
    );

    deviceAPI->getSampleMIMO()->setMessageQueueToGUI(deviceGUI->getInputMessageQueue());
    deviceUISet->m_deviceGUI = deviceGUI;
    const PluginInterface::SamplingDevice *samplingDevice = DeviceEnumerator::instance()->getMIMOSamplingDevice(deviceIndex);
    const PluginInterface::SamplingDevice *selectedDevice = DeviceEnumerator::instance()->getRxSamplingDevice(deviceIndex); // FIXME: Why getRxSamplingDevice?
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
    deviceGUI->setCurrentDeviceIndex(deviceIndex);
    QStringList channelNames;
    QStringList tmpChannelNames;
    m_pluginManager->listMIMOChannels(channelNames);
    m_pluginManager->listRxChannels(tmpChannelNames);
    channelNames.append(tmpChannelNames);
    m_pluginManager->listTxChannels(tmpChannelNames);
    channelNames.append(tmpChannelNames);
    deviceGUI->setChannelNames(channelNames);
    MainSpectrumGUI *spectrumGUI = deviceUISet->m_mainSpectrumGUI;
    spectrumGUI->setDeviceType(MainSpectrumGUI::DeviceMIMO);
    spectrumGUI->setIndex(deviceSetIndex);
    spectrumGUI->setToolTip(samplingDevice->displayedName);
    spectrumGUI->setTitle(samplingDevice->displayedName.split(" ")[0]);
}

void MainWindow::removeDeviceSet(int deviceSetIndex)
{
    qDebug("MainWindow::removeDeviceSet: index: %d", deviceSetIndex);
    if (deviceSetIndex >= (int) m_deviceUIs.size()) {
        return;
    }

    RemoveDeviceSetFSM *fsm = new RemoveDeviceSetFSM(this, deviceSetIndex);
    connect(fsm, &RemoveDeviceSetFSM::finished, fsm, &RemoveDeviceSetFSM::deleteLater);
    fsm->start();
    }

void MainWindow::removeAllDeviceSets()
    {
    qDebug() << "MainWindow::removeAllDeviceSets";
    RemoveAllDeviceSetsFSM *fsm = new RemoveAllDeviceSetsFSM(this);
    connect(fsm, &RemoveAllDeviceSetsFSM::finished, fsm, &RemoveAllDeviceSetsFSM::deleteLater);
    fsm->start();
    }

void MainWindow::removeLastDeviceSet()
{
    if (m_deviceUIs.size() > 0)
    {
    qDebug("MainWindow::removeLastDeviceSet: %s", qPrintable(m_deviceUIs.back()->m_deviceAPI->getHardwareId()));
        removeDeviceSet(m_deviceUIs.size() - 1);
	}
}

void MainWindow::addFeatureSet()
{
    auto newFeatureSetIndex = (int) m_featureUIs.size();

    if (newFeatureSetIndex != 0)
    {
        qWarning("MainWindow::addFeatureSet: attempt to add more than one feature set (%d)", newFeatureSetIndex);
        return;
    }

    m_mainCore->appendFeatureSet();
    m_featureUIs.push_back(new FeatureUISet(newFeatureSetIndex, m_mainCore->m_featureSets[newFeatureSetIndex]));
    emit m_mainCore->featureSetAdded(newFeatureSetIndex);
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
    while (!m_featureUIs.empty())
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
    m_mainCore->m_settings.sortCommands();
    if (m_mainCore->m_logger) {
        m_mainCore->setLoggingOptions();
    }
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
}

void MainWindow::saveDeviceSetPresetSettings(Preset* preset, int deviceSetIndex)
{
	qDebug("MainWindow::saveDeviceSetPresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

    // Save from currently selected source tab
    //int currentSourceTabIndex = ui->tabInputsView->currentIndex();
    const DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];
    deviceUISet->saveDeviceSetSettings(preset);
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

void MainWindow::loadDefaultConfigurations() const
{
    QDirIterator configurationsIt(":configurations", QDirIterator::Subdirectories);
    while (configurationsIt.hasNext())
    {
        QString group = configurationsIt.next();
        QDirIterator groupIt(group, {"*.cfgx"}, QDir::Files);
        while (groupIt.hasNext())
        {
            QFile file(groupIt.next());
			if (file.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				QByteArray base64Str;
				QTextStream instream(&file);
				instream >> base64Str;
				file.close();

				Configuration* configuration = MainCore::instance()->m_settings.newConfiguration("", "");
				configuration->deserialize(QByteArray::fromBase64(base64Str));
			}
            else
            {
                qDebug() << "MainWindow::loadDefaultConfigurations: Failed to open configuration " << file.fileName();
            }
        }
    }

    QDirIterator presetIt(":presets", QDirIterator::Subdirectories);
    while (presetIt.hasNext())
    {
        QString group = presetIt.next();
        QDirIterator groupIt(group, {"*.prex"}, QDir::Files);
        while (groupIt.hasNext())
        {
            QFile file(groupIt.next());
			if (file.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				QByteArray base64Str;
				QTextStream instream(&file);
				instream >> base64Str;
				file.close();

				Preset* preset = MainCore::instance()->m_settings.newPreset("", "");
				preset->deserialize(QByteArray::fromBase64(base64Str));
			}
            else
            {
                qDebug() << "MainWindow::loadDefaultConfigurations: Failed to open preset " << file.fileName();
            }
        }
    }
}

void MainWindow::loadConfiguration(const Configuration *configuration, bool fromDialog)
{
	qDebug("MainWindow::loadConfiguration: configuration [%s | %s] %d workspace(s) - %d device set(s) - %d feature(s)",
		qPrintable(configuration->getGroup()),
		qPrintable(configuration->getDescription()),
        (int)configuration->getNumberOfWorkspaceGeometries(),
        (int)configuration->getDeviceSetPresets().size(),
        (int)configuration->getFeatureSetPreset().getFeatureCount()
    );

    QProgressDialog *waitBox = nullptr;

    if (fromDialog)
    {
        waitBox = new QProgressDialog("Loading configuration...", "", 0, 100, this);
        waitBox->setWindowModality(Qt::WindowModal);
        waitBox->setAttribute(Qt::WA_DeleteOnClose, true);
        waitBox->setMinimumDuration(0);
        waitBox->setCancelButton(nullptr);
        waitBox->setValue(1);
        QApplication::processEvents();
    }

    LoadConfigurationFSM *fsm = new LoadConfigurationFSM(this, configuration, waitBox);
    connect(fsm, &LoadConfigurationFSM::finished, fsm, &LoadConfigurationFSM::deleteLater);
    fsm->start();
    }

void MainWindow::saveConfiguration(Configuration *configuration)
{
	qDebug("MainWindow::saveConfiguration: configuration [%s | %s] %d workspaces",
		qPrintable(configuration->getGroup()),
		qPrintable(configuration->getDescription()),
        (int)m_workspaces.size()
    );

    configuration->clearData();
    QList<Preset>& deviceSetPresets = configuration->getDeviceSetPresets();

    for (const auto& deviceUISet : m_deviceUIs)
    {
        deviceSetPresets.push_back(Preset());
        deviceUISet->saveDeviceSetSettings(&deviceSetPresets.back());
        deviceSetPresets.back().setSpectrumGeometry(MDIUtils::saveMDIGeometry(deviceUISet->m_mainSpectrumGUI));
        deviceSetPresets.back().setSpectrumWorkspaceIndex(deviceUISet->m_mainSpectrumGUI->getWorkspaceIndex());
        deviceSetPresets.back().setDeviceGeometry(MDIUtils::saveMDIGeometry(deviceUISet->m_deviceGUI));
        deviceSetPresets.back().setDeviceWorkspaceIndex(deviceUISet->m_deviceGUI->getWorkspaceIndex());
        qDebug("MainWindow::saveConfiguration: %s device in workspace %d spectrum in %d",
            qPrintable(deviceUISet->m_deviceAPI->getSamplingDeviceId()),
            deviceUISet->m_deviceGUI->getWorkspaceIndex(),
            deviceUISet->m_mainSpectrumGUI->getWorkspaceIndex());
    }

    m_featureUIs[0]->saveFeatureSetSettings(&configuration->getFeatureSetPreset());

    for (const auto& workspace : m_workspaces)
    {
        configuration->getWorkspaceGeometries().push_back(workspace->saveGeometry());
        configuration->getWorkspaceAutoStackOptions().push_back(workspace->getAutoStackOption());
        configuration->getWorkspaceTabSubWindowsOptions().push_back(workspace->getTabSubWindowsOption());
    }
}

QString MainWindow::openGLVersion() const
{
    const QOpenGLContext *glCurrentContext = QOpenGLContext::globalShareContext();
    if (glCurrentContext)
    {
        if (glCurrentContext->isValid())
        {
            int major = glCurrentContext->format().majorVersion();
            int minor = glCurrentContext->format().minorVersion();
            bool es = glCurrentContext->isOpenGLES();
            QString version = QString("%1.%2%3").arg(major).arg(minor).arg(es ? " ES" : "");
            // Waterfall doesn't work if version is less than 2.1, so display in red
            if ((major < 2) || ((major == 2) && (minor == 0))) {
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

void MainWindow::createMenuBar(QToolButton *button) const
{
    QMenu *fileMenu;
    QMenu *viewMenu;
    QMenu *workspacesMenu;
    QMenu *preferencesMenu;
    QMenu *helpMenu;

    if (button == nullptr)
    {
        QMenuBar *menuBar = this->menuBar();
        fileMenu = menuBar->addMenu("&File");
        viewMenu = menuBar->addMenu("&View");
        workspacesMenu = menuBar->addMenu("&Workspaces");
        preferencesMenu = menuBar->addMenu("&Preferences");
        helpMenu = menuBar->addMenu("&Help");
    }
    else
    {
        auto *menu = new QMenu();
        fileMenu = new QMenu("&File");
        menu->addMenu(fileMenu);
        viewMenu = new QMenu("&View");
        menu->addMenu(viewMenu);
        workspacesMenu = new QMenu("&Workspaces");
        menu->addMenu(workspacesMenu);
        preferencesMenu = new QMenu("&Preferences");
        menu->addMenu(preferencesMenu);
        helpMenu = new QMenu("&Help");
        menu->addMenu(helpMenu);
        button->setMenu(menu);
    }

    QAction *exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    exitAction->setToolTip("Exit");
    QObject::connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    QAction *fullscreenAction = viewMenu->addAction("&Fullscreen");
    fullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    fullscreenAction->setToolTip("Toggle fullscreen view");
    fullscreenAction->setCheckable(true);
    QObject::connect(fullscreenAction, &QAction::triggered, this, &MainWindow::on_action_View_Fullscreen_toggled);
#ifdef ANDROID
    QAction *keepscreenonAction = viewMenu->addAction("&Keep screen on");
    keepscreenonAction->setToolTip("Prevent screen from switching off");
    keepscreenonAction->setCheckable(true);
    QObject::connect(keepscreenonAction, &QAction::triggered, this, &MainWindow::on_action_View_KeepScreenOn_toggled);
#endif
#ifdef ENABLE_PROFILER
    QAction *profileAction = viewMenu->addAction("&Profile data...");
    profileAction->setToolTip("View profile data");
    QObject::connect(profileAction, &QAction::triggered, this, &MainWindow::on_action_Profile_triggered);
#endif

    QAction *newWorkspaceAction = workspacesMenu->addAction("&New");
    newWorkspaceAction->setToolTip("Add a new workspace");
    QObject::connect(newWorkspaceAction, &QAction::triggered, this, &MainWindow::addWorkspace);
    QAction *viewAllWorkspacesAction = workspacesMenu->addAction("&View all");
    viewAllWorkspacesAction->setToolTip("View all workspaces");
    QObject::connect(viewAllWorkspacesAction, &QAction::triggered, this, &MainWindow::viewAllWorkspaces);
    QAction *removeEmptyWorkspacesAction = workspacesMenu->addAction("&Remove empty");
    removeEmptyWorkspacesAction->setToolTip("Remove empty workspaces");
    QObject::connect(removeEmptyWorkspacesAction, &QAction::triggered, this, &MainWindow::removeEmptyWorkspaces);

    QAction *configurationsAction = preferencesMenu->addAction("&Configurations...");
    configurationsAction->setToolTip("Manage configurations");
    QObject::connect(configurationsAction, &QAction::triggered, this, &MainWindow::on_action_Configurations_triggered);
    QAction *audioAction = preferencesMenu->addAction("&Audio...");
    audioAction->setToolTip("Audio preferences");
    QObject::connect(audioAction, &QAction::triggered, this, &MainWindow::on_action_Audio_triggered);
    QAction *graphicsAction = preferencesMenu->addAction("&Graphics...");
    graphicsAction->setToolTip("Graphics preferences");
    QObject::connect(graphicsAction, &QAction::triggered, this, &MainWindow::on_action_Graphics_triggered);
    QAction *loggingAction = preferencesMenu->addAction("&Logging...");
    loggingAction->setToolTip("Logging preferences");
    QObject::connect(loggingAction, &QAction::triggered, this, &MainWindow::on_action_Logging_triggered);
    QAction *myPositionAction = preferencesMenu->addAction("My &Position...");
    myPositionAction->setToolTip("Set station position");
    QObject::connect(myPositionAction, &QAction::triggered, this, &MainWindow::on_action_My_Position_triggered);
    QAction *fftAction = preferencesMenu->addAction("&FFT...");
    fftAction->setToolTip("Set FFT preferences");
    QObject::connect(fftAction, &QAction::triggered, this, &MainWindow::on_action_FFT_triggered);
#if QT_CONFIG(process)
    QAction *fftWisdomAction = preferencesMenu->addAction("&FFTW Wisdom...");
    fftWisdomAction->setToolTip("Set FFTW cache");
    QObject::connect(fftWisdomAction, &QAction::triggered, this, &MainWindow::on_action_FFTWisdom_triggered);
#endif
    QMenu *devicesMenu = preferencesMenu->addMenu("&Devices");
    QAction *userArgumentsAction = devicesMenu->addAction("&User arguments...");
    userArgumentsAction->setToolTip("Device custom user arguments");
    QObject::connect(userArgumentsAction, &QAction::triggered, this, &MainWindow::on_action_DeviceUserArguments_triggered);
#if QT_CONFIG(process)
    QAction *commandsAction = preferencesMenu->addAction("C&ommands...");
    commandsAction->setToolTip("External commands dialog");
    QObject::connect(commandsAction, &QAction::triggered, this, &MainWindow::on_action_commands_triggered);
#endif
    QAction *saveAllAction = preferencesMenu->addAction("&Save all");
    saveAllAction->setToolTip("Save all current settings");
    QObject::connect(saveAllAction, &QAction::triggered, this, &MainWindow::on_action_saveAll_triggered);

#if defined(ANDROID) || defined(__EMSCRIPTEN__)
    QAction *welcomeAction = helpMenu->addAction("&Welcome...");
    welcomeAction->setToolTip("Show welcome dialog");
    QObject::connect(welcomeAction, &QAction::triggered, this, &MainWindow::on_action_Welcome_triggered);
#endif
    QAction *quickStartAction = helpMenu->addAction("&Quick start...");
    quickStartAction->setToolTip("Instructions for quick start");
    QObject::connect(quickStartAction, &QAction::triggered, this, &MainWindow::on_action_Quick_Start_triggered);
    QAction *mainWindowAction = helpMenu->addAction("&Main Window...");
    mainWindowAction->setToolTip("Help on main window details");
    QObject::connect(mainWindowAction, &QAction::triggered, this, &MainWindow::on_action_Main_Window_triggered);
    QAction *loadedPluginsAction = helpMenu->addAction("Loaded &Plugins...");
    loadedPluginsAction->setToolTip("List available plugins");
    QObject::connect(loadedPluginsAction, &QAction::triggered, this, &MainWindow::on_action_Loaded_Plugins_triggered);
    QAction *aboutAction = helpMenu->addAction("&About SDRangel...");
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

    if (!m_settingsSaved)
    {
    // Save window size and position
    QSettings s;
    s.setValue("mainWindowGeometry", qCompress(saveGeometry()).toBase64());
    s.setValue("mainWindowState", qCompress(saveState()).toBase64());

    saveConfiguration(m_mainCore->m_settings.getWorkingConfiguration());
    m_mainCore->m_settings.save();

        m_settingsSaved = true;
    }

    if (m_deviceUIs.size() > 0)
    {
        CloseFSM *fsm = new CloseFSM(this);
        connect(fsm, &CloseFSM::finished, fsm, &CloseFSM::deleteLater);
        fsm->start();
        closeEvent->ignore(); // CloseFSM will call close again later once all devices closed
        return;
    }

    if (m_profileDialog) {
        m_profileDialog->close();
    }

    closeEvent->accept();
}

void MainWindow::applySettings()
{
    loadConfiguration(m_mainCore->m_settings.getWorkingConfiguration());

    m_mainCore->m_settings.sortPresets();
    m_mainCore->setLoggingOptions();
}

bool MainWindow::handleMessage(const Message& cmd)
{
    if (MainCore::MsgLoadPreset::match(cmd))
    {
        auto& notif = (const MainCore::MsgLoadPreset&) cmd;
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
        auto& notif = (const MainCore::MsgSavePreset&) cmd;
        saveDeviceSetPresetSettings(notif.getPreset(), notif.getDeviceSetIndex());
        m_mainCore->m_settings.sortPresets();
        m_mainCore->m_settings.save();
        return true;
    }
    else if (MainCore::MsgLoadFeatureSetPreset::match(cmd))
    {
        if (!m_workspaces.empty())
        {
            auto& notif = (const MainCore::MsgLoadFeatureSetPreset&) cmd;
            loadFeatureSetPresetSettings(notif.getPreset(), notif.getFeatureSetIndex(), m_workspaces[0]);
        }

        return true;
    }
    else if (MainCore::MsgSaveFeatureSetPreset::match(cmd))
    {
        auto& notif = (const MainCore::MsgSaveFeatureSetPreset&) cmd;
        saveFeatureSetPresetSettings(notif.getPreset(), notif.getFeatureSetIndex());
        m_mainCore->m_settings.sortFeatureSetPresets();
        m_mainCore->m_settings.save();
        return true;
    }
    else if (MainCore::MsgDeletePreset::match(cmd))
    {
        auto& notif = (const MainCore::MsgDeletePreset&) cmd;
        const Preset *presetToDelete = notif.getPreset();
        // remove preset from settings
        m_mainCore->m_settings.deletePreset(presetToDelete);
        return true;
    }
    else if (MainCore::MsgLoadConfiguration::match(cmd))
    {
        auto& notif = (const MainCore::MsgLoadConfiguration&) cmd;
        const Configuration *configuration = notif.getConfiguration();
        loadConfiguration(configuration, false);
        return true;
    }
    else if (MainCore::MsgSaveConfiguration::match(cmd))
    {
        auto& notif = (const MainCore::MsgSaveConfiguration&) cmd;
        Configuration *configuration = notif.getConfiguration();
        saveConfiguration(configuration);
        return true;
    }
    else if (MainCore::MsgDeleteConfiguration::match(cmd))
    {
        auto& notif = (const MainCore::MsgDeleteConfiguration&) cmd;
        const Configuration *configurationToDelete = notif.getConfiguration();
        // remove configuration from settings
        m_mainCore->m_settings.deleteConfiguration(configurationToDelete);
        return true;
    }
    else if (MainCore::MsgDeleteEmptyWorkspaces::match(cmd))
    {
        removeEmptyWorkspaces();
        return true;
    }
    else if (MainCore::MsgAddWorkspace::match(cmd))
    {
        addWorkspace();
        return true;
    }
    else if (MainCore::MsgDeleteFeatureSetPreset::match(cmd))
    {
        auto& notif = (const MainCore::MsgDeleteFeatureSetPreset&) cmd;
        const FeatureSetPreset *presetToDelete = notif.getPreset();
        // remove preset from settings
        m_mainCore->m_settings.deleteFeatureSetPreset(presetToDelete);
        return true;
    }
    else if (MainCore::MsgAddDeviceSet::match(cmd))
    {
        auto& notif = (const MainCore::MsgAddDeviceSet&) cmd;
        int direction = notif.getDirection();

        if (!m_workspaces.empty())
        {
            if (direction == 1) { // Single stream Tx
                sampleSinkAdd(m_workspaces[0], m_workspaces[0], -1); // create with file output device by default
            } else if (direction == 0) { // Single stream Rx
                sampleSourceAdd(m_workspaces[0], m_workspaces[0], -1); // create with file input device by default
            } else if (direction == 2) { // MIMO
                sampleMIMOAdd(m_workspaces[0], m_workspaces[0], -1); // create with testMI MIMO device by default
            }
        }

        return true;
    }
    else if (MainCore::MsgRemoveLastDeviceSet::match(cmd))
    {
        if (!m_deviceUIs.empty()) {
            removeLastDeviceSet();
        }

        return true;
    }
    else if (MainCore::MsgSetDevice::match(cmd))
    {
        auto& notif = (const MainCore::MsgSetDevice&) cmd;
        int deviceSetIndex = notif.getDeviceSetIndex();

        if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_deviceUIs.size()))
        {
            Workspace *workspace = m_workspaces[m_deviceUIs[deviceSetIndex]->m_deviceGUI->getWorkspaceIndex()];
            sampleDeviceChange(notif.getDeviceType(), notif.getDeviceSetIndex(), notif.getDeviceIndex(), workspace);
        }

        return true;
    }
    else if (MainCore::MsgAddChannel::match(cmd))
    {
        auto& notif = (const MainCore::MsgAddChannel&) cmd;
        int deviceSetIndex = notif.getDeviceSetIndex();

        if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) m_deviceUIs.size()))
        {
            const DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];
            int deviceWorkspaceIndex = deviceUISet->m_deviceGUI->getWorkspaceIndex();
            deviceWorkspaceIndex = deviceWorkspaceIndex < m_workspaces.size() ? deviceWorkspaceIndex : 0;
            int channelRegistrationIndex;

            if (deviceUISet->m_deviceMIMOEngine)
            {
                int nbMIMOChannels = deviceUISet->getNumberOfAvailableMIMOChannels();
                int nbRxChannels = deviceUISet->getNumberOfAvailableRxChannels();
                int direction = notif.getDirection();

                if (direction == 2) {
                    channelRegistrationIndex = notif.getChannelRegistrationIndex();
                } else if (direction == 0) {
                    channelRegistrationIndex = nbMIMOChannels + notif.getChannelRegistrationIndex();
                } else {
                    channelRegistrationIndex = nbMIMOChannels + nbRxChannels + notif.getChannelRegistrationIndex();
                }
            }
            else
            {
                channelRegistrationIndex = notif.getChannelRegistrationIndex();
            }

            channelAddClicked(m_workspaces[deviceWorkspaceIndex], deviceSetIndex, channelRegistrationIndex);
        }

        return true;
    }
    else if (MainCore::MsgDeleteChannel::match(cmd))
    {
        auto& notif = (const MainCore::MsgDeleteChannel&) cmd;
        deleteChannel(notif.getDeviceSetIndex(), notif.getChannelIndex());
        return true;
    }
    else if (MainCore::MsgAddFeature::match(cmd))
    {
        auto& notif = (const MainCore::MsgAddFeature&) cmd;

        if (!m_workspaces.empty()) {
            featureAddClicked(m_workspaces[0], notif.getFeatureRegistrationIndex());
        }

        return true;
    }
    else if (MainCore::MsgDeleteFeature::match(cmd))
    {
        auto& notif = (const MainCore::MsgDeleteFeature&) cmd;
        deleteFeature(0, notif.getFeatureIndex());
        return true;
    }
    else if (MainCore::MsgMoveDeviceUIToWorkspace::match(cmd))
    {
        auto& notif = (const MainCore::MsgMoveDeviceUIToWorkspace&) cmd;
        int deviceSetIndex = notif.getDeviceSetIndex();

        if (deviceSetIndex < (int) m_deviceUIs.size())
        {
            DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];
            DeviceGUI *gui = deviceUISet->m_deviceGUI;
            deviceMove(gui, notif.getWorkspaceIndex());
        }

        return true;
    }
    else if (MainCore::MsgMoveMainSpectrumUIToWorkspace::match(cmd))
    {
        auto& notif = (const MainCore::MsgMoveMainSpectrumUIToWorkspace&) cmd;
        int deviceSetIndex = notif.getDeviceSetIndex();

        if (deviceSetIndex < (int) m_deviceUIs.size())
        {
            DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];
            MainSpectrumGUI *gui = deviceUISet->m_mainSpectrumGUI;
            mainSpectrumMove(gui, notif.getWorkspaceIndex());
        }

        return true;
    }
    else if (MainCore::MsgMoveFeatureUIToWorkspace::match(cmd))
    {
        auto& notif = (const MainCore::MsgMoveFeatureUIToWorkspace&) cmd;
        int featureIndex = notif.getFeatureIndex();

        if (featureIndex < m_featureUIs[0]->getNumberOfFeatures())
        {
            FeatureGUI *gui = m_featureUIs[0]->getFeatureGuiAt(featureIndex);
            featureMove(gui, notif.getWorkspaceIndex());
        }

        return true;
    }
    else if (MainCore::MsgMoveChannelUIToWorkspace::match(cmd))
    {
        auto& notif = (const MainCore::MsgMoveChannelUIToWorkspace&) cmd;
        int deviceSetIndex = notif.getDeviceSetIndex();

        if (deviceSetIndex < (int) m_deviceUIs.size())
        {
            int channelIndex = notif.getChannelIndex();
            DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];

            if (channelIndex < deviceUISet->getNumberOfChannels())
            {
                ChannelGUI *gui = deviceUISet->getChannelGUIAt(channelIndex);
                channelMove(gui, notif.getWorkspaceIndex());
            }
        }

        return true;
    }
    else if (MainCore::MsgApplySettings::match(cmd))
    {
        applySettings();
        return true;
    }
    else if (MainCore::MsgDVSerial::match(cmd))
    {
        return true;
    }

    return false;
}

void MainWindow::handleMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
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
    auto *workspace = new Workspace(workspaceIndex);
    m_workspaces.push_back(workspace);
    if (workspace->getMenuButton()) {
        createMenuBar(workspace->getMenuButton());
    }
    QStringList featureNames;
    m_pluginManager->listFeatures(featureNames);
    m_workspaces.back()->addAvailableFeatures(featureNames);
    this->addDockWidget(Qt::LeftDockWidgetArea, m_workspaces.back());

    QObject::connect(
        m_workspaces.back(),
        &Workspace::addRxDevice,
        this,
        [this](Workspace *inWorkspace, int deviceIndex) { this->sampleSourceAdd(inWorkspace, inWorkspace, deviceIndex); }
    );

    QObject::connect(
        m_workspaces.back(),
        &Workspace::addTxDevice,
        this,
        [this](Workspace *inWorkspace, int deviceIndex) { this->sampleSinkAdd(inWorkspace, inWorkspace, deviceIndex); }
    );

    QObject::connect(
        m_workspaces.back(),
        &Workspace::addMIMODevice,
        this,
        [this](Workspace *inWorkspace, int deviceIndex) { this->sampleMIMOAdd(inWorkspace, inWorkspace, deviceIndex); }
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

    QObject::connect(
        m_workspaces.back(),
        &Workspace::configurationPresetsDialogRequested,
        this,
        &MainWindow::on_action_Configurations_triggered
    );

    QObject::connect(
        m_workspaces.back(),
        &Workspace::startAllDevices,
        this,
        &MainWindow::startAllDevices
    );

    QObject::connect(
        m_workspaces.back(),
        &Workspace::stopAllDevices,
        this,
        &MainWindow::stopAllDevices
    );

    if (m_workspaces.size() > 1)
    {
        for (int i = 1; i < m_workspaces.size(); i++) {
            tabifyDockWidget(m_workspaces[0], m_workspaces[i]);
        }

        m_workspaces.back()->show();
        m_workspaces.back()->raise();
    }
 }

void MainWindow::viewAllWorkspaces() const
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
    auto it = m_workspaces.begin();

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

    // Renumerate
    for (int i = 0; i < m_workspaces.size(); i++)
    {
        Workspace *workspace = m_workspaces[i];
        workspace->setIndex(i);
        QList<QMdiSubWindow *> subWindows = workspace->getSubWindowList();

        for (auto& subWindow : subWindows)
        {
            if (qobject_cast<DeviceGUI*>(subWindow)) {
                qobject_cast<DeviceGUI*>(subWindow)->setWorkspaceIndex(i);
            }

            if (qobject_cast<MainSpectrumGUI*>(subWindow)) {
                qobject_cast<MainSpectrumGUI*>(subWindow)->setWorkspaceIndex(i);
            }

            if (qobject_cast<ChannelGUI*>(subWindow)) {
                qobject_cast<ChannelGUI*>(subWindow)->setWorkspaceIndex(i);
            }

            if (qobject_cast<FeatureGUI*>(subWindow)) {
                qobject_cast<FeatureGUI*>(subWindow)->setWorkspaceIndex(i);
            }
        }
    }

#ifdef ANDROID
    // Need at least one workspace on Android, as no menus without
    if (m_workspaces.size() == 0) {
        addWorkspace();
    }
#endif
}

#ifdef ANDROID
void MainWindow::on_action_View_KeepScreenOn_toggled(bool checked)
{
    if (checked) {
        Android::acquireScreenLock();
    } else {
        Android::releaseScreenLock();
    }
}
#endif

void MainWindow::on_action_View_Fullscreen_toggled(bool checked)
{
	if(checked) {
		showFullScreen();
	} else {
	    showNormal();
	}
}

void MainWindow::on_action_Profile_triggered()
{
    if (m_profileDialog == nullptr)
    {
        m_profileDialog = new ProfileDialog();
        new DialogPositioner(m_profileDialog, true);
    }
    m_profileDialog->show();
    m_profileDialog->raise();
}

void MainWindow::commandKeysConnect(const QObject *object, const char *slot)
{
    setFocus();
    connect(
        m_commandKeyReceiver,
        SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
        object,
        slot
    );
}

void MainWindow::commandKeysDisconnect(const QObject *object, const char *slot) const
{
    disconnect(
        m_commandKeyReceiver,
        SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
        object,
        slot
    );
}

void MainWindow::on_action_saveAll_triggered()
{
    saveConfiguration(m_mainCore->m_settings.getWorkingConfiguration());
    m_mainCore->m_settings.save();
    QMessageBox::information(this, tr("Done"), tr("All current settings saved"));
}

void MainWindow::on_action_Welcome_triggered()
{
    // Show welcome dialog
    WelcomeDialog welcomeDialog(this);
    new DialogPositioner(&welcomeDialog, true);
    welcomeDialog.exec();
}

void MainWindow::on_action_Quick_Start_triggered() const
{
    QDesktopServices::openUrl(QUrl("https://github.com/f4exb/sdrangel/wiki/Quick-start"));
}

void MainWindow::on_action_Main_Window_triggered() const
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
    openConfigurationDialog(false);
}

void MainWindow::openConfigurationDialog(bool openOnly)
{
    ConfigurationsDialog dialog(openOnly, this);
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
        [this](const Configuration* configuration) { this->loadConfiguration(configuration, true); }
    );
    new DialogPositioner(&dialog, true);
    dialog.exec();
}

void MainWindow::on_action_Audio_triggered()
{
	AudioDialogX audioDialog(m_dspEngine->getAudioDeviceManager(), this);
    new DialogPositioner(&audioDialog, true);
	audioDialog.exec();
}

void MainWindow::on_action_Graphics_triggered()
{
    GraphicsDialog graphicsDialog(m_mainCore->m_settings, this);
    new DialogPositioner(&graphicsDialog, true);
    graphicsDialog.exec();
}

void MainWindow::on_action_Logging_triggered()
{
    LoggingDialog loggingDialog(m_mainCore->m_settings, this);
    new DialogPositioner(&loggingDialog, true);
    loggingDialog.exec();
    m_mainCore->setLoggingOptions();
}

void MainWindow::on_action_My_Position_triggered()
{
	MyPositionDialog myPositionDialog(m_mainCore->m_settings, this);
    new DialogPositioner(&myPositionDialog, true);
	myPositionDialog.exec();
}

void MainWindow::on_action_DeviceUserArguments_triggered()
{
    qDebug("MainWindow::on_action_DeviceUserArguments_triggered");
    DeviceUserArgsDialog deviceUserArgsDialog(DeviceEnumerator::instance(), m_mainCore->m_settings.getDeviceUserArgs(), this);
    new DialogPositioner(&deviceUserArgsDialog, true);
    deviceUserArgsDialog.exec();
}

#if QT_CONFIG(process)
void MainWindow::on_action_commands_triggered()
{
    qDebug("MainWindow::on_action_commands_triggered");
    CommandsDialog commandsDialog(this);
    commandsDialog.setApiHost(m_apiServer->getHost());
    commandsDialog.setApiPort(m_apiServer->getPort());
    commandsDialog.setCommandKeyReceiver(m_commandKeyReceiver);
    commandsDialog.populateTree();
    new DialogPositioner(&commandsDialog, true);
    commandsDialog.exec();
}
#endif

void MainWindow::on_action_FFT_triggered()
{
    qDebug("MainWindow::on_action_FFT_triggered");
    FFTDialog fftDialog(m_mainCore->m_settings, this);
    new DialogPositioner(&fftDialog, true);
    fftDialog.exec();
}

#if QT_CONFIG(process)
void MainWindow::on_action_FFTWisdom_triggered()
{
    qDebug("MainWindow::on_action_FFTWisdom_triggered");

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
    new DialogPositioner(&fftWisdomDialog, true);

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
#endif

void MainWindow::samplingDeviceChangeHandler(const DeviceGUI *deviceGUI, int newDeviceIndex)
{
    auto deviceType = (int) deviceGUI->getDeviceType();
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
        QPoint p = deviceUISet->m_deviceGUI->pos();
        workspace->removeFromMdiArea(deviceUISet->m_deviceGUI);
        deviceUISet->m_deviceAPI->stopDeviceEngine();

        // deletes old UI and input object
        deviceUISet->m_deviceAPI->getSampleSource()->setMessageQueueToGUI(nullptr); // have source stop sending messages to the GUI

        delete deviceUISet->m_deviceGUI;
        deviceUISet->m_deviceAPI->resetSamplingDeviceId();
        deviceUISet->m_deviceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceInput(deviceUISet->m_deviceAPI->getSampleSource());
        deviceUISet->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

        sampleSourceCreate(deviceSetIndex, newDeviceIndex, deviceUISet);
        sampleSourceCreateUI(deviceSetIndex, newDeviceIndex, deviceUISet);
        deviceUISet->m_deviceGUI->setWorkspaceIndex(workspace->getIndex());
        workspace->addToMdiArea(deviceUISet->m_deviceGUI);
        deviceUISet->m_deviceGUI->move(p);

        QObject::connect(
            deviceUISet->m_deviceGUI,
            &DeviceGUI::addChannelEmitted,
            this,
            [this, workspace, deviceSetIndex](int channelPluginIndex){ this->channelAddClicked(workspace, deviceSetIndex, channelPluginIndex); }
        );
    }
}

void MainWindow::sampleSinkChange(int deviceSetIndex, int newDeviceIndex, Workspace *workspace)
{
    if (deviceSetIndex >= 0)
    {
        qDebug("MainWindow::sampleSinkChange: deviceSet %d workspace: %d", deviceSetIndex, workspace->getIndex());
        DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];
        QPoint p = deviceUISet->m_deviceGUI->pos();
        workspace->removeFromMdiArea(deviceUISet->m_deviceGUI);
        deviceUISet->m_deviceAPI->saveSamplingDeviceSettings(m_mainCore->m_settings.getWorkingPreset()); // save old API settings
        deviceUISet->m_deviceAPI->stopDeviceEngine();

        // deletes old UI and output object
        deviceUISet->m_deviceAPI->getSampleSink()->setMessageQueueToGUI(nullptr); // have sink stop sending messages to the GUI
        delete m_deviceUIs[deviceSetIndex]->m_deviceGUI;
        deviceUISet->m_deviceAPI->resetSamplingDeviceId();
        deviceUISet->m_deviceAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput(deviceUISet->m_deviceAPI->getSampleSink());
        deviceUISet->m_deviceAPI->clearBuddiesLists(); // clear old API buddies lists

        sampleSinkCreate(deviceSetIndex, newDeviceIndex, deviceUISet);
        sampleSinkCreateUI(deviceSetIndex, newDeviceIndex, deviceUISet);
        deviceUISet->m_deviceGUI->setWorkspaceIndex(workspace->getIndex());
        workspace->addToMdiArea(deviceUISet->m_deviceGUI);
        deviceUISet->m_deviceGUI->move(p);

        QObject::connect(
            deviceUISet->m_deviceGUI,
            &DeviceGUI::addChannelEmitted,
            this,
            [this, workspace, deviceSetIndex](int channelPluginIndex){ this->channelAddClicked(workspace, deviceSetIndex, channelPluginIndex); }
        );
    }
}

void MainWindow::sampleMIMOChange(int deviceSetIndex, int newDeviceIndex, Workspace *workspace)
{
    if (deviceSetIndex >= 0)
    {
        qDebug("MainWindow::sampleSinkChange: deviceSet %d workspace: %d", deviceSetIndex, workspace->getIndex());
        DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];
        QPoint p = deviceUISet->m_deviceGUI->pos();
        workspace->removeFromMdiArea(deviceUISet->m_deviceGUI);
        deviceUISet->m_deviceAPI->saveSamplingDeviceSettings(m_mainCore->m_settings.getWorkingPreset()); // save old API settings
        deviceUISet->m_deviceAPI->stopDeviceEngine();

        // deletes old UI and output object
        deviceUISet->m_deviceAPI->getSampleMIMO()->setMessageQueueToGUI(nullptr); // have sink stop sending messages to the GUI
        delete deviceUISet->m_deviceGUI;
        deviceUISet->m_deviceAPI->resetSamplingDeviceId();
        deviceUISet->m_deviceAPI->getPluginInterface()->deleteSampleMIMOPluginInstanceMIMO(deviceUISet->m_deviceAPI->getSampleMIMO());

        sampleMIMOCreate(deviceSetIndex, newDeviceIndex, deviceUISet);
        sampleMIMOCreateUI(deviceSetIndex, newDeviceIndex, deviceUISet);
        deviceUISet->m_deviceGUI->setWorkspaceIndex(workspace->getIndex());
        workspace->addToMdiArea(deviceUISet->m_deviceGUI);
        deviceUISet->m_deviceGUI->move(p);

        QObject::connect(
            deviceUISet->m_deviceGUI,
            &DeviceGUI::addChannelEmitted,
            this,
            [this, workspace, deviceSetIndex](int channelPluginIndex){ this->channelAddClicked(workspace, deviceSetIndex, channelPluginIndex); }
        );
    }
}

void MainWindow::channelMoveToDeviceSet(ChannelGUI *gui, int dsIndexDestination)
{
    int deviceSetIndex = gui->getDeviceSetIndex();
    int channelIndex = gui->getIndex();
    qDebug("MainWindow::channelMoveToDeviceSet: %s at %d:%d to %d",
        qPrintable(gui->getTitle()), deviceSetIndex, channelIndex, dsIndexDestination);

    if ((deviceSetIndex < (int) m_deviceUIs.size()) && (dsIndexDestination < (int) m_deviceUIs.size()))
    {
        DeviceUISet *deviceUI = m_deviceUIs[deviceSetIndex];
        DeviceUISet *destDeviceUI = m_deviceUIs[dsIndexDestination];
        ChannelAPI *channelAPI = deviceUI->getChannelAt(channelIndex);
        deviceUI->unregisterChannelInstanceAt(channelIndex);

        if (deviceUI->m_deviceSourceEngine) // source devices
        {
            destDeviceUI->registerRxChannelInstance(channelAPI, gui);
        }
        else if (deviceUI->m_deviceSinkEngine) // sink devices
        {
            destDeviceUI->registerTxChannelInstance(channelAPI, gui);
        }
        else if (deviceUI->m_deviceMIMOEngine) // MIMO devices
        {
            destDeviceUI->registerChannelInstance(channelAPI, gui);
        }

        gui->setIndex(channelAPI->getIndexInDeviceSet());
        gui->setDeviceSetIndex(dsIndexDestination);
        DeviceAPI *destDeviceAPI = destDeviceUI->m_deviceAPI;
        gui->setIndexToolTip(destDeviceAPI->getSamplingDeviceDisplayName());
        channelAPI->setDeviceAPI(destDeviceAPI);
        deviceUI->removeChannelMarker(&gui->getChannelMarker());
        destDeviceUI->addChannelMarker(&gui->getChannelMarker());
    }
}

void MainWindow::channelDuplicate(const ChannelGUI *sourceChannelGUI)
{
    channelDuplicateToDeviceSet(sourceChannelGUI, sourceChannelGUI->getDeviceSetIndex()); // Duplicate in same device set
}

void MainWindow::channelDuplicateToDeviceSet(const ChannelGUI *sourceChannelGUI, int dsIndexDestination)
{
    int dsIndexSource = sourceChannelGUI->getDeviceSetIndex();
    int sourceChannelIndex = sourceChannelGUI->getIndex();

    qDebug("MainWindow::channelDuplicateToDeviceSet: %s at %d:%d to %d in workspace %d",
        qPrintable(sourceChannelGUI->getTitle()), dsIndexSource, sourceChannelIndex, dsIndexDestination, sourceChannelGUI->getWorkspaceIndex());

    if ((dsIndexSource < (int) m_deviceUIs.size()) && (dsIndexDestination < (int) m_deviceUIs.size()))
    {
        DeviceUISet *sourceDeviceUI = m_deviceUIs[dsIndexSource];
        const ChannelAPI *sourceChannelAPI = sourceDeviceUI->getChannelAt(sourceChannelIndex);
        ChannelGUI *destChannelGUI = nullptr;
        DeviceUISet *destDeviceUI = m_deviceUIs[dsIndexDestination];

        if (destDeviceUI->m_deviceSourceEngine) // source device => Rx channels
        {
            const PluginAPI::ChannelRegistrations *channelRegistrations = m_pluginManager->getRxChannelRegistrations();
            const PluginInterface *pluginInterface = nullptr;

            for (const auto& channelRegistration : *channelRegistrations)
            {
                if (channelRegistration.m_channelIdURI == sourceChannelAPI->getURI())
                {
                    pluginInterface = channelRegistration.m_plugin;
                    break;
                }
            }

            if (pluginInterface)
            {
                ChannelAPI *channelAPI;
                BasebandSampleSink *rxChannel = nullptr;
                pluginInterface->createRxChannel(destDeviceUI->m_deviceAPI, &rxChannel, &channelAPI);
                destChannelGUI = pluginInterface->createRxChannelGUI(destDeviceUI, rxChannel);
                destDeviceUI->registerRxChannelInstance(channelAPI, destChannelGUI);
                destChannelGUI->setDeviceType(ChannelGUI::DeviceType::DeviceRx);
                destChannelGUI->setIndex(channelAPI->getIndexInDeviceSet());
                QByteArray b = sourceChannelGUI->serialize();
                destChannelGUI->deserialize(b);
            }
        }
        else if (destDeviceUI->m_deviceSinkEngine) // sink device => Tx channels
        {
            const PluginAPI::ChannelRegistrations *channelRegistrations = m_pluginManager->getTxChannelRegistrations(); // Available channel plugins
            const PluginInterface *pluginInterface = nullptr;

            for (const auto& channelRegistration : *channelRegistrations)
            {
                if (channelRegistration.m_channelIdURI == sourceChannelAPI->getURI())
                {
                    pluginInterface = channelRegistration.m_plugin;
                    break;
                }
            }

            if (pluginInterface)
            {
                ChannelAPI *channelAPI;
                BasebandSampleSource *txChannel = nullptr;
                pluginInterface->createTxChannel(destDeviceUI->m_deviceAPI, &txChannel, &channelAPI);
                destChannelGUI = pluginInterface->createTxChannelGUI(destDeviceUI, txChannel);
                destDeviceUI->registerTxChannelInstance(channelAPI, destChannelGUI);
                destChannelGUI->setDeviceType(ChannelGUI::DeviceType::DeviceTx);
                destChannelGUI->setIndex(channelAPI->getIndexInDeviceSet());
                QByteArray b = sourceChannelGUI->serialize();
                destChannelGUI->deserialize(b);
            }
        }
        else if (destDeviceUI->m_deviceMIMOEngine) // MIMO device => Any type of channel is possible
        {
            const PluginAPI::ChannelRegistrations *rxChannelRegistrations = m_pluginManager->getRxChannelRegistrations();
            const PluginAPI::ChannelRegistrations *txChannelRegistrations = m_pluginManager->getTxChannelRegistrations();
            const PluginAPI::ChannelRegistrations *mimoChannelRegistrations = m_pluginManager->getMIMOChannelRegistrations();
            const PluginInterface *pluginInterface = nullptr;

            for (const auto& channelRegistration : *rxChannelRegistrations)
            {
                if (channelRegistration.m_channelIdURI == sourceChannelAPI->getURI())
                {
                    pluginInterface = channelRegistration.m_plugin;
                    break;
                }
            }

            if (pluginInterface) // Rx channel
            {
                ChannelAPI *channelAPI;
                BasebandSampleSink *rxChannel = nullptr;
                pluginInterface->createRxChannel(destDeviceUI->m_deviceAPI, &rxChannel, &channelAPI);
                destChannelGUI = pluginInterface->createRxChannelGUI(destDeviceUI, rxChannel);
                destDeviceUI->registerRxChannelInstance(channelAPI, destChannelGUI);
                destChannelGUI->setDeviceType(ChannelGUI::DeviceType::DeviceMIMO);
                destChannelGUI->setIndex(channelAPI->getIndexInDeviceSet());
                QByteArray b = sourceChannelGUI->serialize();
                destChannelGUI->deserialize(b);
            }
            else
            {
                for (const auto& channelRegistration : *txChannelRegistrations)
                {
                    if (channelRegistration.m_channelIdURI == sourceChannelAPI->getURI())
                    {
                        pluginInterface = channelRegistration.m_plugin;
                        break;
                    }
                }

                if (pluginInterface) // Tx channel
                {
                    ChannelAPI *channelAPI;
                    BasebandSampleSource *txChannel = nullptr;
                    pluginInterface->createTxChannel(destDeviceUI->m_deviceAPI, &txChannel, &channelAPI);
                    destChannelGUI = pluginInterface->createTxChannelGUI(destDeviceUI, txChannel);
                    destDeviceUI->registerTxChannelInstance(channelAPI, destChannelGUI);
                    destChannelGUI->setDeviceType(ChannelGUI::DeviceType::DeviceMIMO);
                    destChannelGUI->setIndex(channelAPI->getIndexInDeviceSet());
                    QByteArray b = sourceChannelGUI->serialize();
                    destChannelGUI->deserialize(b);
                }
                else
                {
                    for (const auto& channelRegistration : *mimoChannelRegistrations)
                    {
                        if (channelRegistration.m_channelIdURI == sourceChannelAPI->getURI())
                        {
                            pluginInterface = channelRegistration.m_plugin;
                            break;
                        }
                    }

                    if (pluginInterface)
                    {
                        ChannelAPI *channelAPI;
                        MIMOChannel *mimoChannel = nullptr;
                        pluginInterface->createMIMOChannel(destDeviceUI->m_deviceAPI, &mimoChannel, &channelAPI);
                        destChannelGUI = pluginInterface->createMIMOChannelGUI(destDeviceUI, mimoChannel);
                        destDeviceUI->registerChannelInstance(channelAPI, destChannelGUI);
                        destChannelGUI->setDeviceType(ChannelGUI::DeviceType::DeviceMIMO);
                        destChannelGUI->setIndex(channelAPI->getIndexInDeviceSet());
                        QByteArray b = sourceChannelGUI->serialize();
                        destChannelGUI->deserialize(b);
                    }
                }
            }
        }

        const DeviceAPI *destDeviceAPI = destDeviceUI->m_deviceAPI;
        int workspaceIndex = sourceChannelGUI->getWorkspaceIndex();
        Workspace *workspace = workspaceIndex < m_workspaces.size() ? m_workspaces[sourceChannelGUI->getWorkspaceIndex()] : nullptr;

        if (destChannelGUI && workspace)
        {
            QObject::connect(
                destChannelGUI,
                &ChannelGUI::moveToWorkspace,
                this,
                [this, destChannelGUI](int wsIndexDest){ this->channelMove(destChannelGUI, wsIndexDest); }
            );
            QObject::connect(
                destChannelGUI,
                &ChannelGUI::duplicateChannelEmitted,
                this,
                [this, destChannelGUI](){ this->channelDuplicate(destChannelGUI); }
            );
            QObject::connect(
                destChannelGUI,
                &ChannelGUI::moveToDeviceSet,
                this,
                [this, destChannelGUI](int dsIndexDest){ this->channelMoveToDeviceSet(destChannelGUI, dsIndexDest); }
            );

            destChannelGUI->setDeviceSetIndex(dsIndexDestination);
            destChannelGUI->setIndexToolTip(destDeviceAPI->getSamplingDeviceDisplayName());
            destChannelGUI->setWorkspaceIndex(workspace->getIndex());
            qDebug("MainWindow::channelDuplicate: adding %s to workspace #%d",
                qPrintable(destChannelGUI->getTitle()), workspace->getIndex());
            workspace->addToMdiArea((QMdiSubWindow*) destChannelGUI);
        }
    }
}

void MainWindow::channelAddClicked(Workspace *workspace, int deviceSetIndex, int channelPluginIndex)
{
    if (deviceSetIndex < (int) m_deviceUIs.size())
    {
        DeviceUISet *deviceUI = m_deviceUIs[deviceSetIndex];
        ChannelGUI *gui;
        ChannelAPI *channelAPI;
        const DeviceAPI *deviceAPI = deviceUI->m_deviceAPI;

        if (deviceUI->m_deviceSourceEngine) // source device => Rx channels
        {
            PluginAPI::ChannelRegistrations *channelRegistrations = m_pluginManager->getRxChannelRegistrations(); // Available channel plugins
            const PluginInterface *pluginInterface = (*channelRegistrations)[channelPluginIndex].m_plugin;
            BasebandSampleSink *rxChannel = nullptr;
            pluginInterface->createRxChannel(deviceUI->m_deviceAPI, &rxChannel, &channelAPI);
            gui = pluginInterface->createRxChannelGUI(deviceUI, rxChannel);
            deviceUI->registerRxChannelInstance(channelAPI, gui);
            gui->setDeviceType(ChannelGUI::DeviceType::DeviceRx);
            gui->setIndex(channelAPI->getIndexInDeviceSet());
            gui->setDisplayedame(pluginInterface->getPluginDescriptor().displayedName);
        }
        else if (deviceUI->m_deviceSinkEngine) // sink device => Tx channels
        {
            PluginAPI::ChannelRegistrations *channelRegistrations = m_pluginManager->getTxChannelRegistrations(); // Available channel plugins
            const PluginInterface *pluginInterface = (*channelRegistrations)[channelPluginIndex].m_plugin;
            BasebandSampleSource *txChannel = nullptr;
            pluginInterface->createTxChannel(deviceUI->m_deviceAPI, &txChannel, &channelAPI);
            gui = pluginInterface->createTxChannelGUI(deviceUI, txChannel);
            deviceUI->registerTxChannelInstance(channelAPI, gui);
            gui->setDeviceType(ChannelGUI::DeviceType::DeviceTx);
            gui->setIndex(channelAPI->getIndexInDeviceSet());
            gui->setDisplayedame(pluginInterface->getPluginDescriptor().displayedName);
        }
        else if (deviceUI->m_deviceMIMOEngine) // MIMO device => all possible channels. Depends on index range
        {
            int nbMIMOChannels = deviceUI->getNumberOfAvailableMIMOChannels();
            int nbRxChannels = deviceUI->getNumberOfAvailableRxChannels();
            int nbTxChannels = deviceUI->getNumberOfAvailableTxChannels();
            qDebug("MainWindow::channelAddClicked: MIMO: dev %d : nbMIMO: %d nbRx: %d nbTx: %d selected: %d",
                deviceSetIndex, nbMIMOChannels, nbRxChannels, nbTxChannels, channelPluginIndex);

            if (channelPluginIndex < nbMIMOChannels)
            {
                PluginAPI::ChannelRegistrations *channelRegistrations = m_pluginManager->getMIMOChannelRegistrations(); // Available channel plugins
                const PluginInterface *pluginInterface = (*channelRegistrations)[channelPluginIndex].m_plugin;
                MIMOChannel *mimoChannel = nullptr;
                pluginInterface->createMIMOChannel(deviceUI->m_deviceAPI, &mimoChannel, &channelAPI);
                gui = pluginInterface->createMIMOChannelGUI(deviceUI, mimoChannel);
                deviceUI->registerChannelInstance(channelAPI, gui);
                gui->setIndex(channelAPI->getIndexInDeviceSet());
                gui->setDisplayedame(pluginInterface->getPluginDescriptor().displayedName);
            }
            else if (channelPluginIndex < nbMIMOChannels + nbRxChannels) // Rx
            {
                PluginAPI::ChannelRegistrations *channelRegistrations = m_pluginManager->getRxChannelRegistrations(); // Available channel plugins
                const PluginInterface *pluginInterface = (*channelRegistrations)[channelPluginIndex - nbMIMOChannels].m_plugin;
                BasebandSampleSink *rxChannel = nullptr;
                pluginInterface->createRxChannel(deviceUI->m_deviceAPI, &rxChannel, &channelAPI);
                gui = pluginInterface->createRxChannelGUI(deviceUI, rxChannel);
                deviceUI->registerRxChannelInstance(channelAPI, gui);
                gui->setIndex(channelAPI->getIndexInDeviceSet());
                gui->setDisplayedame(pluginInterface->getPluginDescriptor().displayedName);
            }
            else if (channelPluginIndex < nbMIMOChannels + nbRxChannels + nbTxChannels)
            {
                PluginAPI::ChannelRegistrations *channelRegistrations = m_pluginManager->getTxChannelRegistrations(); // Available channel plugins
                const PluginInterface *pluginInterface = (*channelRegistrations)[channelPluginIndex - nbMIMOChannels - nbRxChannels].m_plugin;
                BasebandSampleSource *txChannel = nullptr;
                pluginInterface->createTxChannel(deviceUI->m_deviceAPI, &txChannel, &channelAPI);
                gui = pluginInterface->createTxChannelGUI(deviceUI, txChannel);
                deviceUI->registerTxChannelInstance(channelAPI, gui);
                gui->setIndex(channelAPI->getIndexInDeviceSet());
                gui->setDisplayedame(pluginInterface->getPluginDescriptor().displayedName);
            }
            else
            {
                return;
            }

            gui->setDeviceType(ChannelGUI::DeviceType::DeviceMIMO);
        }
        else
        {
            return;
        }

        if (gui)
        {
            QObject::connect(
                gui,
                &ChannelGUI::moveToWorkspace,
                this,
                [this, gui](int wsIndexDest){ this->channelMove(gui, wsIndexDest); }
            );
            QObject::connect(
                gui,
                &ChannelGUI::duplicateChannelEmitted,
                this,
                [this, gui](){ this->channelDuplicate(gui); }
            );
            QObject::connect(
                gui,
                &ChannelGUI::moveToDeviceSet,
                this,
                [this, gui](int dsIndexDest){ this->channelMoveToDeviceSet(gui, dsIndexDest); }
            );

            gui->setDeviceSetIndex(deviceSetIndex);
            gui->setIndexToolTip(deviceAPI->getSamplingDeviceDisplayName());
            gui->setWorkspaceIndex(workspace->getIndex());
            qDebug("MainWindow::channelAddClicked: adding %s to workspace #%d",
                qPrintable(gui->getTitle()), workspace->getIndex());
            workspace->addToMdiArea((QMdiSubWindow*) gui);
            loadDefaultPreset(channelAPI->getURI(), gui);
        }
    }
}

void MainWindow::featureAddClicked(Workspace *workspace, int featureIndex)
{
    qDebug("MainWindow::featureAddClicked: W%d feature at %d", workspace->getIndex(), featureIndex);

    int currentFeatureSetIndex = 0; // Do it in the unique set
    FeatureUISet *featureUISet = m_featureUIs[currentFeatureSetIndex];
    qDebug("MainWindow::featureAddClicked: m_apiAdapter: %p", m_apiAdapter);
    PluginAPI::FeatureRegistrations *featureRegistrations = m_pluginManager->getFeatureRegistrations(); // Available feature plugins
    const PluginInterface *pluginInterface = (*featureRegistrations)[featureIndex].m_plugin;
    Feature *feature = pluginInterface->createFeature(m_apiAdapter);
    FeatureGUI *gui = pluginInterface->createFeatureGUI(featureUISet, feature);
    featureUISet->registerFeatureInstance(gui, feature);
    gui->setIndex(feature->getIndexInFeatureSet());
    gui->setWorkspaceIndex(workspace->getIndex());
    gui->setDisplayedame(pluginInterface->getPluginDescriptor().displayedName);
    workspace->addToMdiArea((QMdiSubWindow*) gui);
    loadDefaultPreset(feature->getURI(), gui);

    QObject::connect(
        gui,
        &FeatureGUI::moveToWorkspace,
        this,
        [this, gui](int wsIndexDest){ this->featureMove(gui, wsIndexDest); }
    );
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

void MainWindow::mainSpectrumShow(int deviceSetIndex)
{
    DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];
    deviceUISet->m_mainSpectrumGUI->show();
    deviceUISet->m_mainSpectrumGUI->raise();
}

void MainWindow::mainSpectrumRequestDeviceCenterFrequency(int deviceSetIndex, qint64 deviceCenterFrequency)
{
    DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];
    DeviceAPI *deviceAPI = deviceUISet->m_deviceAPI;

    if (deviceAPI->getSampleSource()) {
        deviceAPI->getSampleSource()->setCenterFrequency(deviceCenterFrequency);
    } else if (deviceAPI->getSampleSink()) {
        deviceAPI->getSampleSink()->setCenterFrequency(deviceCenterFrequency);
    }
    // Not implemented for MIMO
}

void MainWindow::showAllChannels(int deviceSetIndex)
{
    DeviceUISet *deviceUISet = m_deviceUIs[deviceSetIndex];

    for (int i = 0; i < deviceUISet->getNumberOfChannels(); i++)
    {
        deviceUISet->getChannelGUIAt(i)->show();
        deviceUISet->getChannelGUIAt(i)->raise();
    }
}

// Start all devices in the workspace
void MainWindow::startAllDevices(const Workspace *workspace) const
{
    int workspaceIndex = workspace->getIndex();
    for (auto deviceUI : m_deviceUIs)
    {
        if (deviceUI->m_deviceAPI->getWorkspaceIndex() == workspaceIndex)
        {
            // We use WebAPI rather than call deviceUI->m_deviceAPI->startDeviceEngine();
            // so that the start/stop button in the Device GUI is correctly updated
            int deviceIndex = deviceUI->m_deviceAPI->getDeviceSetIndex();
            ChannelWebAPIUtils::run(deviceIndex);
        }
    }
}

// Stop all devices in the workspace
void MainWindow::stopAllDevices(const Workspace *workspace) const
{
    int workspaceIndex = workspace->getIndex();
    for (auto deviceUI : m_deviceUIs)
    {
        if (deviceUI->m_deviceAPI->getWorkspaceIndex() == workspaceIndex)
        {
            int deviceIndex = deviceUI->m_deviceAPI->getDeviceSetIndex();
            ChannelWebAPIUtils::stop(deviceIndex);
        }
    }
}

void MainWindow::deviceStateChanged(DeviceAPI *deviceAPI)
{
    emit m_mainCore->deviceStateChanged(deviceAPI->getDeviceSetIndex(), deviceAPI);
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
    new DialogPositioner(&dialog, true);
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
                [this, gui](int wsIndexDest){ this->featureMove(gui, wsIndexDest); }
            );
        }
    }
}

void MainWindow::openDeviceSetPresetsDialog(QPoint p, const DeviceGUI *deviceGUI)
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
    new DialogPositioner(&dialog, true);
    dialog.exec();
}

void MainWindow::deleteFeature(int featureSetIndex, int featureIndex)
{
    if ((featureSetIndex >= 0) && (featureSetIndex < (int) m_featureUIs.size()))
    {
        FeatureUISet *featureUISet = m_featureUIs[featureSetIndex];
        featureUISet->deleteFeature(featureIndex);
    }
}

// Look for and load a preset named Defaults/Default for the given plugin id
void MainWindow::loadDefaultPreset(const QString& pluginId, SerializableInterface *serializableInterface)
{
    const QList<PluginPreset*>* presets = m_mainCore->m_settings.getPluginPresets();

    for (const auto preset : *presets)
    {
        if (preset->getGroup() == "Defaults"
            && preset->getDescription() == "Default"
            && preset->getPluginIdURI() == pluginId)
        {
            qDebug() << "MainWindow::loadDefaultPreset: Loading " << preset->getGroup() << preset->getDescription() << "for" << pluginId;
            serializableInterface->deserialize(preset->getConfig());
        }
    }
}

void MainWindow::on_action_About_triggered()
{
	AboutDialog dlg(m_apiHost.isEmpty() ? "0.0.0.0" : m_apiHost, m_apiPort, m_mainCore->m_settings, this);
	dlg.exec();
}

void MainWindow::updateStatus()
{
    if (m_dateTimeWidget) {
        m_dateTimeWidget->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss t"));
    }
}

void MainWindow::commandKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release) const
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
            auto* command_mod = const_cast<Command*>(command);
            command_mod->run(m_apiServer->getHost(), m_apiServer->getPort(), currentDeviceSetIndex);
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
#ifdef ANDROID
    if (event->key() == Qt::Key_Back)
    {
        // On Android, we don't want to exit when back key is pressed, just run in the background
        Android::moveTaskToBack();
    }
    else
#endif
    {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::orientationChanged(Qt::ScreenOrientation orientation)
{
#ifdef ANDROID
    // Adjust workspace tab position, to leave max space for MDI windows
    if ((orientation == Qt::LandscapeOrientation) || (orientation == Qt::InvertedLandscapeOrientation)) {
        setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::West);
    } else {
        setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::South);
    }
#else
    (void) orientation;
#endif
}
