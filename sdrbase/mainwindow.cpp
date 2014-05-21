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
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "audio/audiodeviceinfo.h"
#include "gui/indicator.h"
#include "gui/presetitem.h"
#include "gui/scopewindow.h"
#include "gui/addpresetdialog.h"
#include "gui/pluginsdialog.h"
#include "gui/preferencesdialog.h"
#include "gui/aboutdialog.h"
#include "gui/rollupwidget.h"
#include "dsp/dspengine.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspcommands.h"
#include "plugin/plugingui.h"
#include "plugin/pluginapi.h"
#include "plugin/plugingui.h"

MainWindow::MainWindow(QWidget* parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow),
	m_audioDeviceInfo(new AudioDeviceInfo),
	m_messageQueue(new MessageQueue),
	m_settings(),
	m_dspEngine(new DSPEngine(m_messageQueue)),
	m_lastEngineState((DSPEngine::State)-1),
	m_startOsmoSDRUpdateAfterStop(false),
	m_scopeWindow(NULL),
	m_inputGUI(NULL),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_pluginManager(new PluginManager(this, m_dspEngine))
{
	ui->setupUi(this);
	delete ui->mainToolBar;
	createStatusBar();

	setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

	// work around broken Qt dock widget ordering
	removeDockWidget(ui->inputDock);
	removeDockWidget(ui->processingDock);
	removeDockWidget(ui->presetDock);
	removeDockWidget(ui->channelDock);
	addDockWidget(Qt::LeftDockWidgetArea, ui->inputDock);
	addDockWidget(Qt::LeftDockWidgetArea, ui->processingDock);
	addDockWidget(Qt::LeftDockWidgetArea, ui->presetDock);
	addDockWidget(Qt::RightDockWidgetArea, ui->channelDock);
	ui->inputDock->show();
	ui->processingDock->show();
	ui->presetDock->show();
	ui->channelDock->show();

	ui->menu_Window->addAction(ui->inputDock->toggleViewAction());
	ui->menu_Window->addAction(ui->processingDock->toggleViewAction());
	ui->menu_Window->addAction(ui->presetDock->toggleViewAction());
	ui->menu_Window->addAction(ui->channelDock->toggleViewAction());

	connect(m_messageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()), Qt::QueuedConnection);

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	m_pluginManager->loadPlugins();
	bool sampleSourceSignalsBlocked = ui->sampleSource->blockSignals(true);
	m_pluginManager->fillSampleSourceSelector(ui->sampleSource);
	ui->sampleSource->blockSignals(sampleSourceSignalsBlocked);

	m_dspEngine->start();

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_dspEngine->addSink(m_spectrumVis);

	ui->glSpectrumGUI->setBuddies(m_dspEngine->getMessageQueue(), m_spectrumVis, ui->glSpectrum);

	loadSettings();

	int sampleSourceIndex = m_pluginManager->selectSampleSource(m_settings.getCurrent()->getSource()); // select SampleSource from settings
	if(sampleSourceIndex >= 0) {
		bool sampleSourceSignalsBlocked = ui->sampleSource->blockSignals(true);
		ui->sampleSource->setCurrentIndex(sampleSourceIndex);
		ui->sampleSource->blockSignals(sampleSourceSignalsBlocked);
	}

	loadSettings(m_settings.getCurrent());

	applySettings();
	updatePresets();
}

MainWindow::~MainWindow()
{
	m_dspEngine->stopAcquistion();

	saveSettings();

	m_pluginManager->freeAll();

	m_dspEngine->removeSink(m_spectrumVis);
	delete m_spectrumVis;

	if(m_scopeWindow != NULL) {
		delete m_scopeWindow;
		m_scopeWindow = NULL;
	}

	delete m_pluginManager;

	m_dspEngine->stop();

	delete m_dspEngine;
	delete m_messageQueue;
	delete ui;
}

void MainWindow::addChannelCreateAction(QAction* action)
{
	ui->menu_Channels->addAction(action);
}

void MainWindow::addChannelRollup(QWidget* widget)
{
	((ChannelWindow*)ui->channelDock->widget())->addRollupWidget(widget);
	ui->channelDock->show();
	ui->channelDock->raise();
}

void MainWindow::addViewAction(QAction* action)
{
	ui->menu_Window->addAction(action);
}

void MainWindow::addChannelMarker(ChannelMarker* channelMarker)
{
	ui->glSpectrum->addChannelMarker(channelMarker);
}

void MainWindow::removeChannelMarker(ChannelMarker* channelMarker)
{
	ui->glSpectrum->removeChannelMarker(channelMarker);
}

void MainWindow::setInputGUI(QWidget* gui)
{
	if(m_inputGUI != NULL)
		ui->inputDock->widget()->layout()->removeWidget(m_inputGUI);
	if(gui != NULL)
		ui->inputDock->widget()->layout()->addWidget(gui);
	m_inputGUI = gui;
}

void MainWindow::loadSettings()
{
	m_settings.load();

	for(int i = 0; i < m_settings.getPresetCount(); ++i)
		addPresetToTree(m_settings.getPreset(i));

	Preset* current = m_settings.getCurrent();

	loadSettings(current);
}

void MainWindow::loadSettings(const Preset* preset)
{
	if(preset->getShowScope()) {
		on_action_Oscilloscope_triggered();
		m_scopeWindow->deserialize(preset->getScopeConfig());
	}
	ui->glSpectrumGUI->deserialize(preset->getSpectrumConfig());
	ui->dcOffset->setChecked(preset->getDCOffsetCorrection());
	ui->iqImbalance->setChecked(preset->getIQImbalanceCorrection());

	m_pluginManager->loadSettings(preset);

	// has to be last step
	restoreState(preset->getLayout());
}

void MainWindow::saveSettings()
{
	saveSettings(m_settings.getCurrent());
	m_settings.save();
}

void MainWindow::saveSettings(Preset* preset)
{
	preset->setSpectrumConfig(ui->glSpectrumGUI->serialize());
	if(preset->getShowScope())
		preset->setScopeConfig(m_scopeWindow->serialize());
	else preset->setScopeConfig(QByteArray());

	preset->clearChannels();
	m_pluginManager->saveSettings(preset);

	preset->setLayout(saveState());
}

void MainWindow::createStatusBar()
{
	m_sampleRateWidget = new QLabel(tr("Rate: 0 kHz"), this);
	m_sampleRateWidget->setToolTip(tr("Sample Rate"));
	statusBar()->addPermanentWidget(m_sampleRateWidget);

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
	ui->glSpectrum->setCenterFrequency(m_centerFrequency);
}

void MainWindow::updateSampleRate()
{
	ui->glSpectrum->setSampleRate(m_sampleRate);
	m_sampleRateWidget->setText(tr("Rate: %1 kHz").arg((float)m_sampleRate / 1000));
	if(m_scopeWindow != NULL)
		m_scopeWindow->setSampleRate(m_sampleRate);
}

void MainWindow::updatePresets()
{
	ui->presetTree->resizeColumnToContents(0);
	if(ui->presetTree->currentItem() != NULL) {
		ui->presetDelete->setEnabled(true);
		ui->presetLoad->setEnabled(true);
	} else {
		ui->presetDelete->setEnabled(false);
		ui->presetLoad->setEnabled(false);
	}
}

QTreeWidgetItem* MainWindow::addPresetToTree(const Preset* preset)
{
	QTreeWidgetItem* group = NULL;
	for(int i = 0; i < ui->presetTree->topLevelItemCount(); i++) {
		if(ui->presetTree->topLevelItem(i)->text(0) == preset->getGroup()) {
			group = ui->presetTree->topLevelItem(i);
			break;
		}
	}
	if(group == NULL) {
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

	updatePresets();
	return item;
}

void MainWindow::applySettings()
{
	updateCenterFreqDisplay();
	updateSampleRate();
}

void MainWindow::handleMessages()
{
	Message* message;
	while((message = m_messageQueue->accept()) != NULL) {
		qDebug("Message: %s", message->getIdentifier());
		if(DSPEngineReport::match(message)) {
			DSPEngineReport* rep = (DSPEngineReport*)message;
			m_sampleRate = rep->getSampleRate();
			m_centerFrequency = rep->getCenterFrequency();
			qDebug("SampleRate:%d, CenterFrequency:%llu", rep->getSampleRate(), rep->getCenterFrequency());
			updateCenterFreqDisplay();
			updateSampleRate();
			message->completed();
		} else {
			if(!m_pluginManager->handleMessage(message))
				message->completed();
		}
	}
}

void MainWindow::updateStatus()
{
	int state = m_dspEngine->state();
	if(m_lastEngineState != state) {
		switch(state) {
			case DSPEngine::StNotStarted:
				m_engineIdle->setColor(Qt::gray);
				m_engineRunning->setColor(Qt::gray);
				m_engineError->setColor(Qt::gray);
				statusBar()->clearMessage();
				break;

			case DSPEngine::StIdle:
				m_engineIdle->setColor(Qt::cyan);
				m_engineRunning->setColor(Qt::gray);
				m_engineError->setColor(Qt::gray);
				statusBar()->clearMessage();
				if(m_startOsmoSDRUpdateAfterStop)
					on_actionOsmoSDR_Firmware_Upgrade_triggered();
				break;

			case DSPEngine::StRunning:
				m_engineIdle->setColor(Qt::gray);
				m_engineRunning->setColor(Qt::green);
				m_engineError->setColor(Qt::gray);
				statusBar()->showMessage(tr("Sampling from %1").arg(m_dspEngine->deviceDescription()));
				break;

			case DSPEngine::StError:
				m_engineIdle->setColor(Qt::gray);
				m_engineRunning->setColor(Qt::gray);
				m_engineError->setColor(Qt::red);
				statusBar()->showMessage(tr("Error: %1").arg(m_dspEngine->errorMessage()));
				if(m_startOsmoSDRUpdateAfterStop)
					on_actionOsmoSDR_Firmware_Upgrade_triggered();
				break;
		}
		m_lastEngineState = state;
	}
}

void MainWindow::scopeWindowDestroyed()
{
	ui->action_Oscilloscope->setChecked(false);
	m_settings.getCurrent()->setShowScope(false);
	m_scopeWindow = NULL;
}

void MainWindow::on_action_Start_triggered()
{
	m_dspEngine->startAcquisition();
}

void MainWindow::on_action_Stop_triggered()
{
	m_dspEngine->stopAcquistion();
}

void MainWindow::on_dcOffset_toggled(bool checked)
{
	m_settings.getCurrent()->setDCOffsetCorrection(checked);
	m_dspEngine->configureCorrections(m_settings.getCurrent()->getDCOffsetCorrection(), m_settings.getCurrent()->getIQImbalanceCorrection());
}

void MainWindow::on_iqImbalance_toggled(bool checked)
{
	m_settings.getCurrent()->setIQImbalanceCorrection(checked);
	m_dspEngine->configureCorrections(m_settings.getCurrent()->getDCOffsetCorrection(), m_settings.getCurrent()->getIQImbalanceCorrection());
}

void MainWindow::on_action_View_Fullscreen_toggled(bool checked)
{
	if(checked)
		showFullScreen();
	else showNormal();
}

void MainWindow::on_actionOsmoSDR_Firmware_Upgrade_triggered()
{
	DSPEngine::State engineState = m_dspEngine->state();
	if((engineState != DSPEngine::StIdle) && (engineState != DSPEngine::StError)) {
		m_startOsmoSDRUpdateAfterStop = true;
		m_dspEngine->stopAcquistion();
		return;
	}
	m_startOsmoSDRUpdateAfterStop = false;
/* TODO: osmosdr upgrade
	OsmoSDRUpgrade osmoSDRUpgrade;
	osmoSDRUpgrade.exec();
	*/
}

void MainWindow::on_presetSave_clicked()
{
	QStringList groups;
	QString group;
	for(int i = 0; i < ui->presetTree->topLevelItemCount(); i++)
		groups.append(ui->presetTree->topLevelItem(i)->text(0));

	QTreeWidgetItem* item = ui->presetTree->currentItem();
	if(item != NULL) {
		if(item->type() == PGroup)
			group = item->text(0);
		else if(item->type() == PItem)
			group = item->parent()->text(0);
	}

	AddPresetDialog dlg(groups, group, this);

	if(dlg.exec() == QDialog::Accepted) {
		Preset* preset = m_settings.newPreset(dlg.group(), dlg.description());
		saveSettings(preset);

		ui->presetTree->setCurrentItem(addPresetToTree(preset));
	}
}

void MainWindow::on_presetLoad_clicked()
{
	QTreeWidgetItem* item = ui->presetTree->currentItem();
	if(item == NULL) {
		updatePresets();
		return;
	}
	const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));
	if(preset == NULL)
		return;

	loadSettings(preset);
	applySettings();
}

void MainWindow::on_presetDelete_clicked()
{
	QTreeWidgetItem* item = ui->presetTree->currentItem();
	if(item == NULL) {
		updatePresets();
		return;
	}
	const Preset* preset = qvariant_cast<const Preset*>(item->data(0, Qt::UserRole));
	if(preset == NULL)
		return;

	if(QMessageBox::question(this, tr("Delete Preset"), tr("Do you want to delete preset '%1'?").arg(preset->getDescription()), QMessageBox::No | QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
		delete item;
		m_settings.deletePreset(preset);
	}
}

void MainWindow::on_presetTree_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
	updatePresets();
}

void MainWindow::on_presetTree_itemActivated(QTreeWidgetItem *item, int column)
{
	on_presetLoad_clicked();
}

void MainWindow::on_action_Oscilloscope_triggered()
{
	if(m_scopeWindow != NULL) {
		((QWidget*)m_scopeWindow->parent())->raise();
		return;
	}

	QDockWidget* dock = new QDockWidget(tr("Signalscope"), this);
	dock->setObjectName(QString::fromUtf8("scopeDock"));
	m_scopeWindow = new ScopeWindow(m_dspEngine);
	connect(m_scopeWindow, SIGNAL(destroyed()), this, SLOT(scopeWindowDestroyed()));
	m_scopeWindow->setSampleRate(m_sampleRate);
	dock->setWidget(m_scopeWindow);
	dock->setAllowedAreas(Qt::AllDockWidgetAreas);
	addDockWidget(Qt::BottomDockWidgetArea, dock);
	dock->setAttribute(Qt::WA_DeleteOnClose);
	m_settings.getCurrent()->setShowScope(true);
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
	m_pluginManager->selectSampleSource(ui->sampleSource->currentIndex());
}

void MainWindow::on_action_About_triggered()
{
	AboutDialog dlg(this);
	dlg.exec();
}
