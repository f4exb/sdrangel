///////////////////////////////////////////////////////////////////////////////////
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
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspengine.h"
#include "device/deviceset.h"
#include "util/db.h"
#include "maincore.h"

#include "ui_simplepttgui.h"
#include "simplepttreport.h"
#include "simpleptt.h"
#include "simplepttgui.h"

SimplePTTGUI* SimplePTTGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
	SimplePTTGUI* gui = new SimplePTTGUI(pluginAPI, featureUISet, feature);
	return gui;
}

void SimplePTTGUI::destroy()
{
	delete this;
}

void SimplePTTGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	applySettings(true);
}

QByteArray SimplePTTGUI::serialize() const
{
    return m_settings.serialize();
}

bool SimplePTTGUI::deserialize(const QByteArray& data)
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

bool SimplePTTGUI::handleMessage(const Message& message)
{
    if (SimplePTT::MsgConfigureSimplePTT::match(message))
    {
        qDebug("SimplePTTGUI::handleMessage: SimplePTT::MsgConfigureSimplePTT");
        const SimplePTT::MsgConfigureSimplePTT& cfg = (SimplePTT::MsgConfigureSimplePTT&) message;

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
    else if (SimplePTTReport::MsgRadioState::match(message))
    {
        qDebug("SimplePTTGUI::handleMessage: SimplePTTReport::MsgRadioState");
        const SimplePTTReport::MsgRadioState& cfg = (SimplePTTReport::MsgRadioState&) message;
        SimplePTTReport::RadioState state = cfg.getState();
        ui->statusIndicator->setStyleSheet("QLabel { background-color: " +
			m_statusColors[(int) state] + "; border-radius: 12px; }");
        ui->statusIndicator->setToolTip(m_statusTooltips[(int) state]);

        return true;
    }
    else if (SimplePTTReport::MsgVox::match(message))
    {
        qDebug("SimplePTTGUI::handleMessage: SimplePTTReport::MsgVox");
        const SimplePTTReport::MsgVox& cfg = (const SimplePTTReport::MsgVox&) message;

        if (cfg.getVox()) {
            ui->voxLevel->setStyleSheet("QDial { background-color : green; }");
        } else {
            ui->voxLevel->setStyleSheet("QDial { background:rgb(79,79,79); }");
        }

        return true;
    }
    else if (SimplePTT::MsgPTT::match(message))
    {
        qDebug("SimplePTTGUI::handleMessage: SimplePTT::MsgPTT");
        const SimplePTT::MsgPTT& cfg = (SimplePTT::MsgPTT&) message;
        bool ptt = cfg.getTx();
        blockApplySettings(true);
        ui->ptt->setChecked(ptt);
        blockApplySettings(false);

        return true;
    }

	return false;
}

void SimplePTTGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void SimplePTTGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

SimplePTTGUI::SimplePTTGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
	FeatureGUI(parent),
	ui(new Ui::SimplePTTGUI),
	m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
	m_doApplySettings(true),
    m_lastFeatureState(0)
{
    m_feature = feature;
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/simpleptt/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_simplePTT = reinterpret_cast<SimplePTT*>(feature);
    m_simplePTT->setMessageQueueToGUI(&m_inputMessageQueue);

    m_settings.setRollupState(&m_rollupState);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
	CRightClickEnabler *voxRightClickEnabler = new CRightClickEnabler(ui->vox);
	connect(voxRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect()));

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	m_statusTooltips.push_back("Idle");  // 0 - all off
	m_statusTooltips.push_back("Rx on"); // 1 - Rx on
	m_statusTooltips.push_back("Tx on"); // 2 - Tx on

	m_statusColors.push_back("gray");             // All off
	m_statusColors.push_back("rgb(85, 232, 85)"); // Rx on (green)
	m_statusColors.push_back("rgb(232, 85, 85)"); // Tx on (red)

    updateDeviceSetLists();
    displaySettings();
	applySettings(true);
    makeUIConnections();
    DialPopup::addPopupsToChildDials(this);
}

SimplePTTGUI::~SimplePTTGUI()
{
	delete ui;
}

void SimplePTTGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_settingsKeys.append("workspaceIndex");
    m_feature->setWorkspaceIndex(index);
}

void SimplePTTGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SimplePTTGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->rxtxDelay->setValue(m_settings.m_rx2TxDelayMs);
    ui->txrxDelay->setValue(m_settings.m_tx2RxDelayMs);
    getRollupContents()->restoreState(m_rollupState);
    ui->vox->setChecked(m_settings.m_vox);
    ui->voxEnable->setChecked(m_settings.m_voxEnable);
    ui->voxLevel->setValue(m_settings.m_voxLevel);
    ui->voxLevelText->setText(tr("%1").arg(m_settings.m_voxLevel));
    ui->voxHold->setValue(m_settings.m_voxHold);
    blockApplySettings(false);
}

void SimplePTTGUI::updateDeviceSetLists()
{
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
    std::vector<DeviceSet*>::const_iterator it = deviceSets.begin();

    ui->rxDevice->blockSignals(true);
    ui->txDevice->blockSignals(true);

    ui->rxDevice->clear();
    ui->txDevice->clear();
    unsigned int deviceIndex = 0;
    unsigned int rxIndex = 0;
    unsigned int txIndex = 0;

    for (; it != deviceSets.end(); ++it, deviceIndex++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  (*it)->m_deviceSourceEngine;
        DSPDeviceSinkEngine *deviceSinkEngine = (*it)->m_deviceSinkEngine;

        if (deviceSourceEngine)
        {
            ui->rxDevice->addItem(QString("R%1").arg(deviceIndex), deviceIndex);
            rxIndex++;
        }
        else if (deviceSinkEngine)
        {
            ui->txDevice->addItem(QString("T%1").arg(deviceIndex), deviceIndex);
            txIndex++;
        }
    }

    int rxDeviceIndex;
    int txDeviceIndex;

    if (rxIndex > 0)
    {
        if (m_settings.m_rxDeviceSetIndex < 0) {
            ui->rxDevice->setCurrentIndex(0);
        } else {
            ui->rxDevice->setCurrentIndex(m_settings.m_rxDeviceSetIndex);
        }

        rxDeviceIndex = ui->rxDevice->currentData().toInt();
    }
    else
    {
        rxDeviceIndex = -1;
    }


    if (txIndex > 0)
    {
        if (m_settings.m_txDeviceSetIndex < 0) {
            ui->txDevice->setCurrentIndex(0);
        } else {
            ui->txDevice->setCurrentIndex(m_settings.m_txDeviceSetIndex);
        }

        txDeviceIndex = ui->txDevice->currentData().toInt();
    }
    else
    {
        txDeviceIndex = -1;
    }

    if ((rxDeviceIndex != m_settings.m_rxDeviceSetIndex) ||
        (txDeviceIndex != m_settings.m_txDeviceSetIndex))
    {
        qDebug("SimplePTTGUI::updateDeviceSetLists: device index changed: %d:%d", rxDeviceIndex, txDeviceIndex);
        m_settings.m_rxDeviceSetIndex = rxDeviceIndex;
        m_settings.m_txDeviceSetIndex = txDeviceIndex;
        m_settingsKeys.append("rxDeviceSetIndex");
        m_settingsKeys.append("txDeviceSetIndex");
        applySettings();
    }

    ui->rxDevice->blockSignals(false);
    ui->txDevice->blockSignals(false);
}

void SimplePTTGUI::onMenuDialogCalled(const QPoint &p)
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

void SimplePTTGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        if (checked)
        {
            updateDeviceSetLists();
            displaySettings();
            applySettings();
        }

        SimplePTT::MsgStartStop *message = SimplePTT::MsgStartStop::create(checked);
        m_simplePTT->getInputMessageQueue()->push(message);
    }
}

void SimplePTTGUI::on_devicesRefresh_clicked()
{
    updateDeviceSetLists();
    displaySettings();
}

void SimplePTTGUI::on_rxDevice_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_rxDeviceSetIndex = index;
        m_settingsKeys.append("rxDeviceSetIndex");
        applySettings();
    }
}

void SimplePTTGUI::on_txDevice_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_txDeviceSetIndex = index;
        m_settingsKeys.append("txDeviceSetIndex");
        applySettings();
    }

}

void SimplePTTGUI::on_rxtxDelay_valueChanged(int value)
{
    m_settings.m_rx2TxDelayMs = value;
    m_settingsKeys.append("rx2TxDelayMs");
    applySettings();
}

void SimplePTTGUI::on_txrxDelay_valueChanged(int value)
{
    m_settings.m_tx2RxDelayMs = value;
    m_settingsKeys.append("tx2RxDelayMs");
    applySettings();
}

void SimplePTTGUI::on_ptt_toggled(bool checked)
{
    applyPTT(checked);
}

void SimplePTTGUI::on_vox_toggled(bool checked)
{
    m_settings.m_vox = checked;
    m_settingsKeys.append("vox");
    applySettings();
}

void SimplePTTGUI::on_voxEnable_clicked(bool checked)
{
    m_settings.m_voxEnable = checked;
    m_settingsKeys.append("voxEnable");
    applySettings();
}

void SimplePTTGUI::on_voxLevel_valueChanged(int value)
{
    m_settings.m_voxLevel = value;
    ui->voxLevelText->setText(tr("%1").arg(m_settings.m_voxLevel));
    m_settingsKeys.append("voxLevel");
    applySettings();
}

void SimplePTTGUI::on_voxHold_valueChanged(int value)
{
    m_settings.m_voxHold = value;
    m_settingsKeys.append("voxHold");
    applySettings();
}

void SimplePTTGUI::updateStatus()
{
    int state = m_simplePTT->getState();

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
                QMessageBox::information(this, tr("Message"), m_simplePTT->getErrorMessage());
                break;
            default:
                break;
        }

        m_lastFeatureState = state;
    }

    if (m_settings.m_vox)
    {
        float peak;
        m_simplePTT->getAudioPeak(peak);
        int peakDB = CalcDb::dbPower(peak);
        ui->audioPeak->setText(tr("%1 dB").arg(peakDB));
    }
}

void SimplePTTGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
	    SimplePTT::MsgConfigureSimplePTT* message = SimplePTT::MsgConfigureSimplePTT::create( m_settings, m_settingsKeys, force);
	    m_simplePTT->getInputMessageQueue()->push(message);
	}

    m_settingsKeys.clear();
}

void SimplePTTGUI::applyPTT(bool tx)
{
	if (m_doApplySettings)
	{
        qDebug("SimplePTTGUI::applyPTT: %s", tx ? "tx" : "rx");
	    SimplePTT::MsgPTT* message = SimplePTT::MsgPTT::create(tx);
	    m_simplePTT->getInputMessageQueue()->push(message);
	}
}

void SimplePTTGUI::audioSelect()
{
    qDebug("SimplePTTGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName, true);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        m_settingsKeys.append("audioDeviceName");
        applySettings();
    }
}

void SimplePTTGUI::makeUIConnections()
{
	QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &SimplePTTGUI::on_startStop_toggled);
	QObject::connect(ui->devicesRefresh, &QPushButton::clicked, this, &SimplePTTGUI::on_devicesRefresh_clicked);
	QObject::connect(ui->rxDevice, qOverload<int>(&QComboBox::currentIndexChanged), this, &SimplePTTGUI::on_rxDevice_currentIndexChanged);
	QObject::connect(ui->txDevice, qOverload<int>(&QComboBox::currentIndexChanged), this, &SimplePTTGUI::on_txDevice_currentIndexChanged);
	QObject::connect(ui->rxtxDelay, qOverload<int>(&QSpinBox::valueChanged), this, &SimplePTTGUI::on_rxtxDelay_valueChanged);
	QObject::connect(ui->txrxDelay, qOverload<int>(&QSpinBox::valueChanged), this, &SimplePTTGUI::on_txrxDelay_valueChanged);
	QObject::connect(ui->ptt, &ButtonSwitch::toggled, this, &SimplePTTGUI::on_ptt_toggled);
	QObject::connect(ui->vox, &ButtonSwitch::toggled, this, &SimplePTTGUI::on_vox_toggled);
	QObject::connect(ui->voxEnable, &QCheckBox::clicked, this, &SimplePTTGUI::on_voxEnable_clicked);
	QObject::connect(ui->voxLevel, &QDial::valueChanged, this, &SimplePTTGUI::on_voxLevel_valueChanged);
	QObject::connect(ui->voxHold, qOverload<int>(&QSpinBox::valueChanged), this, &SimplePTTGUI::on_voxHold_valueChanged);
}
