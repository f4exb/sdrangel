///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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
#include <QMouseEvent>

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "device/deviceset.h"
#include "util/db.h"
#include "maincore.h"

#include "ui_jogdialcontrollergui.h"
#include "jogdialcontroller.h"
#include "jogdialcontrollergui.h"

JogdialControllerGUI* JogdialControllerGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
	JogdialControllerGUI* gui = new JogdialControllerGUI(pluginAPI, featureUISet, feature);
	return gui;
}

void JogdialControllerGUI::destroy()
{
	delete this;
}

void JogdialControllerGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	applySettings(true);
}

QByteArray JogdialControllerGUI::serialize() const
{
    return m_settings.serialize();
}

bool JogdialControllerGUI::deserialize(const QByteArray& data)
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

bool JogdialControllerGUI::handleMessage(const Message& message)
{
    if (JogdialController::MsgConfigureJogdialController::match(message))
    {
        qDebug("JogdialControllerGUI::handleMessage: JogdialController::MsgConfigureJogdialController");
        const JogdialController::MsgConfigureJogdialController& cfg = (JogdialController::MsgConfigureJogdialController&) message;

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
    else if (JogdialController::MsgReportChannels::match(message))
    {
        qDebug("JogdialControllerGUI::handleMessage: JogdialController::MsgReportChannels");
        JogdialController::MsgReportChannels& report = (JogdialController::MsgReportChannels&) message;
        m_availableChannels = report.getAvailableChannels();
        updateChannelList();

        return true;
    }
    else if (JogdialController::MsgReportControl::match(message))
    {
        qDebug("JogdialControllerGUI::handleMessage: JogdialController::MsgReportControl");
        JogdialController::MsgReportControl& report = (JogdialController::MsgReportControl&) message;
        ui->controlLabel->setText(report.getDeviceElseChannel() ? "D" : "C");

        return true;
    }
    else if (JogdialController::MsgSelectChannel::match(message))
    {
        qDebug("JogdialControllerGUI::handleMessage: JogdialController::MsgSelectChannel");
        JogdialController::MsgSelectChannel& report = (JogdialController::MsgSelectChannel&) message;
        int index = report.getIndex();

        if ((index >= 0) && (index < m_availableChannels.size()))
        {
            ui->channels->blockSignals(true);
            ui->channels->setCurrentIndex(index);
            ui->channels->blockSignals(false);
        }

        return true;
    }

	return false;
}

void JogdialControllerGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void JogdialControllerGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

JogdialControllerGUI::JogdialControllerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
	FeatureGUI(parent),
	ui(new Ui::JogdialControllerGUI),
	m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
	m_doApplySettings(true),
    m_lastFeatureState(0),
    m_selectedChannel(nullptr)
{
    m_feature = feature;
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/jogdialcontroller/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_jogdialController = reinterpret_cast<JogdialController*>(feature);
    m_jogdialController->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(1000);

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));
    this->installEventFilter(&m_commandKeyReceiver);

    m_settings.setRollupState(&m_rollupState);

    displaySettings();
	applySettings(true);
    makeUIConnections();
}

JogdialControllerGUI::~JogdialControllerGUI()
{
	delete ui;
}

void JogdialControllerGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void JogdialControllerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void JogdialControllerGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void JogdialControllerGUI::updateChannelList()
{
    ui->channels->blockSignals(true);
    ui->channels->clear();

    QList<JogdialControllerSettings::AvailableChannel>::const_iterator it = m_availableChannels.begin();
    int selectedItem = -1;

    for (int i = 0; it != m_availableChannels.end(); ++it, i++)
    {
        ui->channels->addItem(tr("%1%2:%3 %4")
            .arg(it->m_tx ? "T" : "R")
            .arg(it->m_deviceSetIndex)
            .arg(it->m_channelIndex)
            .arg(it->m_channelId)
        );

        if (it->m_channelAPI == m_selectedChannel) {
            selectedItem = i;
        }
    }

    int currentSelectedChannelIndex = ui->channels->currentIndex();
    ui->channels->blockSignals(false);

    if (m_availableChannels.size() > 0)
    {
        if (selectedItem >= 0) {
            ui->channels->setCurrentIndex(selectedItem);
        } else {
            ui->channels->setCurrentIndex(0);
        }
    }

    if (currentSelectedChannelIndex == ui->channels->currentIndex()) {
        on_channels_currentIndexChanged(ui->channels->currentIndex()); // force sending
    }
}

void JogdialControllerGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
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

void JogdialControllerGUI::on_startStop_toggled(bool checked)
{
    if (checked)
    {
        setFocus();
        setFocusPolicy(Qt::StrongFocus);
        connect(&m_commandKeyReceiver, SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
            m_jogdialController, SLOT(commandKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
    }
    else
    {
        disconnect(&m_commandKeyReceiver, SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
            m_jogdialController, SLOT(commandKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
        setFocusPolicy(Qt::NoFocus);
        clearFocus();
    }

    JogdialController::MsgStartStop *message = JogdialController::MsgStartStop::create(checked);
    m_jogdialController->getInputMessageQueue()->push(message);
}

void JogdialControllerGUI::on_devicesRefresh_clicked()
{
    JogdialController::MsgRefreshChannels *msg = JogdialController::MsgRefreshChannels::create();
    m_jogdialController->getInputMessageQueue()->push(msg);
}

void JogdialControllerGUI::on_channels_currentIndexChanged(int index)
{
    if ((index >= 0) && (index < m_availableChannels.size()))
    {
        m_selectedChannel = m_availableChannels[index].m_channelAPI;
        JogdialController::MsgSelectChannel *msg = JogdialController::MsgSelectChannel::create(index);
        m_jogdialController->getInputMessageQueue()->push(msg);
    }
}

void JogdialControllerGUI::tick()
{
}

void JogdialControllerGUI::updateStatus()
{
    int state = m_jogdialController->getState();

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
                QMessageBox::information(this, tr("Message"), m_jogdialController->getErrorMessage());
                break;
            default:
                break;
        }

        m_lastFeatureState = state;
    }
}

void JogdialControllerGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
	    JogdialController::MsgConfigureJogdialController* message = JogdialController::MsgConfigureJogdialController::create( m_settings, m_settingsKeys, force);
	    m_jogdialController->getInputMessageQueue()->push(message);
	}

    m_settingsKeys.clear();
}

void JogdialControllerGUI::focusInEvent(QFocusEvent*)
{
    qDebug("JogdialControllerGUI::focusInEvent");
    ui->focusIndicator->setStyleSheet("QLabel { background-color: rgb(85, 232, 85); border-radius: 8px; }"); // green
    ui->focusIndicator->setToolTip("Active");
}

void JogdialControllerGUI::focusOutEvent(QFocusEvent*)
{
    qDebug("JogdialControllerGUI::focusOutEvent");
    ui->focusIndicator->setStyleSheet("QLabel { background-color: gray; border-radius: 8px; }"); // gray
    ui->focusIndicator->setToolTip("Idle");
}

void JogdialControllerGUI::makeUIConnections()
{
	QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &JogdialControllerGUI::on_startStop_toggled);
	QObject::connect(ui->devicesRefresh, &QPushButton::clicked, this, &JogdialControllerGUI::on_devicesRefresh_clicked);
	QObject::connect(ui->channels, qOverload<int>(&QComboBox::currentIndexChanged), this, &JogdialControllerGUI::on_channels_currentIndexChanged);
}
