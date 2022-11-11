///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "device/deviceset.h"
#include "maincore.h"

#include "ui_rigctlservergui.h"
#include "rigctlserver.h"
#include "rigctlservergui.h"

RigCtlServerGUI* RigCtlServerGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
	RigCtlServerGUI* gui = new RigCtlServerGUI(pluginAPI, featureUISet, feature);
	return gui;
}

void RigCtlServerGUI::destroy()
{
	delete this;
}

void RigCtlServerGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	applySettings(true);
}

QByteArray RigCtlServerGUI::serialize() const
{
    return m_settings.serialize();
}

bool RigCtlServerGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        updateDeviceSetList();
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

bool RigCtlServerGUI::handleMessage(const Message& message)
{
    if (RigCtlServer::MsgConfigureRigCtlServer::match(message))
    {
        qDebug("RigCtlServerGUI::handleMessage: RigCtlServer::MsgConfigureRigCtlServer");
        const RigCtlServer::MsgConfigureRigCtlServer& cfg = (RigCtlServer::MsgConfigureRigCtlServer&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (RigCtlServerSettings::MsgChannelIndexChange::match(message))
    {
        const RigCtlServerSettings::MsgChannelIndexChange& cfg = (RigCtlServerSettings::MsgChannelIndexChange&) message;
        int newChannelIndex = cfg.getIndex();
        qDebug("RigCtlServerGUI::handleMessage: RigCtlServerSettings::MsgChannelIndexChange: %d", newChannelIndex);
        ui->channel->blockSignals(true);
        ui->channel->setCurrentIndex(newChannelIndex);
        m_settings.m_channelIndex = newChannelIndex;
        ui->channel->blockSignals(false);

        return true;
    }

	return false;
}

void RigCtlServerGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void RigCtlServerGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

RigCtlServerGUI::RigCtlServerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
	FeatureGUI(parent),
	ui(new Ui::RigCtlServerGUI),
	m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
	m_doApplySettings(true),
    m_lastFeatureState(0)
{
    m_feature = feature;
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/rigctlserver/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_rigCtlServer = reinterpret_cast<RigCtlServer*>(feature);
    m_rigCtlServer->setMessageQueueToGUI(&m_inputMessageQueue);

    m_settings.setRollupState(&m_rollupState);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(1000);

    updateDeviceSetList();
    displaySettings();
	applySettings(true);
    makeUIConnections();
}

RigCtlServerGUI::~RigCtlServerGUI()
{
	delete ui;
}

void RigCtlServerGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void RigCtlServerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RigCtlServerGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->rigCtrlPort->setValue(m_settings.m_rigCtlPort);
    ui->maxFrequencyOffset->setValue(m_settings.m_maxFrequencyOffset);
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void RigCtlServerGUI::updateDeviceSetList()
{
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
    std::vector<DeviceSet*>::const_iterator it = deviceSets.begin();

    ui->device->blockSignals(true);

    ui->device->clear();
    unsigned int deviceIndex = 0;

    for (; it != deviceSets.end(); ++it, deviceIndex++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine = (*it)->m_deviceSourceEngine;

        if (deviceSourceEngine) {
            ui->device->addItem(QString("R%1").arg(deviceIndex), deviceIndex);
        }
    }

    int newDeviceIndex;

    if (it != deviceSets.begin())
    {
        if (m_settings.m_deviceIndex < 0) {
            ui->device->setCurrentIndex(0);
        } else {
            ui->device->setCurrentIndex(m_settings.m_deviceIndex);
        }

        newDeviceIndex = ui->device->currentData().toInt();
    }
    else
    {
        newDeviceIndex = -1;
    }


    if (newDeviceIndex != m_settings.m_deviceIndex)
    {
        qDebug("RigCtlServerGUI::updateDeviceSetLists: device index changed: %d", newDeviceIndex);
        m_settings.m_deviceIndex = newDeviceIndex;
    }

    updateChannelList();

    ui->device->blockSignals(false);
}

bool RigCtlServerGUI::updateChannelList()
{
    int newChannelIndex;
    ui->channel->blockSignals(true);
    ui->channel->clear();

    if (m_settings.m_deviceIndex < 0)
    {
        newChannelIndex = -1;
    }
    else
    {
        MainCore *mainCore = MainCore::instance();
        std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
        DeviceSet *deviceSet = deviceSets[m_settings.m_deviceIndex];
        int nbChannels = deviceSet->getNumberOfChannels();

        for (int ch = 0; ch < nbChannels; ch++) {
            ui->channel->addItem(QString("%1").arg(ch), ch);
        }

        if (nbChannels > 0)
        {
            if (m_settings.m_channelIndex < 0) {
                ui->channel->setCurrentIndex(0);
            } else {
                ui->channel->setCurrentIndex(m_settings.m_channelIndex);
            }

            newChannelIndex = ui->channel->currentIndex();
        }
        else
        {
            newChannelIndex = -1;
        }
    }

    ui->channel->blockSignals(false);

    if (newChannelIndex != m_settings.m_channelIndex)
    {
        qDebug("RigCtlServerGUI::updateChannelList: channel index changed: %d", newChannelIndex);
        m_settings.m_channelIndex = newChannelIndex;
        return true;
    }

    return false;
}

void RigCtlServerGUI::onMenuDialogCalled(const QPoint &p)
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

        applySettings();
    }

    resetContextMenuType();
}

void RigCtlServerGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        RigCtlServer::MsgStartStop *message = RigCtlServer::MsgStartStop::create(checked);
        m_rigCtlServer->getInputMessageQueue()->push(message);
    }
}

void RigCtlServerGUI::on_enable_toggled(bool checked)
{
    m_settings.m_enabled = checked;
    applySettings();
}

void RigCtlServerGUI::on_devicesRefresh_clicked()
{
    updateDeviceSetList();
    displaySettings();
    applySettings();
}

void RigCtlServerGUI::on_device_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_deviceIndex = ui->device->currentData().toInt();
        updateChannelList();
        applySettings();
    }
}

void RigCtlServerGUI::on_channel_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_channelIndex = index;
        applySettings();
    }
}

void RigCtlServerGUI::on_rigCtrlPort_valueChanged(int value)
{
    m_settings.m_rigCtlPort = value;
    applySettings();
}

void RigCtlServerGUI::on_maxFrequencyOffset_valueChanged(int value)
{
    m_settings.m_maxFrequencyOffset = value;
    applySettings();
}

void RigCtlServerGUI::updateStatus()
{
    int state = m_rigCtlServer->getState();

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
                QMessageBox::information(this, tr("Message"), m_rigCtlServer->getErrorMessage());
                break;
            default:
                break;
        }

        m_lastFeatureState = state;
    }
}

void RigCtlServerGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
	    RigCtlServer::MsgConfigureRigCtlServer* message = RigCtlServer::MsgConfigureRigCtlServer::create( m_settings, force);
	    m_rigCtlServer->getInputMessageQueue()->push(message);
	}
}

void RigCtlServerGUI::makeUIConnections()
{
	QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &RigCtlServerGUI::on_startStop_toggled);
	QObject::connect(ui->enable, &QCheckBox::toggled, this, &RigCtlServerGUI::on_enable_toggled);
	QObject::connect(ui->devicesRefresh, &QPushButton::clicked, this, &RigCtlServerGUI::on_devicesRefresh_clicked);
	QObject::connect(ui->device, qOverload<int>(&QComboBox::currentIndexChanged), this, &RigCtlServerGUI::on_device_currentIndexChanged);
	QObject::connect(ui->channel, qOverload<int>(&QComboBox::currentIndexChanged), this, &RigCtlServerGUI::on_channel_currentIndexChanged);
	QObject::connect(ui->rigCtrlPort, qOverload<int>(&QSpinBox::valueChanged), this, &RigCtlServerGUI::on_rigCtrlPort_valueChanged);
	QObject::connect(ui->maxFrequencyOffset, qOverload<int>(&QSpinBox::valueChanged), this, &RigCtlServerGUI::on_maxFrequencyOffset_valueChanged);
}
