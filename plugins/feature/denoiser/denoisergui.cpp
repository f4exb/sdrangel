///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "dsp/dspengine.h"
#include "util/db.h"
#include "maincore.h"

#include "ui_denoisergui.h"
#include "denoiser.h"
#include "denoisergui.h"

DenoiserGUI* DenoiserGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
	DenoiserGUI* gui = new DenoiserGUI(pluginAPI, featureUISet, feature);
	return gui;
}

void DenoiserGUI::destroy()
{
	delete this;
}

void DenoiserGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	applySettings(true);
}

QByteArray DenoiserGUI::serialize() const
{
    return m_settings.serialize();
}

bool DenoiserGUI::deserialize(const QByteArray& data)
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

bool DenoiserGUI::handleMessage(const Message& message)
{
    if (Denoiser::MsgConfigureDenoiser::match(message))
    {
        qDebug("DenoiserGUI::handleMessage: Denoiser::MsgConfigureDenoiser");
        const Denoiser::MsgConfigureDenoiser& cfg = (Denoiser::MsgConfigureDenoiser&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (Denoiser::MsgReportChannels::match(message))
    {
        qDebug("DenoiserGUI::handleMessage: Denoiser::MsgReportChannels");
        Denoiser::MsgReportChannels& report = (Denoiser::MsgReportChannels&) message;
        m_availableChannels = report.getAvailableChannels();
        updateChannelList();

        return true;
    }
    else if (Denoiser::MsgReportSampleRate::match(message))
    {
        Denoiser::MsgReportSampleRate& report = (Denoiser::MsgReportSampleRate&) message;
        int sampleRate = report.getSampleRate();
        qDebug("DenoiserGUI::handleMessage: Denoiser::MsgReportSampleRate: %d", sampleRate);
        displaySampleRate(sampleRate);
        m_sampleRate = sampleRate;

        return true;
    }

	return false;
}

void DenoiserGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void DenoiserGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    RollupContents *rollupContents = getRollupContents();

    rollupContents->saveState(m_rollupState);
    applySettings();
}

DenoiserGUI::DenoiserGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
	FeatureGUI(parent),
	ui(new Ui::DenoiserGUI),
	m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_sampleRate(48000),
	m_doApplySettings(true),
    m_lastFeatureState(0),
    m_selectedChannel(nullptr)
{
    m_feature = feature;
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/denoiser/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_denoiser = reinterpret_cast<Denoiser*>(feature);
    m_denoiser->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
	connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(1000);

    displaySampleRate(m_sampleRate);

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_settings.setRollupState(&m_rollupState);

    displaySettings();
	applySettings(true);
    makeUIConnections();
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
    m_denoiser->getAvailableChannelsReport();
}

DenoiserGUI::~DenoiserGUI()
{
	delete ui;
}

void DenoiserGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void DenoiserGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void DenoiserGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->record->setChecked(m_settings.m_recordToFile);
    ui->fileNameText->setText(m_settings.m_fileRecordName);
    ui->showFileDialog->setEnabled(!m_settings.m_recordToFile);
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void DenoiserGUI::displaySampleRate(int sampleRate)
{
	QString s = QString::number(sampleRate/1000.0, 'f', 1);
	ui->sinkSampleRateText->setText(tr("%1 kS/s").arg(s));
}

void DenoiserGUI::updateChannelList()
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

void DenoiserGUI::onMenuDialogCalled(const QPoint &p)
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

void DenoiserGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        Denoiser::MsgStartStop *message = Denoiser::MsgStartStop::create(checked);
        m_denoiser->getInputMessageQueue()->push(message);
    }
}

void DenoiserGUI::on_channels_currentIndexChanged(int index)
{
    if ((index >= 0) && (index < m_availableChannels.size()))
    {
        m_selectedChannel = qobject_cast<ChannelAPI*>(m_availableChannels[index].m_object);
        Denoiser::MsgSelectChannel *msg = Denoiser::MsgSelectChannel::create(m_selectedChannel);
        m_denoiser->getInputMessageQueue()->push(msg);
    }
}

void DenoiserGUI::on_channelApply_clicked()
{
    if (ui->channels->count() > 0) {
        on_channels_currentIndexChanged(ui->channels->currentIndex());
    }
}

void DenoiserGUI::on_record_toggled(bool checked)
{
    ui->showFileDialog->setEnabled(!checked);
    m_settings.m_recordToFile = checked;
    m_settingsKeys.append("recordToFile");
    applySettings();
}

void DenoiserGUI::on_showFileDialog_clicked(bool checked)
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

void DenoiserGUI::audioSelect(const QPoint& p)
{
    qDebug("DenoiserGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        m_settingsKeys.append("audioDeviceName");
        applySettings();
    }
}


void DenoiserGUI::tick()
{
	m_channelPowerAvg(m_denoiser->getMagSqAvg());
	double powDb = CalcDb::dbPower((double) m_channelPowerAvg);
	ui->channelPower->setText(tr("%1 dB").arg(powDb, 0, 'f', 1));
}

void DenoiserGUI::updateStatus()
{
    int state = m_denoiser->getState();

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
                QMessageBox::information(this, tr("Message"), m_denoiser->getErrorMessage());
                break;
            default:
                break;
        }

        m_lastFeatureState = state;
    }
}

void DenoiserGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
	    Denoiser::MsgConfigureDenoiser* message = Denoiser::MsgConfigureDenoiser::create( m_settings, m_settingsKeys, force);
	    m_denoiser->getInputMessageQueue()->push(message);
	}

    m_settingsKeys.clear();
}

void DenoiserGUI::makeUIConnections()
{
	QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &DenoiserGUI::on_startStop_toggled);
	QObject::connect(ui->channels, qOverload<int>(&QComboBox::currentIndexChanged), this, &DenoiserGUI::on_channels_currentIndexChanged);
	QObject::connect(ui->channelApply, &QPushButton::clicked, this, &DenoiserGUI::on_channelApply_clicked);
    QObject::connect(ui->record, &ButtonSwitch::toggled, this, &DenoiserGUI::on_record_toggled);
    QObject::connect(ui->showFileDialog, &QPushButton::clicked, this, &DenoiserGUI::on_showFileDialog_clicked);
}
