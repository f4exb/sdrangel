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

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/crightclickenabler.h"
#include "gui/dialogpositioner.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "device/deviceset.h"
#include "maincore.h"

#include "limerfeusbcalib.h"
#include "ui_limerfegui.h"
#include "limerfe.h"
#include "limerfegui.h"

LimeRFEGUI* LimeRFEGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
	LimeRFEGUI* gui = new LimeRFEGUI(pluginAPI, featureUISet, feature);
	return gui;
}

void LimeRFEGUI::destroy()
{
	delete this;
}

void LimeRFEGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	applySettings(true);
}

QByteArray LimeRFEGUI::serialize() const
{
    return m_settings.serialize();
}

bool LimeRFEGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        displaySettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void LimeRFEGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_settingsKeys.append("workspaceIndex");
    m_feature->setWorkspaceIndex(index);
}

void LimeRFEGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void LimeRFEGUI::onMenuDialogCalled(const QPoint &p)
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

LimeRFEGUI::LimeRFEGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
	FeatureGUI(parent),
	ui(new Ui::LimeRFEGUI),
	m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_rxOn(false),
    m_txOn(false),
	m_doApplySettings(true),
    m_rxTxToggle(false),
    m_currentPowerCorrection(0.0),
    m_avgPower(false),
    m_deviceSetSync(false)
{
    m_feature = feature;
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/limerfe/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_limeRFE = reinterpret_cast<LimeRFE*>(feature);
    m_limeRFE->setMessageQueueToGUI(&m_inputMessageQueue);

    for (const auto& comPortName : m_limeRFE->getComPorts()) {
        ui->device->addItem(comPortName);
    }

    m_settings.setRollupState(&m_rollupState);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    updateDeviceSetList();
    displaySettings();
    highlightApplyButton(false);
    m_timer.setInterval(500);
    makeUIConnections();
}

LimeRFEGUI::~LimeRFEGUI()
{
	delete ui;
}

void LimeRFEGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
	    LimeRFE::MsgConfigureLimeRFE* message = LimeRFE::MsgConfigureLimeRFE::create( m_settings, m_settingsKeys, force);
	    m_limeRFE->getInputMessageQueue()->push(message);
	}

    m_settingsKeys.clear();
}

void LimeRFEGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    setRxChannels();
    ui->rxPort->setCurrentIndex(m_settings.m_rxPort);
    ui->attenuation->setCurrentIndex(m_settings.m_attenuationFactor);
    ui->amFmNotchFilter->setChecked(m_settings.m_amfmNotch);
    setTxChannels();
    ui->txPort->setCurrentIndex(m_settings.m_txPort);
    ui->txFollowsRx->setChecked(m_settings.m_txRxDriven);
    ui->rxTxToggle->setChecked(m_rxTxToggle);
    displayMode();
    displayPower();
    blockApplySettings(false);
}

void LimeRFEGUI::displayMode()
{
    QString s;

    if (m_rxOn)
    {
        if (m_txOn) {
            s = "Rx/Tx";
        } else {
            s = "Rx";
        }
    }
    else
    {
        if (m_txOn) {
            s = "Tx";
        } else {
            s = "None";
        }
    }

    ui->modeText->setText(s);

    ui->modeRx->blockSignals(true);
    ui->modeTx->blockSignals(true);

    if (m_rxOn) {
		ui->modeRx->setStyleSheet("QToolButton { background-color : green; }");
    } else {
		ui->modeRx->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    if (m_txOn) {
        ui->modeTx->setStyleSheet("QToolButton { background-color : red; }");
    } else {
		ui->modeTx->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    ui->modeRx->setChecked(m_rxOn);
    ui->modeTx->setChecked(m_txOn);

    ui->modeRx->blockSignals(false);
    ui->modeTx->blockSignals(false);
}

void LimeRFEGUI::displayPower()
{
    ui->powerEnable->blockSignals(true);
    ui->powerSource->blockSignals(true);

    ui->powerEnable->setChecked(m_settings.m_swrEnable);
    ui->powerSource->setCurrentIndex((int) m_settings.m_swrSource);

    ui->powerEnable->blockSignals(false);
    ui->powerSource->blockSignals(false);
}

void LimeRFEGUI::refreshPower()
{
    int fwdPower, refPower;
    int rc = m_limeRFE->getFwdPower(fwdPower);

    if (rc != 0)
    {
        ui->statusText->setText(m_limeRFE->getError(rc).c_str());
        return;
    }

    rc = m_limeRFE->getRefPower(refPower);

    if (rc != 0)
    {
        ui->statusText->setText(m_limeRFE->getError(rc).c_str());
        return;
    }

    double fwdPowerDB = fwdPower / 10.0;
    double refPowerDB = refPower / 10.0;
    double retLossDB = fwdPowerDB - refPowerDB;

    ui->powerFwdText->setText(QString::number(fwdPowerDB, 'f', 1));
    ui->powerRefText->setText(QString::number(refPowerDB, 'f', 1));
    ui->returnLossText->setText(QString::number(retLossDB, 'f', 1));

    double denom = CalcDb::powerFromdB(retLossDB/2.0) - 1.0;

    if (denom == 0.0)
    {
        ui->swrText->setText("---");
    }
    else
    {
        double vswr = (CalcDb::powerFromdB(retLossDB/2.0) + 1.0) / denom;
        vswr = vswr < 0.0 ? 0.0 : vswr > 99.999 ? 99.999 : vswr;
        ui->swrText->setText(QString::number(vswr, 'f', 3));
    }

    updateAbsPower(m_currentPowerCorrection);
}

void LimeRFEGUI::setRxChannels()
{
    ui->rxChannel->blockSignals(true);
    ui->rxPort->blockSignals(true);
    ui->rxChannel->clear();
    ui->rxPort->clear();

    if (m_settings.m_rxChannels == LimeRFESettings::ChannelsWideband)
    {
        ui->rxChannel->addItem("1-1000MHz");
        ui->rxChannel->addItem("1-4GHz");
        ui->rxChannel->setCurrentIndex((int) m_settings.m_rxWidebandChannel);
        ui->rxPort->addItem("TX/RX (J3)");
        ui->rxPort->addItem("TX/RX 30M (J5)");
        ui->rxPort->setCurrentIndex((int) m_settings.m_rxPort);
        ui->txFollowsRx->setEnabled(true);
        ui->rxPort->setEnabled(true);
    }
    else if (m_settings.m_rxChannels == LimeRFESettings::ChannelsHAM)
    {
        ui->rxChannel->addItem("<30MHz");
        ui->rxChannel->addItem("50-70MHz");
        ui->rxChannel->addItem("144-146MHz");
        ui->rxChannel->addItem("220-225MHz");
        ui->rxChannel->addItem("430-440MHz");
        ui->rxChannel->addItem("902-928MHz");
        ui->rxChannel->addItem("1240-1325MHz");
        ui->rxChannel->addItem("2300-2450MHz");
        ui->rxChannel->addItem("3300-3500MHz");
        ui->rxChannel->setCurrentIndex((int) m_settings.m_rxHAMChannel);
        ui->txFollowsRx->setEnabled(true);

        switch(m_settings.m_rxHAMChannel)
        {
            case LimeRFESettings::HAM_30M:
            case LimeRFESettings::HAM_50_70MHz:
            case LimeRFESettings::HAM_144_146MHz:
            case LimeRFESettings::HAM_220_225MHz:
            case LimeRFESettings::HAM_430_440MHz:
                ui->rxPort->addItem("TX/RX (J3)");
                ui->rxPort->addItem("TX/RX 30M (J5)");
                ui->rxPort->setEnabled(true);
                ui->rxPort->setCurrentIndex((int) m_settings.m_rxPort);
                break;
            case LimeRFESettings::HAM_902_928MHz:
            case LimeRFESettings::HAM_1240_1325MHz:
            case LimeRFESettings::HAM_2300_2450MHz:
            case LimeRFESettings::HAM_3300_3500MHz:
                ui->rxPort->addItem("TX/RX (J3)");
                ui->rxPort->setEnabled(false);
                m_settings.m_rxPort = LimeRFESettings::RxPortJ3;
                m_settingsKeys.append("rxPort");
                ui->rxPort->setCurrentIndex((int) m_settings.m_rxPort);
                break;
            default:
                break;
        }
    }
    else if (m_settings.m_rxChannels == LimeRFESettings::ChannelsCellular)
    {
        ui->rxChannel->addItem("Band1");
        ui->rxChannel->addItem("Band2");
        ui->rxChannel->addItem("Band3");
        ui->rxChannel->addItem("Band7");
        ui->rxChannel->addItem("Band38");
        ui->rxChannel->setCurrentIndex((int) m_settings.m_rxCellularChannel);
        ui->rxPort->addItem("TX/RX (J3)");
        ui->rxPort->setEnabled(false);
        m_settings.m_rxPort = LimeRFESettings::RxPortJ3;
        m_settingsKeys.append("rxPort");
        ui->rxPort->setCurrentIndex((int) m_settings.m_rxPort);
        m_settings.m_txRxDriven = true;
        m_settingsKeys.append("txRxDriven");
        ui->txFollowsRx->setEnabled(false);
        ui->txFollowsRx->setChecked(m_settings.m_txRxDriven);
    }

    ui->rxChannelGroup->setCurrentIndex((int) m_settings.m_rxChannels);
    ui->rxPort->blockSignals(false);
    ui->rxChannel->blockSignals(false);
}

void LimeRFEGUI::setTxChannels()
{
    ui->txChannel->blockSignals(true);
    ui->txPort->blockSignals(true);
    ui->powerCorrValue->blockSignals(true);

    ui->txChannel->clear();
    ui->txPort->clear();

    if (m_settings.m_txChannels == LimeRFESettings::ChannelsWideband)
    {
        ui->txChannel->addItem("1-1000MHz");
        ui->txChannel->addItem("1-4GHz");
        ui->txChannel->setCurrentIndex((int) m_settings.m_txWidebandChannel);
        ui->txPort->addItem("TX/RX (J3)");
        ui->txPort->addItem("TX (J4)");
        ui->txPort->setCurrentIndex((int) m_settings.m_txPort);
        ui->txPort->setEnabled(true);
    }
    else if (m_settings.m_txChannels == LimeRFESettings::ChannelsHAM)
    {
        ui->txChannel->addItem("<30MHz");
        ui->txChannel->addItem("50-70MHz");
        ui->txChannel->addItem("144-146MHz");
        ui->txChannel->addItem("220-225MHz");
        ui->txChannel->addItem("430-440MHz");
        ui->txChannel->addItem("902-928MHz");
        ui->txChannel->addItem("1240-1325MHz");
        ui->txChannel->addItem("2300-2450MHz");
        ui->txChannel->addItem("3300-3500MHz");
        ui->txChannel->setCurrentIndex((int) m_settings.m_txHAMChannel);

        switch(m_settings.m_txHAMChannel)
        {
            case LimeRFESettings::HAM_30M:
            case LimeRFESettings::HAM_50_70MHz:
                ui->txPort->addItem("TX/RX (J3)");
                ui->txPort->addItem("TX (J4)");
                ui->txPort->addItem("TX/RX 30M (J5)");
                ui->txPort->setEnabled(false);
                m_settings.m_txPort = LimeRFESettings::TxPortJ5;
                m_settingsKeys.append("txPort");
                ui->txPort->setCurrentIndex((int) m_settings.m_txPort);
                break;
            case LimeRFESettings::HAM_144_146MHz:
            case LimeRFESettings::HAM_220_225MHz:
            case LimeRFESettings::HAM_430_440MHz:
            case LimeRFESettings::HAM_902_928MHz:
            case LimeRFESettings::HAM_1240_1325MHz:
            case LimeRFESettings::HAM_2300_2450MHz:
            case LimeRFESettings::HAM_3300_3500MHz:
                ui->txPort->addItem("TX/RX (J3)");
                ui->txPort->addItem("TX (J4)");
                ui->txPort->setCurrentIndex(m_settings.m_txPort < 2 ? m_settings.m_txPort : 1);
                ui->txPort->setEnabled(true);
                break;
            default:
                break;
        }
    }
    else if (m_settings.m_txChannels == LimeRFESettings::ChannelsCellular)
    {
        ui->txChannel->addItem("Band1");
        ui->txChannel->addItem("Band2");
        ui->txChannel->addItem("Band3");
        ui->txChannel->addItem("Band7");
        ui->txChannel->addItem("Band38");
        ui->txChannel->setCurrentIndex((int) m_settings.m_txCellularChannel);
        ui->txPort->addItem("TX/RX (J3)");
        m_settings.m_txPort = LimeRFESettings::TxPortJ3;
        m_settingsKeys.append("txPort");
        ui->txPort->setEnabled(false);
        ui->txPort->setCurrentIndex((int) m_settings.m_txPort);
    }

    ui->txChannelGroup->setCurrentIndex((int) m_settings.m_txChannels);
    m_currentPowerCorrection = getPowerCorrection();
    ui->powerCorrValue->setText(QString::number(m_currentPowerCorrection, 'f', 1));
    updateAbsPower(m_currentPowerCorrection);

    ui->powerCorrValue->blockSignals(false);
    ui->txPort->blockSignals(false);
    ui->txChannel->blockSignals(false);
}

int LimeRFEGUI::getPowerCorectionIndex()
{
    LimeRFEUSBCalib::ChannelRange range;

    switch (m_settings.m_txChannels)
    {
        case LimeRFESettings::ChannelsWideband:
        {
            switch (m_settings.m_txWidebandChannel)
            {
                case LimeRFESettings::WidebandLow:
                    range = LimeRFEUSBCalib::WidebandLow;
                    break;
                case LimeRFESettings::WidebandHigh:
                    range = LimeRFEUSBCalib::WidebandHigh;
                    break;
                default:
                    return -1;
                    break;
            }
            break;
        }
        case LimeRFESettings::ChannelsHAM:
        {
            switch (m_settings.m_txHAMChannel)
            {
                case LimeRFESettings::HAM_30M:
                    range = LimeRFEUSBCalib::HAM_30MHz;
                    break;
                case LimeRFESettings::HAM_50_70MHz:
                    range = LimeRFEUSBCalib::HAM_50_70MHz;
                    break;
                case LimeRFESettings::HAM_144_146MHz:
                    range = LimeRFEUSBCalib::HAM_144_146MHz;
                    break;
                case LimeRFESettings::HAM_220_225MHz:
                    range = LimeRFEUSBCalib::HAM_220_225MHz;
                    break;
                case LimeRFESettings::HAM_430_440MHz:
                    range = LimeRFEUSBCalib::HAM_430_440MHz;
                    break;
                case LimeRFESettings::HAM_902_928MHz:
                    range = LimeRFEUSBCalib::HAM_902_928MHz;
                    break;
                case LimeRFESettings::HAM_1240_1325MHz:
                    range = LimeRFEUSBCalib::HAM_1240_1325MHz;
                    break;
                case LimeRFESettings::HAM_2300_2450MHz:
                    range = LimeRFEUSBCalib::HAM_2300_2450MHz;
                    break;
                case LimeRFESettings::HAM_3300_3500MHz:
                    range = LimeRFEUSBCalib::HAM_3300_3500MHz;
                    break;
                default:
                    return -1;
                    break;
            }
            break;
        }
        case LimeRFESettings::ChannelsCellular:
        {
            switch (m_settings.m_txCellularChannel)
            {
                case LimeRFESettings::CellularBand1:
                    range = LimeRFEUSBCalib::CellularBand1;
                    break;
                case LimeRFESettings::CellularBand2:
                    range = LimeRFEUSBCalib::CellularBand2;
                    break;
                case LimeRFESettings::CellularBand3:
                    range = LimeRFEUSBCalib::CellularBand3;
                    break;
                case LimeRFESettings::CellularBand7:
                    range = LimeRFEUSBCalib::CellularBand7;
                    break;
                case LimeRFESettings::CellularBand38:
                    range = LimeRFEUSBCalib::CellularBand38;
                    break;
                default:
                    return -1;
                    break;
            }
            break;
        }
        default:
            return -1;
            break;
    }

    return (int) range;
}

double LimeRFEGUI::getPowerCorrection()
{
    int index = getPowerCorectionIndex();

    QMap<int, double>::const_iterator it = m_settings.m_calib.m_calibrations.find(index);

    if (it != m_settings.m_calib.m_calibrations.end()) {
        return it.value();
    } else {
        return 0.0;
    }
}

void LimeRFEGUI::setPowerCorrection(double dbValue)
{
    int index = getPowerCorectionIndex();

    if (index < 0) {
        return;
    }

    m_settings.m_calib.m_calibrations[index] = dbValue;
}

void LimeRFEGUI::updateAbsPower(double powerCorrDB)
{
    bool ok;
    double power = ui->powerFwdText->text().toDouble(&ok);

    if (ok)
    {
        double powerCorrected = power + powerCorrDB;
        double powerDisplayed = powerCorrected;

        if (m_avgPower)
        {
            m_powerMovingAverage(powerCorrected);
            powerDisplayed = m_powerMovingAverage.asDouble();
        }

        ui->powerAbsDbText->setText(tr("%1 dBm").arg(QString::number(powerDisplayed, 'f', 1)));
        double powerWatts = CalcDb::powerFromdB(powerDisplayed - 30.0);
        powerWatts = powerWatts > 8.0 ? 8.0 : powerWatts;
        ui->powerAbsWText->setText(tr("%1 W").arg(QString::number(powerWatts, 'f', 3)));
    }
}

void LimeRFEGUI::updateDeviceSetList()
{
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
    std::vector<DeviceSet*>::const_iterator it = deviceSets.begin();

    ui->deviceSetRx->blockSignals(true);
    ui->deviceSetTx->blockSignals(true);

    // Save current positions
    int rxIndex = ui->deviceSetRx->currentIndex();
    int txIndex = ui->deviceSetTx->currentIndex();

    ui->deviceSetRx->clear();
    ui->deviceSetTx->clear();
    unsigned int deviceSetIndex = 0;

    for (; it != deviceSets.end(); ++it, deviceSetIndex++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  (*it)->m_deviceSourceEngine;
        DSPDeviceSinkEngine *deviceSinkEngine = (*it)->m_deviceSinkEngine;

        if (deviceSourceEngine) {
            ui->deviceSetRx->addItem(QString("R:%1").arg(deviceSetIndex), deviceSetIndex);
        } else if (deviceSinkEngine) {
            ui->deviceSetTx->addItem(QString("T:%1").arg(deviceSetIndex), deviceSetIndex);
        }
    }

    // Restore current positions (if possible)
    ui->deviceSetRx->setCurrentIndex(rxIndex < 0 ? 0 : rxIndex);
    ui->deviceSetTx->setCurrentIndex(txIndex < 0 ? 0 : txIndex);

    ui->deviceSetRx->blockSignals(false);
    ui->deviceSetTx->blockSignals(false);
}

void LimeRFEGUI::highlightApplyButton(bool highlight)
{
    if (highlight) {
        ui->apply->setStyleSheet("QPushButton { background-color : green; }");
    } else {
        ui->apply->setStyleSheet("QPushButton { background:rgb(64, 64, 64); }");
    }
}

void LimeRFEGUI::on_openDevice_clicked()
{
    int rc = m_limeRFE->openDevice(ui->device->currentText().toStdString());
    ui->statusText->append(QString("Open %1: %2").arg(ui->device->currentText()).arg(m_limeRFE->getError(rc).c_str()));

    if (rc != 0) {
        return;
    }

    rc = m_limeRFE->getState();
    ui->statusText->append(QString("Get state: %1").arg(m_limeRFE->getError(rc).c_str()));
}

void LimeRFEGUI::on_closeDevice_clicked()
{
    ui->statusText->clear();
    m_limeRFE->closeDevice();
    ui->statusText->setText("Closed");
}

void LimeRFEGUI::on_deviceToGUI_clicked()
{
    int rc = m_limeRFE->getState();

    if (rc != 0)
    {
        ui->statusText->setText(m_limeRFE->getError(rc).c_str());
        return;
    }

    m_limeRFE->stateToSettings(m_settings, m_settingsKeys);
    m_rxOn = m_limeRFE->getRx();
    m_txOn = m_limeRFE->getTx();
    displaySettings();
    highlightApplyButton(false);
}

void LimeRFEGUI::on_rxChannelGroup_currentIndexChanged(int index)
{
    m_settings.m_rxChannels = (LimeRFESettings::ChannelGroups) index;
    m_settingsKeys.append("rxChannels");
    setRxChannels();

    if (m_settings.m_txRxDriven)
    {
        m_settings.m_txChannels = m_settings.m_rxChannels;
        m_settingsKeys.append("txChannels");
        ui->txChannelGroup->setCurrentIndex((int) m_settings.m_txChannels);
    }

    highlightApplyButton(true);
}

void LimeRFEGUI::on_rxChannel_currentIndexChanged(int index)
{
    if (m_settings.m_rxChannels == LimeRFESettings::ChannelsWideband)
    {
        m_settings.m_rxWidebandChannel = (LimeRFESettings::WidebandChannel) index;
        m_settingsKeys.append("rxWidebandChannel");
    }
    else if (m_settings.m_rxChannels == LimeRFESettings::ChannelsHAM)
    {
        m_settings.m_rxHAMChannel = (LimeRFESettings::HAMChannel) index;
        m_settingsKeys.append("rxHAMChannel");
    }
    else if (m_settings.m_rxChannels == LimeRFESettings::ChannelsCellular)
    {
        m_settings.m_rxCellularChannel = (LimeRFESettings::CellularChannel) index;
        m_settingsKeys.append("rxCellularChannel");
    }

    setRxChannels();

    if (m_settings.m_txRxDriven)
    {
        m_settings.m_txWidebandChannel = m_settings.m_rxWidebandChannel;
        m_settings.m_txHAMChannel = m_settings.m_rxHAMChannel;
        m_settings.m_txCellularChannel = m_settings.m_rxCellularChannel;
        m_settingsKeys.append("txWidebandChannel");
        m_settingsKeys.append("txHAMChannel");
        m_settingsKeys.append("txCellularChannel");
        setTxChannels();
    }

    highlightApplyButton(true);
}

void LimeRFEGUI::on_rxPort_currentIndexChanged(int index)
{
    m_settings.m_rxPort = (LimeRFESettings::RxPort) index;
    m_settingsKeys.append("rxPort");
    highlightApplyButton(true);
}

void LimeRFEGUI::on_txFollowsRx_clicked()
{
    bool checked = ui->txFollowsRx->isChecked();
    m_settings.m_txRxDriven = checked;
    ui->txChannelGroup->setEnabled(!checked);
    ui->txChannel->setEnabled(!checked);
    m_settings.m_txChannels = m_settings.m_rxChannels;
    m_settings.m_txWidebandChannel = m_settings.m_rxWidebandChannel;
    m_settings.m_txHAMChannel = m_settings.m_rxHAMChannel;
    m_settings.m_txCellularChannel = m_settings.m_rxCellularChannel;
    m_settingsKeys.append("txRxDriven");
    m_settingsKeys.append("txChannels");
    m_settingsKeys.append("txWidebandChannel");
    m_settingsKeys.append("txHAMChannel");
    m_settingsKeys.append("txCellularChannel");
    ui->txChannelGroup->setCurrentIndex((int) m_settings.m_txChannels);

    if (checked) {
        highlightApplyButton(true);
    }
}

void LimeRFEGUI::on_txChannelGroup_currentIndexChanged(int index)
{
    m_settings.m_txChannels = (LimeRFESettings::ChannelGroups) index;
    m_settingsKeys.append("txChannels");
    setTxChannels();
    highlightApplyButton(true);
}

void LimeRFEGUI::on_txChannel_currentIndexChanged(int index)
{
    if (m_settings.m_txChannels == LimeRFESettings::ChannelsWideband)
    {
        m_settings.m_txWidebandChannel = (LimeRFESettings::WidebandChannel) index;
        m_settingsKeys.append("txWidebandChannel");
    }
    else if (m_settings.m_txChannels == LimeRFESettings::ChannelsHAM)
    {
        m_settings.m_txHAMChannel = (LimeRFESettings::HAMChannel) index;
        m_settingsKeys.append("txHAMChannel");
    }
    else if (m_settings.m_txChannels == LimeRFESettings::ChannelsCellular)
    {
        m_settings.m_txCellularChannel = (LimeRFESettings::CellularChannel) index;
        m_settingsKeys.append("txCellularChannel");
    }

    setTxChannels();
    highlightApplyButton(true);
}

void LimeRFEGUI::on_txPort_currentIndexChanged(int index)
{
    m_settings.m_txPort = (LimeRFESettings::TxPort) index;
    m_settingsKeys.append("txPort");
    highlightApplyButton(true);
}

void LimeRFEGUI::on_powerEnable_clicked()
{
    m_settings.m_swrEnable = ui->powerEnable->isChecked();
    m_settingsKeys.append("swrEnable");
    highlightApplyButton(true);
}

void LimeRFEGUI::on_powerSource_currentIndexChanged(int index)
{
    m_settings.m_swrSource = (LimeRFESettings::SWRSource) index;
    m_settingsKeys.append("swrSource");
    highlightApplyButton(true);
}

void LimeRFEGUI::on_powerRefresh_clicked()
{
    refreshPower();
}

void LimeRFEGUI::on_powerAutoRefresh_toggled(bool checked)
{
    if (checked)
    {
        connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
        m_timer.start();
    }
    else
    {
        m_timer.stop();
        disconnect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
    }
}

void LimeRFEGUI::on_powerAbsAvg_clicked()
{
    m_avgPower = ui->powerAbsAvg->isChecked();
}

void LimeRFEGUI::on_powerCorrValue_textEdited(const QString &text)
{
    bool ok;
    double powerCorrection = text.toDouble(&ok);

    if (ok)
    {
        setPowerCorrection(powerCorrection);
        m_currentPowerCorrection = powerCorrection;
        updateAbsPower(powerCorrection);
    }
}

void LimeRFEGUI::on_deviceSetRefresh_clicked()
{
    updateDeviceSetList();
}

void LimeRFEGUI::on_deviceSetSync_clicked()
{
    m_deviceSetSync = ui->deviceSetSync->isChecked();

    if (m_deviceSetSync) {
        syncRxTx();
    }
}

void LimeRFEGUI::syncRxTx()
{
    if (!m_txOn) {
        stopStartTx(m_txOn);
    }

    stopStartRx(m_rxOn);

    if (m_txOn) {
        stopStartTx(m_txOn);
    }
}

void LimeRFEGUI::stopStartRx(bool start)
{
    if (ui->deviceSetRx->currentIndex() < 0) {
        return;
    }

    int deviceSetIndex = ui->deviceSetRx->currentData().toInt();
    m_limeRFE->turnDevice(deviceSetIndex, start);
}

void LimeRFEGUI::stopStartTx(bool start)
{
    if (ui->deviceSetTx->currentIndex() < 0) {
        return;
    }

    int deviceSetIndex = ui->deviceSetTx->currentData().toInt();
    m_limeRFE->turnDevice(deviceSetIndex, start);
}

void LimeRFEGUI::on_modeRx_toggled(bool checked)
{
    int rc;
    ui->statusText->clear();
    m_rxOn = checked;

    if (m_rxTxToggle)
    {
        m_txOn = !checked;

        if (checked) // Rx on
        {
            rc = m_limeRFE->setTx(false); // stop Tx first
            ui->statusText->append(QString("Stop TX: %1").arg(m_limeRFE->getError(rc).c_str()));
        }

        rc = m_limeRFE->setRx(m_rxOn); // Rx on or off
        ui->statusText->append(QString("RX: %1").arg(m_limeRFE->getError(rc).c_str()));

        if (!checked) // Rx off
        {
            rc = m_limeRFE->setTx(true); // start Tx next
            ui->statusText->append(QString("Start TX: %1").arg(m_limeRFE->getError(rc).c_str()));
        }
    }
    else
    {
        rc = m_limeRFE->setRx(m_rxOn);
        ui->statusText->setText(m_limeRFE->getError(rc).c_str());
    }

    if (m_deviceSetSync) {
        syncRxTx();
    }

    displayMode();
}

void LimeRFEGUI::on_modeTx_toggled(bool checked)
{
    int rc;
    ui->statusText->clear();
    m_txOn = checked;

    if (m_rxTxToggle)
    {
        m_rxOn = !checked;

        if (checked) // Tx on
        {
            rc = m_limeRFE->setRx(false); // stop Rx first
            ui->statusText->append(QString("Stop RX: %1").arg(m_limeRFE->getError(rc).c_str()));
        }

        rc = m_limeRFE->setTx(m_txOn); // Tx on or off
        ui->statusText->append(QString("TX: %1").arg(m_limeRFE->getError(rc).c_str()));

        if (!checked) // Tx off
        {
            rc = m_limeRFE->setRx(true); // start Rx next
            ui->statusText->append(QString("Start RX: %1").arg(m_limeRFE->getError(rc).c_str()));
        }
    }
    else
    {
        rc = m_limeRFE->setTx(m_txOn);
        ui->statusText->setText(m_limeRFE->getError(rc).c_str());
    }

    if (m_deviceSetSync) {
        syncRxTx();
    }

    displayMode();
}

void LimeRFEGUI::on_rxTxToggle_clicked()
{
    m_rxTxToggle = ui->rxTxToggle->isChecked();

    if (m_rxTxToggle && m_rxOn && m_txOn)
    {
        m_txOn = false;
        int rc = m_limeRFE->setTx(m_txOn);
        ui->statusText->setText(m_limeRFE->getError(rc).c_str());
        displayMode();

        if (m_deviceSetSync) {
            syncRxTx();
        }
    }
}

void LimeRFEGUI::on_attenuation_currentIndexChanged(int index)
{
    m_settings.m_attenuationFactor = index;
    m_settingsKeys.append("attenuationFactor");
    highlightApplyButton(true);
}

void LimeRFEGUI::on_amFmNotchFilter_clicked()
{
    m_settings.m_amfmNotch = ui->amFmNotchFilter->isChecked();
    m_settingsKeys.append("amfmNotch");
    highlightApplyButton(true);
}

void LimeRFEGUI::on_apply_clicked()
{
    ui->statusText->clear();
    m_limeRFE->settingsToState(m_settings);
    int rc = m_limeRFE->configure();
    ui->statusText->setText(m_limeRFE->getError(rc).c_str());
    highlightApplyButton(false);
}

void LimeRFEGUI::tick()
{
    refreshPower();
}

bool LimeRFEGUI::handleMessage(const Message& message)
{
    if (LimeRFE::MsgConfigureLimeRFE::match(message))
    {
        qDebug("LimeRFEGUI::handleMessage: LimeRFE::MsgConfigureLimeRFE");
        const LimeRFE::MsgConfigureLimeRFE& cfg = (LimeRFE::MsgConfigureLimeRFE&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        displaySettings();
        highlightApplyButton(cfg.getForce());
        return true;
    }
    else if (LimeRFE::MsgReportSetRx::match(message))
    {
        bool on = ((LimeRFE::MsgReportSetRx&) message).isOn();
        qDebug("LimeRFEGUI::handleMessage: LimeRFE::MsgReportSetRx: %s", on ? "on" : "off");
        m_rxOn = on;
        displaySettings();
        return true;
    }
    else if (LimeRFE::MsgReportSetTx::match(message))
    {
        bool on = ((LimeRFE::MsgReportSetTx&) message).isOn();
        qDebug("LimeRFEGUI::handleMessage: LimeRFE::MsgReportSetTx: %s", on ? "on" : "off");
        m_txOn = on;
        displaySettings();
        return true;
    }

	return false;
}

void LimeRFEGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void LimeRFEGUI::makeUIConnections()
{
    QObject::connect(ui->openDevice, &QPushButton::clicked, this, &LimeRFEGUI::on_openDevice_clicked);
    QObject::connect(ui->closeDevice, &QPushButton::clicked, this, &LimeRFEGUI::on_closeDevice_clicked);
    QObject::connect(ui->deviceToGUI, &QPushButton::clicked, this, &LimeRFEGUI::on_deviceToGUI_clicked);
    QObject::connect(ui->rxChannelGroup, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeRFEGUI::on_rxChannelGroup_currentIndexChanged);
    QObject::connect(ui->rxChannel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeRFEGUI::on_rxChannel_currentIndexChanged);
    QObject::connect(ui->rxPort, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeRFEGUI::on_rxPort_currentIndexChanged);
    QObject::connect(ui->attenuation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeRFEGUI::on_attenuation_currentIndexChanged);
    QObject::connect(ui->amFmNotchFilter, &QCheckBox::clicked, this, &LimeRFEGUI::on_amFmNotchFilter_clicked);
    QObject::connect(ui->txFollowsRx, &QCheckBox::clicked, this, &LimeRFEGUI::on_txFollowsRx_clicked);
    QObject::connect(ui->txChannelGroup, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeRFEGUI::on_txChannelGroup_currentIndexChanged);
    QObject::connect(ui->txChannel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeRFEGUI::on_txChannel_currentIndexChanged);
    QObject::connect(ui->txPort, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeRFEGUI::on_txPort_currentIndexChanged);
    QObject::connect(ui->powerEnable, &QCheckBox::clicked, this, &LimeRFEGUI::on_powerEnable_clicked);
    QObject::connect(ui->powerSource, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeRFEGUI::on_powerSource_currentIndexChanged);
    QObject::connect(ui->powerRefresh, &QPushButton::clicked, this, &LimeRFEGUI::on_powerRefresh_clicked);
    QObject::connect(ui->powerAutoRefresh, &ButtonSwitch::toggled, this, &LimeRFEGUI::on_powerAutoRefresh_toggled);
    QObject::connect(ui->powerAbsAvg, &QCheckBox::clicked, this, &LimeRFEGUI::on_powerAbsAvg_clicked);
    QObject::connect(ui->powerCorrValue, &QLineEdit::textEdited, this, &LimeRFEGUI::on_powerCorrValue_textEdited);
    QObject::connect(ui->modeRx, &QToolButton::toggled, this, &LimeRFEGUI::on_modeRx_toggled);
    QObject::connect(ui->modeTx, &QToolButton::toggled, this, &LimeRFEGUI::on_modeTx_toggled);
    QObject::connect(ui->rxTxToggle, &QCheckBox::clicked, this, &LimeRFEGUI::on_rxTxToggle_clicked);
    QObject::connect(ui->deviceSetRefresh, &QPushButton::clicked, this, &LimeRFEGUI::on_deviceSetRefresh_clicked);
    QObject::connect(ui->deviceSetSync, &QCheckBox::clicked, this, &LimeRFEGUI::on_deviceSetSync_clicked);
    QObject::connect(ui->apply, &QPushButton::clicked, this, &LimeRFEGUI::on_apply_clicked);
}
