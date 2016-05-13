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
#include <QComboBox>
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
#include "dsp/dspengine.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspcommands.h"
#include "plugin/plugingui.h"
#include "plugin/pluginapi.h"
#include "plugin/plugingui.h"

#include "gui/glspectrum.h"
#include "gui/glspectrumgui.h"

#include <string>
#include <QDebug>

MainWindow::MainWindow(QWidget* parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow),
	m_audioDeviceInfo(new AudioDeviceInfo),
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
	removeDockWidget(ui->inputDock);
	removeDockWidget(ui->spectraDisplayDock);
	removeDockWidget(ui->presetDock);
	removeDockWidget(ui->channelDock);
	addDockWidget(Qt::LeftDockWidgetArea, ui->inputDock);
	addDockWidget(Qt::LeftDockWidgetArea, ui->spectraDisplayDock);
	addDockWidget(Qt::LeftDockWidgetArea, ui->presetDock);
	addDockWidget(Qt::RightDockWidgetArea, ui->channelDock);

	ui->inputDock->show();
	ui->spectraDisplayDock->show();
	ui->presetDock->show();
	ui->channelDock->show();

	ui->menu_Window->addAction(ui->inputDock->toggleViewAction());
	ui->menu_Window->addAction(ui->spectraDisplayDock->toggleViewAction());
	ui->menu_Window->addAction(ui->presetDock->toggleViewAction());
	ui->menu_Window->addAction(ui->channelDock->toggleViewAction());

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()), Qt::QueuedConnection);

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(1000);

	m_masterTimer.start(50);

	qDebug() << "MainWindow::MainWindow: m_pluginManager->loadPlugins ...";

    addDevice(); // add the first device

//    DSPDeviceEngine *dspDeviceEngine = m_dspEngine->addDeviceEngine();
//    dspDeviceEngine->start();
//
//    m_deviceUIs.push_back(new DeviceUISet(m_masterTimer));
//
//    m_pluginManager = new PluginManager(this, dspDeviceEngine, m_deviceUIs.back()->m_spectrum);
//	m_pluginManager->loadPlugins();
//
//	ui->tabSpectra->addTab(m_deviceUIs.back()->m_spectrum, "X0");
//	ui->tabSpectraGUI->addTab(m_deviceUIs.back()->m_spectrumGUI, "X0");
//	dspDeviceEngine->addSink(m_deviceUIs.back()->m_spectrumVis);
//	ui->tabChannels->addTab(m_deviceUIs.back()->m_channelWindow, "X0");
//	bool sampleSourceSignalsBlocked = m_deviceUIs.back()->m_sampleSource->blockSignals(true);
//	m_pluginManager->fillSampleSourceSelector(m_deviceUIs.back()->m_sampleSource);
//	connect(m_deviceUIs.back()->m_sampleSource, SIGNAL(currentIndexChanged(int)), this, SLOT(on_sampleSource_currentIndexChanged(int)));
//	m_deviceUIs.back()->m_sampleSource->blockSignals(sampleSourceSignalsBlocked);
//	ui->tabInputs->addTab(m_deviceUIs.back()->m_sampleSource, "X0");

	qDebug() << "MainWindow::MainWindow: loadSettings...";

	loadSettings();

	qDebug() << "MainWindow::MainWindow: select SampleSource from settings...";

	int sampleSourceIndex = m_settings.getSourceIndex();
	sampleSourceIndex = m_deviceUIs.back()->m_pluginManager->selectSampleSourceByIndex(sampleSourceIndex);

	if (sampleSourceIndex >= 0)
	{
		//bool sampleSourceSignalsBlocked = ui->sampleSource->blockSignals(true);
		//ui->sampleSource->setCurrentIndex(sampleSourceIndex);
		//ui->sampleSource->blockSignals(sampleSourceSignalsBlocked);

		bool sampleSourceSignalsBlocked = m_deviceUIs.back()->m_sampleSource->blockSignals(true);
		m_deviceUIs.back()->m_sampleSource->setCurrentIndex(sampleSourceIndex);
		m_deviceUIs.back()->m_sampleSource->blockSignals(sampleSourceSignalsBlocked);
	}

	qDebug() << "MainWindow::MainWindow: load current preset settings...";

	loadPresetSettings(m_settings.getWorkingPreset());

	qDebug() << "MainWindow::MainWindow: apply settings...";

	applySettings();

	qDebug() << "MainWindow::MainWindow: update preset controls...";

	updatePresetControls();

	qDebug() << "MainWindow::MainWindow: end";
}

MainWindow::~MainWindow()
{
    saveSettings();

    while (m_deviceUIs.size() > 0)
    {
        removeLastDevice();
    }

//	m_dspEngine->stopAllAcquisitions(); // FIXME: also present in m_pluginManager->freeAll()
//	//m_pluginManager->freeAll();
//    for (int i = 0; i < m_deviceUIs.size(); i++)
//    {
//        m_deviceUIs[i]->m_pluginManager->freeAll();
//        delete m_deviceUIs[i];
//    }
//
//    m_dspEngine->stopAllDeviceEngines();
//
//    //delete m_pluginManager;
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

    PluginManager *pluginManager = new PluginManager(this, m_deviceUIs.size()-1, dspDeviceEngine, m_deviceUIs.back()->m_spectrum);
    m_deviceUIs.back()->m_pluginManager = pluginManager;
    pluginManager->loadPlugins();

    dspDeviceEngine->addSink(m_deviceUIs.back()->m_spectrumVis);
    ui->tabSpectra->addTab(m_deviceUIs.back()->m_spectrum, tabNameCStr);
    ui->tabSpectraGUI->addTab(m_deviceUIs.back()->m_spectrumGUI, tabNameCStr);
    ui->tabChannels->addTab(m_deviceUIs.back()->m_channelWindow, tabNameCStr);

    bool sampleSourceSignalsBlocked = m_deviceUIs.back()->m_sampleSource->blockSignals(true);
    pluginManager->fillSampleSourceSelector(m_deviceUIs.back()->m_sampleSource);
    connect(m_deviceUIs.back()->m_sampleSource, SIGNAL(currentIndexChanged(int)), this, SLOT(on_sampleSource_currentIndexChanged(int)));
    m_deviceUIs.back()->m_sampleSource->blockSignals(sampleSourceSignalsBlocked);
    ui->tabInputs->addTab(m_deviceUIs.back()->m_sampleSource, tabNameCStr);

//    if (dspDeviceEngineUID == 0)
//    {
//        m_pluginManager = pluginManager;
//    }
}

void MainWindow::removeLastDevice()
{
    DSPDeviceEngine *lastDeviceEngine = m_deviceUIs.back()->m_deviceEngine;
    lastDeviceEngine->stopAcquistion();
    lastDeviceEngine->removeSink(m_deviceUIs.back()->m_spectrumVis);

    ui->tabChannels->removeTab(ui->tabChannels->count() - 1);
    ui->tabSpectraGUI->removeTab(ui->tabSpectraGUI->count() - 1);
    ui->tabSpectra->removeTab(ui->tabSpectra->count() - 1);

    // PluginManager destructor does freeAll() which does stopAcquistion() but stopAcquistion()
    // can be done several times only the first is active so it is fine to do it here
    // On the other hand freeAll() must be executed only once
    delete m_deviceUIs.back()->m_pluginManager;
    //m_deviceUIs.back()->m_pluginManager->freeAll();

    lastDeviceEngine->stop();
    m_dspEngine->removeLastDeviceEngine();

    ui->tabInputs->removeTab(ui->tabInputs->count() - 1);

    delete m_deviceUIs.back();
    m_deviceUIs.pop_back();
}

void MainWindow::addChannelCreateAction(QAction* action)
{
	ui->menu_Channels->addAction(action);
}

void MainWindow::addChannelRollup(QWidget* widget)
{
	m_deviceUIs.back()->m_channelWindow->addRollupWidget(widget);
	ui->channelDock->show();
	ui->channelDock->raise();
}

void MainWindow::addViewAction(QAction* action)
{
	ui->menu_Window->addAction(action);
}

void MainWindow::addChannelMarker(ChannelMarker* channelMarker)
{
	m_deviceUIs.back()->m_spectrum->addChannelMarker(channelMarker);
}

void MainWindow::removeChannelMarker(ChannelMarker* channelMarker)
{
	m_deviceUIs.back()->m_spectrum->removeChannelMarker(channelMarker);
}

//void MainWindow::setInputGUI(QWidget* gui)
//{
//    // FIXME: Ceci est un tres tres gros CACA!
//	if(m_inputGUI != 0)
//		ui->inputDock->widget()->layout()->removeWidget(m_inputGUI);
//	if(gui != 0)
//		ui->inputDock->widget()->layout()->addWidget(gui);
//	m_inputGUI = gui;
//}

void MainWindow::setInputGUI(int deviceTabIndex, QWidget* gui)
{
    ui->stackInputs->addWidget(gui);
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

void MainWindow::loadPresetSettings(const Preset* preset)
{
	qDebug("MainWindow::loadPresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

	// Load into currently selected source tab
	int currentSourceTabIndex = ui->tabInputs->currentIndex();

	if (currentSourceTabIndex >= 0)
	{
        DeviceUISet *deviceUI = m_deviceUIs[currentSourceTabIndex];
        deviceUI->m_spectrumGUI->deserialize(preset->getSpectrumConfig());
        deviceUI->m_pluginManager->loadSettings(preset);
	}

//	m_deviceUIs.back()->m_spectrumGUI->deserialize(preset->getSpectrumConfig());
//	m_pluginManager->loadSettings(preset);

	// has to be last step
	restoreState(preset->getLayout());
}

void MainWindow::saveSettings()
{
	qDebug() << "MainWindow::saveSettings";

	savePresetSettings(m_settings.getWorkingPreset());
	m_settings.save();
}

void MainWindow::savePresetSettings(Preset* preset)
{
	qDebug("MainWindow::savePresetSettings: preset [%s | %s]",
		qPrintable(preset->getGroup()),
		qPrintable(preset->getDescription()));

	preset->setSpectrumConfig(m_deviceUIs.back()->m_spectrumGUI->serialize());
	preset->clearChannels();

    // Save from currently selected source tab
    int currentSourceTabIndex = ui->tabInputs->currentIndex();

    if (currentSourceTabIndex >= 0)
    {
        DeviceUISet *deviceUI = m_deviceUIs[currentSourceTabIndex];
        deviceUI->m_pluginManager->saveSettings(preset);
    }

    preset->setLayout(saveState());
}

void MainWindow::createStatusBar()
{
    m_showSystemWidget = new QLabel("SDRangel v2.0.0 " + QSysInfo::prettyProductName(), this);
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
//
//		if (!m_pluginManager->handleMessage(*message))
//		{
//			delete message;
//		}
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
		savePresetSettings(preset);

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
				savePresetSettings(preset_mod);
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
	saveSettings();
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

	loadPresetSettings(preset);
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

//void MainWindow::on_action_Loaded_Plugins_triggered()
//{
//	PluginsDialog pluginsDialog(m_pluginManager, this);
//
//	pluginsDialog.exec();
//}

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

void MainWindow::on_sampleSource_currentIndexChanged(int index)
{
    // Do it in the currently selected source tab
    int currentSourceTabIndex = ui->tabInputs->currentIndex();

    if (currentSourceTabIndex >= 0)
    {
        DeviceUISet *deviceUI = m_deviceUIs[currentSourceTabIndex];
        deviceUI->m_pluginManager->saveSourceSettings(m_settings.getWorkingPreset());
        deviceUI->m_pluginManager->selectSampleSourceByIndex(m_deviceUIs.back()->m_sampleSource->currentIndex());
        m_settings.setSourceIndex(deviceUI->m_sampleSource->currentIndex());
        deviceUI->m_pluginManager->loadSourceSettings(m_settings.getWorkingPreset());
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
	m_sampleSource = new QComboBox;
	m_deviceEngine = 0;
	m_pluginManager = 0;

	// m_spectrum needs to have its font to be set since it cannot be inherited from the main window
	QFont font;
    font.setFamily(QStringLiteral("Sans Serif"));
    font.setPointSize(9);
    m_spectrum->setFont(font);

}

MainWindow::DeviceUISet::~DeviceUISet()
{
	delete m_sampleSource;
	delete m_channelWindow;
	delete m_spectrumGUI;
	delete m_spectrumVis;
	delete m_spectrum;
}
