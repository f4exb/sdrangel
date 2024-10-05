///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#include <QScrollBar>

#include "feature/featureuiset.h"
#include "dsp/spectrumvis.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/glspectrum.h"
#include "gui/glscope.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "util/db.h"
#include "maincore.h"

#include "ui_morsedecodergui.h"
#include "morsedecoder.h"
#include "morsedecodergui.h"

MorseDecoderGUI* MorseDecoderGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
	MorseDecoderGUI* gui = new MorseDecoderGUI(pluginAPI, featureUISet, feature);
	return gui;
}

void MorseDecoderGUI::destroy()
{
	delete this;
}

void MorseDecoderGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	applySettings(true);
}

QByteArray MorseDecoderGUI::serialize() const
{
    return m_settings.serialize();
}

bool MorseDecoderGUI::deserialize(const QByteArray& data)
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

bool MorseDecoderGUI::handleMessage(const Message& message)
{
    if (MorseDecoder::MsgConfigureMorseDecoder::match(message))
    {
        qDebug("MorseDecoderGUI::handleMessage: MorseDecoder::MsgConfigureMorseDecoder");
        const MorseDecoder::MsgConfigureMorseDecoder& cfg = (MorseDecoder::MsgConfigureMorseDecoder&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        ui->scopeGUI->updateSettings();
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (MorseDecoder::MsgReportChannels::match(message))
    {
        qDebug("MorseDecoderGUI::handleMessage: MorseDecoder::MsgReportChannels");
        MorseDecoder::MsgReportChannels& report = (MorseDecoder::MsgReportChannels&) message;
        m_availableChannels = report.getAvailableChannels();
        updateChannelList();

        return true;
    }
    else if (MorseDecoder::MsgReportSampleRate::match(message))
    {
        MorseDecoder::MsgReportSampleRate& report = (MorseDecoder::MsgReportSampleRate&) message;
        int sampleRate = report.getSampleRate();
        qDebug("MorseDecoderGUI::handleMessage: MorseDecoder::MsgReportSampleRate: %d", sampleRate);
        displaySampleRate(sampleRate);
        m_sampleRate = sampleRate;

        return true;
    }
    else if (MorseDecoder::MsgReportText::match(message))
    {
        MorseDecoder::MsgReportText& report = (MorseDecoder::MsgReportText&) message;
        textReceived(report.getText());
        updateMorseStats(report.m_estimatedPitchHz, report.m_estimatedSpeedWPM, report.m_costFunction);
    }

	return false;
}

void MorseDecoderGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void MorseDecoderGUI::textReceived(const QString& text)
{
    // Is the scroll bar at the bottom?
    int scrollPos = ui->text->verticalScrollBar()->value();
    bool atBottom = scrollPos >= ui->text->verticalScrollBar()->maximum();

    // Move cursor to end where we want to append new text
    // (user may have moved it by clicking / highlighting text)
    ui->text->moveCursor(QTextCursor::End);

    // Restore scroll position
    ui->text->verticalScrollBar()->setValue(scrollPos);

    // Format and insert text
    ui->text->insertPlainText(MorseDecoderSettings::formatText(text));

    // Scroll to bottom, if we we're previously at the bottom
    if (atBottom) {
        ui->text->verticalScrollBar()->setValue(ui->text->verticalScrollBar()->maximum());
    }
}

void MorseDecoderGUI::updateMorseStats(float estPitch, float estWPM, float cost)
{
    ui->pitchText->setText(QString("%1 Hz").arg(estPitch, 0, 'f', 1));
    ui->speedText->setText(QString("%1 WPM").arg(estWPM, 0, 'f', 0));
    ui->cText->setText(QString("%1").arg(cost, 0, 'f', 3));
}

void MorseDecoderGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    RollupContents *rollupContents = getRollupContents();

    rollupContents->saveState(m_rollupState);
    applySettings();
}

MorseDecoderGUI::MorseDecoderGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
	FeatureGUI(parent),
	ui(new Ui::MorseDecoderGUI),
	m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_sampleRate(48000),
	m_doApplySettings(true),
    m_lastFeatureState(0),
    m_selectedChannel(nullptr)
{
    m_feature = feature;
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/morsedecoder/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_morseDecoder = reinterpret_cast<MorseDecoder*>(feature);
    m_morseDecoder->setMessageQueueToGUI(&m_inputMessageQueue);
    m_scopeVis = m_morseDecoder->getScopeVis();
    m_scopeVis->setGLScope(ui->glScope);
    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);
    m_scopeVis->setLiveRate(4800); // 1 second

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(1000);

    displaySampleRate(m_sampleRate);

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    m_settings.setScopeGUI(ui->scopeGUI);
    m_settings.setRollupState(&m_rollupState);

    displaySettings();
	applySettings(true);
    makeUIConnections();
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
    m_morseDecoder->getAvailableChannelsReport();
}

MorseDecoderGUI::~MorseDecoderGUI()
{
	delete ui;
}

void MorseDecoderGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void MorseDecoderGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void MorseDecoderGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    getRollupContents()->restoreState(m_rollupState);
    ui->statLock->setChecked(!m_settings.m_auto);
    ui->showThreshold->setChecked(m_settings.m_showThreshold);
    ui->logFilename->setToolTip(QString(".txt log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);
    blockApplySettings(false);
}

void MorseDecoderGUI::displaySampleRate(int sampleRate)
{
	QString s = QString::number(sampleRate/1000.0, 'f', 1);
	ui->sinkSampleRateText->setText(tr("%1 kS/s").arg(s));
}

void MorseDecoderGUI::updateChannelList()
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

void MorseDecoderGUI::onMenuDialogCalled(const QPoint &p)
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

void MorseDecoderGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        MorseDecoder::MsgStartStop *message = MorseDecoder::MsgStartStop::create(checked);
        m_morseDecoder->getInputMessageQueue()->push(message);
    }
}

void MorseDecoderGUI::on_channels_currentIndexChanged(int index)
{
    if ((index >= 0) && (index < m_availableChannels.size()))
    {
        m_selectedChannel = qobject_cast<ChannelAPI*>(m_availableChannels[index].m_object);
        MorseDecoder::MsgSelectChannel *msg = MorseDecoder::MsgSelectChannel::create(m_selectedChannel);
        m_morseDecoder->getInputMessageQueue()->push(msg);
    }
}

void MorseDecoderGUI::on_channelApply_clicked()
{
    if (ui->channels->count() > 0) {
        on_channels_currentIndexChanged(ui->channels->currentIndex());
    }
}

void MorseDecoderGUI::on_statLock_toggled(bool checked)
{
    m_settings.m_auto = !checked;
    m_settingsKeys.append("auto");
    applySettings();
}

void MorseDecoderGUI::on_showThreshold_clicked(bool checked)
{
    m_settings.m_showThreshold = checked;
    m_settingsKeys.append("showThreshold");
    applySettings();
}

void MorseDecoderGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    m_settingsKeys.append("logEnabled");
    applySettings();
}

void MorseDecoderGUI::on_logFilename_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select file to log received text to", "", "*.txt");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);

    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            m_settings.m_logFilename = fileNames[0];
            ui->logFilename->setToolTip(QString(".txt log filename: %1").arg(m_settings.m_logFilename));
            m_settingsKeys.append("logFilename");
            applySettings();
        }
    }
}

void MorseDecoderGUI::on_clearTable_clicked()
{
    ui->text->clear();
}

void MorseDecoderGUI::tick()
{
}

void MorseDecoderGUI::updateStatus()
{
    int state = m_morseDecoder->getState();

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
                QMessageBox::information(this, tr("Message"), m_morseDecoder->getErrorMessage());
                break;
            default:
                break;
        }

        m_lastFeatureState = state;
    }
}

void MorseDecoderGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        MorseDecoder::MsgConfigureMorseDecoder* message = MorseDecoder::MsgConfigureMorseDecoder::create( m_settings, m_settingsKeys, force);
        m_morseDecoder->getInputMessageQueue()->push(message);
	}

    m_settingsKeys.clear();
}

void MorseDecoderGUI::makeUIConnections()
{
	QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &MorseDecoderGUI::on_startStop_toggled);
	QObject::connect(ui->channels, qOverload<int>(&QComboBox::currentIndexChanged), this, &MorseDecoderGUI::on_channels_currentIndexChanged);
	QObject::connect(ui->channelApply, &QPushButton::clicked, this, &MorseDecoderGUI::on_channelApply_clicked);
    QObject::connect(ui->statLock, &QToolButton::toggled, this, &MorseDecoderGUI::on_statLock_toggled);
    QObject::connect(ui->showThreshold, &ButtonSwitch::clicked, this, &MorseDecoderGUI::on_showThreshold_clicked);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &MorseDecoderGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &MorseDecoderGUI::on_logFilename_clicked);
    QObject::connect(ui->clearTable, &QPushButton::clicked, this, &MorseDecoderGUI::on_clearTable_clicked);
}
