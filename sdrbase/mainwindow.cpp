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

#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QDateTime>
#include <QSysInfo>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "audio/audiodeviceinfo.h"
#include "gui/indicator.h"
#include "gui/presetitem.h"
#include "gui/addpresetdialog.h"
#include "gui/pluginsdialog.h"
#include "gui/aboutdialog.h"
#include "gui/rollupwidget.h"
#include "gui/channelwindow.h"
#include "gui/audiodialog.h"
#include "gui/samplingdevicecontrol.h"
#include "dsp/dspengine.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspcommands.h"
#include "plugin/plugingui.h"
#include "plugin/pluginapi.h"
#include "device/deviceapi.h"
#include "plugin/plugingui.h"

#include "gui/glspectrum.h"
#include "gui/glspectrumgui.h"

#include <string>
#include <QDebug>

MainWindow::MainWindow(QWidget* parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow),
	m_audioDeviceInfo(new AudioDeviceInfo),
	m_masterTabIndex(0),
	m_settings(),
	m_dspEngine(DSPEngine::instance()),
	m_lastEngineState((DSPDeviceEngine::State)-1),
	m_inputGUI(0),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_sampleFileName(std::string("./test.sdriq"))
{
	qDebug() << "MainWindow::MainWindow: start";

	ui->setupUi(this);
	createStatusBar();

	setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

	// work around broken Qt dock widget ordering
	removeDockWidget(ui->inputSelectDock);
    removeDockWidget(ui->inputViewDock);
	removeDockWidget(ui->spectraDisplayDock);
	removeDockWidget(ui->presetDock);
	removeDockWidget(ui->channelDock);
	addDockWidget(Qt::LeftDockWidgetArea, ui->inputSelectDock);
    addDockWidget(Qt::LeftDockWidgetArea, ui->inputViewDock);
	addDockWidget(Qt::LeftDockWidgetArea, ui->spectraDisplayDock);
	addDockWidget(Qt::LeftDockWidgetArea, ui->presetDock);
	addDockWidget(Qt::RightDockWidgetArea, ui->channelDock);

	ui->inputSelectDock->show();
    ui->inputSelectDock->show();
	ui->spectraDisplayDock->show();
	ui->presetDock->show();
	ui->channelDock->show();

	ui->menu_Window->addAction(ui->inputSelectDock->toggleViewAction());
    ui->menu_Window->addAction(ui->inputViewDock->toggleViewAction());
	ui->menu_Window->addAction(ui->spectraDisplayDock->toggleViewAction());
	ui->menu_Window->addAction(ui->presetDock->toggleViewAction());
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
    m_pluginManager->loadPlugins();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()), Qt::QueuedConnection);

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(1000);

	m_masterTimer.start(50);

    qDebug() << "MainWindow::MainWindow: add the first device...";

    addDevice(); // add the first device

    qDebug() << "MainWindow::MainWindow: load settings...";

	loadSettings();

	qDebug() << "MainWindow::MainWindow: select SampleSource from settings...";

	int sampleSourceIndex = m_settings.getSourceIndex();
	sampleSourceIndex = m_pluginManager->selectSampleSourceByIndex(sampleSourceIndex, m_deviceUIs.back()->m_deviceAPI);

	if (sampleSourceIndex < 0)
	{
	    qCritical("MainWindow::MainWindow: no sample source. Exit");
	    exit(0);
	}

    bool sampleSourceSignalsBlocked = m_deviceUIs.back()->m_samplingDeviceControl->getDeviceSelector()->blockSignals(true);
    m_deviceUIs.back()->m_samplingDeviceControl->getDeviceSelector()->setCurrentIndex(sampleSourceIndex);
    m_deviceUIs.back()->m_samplingDeviceControl->getDeviceSelector()->blockSignals(sampleSourceSignalsBlocked);

	qDebug() << "MainWindow::MainWindow: load current preset settings...";

	loadPresetSettings(m_settings.getWorkingPreset(), 0);

	qDebug() << "MainWindow::MainWindow: apply settings...";

	applySettings();

	qDebug() << "MainWindow::MainWindow: update preset controls...";

	updatePresetControls();

	connect(ui->tabInputsView, SIGNAL(currentChanged(int)), this, SLOT(tabInputViewIndexChanged()));

    qDebug() << "MainWindow::MainWindow: end";
}

MainWindow::~MainWindow()
{
    delete m_pluginManager;
	delete m_dateTimeWidget;
	delete m_showSystemWidget;

	delete ui;
}

void MainWindow::addDevice()
{
    DSPDeviceEngine *dspDeviceEngine = m_dspEngine->addDeviceEngine();
    dspDeviceEngine->start();

    uint dspDeviceEngineUID =  dspDeviceEngine->getUID();
    char tabNameCStr[16];
    sprintf(tabNameCStr, "R%d", dspDeviceEngineUID);

    m_deviceUIs.push_back(new DeviceUISet(m_masterTimer));
    m_deviceUIs.back()->m_deviceEngine = dspDeviceEngine;

    DeviceAPI *deviceAPI = new DeviceAPI(this, m_deviceUIs.size()-1, dspDeviceEngine, m_deviceUIs.back()->m_spectrum, m_deviceUIs.back()->m_channelWindow);

    m_deviceUIs.back()->m_deviceAPI = deviceAPI;
    m_deviceUIs.back()->m_samplingDeviceControl->setDeviceAPI(deviceAPI);
    m_deviceUIs.back()->m_samplingDeviceControl->setPluginManager(m_pluginManager);
    m_deviceUIs.back()->m_samplingDeviceControl->populateChannelSelector();

    dspDeviceEngine->addSink(m_deviceUIs.back()->m_spectrumVis);
    ui->tabSpectra->addTab(m_deviceUIs.back()->m_spectrum, tabNameCStr);
    ui->tabSpectraGUI->addTab(m_deviceUIs.back()->m_spectrumGUI, tabNameCStr);
    ui->tabChannels->addTab(m_deviceUIs.back()->m_channelWindow, tabNameCStr);

    bool sampleSourceSignalsBlocked = m_deviceUIs.back()->m_samplingDeviceControl->getDeviceSelector()->blockSignals(true);
    m_pluginManager->duplicateLocalSampleSourceDevices(dspDeviceEngineUID);
    m_pluginManager->fillSampleSourceSelector(m_deviceUIs.back()->m_samplingDeviceControl->getDeviceSelector(), dspDeviceEngineUID);

    connect(m_deviceUIs.back()->m_samplingDeviceControl->getDeviceSelectionConfirm(), SIGNAL(clicked(bool)), this, SLOT(on_sampleSource_confirmClicked(bool)));

    m_deviceUIs.back()->m_samplingDeviceControl->getDeviceSelector()->blockSignals(sampleSourceSignalsBlocked);
    ui->tabInputsSelect->addTab(m_deviceUIs.back()->m_samplingDeviceControl, tabNameCStr);

    int sampleSourceIndex = m_pluginManager->selectSampleSourceBySerialOrSequence("sdrangel.samplesource.filesource", "0", 0, m_deviceUIs.back()->m_deviceAPI);
}

void MainWindow::removeLastDevice()
{
    DSPDeviceEngine *lastDeviceEngine = m_deviceUIs.back()->m_deviceEngine;
    lastDeviceEngine->stopAcquistion();
    lastDeviceEngine->removeSink(m_deviceUIs.back()->m_spectrumVis);

    ui->tabSpectraGUI->removeTab(ui->tabSpectraGUI->count() - 1);
    ui->tabSpectra->removeTab(ui->tabSpectra->count() - 1);

    m_deviceUIs.back()->m_deviceAPI->freeAll();

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
    m_dspEngine->removeLastDeviceEngine();

    m_deviceUIs.pop_back();
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

void MainWindow::setInputGUI(int deviceTabIndex, QWidget* gui, const QString& sourceDisplayName)
{
    char tabNameCStr[16];
    sprintf(tabNameCStr, "R%d", deviceTabIndex);

    qDebug("MainWindow::setInputGUI: insert tab at %d", deviceTabIndex);

    if (deviceTabIndex < m_deviceWidgetTabs.size())
    {
        m_deviceWidgetTabs[deviceTabIndex] = {gui, sourceDisplayName, QString(tabNameCStr)};
    }
    else
    {
        m_deviceWidgetTabs.append({gui, sourceDisplayName, QString(tabNameCStr)});
    }

    ui->tabInputsView->clear();

    for (int i = 0; i < m_deviceWidgetTabs.size(); i++)
    {
        qDebug("MainWindow::setInputGUI: adding tab for %s", m_deviceWidgetTabs[i].displayName.toStdString().c_str());
        ui->tabInputsView->addTab(m_deviceWidgetTabs[i].gui, m_deviceWidgetTabs[i].tabName);
        ui->tabInputsView->setTabToolTip(i, m_deviceWidgetTabs[i].displayName);
    }

    ui->tabInputsView->setCurrentIndex(deviceTabIndex);
}

void MainWindow::loadSettings()
{
	qDebug() << "MainWindow::loadSettings";

    m_settings.load();

    for(int i = 0; i < m_settings.getPresetCount(); ++i)
    {
    	addPresetToTree(m_settings.getPreset(i));
    }
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
        deviceUI->m_deviceAPI->loadChannelSettings(preset, &(m_pluginManager->m_pluginAPI));
        deviceUI->m_deviceAPI->loadSourceSettings(preset);
	}

	// has to be last step
	restoreState(preset->getLayout());
}

void MainWindow::savePresetSettings(Preset* preset, int tabIndex)
{
	qDebug("MainWindow::savePresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

    // Save from currently selected source tab
    //int currentSourceTabIndex = ui->tabInputsView->currentIndex();
    DeviceUISet *deviceUI = m_deviceUIs[tabIndex];

	preset->setSpectrumConfig(deviceUI->m_spectrumGUI->serialize());
	preset->clearChannels();
    deviceUI->m_deviceAPI->saveChannelSettings(preset);
    deviceUI->m_deviceAPI->saveSourceSettings(preset);

    preset->setLayout(saveState());
}

void MainWindow::createStatusBar()
{
    QString qtVersionStr = QString("Qt %1 ").arg(QT_VERSION_STR);
    m_showSystemWidget = new QLabel("SDRangel v2.0.1 " + qtVersionStr + QSysInfo::prettyProductName(), this);
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
	sl.append(QString("%1 kHz").arg(preset->getCenterFrequency() / 1000));
	sl.append(preset->getDescription());
	PresetItem* item = new PresetItem(group, sl, preset->getCenterFrequency(), PItem);
	item->setTextAlignment(0, Qt::AlignRight);
	item->setData(0, Qt::UserRole, qVariantFromValue(preset));
	ui->presetTree->resizeColumnToContents(0);

	updatePresetControls();
	return item;
}

void MainWindow::applySettings()
{
}

void MainWindow::handleMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		qDebug("MainWindow::handleMessages: message: %s", message->getIdentifier());
		delete message;
	}
}

void MainWindow::on_action_View_Fullscreen_toggled(bool checked)
{
	if(checked)
		showFullScreen();
	else showNormal();
}

void MainWindow::on_presetSave_clicked()
{
	QStringList groups;
	QString group;
	QString description = "";
	for(int i = 0; i < ui->presetTree->topLevelItemCount(); i++)
		groups.append(ui->presetTree->topLevelItem(i)->text(0));

	QTreeWidgetItem* item = ui->presetTree->currentItem();
	if(item != 0) {
		if(item->type() == PGroup)
			group = item->text(0);
		else if(item->type() == PItem) {
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
}

void MainWindow::on_presetUpdate_clicked()
{
	QTreeWidgetItem* item = ui->presetTree->currentItem();

	if(item != 0) {
		if(item->type() == PItem) {
			const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));
			if (preset != 0) {
				Preset* preset_mod = const_cast<Preset*>(preset);
				savePresetSettings(preset_mod, ui->tabInputsView->currentIndex());
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
	if(item == 0) {
		updatePresetControls();
		return;
	}
	const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));
	if(preset == 0)
		return;

	if(QMessageBox::question(this, tr("Delete Preset"), tr("Do you want to delete preset '%1'?").arg(preset->getDescription()), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
		delete item;
		m_settings.deletePreset(preset);
	}
}

void MainWindow::on_presetTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
	updatePresetControls();
}

void MainWindow::on_presetTree_itemActivated(QTreeWidgetItem *item, int column)
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
	AudioDialog audioDialog(m_audioDeviceInfo, this);
	audioDialog.exec();
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

void MainWindow::on_sampleSource_confirmClicked(bool checked)
{
    // Do it in the currently selected source tab
    int currentSourceTabIndex = ui->tabInputsSelect->currentIndex();

    if (currentSourceTabIndex >= 0)
    {
        qDebug("MainWindow::on_sampleSource_currentIndexChanged: tab at %d", currentSourceTabIndex);
        DeviceUISet *deviceUI = m_deviceUIs[currentSourceTabIndex];
        deviceUI->m_deviceAPI->saveSourceSettings(m_settings.getWorkingPreset());
        int selectedComboIndex = deviceUI->m_samplingDeviceControl->getDeviceSelector()->currentIndex();
        void *devicePtr = deviceUI->m_samplingDeviceControl->getDeviceSelector()->itemData(selectedComboIndex).value<void *>();
        m_pluginManager->selectSampleSourceByDevice(devicePtr, deviceUI->m_deviceAPI);
        m_settings.setSourceIndex(deviceUI->m_samplingDeviceControl->getDeviceSelector()->currentIndex());
        deviceUI->m_deviceAPI->loadSourceSettings(m_settings.getWorkingPreset());
    }
}

void MainWindow::on_action_About_triggered()
{
	AboutDialog dlg(this);
	dlg.exec();
}

void MainWindow::on_action_addDevice_triggered()
{
    addDevice();
}

void MainWindow::on_action_removeDevice_triggered()
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

MainWindow::DeviceUISet::DeviceUISet(QTimer& timer)
{
	m_spectrum = new GLSpectrum;
	m_spectrumVis = new SpectrumVis(m_spectrum);
	m_spectrum->connectTimer(timer);
	m_spectrumGUI = new GLSpectrumGUI;
	m_spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, m_spectrum);
	m_channelWindow = new ChannelWindow;
	m_samplingDeviceControl = new SamplingDeviceControl;
	m_deviceEngine = 0;
	m_deviceAPI = 0;

	// m_spectrum needs to have its font to be set since it cannot be inherited from the main window
	QFont font;
    font.setFamily(QStringLiteral("Sans Serif"));
    font.setPointSize(9);
    m_spectrum->setFont(font);

}

MainWindow::DeviceUISet::~DeviceUISet()
{
	delete m_samplingDeviceControl;
	delete m_channelWindow;
	delete m_spectrumGUI;
	delete m_spectrumVis;
	delete m_spectrum;
}
