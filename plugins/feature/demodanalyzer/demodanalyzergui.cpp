///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include <QMessageBox>
#include <QFileDialog>

#include "feature/featureuiset.h"
#include "dsp/spectrumvis.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/glspectrum.h"
#include "gui/glscope.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "util/db.h"
#include "maincore.h"

#include "ui_demodanalyzergui.h"
#include "demodanalyzer.h"
#include "demodanalyzergui.h"

DemodAnalyzerGUI* DemodAnalyzerGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
	DemodAnalyzerGUI* gui = new DemodAnalyzerGUI(pluginAPI, featureUISet, feature);
	return gui;
}

void DemodAnalyzerGUI::destroy()
{
	delete this;
}

void DemodAnalyzerGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	applySettings(true);
}

QByteArray DemodAnalyzerGUI::serialize() const
{
    return m_settings.serialize();
}

bool DemodAnalyzerGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        displaySettings();
        applySettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool DemodAnalyzerGUI::handleMessage(const Message& message)
{
    if (DemodAnalyzer::MsgConfigureDemodAnalyzer::match(message))
    {
        qDebug("DemodAnalyzerGUI::handleMessage: DemodAnalyzer::MsgConfigureDemodAnalyzer");
        const DemodAnalyzer::MsgConfigureDemodAnalyzer& cfg = (DemodAnalyzer::MsgConfigureDemodAnalyzer&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        ui->spectrumGUI->updateSettings();
        ui->scopeGUI->updateSettings();
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (DemodAnalyzer::MsgReportChannels::match(message))
    {
        qDebug("DemodAnalyzerGUI::handleMessage: DemodAnalyzer::MsgReportChannels");
        DemodAnalyzer::MsgReportChannels& report = (DemodAnalyzer::MsgReportChannels&) message;
        m_availableChannels = report.getAvailableChannels();
        updateChannelList();

        return true;
    }
    else if (DemodAnalyzer::MsgReportSampleRate::match(message))
    {
        DemodAnalyzer::MsgReportSampleRate& report = (DemodAnalyzer::MsgReportSampleRate&) message;
        int sampleRate = report.getSampleRate();
        qDebug("DemodAnalyzerGUI::handleMessage: DemodAnalyzer::MsgReportSampleRate: %d", sampleRate);
        ui->glSpectrum->setSampleRate(sampleRate/(1<<m_settings.m_log2Decim));
        m_scopeVis->setLiveRate(sampleRate/(1<<m_settings.m_log2Decim));
        displaySampleRate(sampleRate/(1<<m_settings.m_log2Decim));
        m_sampleRate = sampleRate;

        return true;
    }

	return false;
}

void DemodAnalyzerGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void DemodAnalyzerGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    RollupContents *rollupContents = getRollupContents();

    rollupContents->saveState(m_rollupState);
    applySettings();
}

DemodAnalyzerGUI::DemodAnalyzerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
	FeatureGUI(parent),
	ui(new Ui::DemodAnalyzerGUI),
	m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_sampleRate(48000),
	m_doApplySettings(true),
    m_lastFeatureState(0),
    m_selectedChannel(nullptr)
{
    m_feature = feature;
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/demodanalyzer/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_demodAnalyzer = reinterpret_cast<DemodAnalyzer*>(feature);
    m_demodAnalyzer->setMessageQueueToGUI(&m_inputMessageQueue);
    m_scopeVis = m_demodAnalyzer->getScopeVis();
    m_scopeVis->setGLScope(ui->glScope);
    m_spectrumVis = m_demodAnalyzer->getSpectrumVis();
	m_spectrumVis->setGLSpectrum(ui->glSpectrum);
    m_scopeVis->setSpectrumVis(m_spectrumVis);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(1000);

	ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);
	ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);
    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(m_sampleRate/(1<<m_settings.m_log2Decim));
    m_scopeVis->setLiveRate(m_sampleRate/(1<<m_settings.m_log2Decim));
    displaySampleRate(m_sampleRate/(1<<m_settings.m_log2Decim));

	ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setScopeGUI(ui->scopeGUI);
    m_settings.setRollupState(&m_rollupState);

    displaySettings();
	applySettings(true);
    makeUIConnections();
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
    m_demodAnalyzer->getAvailableChannelsReport();
}

DemodAnalyzerGUI::~DemodAnalyzerGUI()
{
	delete ui;
}

void DemodAnalyzerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void DemodAnalyzerGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void DemodAnalyzerGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->log2Decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->record->setChecked(m_settings.m_recordToFile);
    ui->fileNameText->setText(m_settings.m_fileRecordName);
    ui->showFileDialog->setEnabled(!m_settings.m_recordToFile);
    ui->recordSilenceTime->setValue(m_settings.m_recordSilenceTime);
    ui->recordSilenceText->setText(tr("%1").arg(m_settings.m_recordSilenceTime / 10.0, 0, 'f', 1));
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void DemodAnalyzerGUI::displaySampleRate(int sampleRate)
{
	QString s = QString::number(sampleRate/1000.0, 'f', 1);
	ui->sinkSampleRateText->setText(tr("%1 kS/s").arg(s));
}

void DemodAnalyzerGUI::updateChannelList()
{
    ui->channels->blockSignals(true);
    ui->channels->clear();

    AvailableChannelOrFeatureList::const_iterator it = m_availableChannels.begin();
    int selectedItem = -1;

    for (int i = 0; it != m_availableChannels.end(); ++it, i++)
    {
        ui->channels->addItem(it->getLongId());

        if (it->m_object == m_selectedChannel) {
            selectedItem = i;
        }
    }

    ui->channels->blockSignals(false);

    if (m_availableChannels.size() > 0)
    {
        if (selectedItem >= 0) {
            ui->channels->setCurrentIndex(selectedItem);
        } else {
            ui->channels->setCurrentIndex(0);
        }
    }
}

void DemodAnalyzerGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
    {
        BasicFeatureSettingsDialog dialog(this);
        dialog.setTitle(m_settings.m_title);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIFeatureSetIndex(m_settings.m_reverseAPIFeatureSetIndex);
        dialog.setReverseAPIFeatureIndex(m_settings.m_reverseAPIFeatureIndex);
        dialog.setDefaultTitle(m_displayedName);

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        m_settingsKeys.append("title");
        m_settingsKeys.append("rgbColor");
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIFeatureSetIndex");
        m_settingsKeys.append("reverseAPIFeatureIndex");

        applySettings();
    }

    resetContextMenuType();
}

void DemodAnalyzerGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        DemodAnalyzer::MsgStartStop *message = DemodAnalyzer::MsgStartStop::create(checked);
        m_demodAnalyzer->getInputMessageQueue()->push(message);
    }
}

void DemodAnalyzerGUI::on_channels_currentIndexChanged(int index)
{
    if ((index >= 0) && (index < m_availableChannels.size()))
    {
        m_selectedChannel = qobject_cast<ChannelAPI*>(m_availableChannels[index].m_object);
        DemodAnalyzer::MsgSelectChannel *msg = DemodAnalyzer::MsgSelectChannel::create(m_selectedChannel);
        m_demodAnalyzer->getInputMessageQueue()->push(msg);
    }
}

void DemodAnalyzerGUI::on_channelApply_clicked()
{
    if (ui->channels->count() > 0) {
        on_channels_currentIndexChanged(ui->channels->currentIndex());
    }
}

void DemodAnalyzerGUI::on_log2Decim_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 6)) {
        return;
    }

    m_settings.m_log2Decim = index;
    ui->glSpectrum->setSampleRate(m_sampleRate/(1<<m_settings.m_log2Decim));
    m_scopeVis->setLiveRate(m_sampleRate/(1<<m_settings.m_log2Decim));
    displaySampleRate(m_sampleRate/(1<<m_settings.m_log2Decim));
    m_settingsKeys.append("log2Decim");
    applySettings();
}

void DemodAnalyzerGUI::on_record_toggled(bool checked)
{
    ui->showFileDialog->setEnabled(!checked);
    m_settings.m_recordToFile = checked;
    m_settingsKeys.append("recordToFile");
    applySettings();
}

void DemodAnalyzerGUI::on_showFileDialog_clicked(bool checked)
{
    (void) checked;
    QFileDialog fileDialog(
        this,
        tr("Save record file"),
        m_settings.m_fileRecordName,
        tr("WAV Files (*.wav)")
    );

    fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
    fileDialog.setFileMode(QFileDialog::AnyFile);
    QStringList fileNames;

    if (fileDialog.exec())
    {
        fileNames = fileDialog.selectedFiles();

        if (fileNames.size() > 0)
        {
            m_settings.m_fileRecordName = fileNames.at(0);
		    ui->fileNameText->setText(m_settings.m_fileRecordName);
            m_settingsKeys.append("fileRecordName");
            applySettings();
        }
    }
}

void DemodAnalyzerGUI::on_recordSilenceTime_valueChanged(int value)
{
    m_settings.m_recordSilenceTime = value;
    ui->recordSilenceText->setText(tr("%1").arg(value / 10.0, 0, 'f', 1));
    m_settingsKeys.append("recordSilenceTime");
    applySettings();
}

void DemodAnalyzerGUI::tick()
{
	m_channelPowerAvg(m_demodAnalyzer->getMagSqAvg());
	double powDb = CalcDb::dbPower((double) m_channelPowerAvg);
	ui->channelPower->setText(tr("%1 dB").arg(powDb, 0, 'f', 1));
}

void DemodAnalyzerGUI::updateStatus()
{
    int state = m_demodAnalyzer->getState();

    if (m_lastFeatureState != state)
    {
        switch (state)
        {
            case Feature::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case Feature::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case Feature::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case Feature::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_demodAnalyzer->getErrorMessage());
                break;
            default:
                break;
        }

        m_lastFeatureState = state;
    }
}

void DemodAnalyzerGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
	    DemodAnalyzer::MsgConfigureDemodAnalyzer* message = DemodAnalyzer::MsgConfigureDemodAnalyzer::create( m_settings, m_settingsKeys, force);
	    m_demodAnalyzer->getInputMessageQueue()->push(message);
	}

    m_settingsKeys.clear();
}

void DemodAnalyzerGUI::makeUIConnections()
{
	QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &DemodAnalyzerGUI::on_startStop_toggled);
	QObject::connect(ui->channels, qOverload<int>(&QComboBox::currentIndexChanged), this, &DemodAnalyzerGUI::on_channels_currentIndexChanged);
	QObject::connect(ui->channelApply, &QPushButton::clicked, this, &DemodAnalyzerGUI::on_channelApply_clicked);
	QObject::connect(ui->log2Decim, qOverload<int>(&QComboBox::currentIndexChanged), this, &DemodAnalyzerGUI::on_log2Decim_currentIndexChanged);
    QObject::connect(ui->record, &ButtonSwitch::toggled, this, &DemodAnalyzerGUI::on_record_toggled);
    QObject::connect(ui->showFileDialog, &QPushButton::clicked, this, &DemodAnalyzerGUI::on_showFileDialog_clicked);
    QObject::connect(ui->recordSilenceTime, &QSlider::valueChanged, this, &DemodAnalyzerGUI::on_recordSilenceTime_valueChanged);
}
