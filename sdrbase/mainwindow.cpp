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
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "audio/audiodeviceinfo.h"
#include "gui/indicator.h"
#include "gui/presetitem.h"
#include "gui/addpresetdialog.h"
#include "gui/pluginsdialog.h"
#include "gui/preferencesdialog.h"
#include "gui/aboutdialog.h"
#include "gui/rollupwidget.h"
#include "gui/channelwindow.h"
#include "dsp/dspengine.h"
#include "dsp/spectrumvis.h"
#include "dsp/filesink.h"
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
	m_startOsmoSDRUpdateAfterStop(false),
	m_inputGUI(0),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_sampleFileName(std::string("./test.sdriq"))
{
	qDebug() << "MainWindow::MainWindow: start";
	m_pluginManager = new PluginManager(this, m_dspEngine);
	connect(m_dspEngine->getOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleDSPMessages()), Qt::QueuedConnection);
	m_dspEngine->start();

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
	m_statusTimer.start(500);

	m_masterTimer.start(50);

	qDebug() << "MainWindow::MainWindow: m_pluginManager->loadPlugins ...";

	m_pluginManager->loadPlugins();

	//bool sampleSourceSignalsBlocked = ui->sampleSource->blockSignals(true);
	//m_pluginManager->fillSampleSourceSelector(ui->sampleSource);
	//ui->sampleSource->blockSignals(sampleSourceSignalsBlocked);

	//m_rxSpectrumVis = new SpectrumVis(ui->rxSpectrum);
	//ui->rxSpectrum->connectTimer(m_masterTimer);
	//ui->rxSpectrumGUI->setBuddies(m_rxSpectrumVis->getInputMessageQueue(), m_rxSpectrumVis, ui->rxSpectrum);
	//m_dspEngine->

	// TODO: This will go in a create new device tab method:

	m_deviceUIs.push_back(new DeviceUISet(m_masterTimer));

	ui->tabSpectra->addTab(m_deviceUIs.back()->m_spectrum, "X0");
	ui->tabSpectraGUI->addTab(m_deviceUIs.back()->m_spectrumGUI, "X0");
	m_dspEngine->addSink(m_deviceUIs.back()->m_spectrumVis);
	ui->tabChannels->addTab(m_deviceUIs.back()->m_channelWindow, "X0");
	bool sampleSourceSignalsBlocked = m_deviceUIs.back()->m_sampleSource->blockSignals(true);
	m_pluginManager->fillSampleSourceSelector(m_deviceUIs.back()->m_sampleSource);
	connect(m_deviceUIs.back()->m_sampleSource, SIGNAL(currentIndexChanged(int)), this, SLOT(on_sampleSource_currentIndexChanged(int)));
	m_deviceUIs.back()->m_sampleSource->blockSignals(sampleSourceSignalsBlocked);
	ui->tabInputs->addTab(m_deviceUIs.back()->m_sampleSource, "X0");

	m_fileSink = new FileSink();
	m_dspEngine->addSink(m_fileSink);

	qDebug() << "MainWindow::MainWindow: loadSettings...";

	loadSettings();

	qDebug() << "MainWindow::MainWindow: select SampleSource from settings...";

	int sampleSourceIndex = m_settings.getSourceIndex();
	sampleSourceIndex = m_pluginManager->selectSampleSourceByIndex(sampleSourceIndex);

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
	m_dspEngine->stopAcquistion();

	saveSettings();

	m_pluginManager->freeAll();

	for (int i = 0; i < m_deviceUIs.size(); i++)
	{
		delete m_deviceUIs[i];
	}

	m_dspEngine->removeSink(m_fileSink);
	//m_dspEngine->removeSink(m_rxSpectrumVis);
	delete m_fileSink;
	//delete m_rxSpectrumVis;
	delete m_pluginManager;

	m_dspEngine->stop();

	delete ui;
}

void MainWindow::addChannelCreateAction(QAction* action)
{
	ui->menu_Channels->addAction(action);
}

void MainWindow::addChannelRollup(QWidget* widget)
{
	m_deviceUIs.back()->m_channelWindow->addRollupWidget(widget);
	//((ChannelWindow*)ui->rxChannels)->addRollupWidget(widget);
	//((ChannelWindow*)ui->channelDock->widget())->addRollupWidget(widget);
	ui->channelDock->show();
	ui->channelDock->raise();
}

void MainWindow::addViewAction(QAction* action)
{
	ui->menu_Window->addAction(action);
}

void MainWindow::addChannelMarker(ChannelMarker* channelMarker)
{
	//ui->rxSpectrum->addChannelMarker(channelMarker);
	m_deviceUIs.back()->m_spectrum->addChannelMarker(channelMarker);
}

void MainWindow::removeChannelMarker(ChannelMarker* channelMarker)
{
	//ui->rxSpectrum->removeChannelMarker(channelMarker);
	m_deviceUIs.back()->m_spectrum->removeChannelMarker(channelMarker);
}

void MainWindow::setInputGUI(QWidget* gui)
{
	if(m_inputGUI != 0)
		ui->inputDock->widget()->layout()->removeWidget(m_inputGUI);
	if(gui != 0)
		ui->inputDock->widget()->layout()->addWidget(gui);
	m_inputGUI = gui;
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

	//ui->rxSpectrumGUI->deserialize(preset->getSpectrumConfig());
	m_deviceUIs.back()->m_spectrumGUI->deserialize(preset->getSpectrumConfig());
	m_pluginManager->loadSettings(preset);

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

	//preset->setSpectrumConfig(ui->rxSpectrumGUI->serialize());
	preset->setSpectrumConfig(m_deviceUIs.back()->m_spectrumGUI->serialize());
	preset->clearChannels();
    m_pluginManager->saveSettings(preset);

    preset->setLayout(saveState());
}

void MainWindow::createStatusBar()
{
	m_sampleRateWidget = new QLabel(tr("Rate: 0 kHz"), this);
	m_sampleRateWidget->setToolTip(tr("Sample Rate"));
	statusBar()->addPermanentWidget(m_sampleRateWidget);

	m_recording = new Indicator(tr("Rec"), this);
	m_recording->setToolTip(tr("Recording"));
	statusBar()->addPermanentWidget(m_recording);

	m_engineIdle = new Indicator(tr("Idle"), this);
	m_engineIdle->setToolTip(tr("DSP engine is idle"));
	statusBar()->addPermanentWidget(m_engineIdle);

	m_engineRunning = new Indicator(tr("Run"), this);
	m_engineRunning->setToolTip(tr("DSP engine is running"));
	statusBar()->addPermanentWidget(m_engineRunning);

	m_engineError = new Indicator(tr("Err"), this);
	m_engineError->setToolTip(tr("DSP engine failed"));
	statusBar()->addPermanentWidget(m_engineError);
}

void MainWindow::closeEvent(QCloseEvent*)
{
}

void MainWindow::updateCenterFreqDisplay()
{
	//ui->rxSpectrum->setCenterFrequency(m_centerFrequency);
	m_deviceUIs.back()->m_spectrum->setCenterFrequency(m_centerFrequency);
}

void MainWindow::updateSampleRate()
{
	//ui->rxSpectrum->setSampleRate(m_sampleRate);
	m_deviceUIs.back()->m_spectrum->setSampleRate(m_sampleRate);
	m_sampleRateWidget->setText(tr("Rate: %1 kHz").arg((float)m_sampleRate / 1000));
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
	updateCenterFreqDisplay();
	updateSampleRate();
}

void MainWindow::handleDSPMessages()
{
	Message* message;

	while ((message = m_dspEngine->getOutputMessageQueue()->pop()) != 0)
	{
		qDebug("Message: %s", message->getIdentifier());

		std::cerr << "MainWindow::handleDSPMessages: " << message->getIdentifier() << std::endl;

		if (DSPSignalNotification::match(*message))
		{
			DSPSignalNotification* notif = (DSPSignalNotification*) message;
			m_sampleRate = notif->getSampleRate();
			m_centerFrequency = notif->getCenterFrequency();
			qDebug("SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
			updateCenterFreqDisplay();
			updateSampleRate();
			qDebug() << "MainWindow::handleDSPMessages: forward to file sink";
			m_fileSink->handleMessage(*notif);

			delete message;
		}
	}
}

void MainWindow::handleMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		qDebug("Message: %s", message->getIdentifier());
		std::cerr << "MainWindow::handleMessages: " << message->getIdentifier() << std::endl;

		if (!m_pluginManager->handleMessage(*message))
		{
			delete message;
		}
	}
}

void MainWindow::updateStatus()
{
	int state = m_dspEngine->state();
	if(m_lastEngineState != state) {
		switch(state) {
			case DSPDeviceEngine::StNotStarted:
				m_engineIdle->setColor(Qt::gray);
				m_engineRunning->setColor(Qt::gray);
				m_engineError->setColor(Qt::gray);
				statusBar()->clearMessage();
				break;

			case DSPDeviceEngine::StIdle:
				m_engineIdle->setColor(Qt::cyan);
				m_engineRunning->setColor(Qt::gray);
				m_engineError->setColor(Qt::gray);
				statusBar()->clearMessage();
				break;

			case DSPDeviceEngine::StRunning:
				m_engineIdle->setColor(Qt::gray);
				m_engineRunning->setColor(Qt::green);
				m_engineError->setColor(Qt::gray);
				statusBar()->showMessage(tr("Sampling from %1").arg(m_dspEngine->sourceDeviceDescription()));
				break;

			case DSPDeviceEngine::StError:
				m_engineIdle->setColor(Qt::gray);
				m_engineRunning->setColor(Qt::gray);
				m_engineError->setColor(Qt::red);
				statusBar()->showMessage(tr("Error: %1").arg(m_dspEngine->errorMessage()));
				break;
		}
		m_lastEngineState = state;
	}
}

void MainWindow::on_action_Start_triggered()
{
	if (m_dspEngine->initAcquisition())
	{
		m_dspEngine->startAcquisition();
	}
}

void MainWindow::on_action_Stop_triggered()
{
	m_dspEngine->stopAcquistion();
}

void MainWindow::on_action_Start_Recording_triggered()
{
	m_recording->setColor(Qt::red);
	m_fileSink->startRecording();
}

void MainWindow::on_action_Stop_Recording_triggered()
{
	m_recording->setColor(Qt::gray);
	m_fileSink->stopRecording();
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

void MainWindow::on_action_Loaded_Plugins_triggered()
{
	PluginsDialog pluginsDialog(m_pluginManager, this);

	pluginsDialog.exec();
}

void MainWindow::on_action_Preferences_triggered()
{
	PreferencesDialog preferencesDialog(m_audioDeviceInfo, this);

	preferencesDialog.exec();
}

void MainWindow::on_sampleSource_currentIndexChanged(int index)
{
	m_pluginManager->saveSourceSettings(m_settings.getWorkingPreset());
	//m_pluginManager->selectSampleSourceByIndex(ui->sampleSource->currentIndex());
	//m_settings.setSourceIndex(ui->sampleSource->currentIndex());
	m_pluginManager->selectSampleSourceByIndex(m_deviceUIs.back()->m_sampleSource->currentIndex());
	m_settings.setSourceIndex(m_deviceUIs.back()->m_sampleSource->currentIndex());
	m_pluginManager->loadSourceSettings(m_settings.getWorkingPreset());
}

void MainWindow::on_action_About_triggered()
{
	AboutDialog dlg(this);
	dlg.exec();
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
