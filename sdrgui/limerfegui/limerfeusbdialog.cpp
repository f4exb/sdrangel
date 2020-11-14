///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <vector>
#include "util/serialutil.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "mainwindow.h"
#include "device/deviceuiset.h"
#include "gui/doublevalidator.h"

#include "limerfeusbdialog.h"
#include "ui_limerfeusbdialog.h"

LimeRFEUSBDialog::LimeRFEUSBDialog(LimeRFEUSBCalib& limeRFEUSBCalib, MainWindow* mainWindow) :
    QDialog(mainWindow),
    ui(new Ui::LimeRFEUSBDialog),
    m_limeRFEUSBCalib(limeRFEUSBCalib),
    m_rxTxToggle(false),
    m_currentPowerCorrection(0.0),
    m_avgPower(false),
    m_deviceSetSync(false)
{
    ui->setupUi(this);
    m_mainWindow = mainWindow;
    ui->powerCorrValue->setValidator(new DoubleValidator(-99.9, 99.9, 1, ui->powerCorrValue));
    std::vector<std::string> comPorts;
    SerialUtil::getComPorts(comPorts, "ttyUSB[0-9]+"); // regex is for Linux only

    for (std::vector<std::string>::const_iterator it = comPorts.begin(); it != comPorts.end(); ++it) {
        ui->device->addItem(QString(it->c_str()));
    }

    updateDeviceSetList();
    displaySettings(); // default values
    highlightApplyButton(false);
    m_timer.setInterval(500);
}

LimeRFEUSBDialog::~LimeRFEUSBDialog()
{
    delete ui;
}

void LimeRFEUSBDialog::displaySettings()
{
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
}

void LimeRFEUSBDialog::displayMode()
{
    QString s;

    if (m_settings.m_rxOn)
    {
        if (m_settings.m_txOn) {
            s = "Rx/Tx";
        } else {
            s = "Rx";
        }
    }
    else
    {
        if (m_settings.m_txOn) {
            s = "Tx";
        } else {
            s = "None";
        }
    }

    ui->modeText->setText(s);

    ui->modeRx->blockSignals(true);
    ui->modeTx->blockSignals(true);

    if (m_settings.m_rxOn) {
		ui->modeRx->setStyleSheet("QToolButton { background-color : green; }");
    } else {
		ui->modeRx->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    if (m_settings.m_txOn) {
        ui->modeTx->setStyleSheet("QToolButton { background-color : red; }");
    } else {
		ui->modeTx->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    ui->modeRx->setChecked(m_settings.m_rxOn);
    ui->modeTx->setChecked(m_settings.m_txOn);

    ui->modeRx->blockSignals(false);
    ui->modeTx->blockSignals(false);
}

void LimeRFEUSBDialog::displayPower()
{
    ui->powerEnable->blockSignals(true);
    ui->powerSource->blockSignals(true);

    ui->powerEnable->setChecked(m_settings.m_swrEnable);
    ui->powerSource->setCurrentIndex((int) m_settings.m_swrSource);

    ui->powerEnable->blockSignals(false);
    ui->powerSource->blockSignals(false);
}

void LimeRFEUSBDialog::refreshPower()
{
    int fwdPower, refPower;
    int rc = m_controller.getFwdPower(fwdPower);

    if (rc != 0)
    {
        ui->statusText->setText(m_controller.getError(rc).c_str());
        return;
    }

    rc = m_controller.getRefPower(refPower);

    if (rc != 0)
    {
        ui->statusText->setText(m_controller.getError(rc).c_str());
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

void LimeRFEUSBDialog::setRxChannels()
{
    ui->rxChannel->blockSignals(true);
    ui->rxPort->blockSignals(true);
    ui->rxChannel->clear();
    ui->rxPort->clear();

    if (m_settings.m_rxChannels == LimeRFEController::ChannelsWideband)
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
    else if (m_settings.m_rxChannels == LimeRFEController::ChannelsHAM)
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
            case LimeRFEController::HAM_30M:
            case LimeRFEController::HAM_50_70MHz:
            case LimeRFEController::HAM_144_146MHz:
            case LimeRFEController::HAM_220_225MHz:
            case LimeRFEController::HAM_430_440MHz:
                ui->rxPort->addItem("TX/RX (J3)");
                ui->rxPort->addItem("TX/RX 30M (J5)");
                ui->rxPort->setEnabled(true);
                ui->rxPort->setCurrentIndex((int) m_settings.m_rxPort);
                break;
            case LimeRFEController::HAM_902_928MHz:
            case LimeRFEController::HAM_1240_1325MHz:
            case LimeRFEController::HAM_2300_2450MHz:
            case LimeRFEController::HAM_3300_3500MHz:
                ui->rxPort->addItem("TX/RX (J3)");
                ui->rxPort->setEnabled(false);
                m_settings.m_rxPort = LimeRFEController::RxPortJ3;
                ui->rxPort->setCurrentIndex((int) m_settings.m_rxPort);
                break;
            default:
                break;
        }
    }
    else if (m_settings.m_rxChannels == LimeRFEController::ChannelsCellular)
    {
        ui->rxChannel->addItem("Band1");
        ui->rxChannel->addItem("Band2");
        ui->rxChannel->addItem("Band3");
        ui->rxChannel->addItem("Band7");
        ui->rxChannel->addItem("Band38");
        ui->rxChannel->setCurrentIndex((int) m_settings.m_rxCellularChannel);
        ui->rxPort->addItem("TX/RX (J3)");
        ui->rxPort->setEnabled(false);
        m_settings.m_rxPort = LimeRFEController::RxPortJ3;
        ui->rxPort->setCurrentIndex((int) m_settings.m_rxPort);
        m_settings.m_txRxDriven = true;
        ui->txFollowsRx->setEnabled(false);
        ui->txFollowsRx->setChecked(m_settings.m_txRxDriven);
    }

    ui->rxChannelGroup->setCurrentIndex((int) m_settings.m_rxChannels);
    ui->rxPort->blockSignals(false);
    ui->rxChannel->blockSignals(false);
}

void LimeRFEUSBDialog::setTxChannels()
{
    ui->txChannel->blockSignals(true);
    ui->txPort->blockSignals(true);
    ui->powerCorrValue->blockSignals(true);

    ui->txChannel->clear();
    ui->txPort->clear();

    if (m_settings.m_txChannels == LimeRFEController::ChannelsWideband)
    {
        ui->txChannel->addItem("1-1000MHz");
        ui->txChannel->addItem("1-4GHz");
        ui->txChannel->setCurrentIndex((int) m_settings.m_txWidebandChannel);
        ui->txPort->addItem("TX/RX (J3)");
        ui->txPort->addItem("TX (J4)");
        ui->txPort->setCurrentIndex((int) m_settings.m_txPort);
        ui->txPort->setEnabled(true);
    }
    else if (m_settings.m_txChannels == LimeRFEController::ChannelsHAM)
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
            case LimeRFEController::HAM_30M:
            case LimeRFEController::HAM_50_70MHz:
                ui->txPort->addItem("TX/RX (J3)");
                ui->txPort->addItem("TX (J4)");
                ui->txPort->addItem("TX/RX 30M (J5)");
                ui->txPort->setEnabled(false);
                m_settings.m_txPort = LimeRFEController::TxPortJ5;
                ui->txPort->setCurrentIndex((int) m_settings.m_txPort);
                break;
            case LimeRFEController::HAM_144_146MHz:
            case LimeRFEController::HAM_220_225MHz:
            case LimeRFEController::HAM_430_440MHz:
            case LimeRFEController::HAM_902_928MHz:
            case LimeRFEController::HAM_1240_1325MHz:
            case LimeRFEController::HAM_2300_2450MHz:
            case LimeRFEController::HAM_3300_3500MHz:
                ui->txPort->addItem("TX/RX (J3)");
                ui->txPort->addItem("TX (J4)");
                ui->txPort->setCurrentIndex(m_settings.m_txPort < 2 ? m_settings.m_txPort : 1);
                ui->txPort->setEnabled(true);
                break;
            default:
                break;
        }
    }
    else if (m_settings.m_txChannels == LimeRFEController::ChannelsCellular)
    {
        ui->txChannel->addItem("Band1");
        ui->txChannel->addItem("Band2");
        ui->txChannel->addItem("Band3");
        ui->txChannel->addItem("Band7");
        ui->txChannel->addItem("Band38");
        ui->txChannel->setCurrentIndex((int) m_settings.m_txCellularChannel);
        ui->txPort->addItem("TX/RX (J3)");
        m_settings.m_txPort = LimeRFEController::TxPortJ3;
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

int LimeRFEUSBDialog::getPowerCorectionIndex()
{
    LimeRFEUSBCalib::ChannelRange range;

    switch (m_settings.m_txChannels)
    {
        case LimeRFEController::ChannelsWideband:
        {
            switch (m_settings.m_txWidebandChannel)
            {
                case LimeRFEController::WidebandLow:
                    range = LimeRFEUSBCalib::WidebandLow;
                    break;
                case LimeRFEController::WidebandHigh:
                    range = LimeRFEUSBCalib::WidebandHigh;
                    break;
                default:
                    return -1;
                    break;
            }
            break;
        }
        case LimeRFEController::ChannelsHAM:
        {
            switch (m_settings.m_txHAMChannel)
            {
                case LimeRFEController::HAM_30M:
                    range = LimeRFEUSBCalib::HAM_30MHz;
                    break;
                case LimeRFEController::HAM_50_70MHz:
                    range = LimeRFEUSBCalib::HAM_50_70MHz;
                    break;
                case LimeRFEController::HAM_144_146MHz:
                    range = LimeRFEUSBCalib::HAM_144_146MHz;
                    break;
                case LimeRFEController::HAM_220_225MHz:
                    range = LimeRFEUSBCalib::HAM_220_225MHz;
                    break;
                case LimeRFEController::HAM_430_440MHz:
                    range = LimeRFEUSBCalib::HAM_430_440MHz;
                    break;
                case LimeRFEController::HAM_902_928MHz:
                    range = LimeRFEUSBCalib::HAM_902_928MHz;
                    break;
                case LimeRFEController::HAM_1240_1325MHz:
                    range = LimeRFEUSBCalib::HAM_1240_1325MHz;
                    break;
                case LimeRFEController::HAM_2300_2450MHz:
                    range = LimeRFEUSBCalib::HAM_2300_2450MHz;
                    break;
                case LimeRFEController::HAM_3300_3500MHz:
                    range = LimeRFEUSBCalib::HAM_3300_3500MHz;
                    break;
                default:
                    return -1;
                    break;
            }
            break;
        }
        case LimeRFEController::ChannelsCellular:
        {
            switch (m_settings.m_txCellularChannel)
            {
                case LimeRFEController::CellularBand1:
                    range = LimeRFEUSBCalib::CellularBand1;
                    break;
                case LimeRFEController::CellularBand2:
                    range = LimeRFEUSBCalib::CellularBand2;
                    break;
                case LimeRFEController::CellularBand3:
                    range = LimeRFEUSBCalib::CellularBand3;
                    break;
                case LimeRFEController::CellularBand7:
                    range = LimeRFEUSBCalib::CellularBand7;
                    break;
                case LimeRFEController::CellularBand38:
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

double LimeRFEUSBDialog::getPowerCorrection()
{
    int index = getPowerCorectionIndex();

    QMap<int, double>::const_iterator it = m_limeRFEUSBCalib.m_calibrations.find(index);

    if (it != m_limeRFEUSBCalib.m_calibrations.end()) {
        return it.value();
    } else {
        return 0.0;
    }
}

void LimeRFEUSBDialog::setPowerCorrection(double dbValue)
{
    int index = getPowerCorectionIndex();

    if (index < 0) {
        return;
    }

    m_limeRFEUSBCalib.m_calibrations[index] = dbValue;
}

void LimeRFEUSBDialog::on_openDevice_clicked()
{
    int rc = m_controller.openDevice(ui->device->currentText().toStdString());
    ui->statusText->append(QString("Open %1: %2").arg(ui->device->currentText()).arg(m_controller.getError(rc).c_str()));

    if (rc != 0) {
        return;
    }

    rc = m_controller.getState();
    ui->statusText->append(QString("Get state: %1").arg(m_controller.getError(rc).c_str()));
}

void LimeRFEUSBDialog::on_closeDevice_clicked()
{
    ui->statusText->clear();
    m_controller.closeDevice();
    ui->statusText->setText("Closed");
}

void LimeRFEUSBDialog::on_deviceToGUI_clicked()
{
    int rc = m_controller.getState();

    if (rc != 0)
    {
        ui->statusText->setText(m_controller.getError(rc).c_str());
        return;
    }

    m_controller.stateToSettings(m_settings);
    displaySettings();
    highlightApplyButton(false);
}

void LimeRFEUSBDialog::on_rxChannelGroup_currentIndexChanged(int index)
{
    m_settings.m_rxChannels = (LimeRFEController::ChannelGroups) index;
    setRxChannels();

    if (m_settings.m_txRxDriven)
    {
        m_settings.m_txChannels = m_settings.m_rxChannels;
        ui->txChannelGroup->setCurrentIndex((int) m_settings.m_txChannels);
    }

    highlightApplyButton(true);
}

void LimeRFEUSBDialog::on_rxChannel_currentIndexChanged(int index)
{
    if (m_settings.m_rxChannels == LimeRFEController::ChannelsWideband) {
        m_settings.m_rxWidebandChannel = (LimeRFEController::WidebandChannel) index;
    } else if (m_settings.m_rxChannels == LimeRFEController::ChannelsHAM) {
        m_settings.m_rxHAMChannel = (LimeRFEController::HAMChannel) index;
    } else if (m_settings.m_rxChannels == LimeRFEController::ChannelsCellular) {
        m_settings.m_rxCellularChannel = (LimeRFEController::CellularChannel) index;
    }

    setRxChannels();

    if (m_settings.m_txRxDriven)
    {
        m_settings.m_txWidebandChannel = m_settings.m_rxWidebandChannel;
        m_settings.m_txHAMChannel = m_settings.m_rxHAMChannel;
        m_settings.m_txCellularChannel = m_settings.m_rxCellularChannel;
        setTxChannels();
    }

    highlightApplyButton(true);
}

void LimeRFEUSBDialog::on_rxPort_currentIndexChanged(int index)
{
    m_settings.m_rxPort = (LimeRFEController::RxPort) index;
    highlightApplyButton(true);
}

void LimeRFEUSBDialog::on_txFollowsRx_clicked()
{
    bool checked = ui->txFollowsRx->isChecked();
    m_settings.m_txRxDriven = checked;
    ui->txChannelGroup->setEnabled(!checked);
    ui->txChannel->setEnabled(!checked);
    m_settings.m_txChannels = m_settings.m_rxChannels;
    m_settings.m_txWidebandChannel = m_settings.m_rxWidebandChannel;
    m_settings.m_txHAMChannel = m_settings.m_rxHAMChannel;
    m_settings.m_txCellularChannel = m_settings.m_rxCellularChannel;
    ui->txChannelGroup->setCurrentIndex((int) m_settings.m_txChannels);

    if (checked) {
        highlightApplyButton(true);
    }
}

void LimeRFEUSBDialog::on_txChannelGroup_currentIndexChanged(int index)
{
    m_settings.m_txChannels = (LimeRFEController::ChannelGroups) index;
    setTxChannels();
    highlightApplyButton(true);
}

void LimeRFEUSBDialog::on_txChannel_currentIndexChanged(int index)
{
    if (m_settings.m_txChannels == LimeRFEController::ChannelsWideband) {
        m_settings.m_txWidebandChannel = (LimeRFEController::WidebandChannel) index;
    } else if (m_settings.m_txChannels == LimeRFEController::ChannelsHAM) {
        m_settings.m_txHAMChannel = (LimeRFEController::HAMChannel) index;
    } else if (m_settings.m_txChannels == LimeRFEController::ChannelsCellular) {
        m_settings.m_txCellularChannel = (LimeRFEController::CellularChannel) index;
    }

    setTxChannels();
    highlightApplyButton(true);
}

void LimeRFEUSBDialog::on_txPort_currentIndexChanged(int index)
{
    m_settings.m_txPort = (LimeRFEController::TxPort) index;
    highlightApplyButton(true);
}

void LimeRFEUSBDialog::on_powerEnable_clicked()
{
    m_settings.m_swrEnable = ui->powerEnable->isChecked();
    highlightApplyButton(true);
}

void LimeRFEUSBDialog::on_powerSource_currentIndexChanged(int index)
{
    m_settings.m_swrSource = (LimeRFEController::SWRSource) index;
    highlightApplyButton(true);
}

void LimeRFEUSBDialog::on_powerRefresh_clicked()
{
    refreshPower();
}

void LimeRFEUSBDialog::on_powerAutoRefresh_toggled(bool checked)
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

void LimeRFEUSBDialog::on_powerAbsAvg_clicked()
{
    m_avgPower = ui->powerAbsAvg->isChecked();
}

void LimeRFEUSBDialog::on_powerCorrValue_textEdited(const QString &text)
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

void LimeRFEUSBDialog::on_deviceSetRefresh_clicked()
{
    updateDeviceSetList();
}

void LimeRFEUSBDialog::on_deviceSetSync_clicked()
{
    m_deviceSetSync = ui->deviceSetSync->isChecked();

    if (m_deviceSetSync) {
        syncRxTx();
    }
}

void LimeRFEUSBDialog::syncRxTx()
{
    if (!m_settings.m_txOn) {
        stopStartTx(m_settings.m_txOn);
    }

    stopStartRx(m_settings.m_rxOn);

    if (m_settings.m_txOn) {
        stopStartTx(m_settings.m_txOn);
    }
}

void LimeRFEUSBDialog::stopStartRx(bool start)
{
    unsigned int rxDeviceSetSequence = ui->deviceSetRx->currentIndex();

    if (rxDeviceSetSequence >= m_sourceEngines.size()) {
        return;
    }

    DSPDeviceSourceEngine *deviceSourceEngine = m_sourceEngines[rxDeviceSetSequence];

    if (start)
    {
        if (deviceSourceEngine->initAcquisition()) {
            deviceSourceEngine->startAcquisition();
        }

        MainCore *mainCore = MainCore::instance();
        MainCore::MsgDeviceSetFocus *msg = MainCore::MsgDeviceSetFocus::create(m_rxDeviceSetIndex[rxDeviceSetSequence]);
        mainCore->getMainMessageQueue()->push(msg);
    }
    else
    {
        deviceSourceEngine->stopAcquistion();
    }
}

void LimeRFEUSBDialog::stopStartTx(bool start)
{
    unsigned int txDeviceSetSequence = ui->deviceSetTx->currentIndex();

    if (txDeviceSetSequence >= m_sinkEngines.size()) {
        return;
    }

    DSPDeviceSinkEngine *deviceSinkEngine = m_sinkEngines[txDeviceSetSequence];

    if (start)
    {
        if (deviceSinkEngine->initGeneration()) {
            deviceSinkEngine->startGeneration();
        }

        MainCore *mainCore = MainCore::instance();
        MainCore::MsgDeviceSetFocus *msg = MainCore::MsgDeviceSetFocus::create(m_txDeviceSetIndex[txDeviceSetSequence]);
        mainCore->getMainMessageQueue()->push(msg);
    }
    else
    {
        deviceSinkEngine->stopGeneration();
    }
}

void LimeRFEUSBDialog::updateAbsPower(double powerCorrDB)
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

void LimeRFEUSBDialog::updateDeviceSetList()
{
    std::vector<DeviceUISet*>& deviceUISets = m_mainWindow->getDeviceUISets();
    std::vector<DeviceUISet*>::const_iterator it = deviceUISets.begin();
    m_sourceEngines.clear();
    m_rxDeviceSetIndex.clear();
    m_sinkEngines.clear();
    m_txDeviceSetIndex.clear();
    ui->deviceSetRx->clear();
    ui->deviceSetTx->clear();
    unsigned int deviceIndex = 0;
    unsigned int rxIndex = 0;
    unsigned int txIndex = 0;

    for (; it != deviceUISets.end(); ++it, deviceIndex++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  (*it)->m_deviceSourceEngine;
        DSPDeviceSinkEngine *deviceSinkEngine = (*it)->m_deviceSinkEngine;

        if (deviceSourceEngine)
        {
            m_sourceEngines.push_back(deviceSourceEngine);
            m_rxDeviceSetIndex.push_back(deviceIndex);
            ui->deviceSetRx->addItem(QString("%1").arg(deviceIndex));
            rxIndex++;
        }
        else if (deviceSinkEngine)
        {
            m_sinkEngines.push_back(deviceSinkEngine);
            m_txDeviceSetIndex.push_back(deviceIndex);
            ui->deviceSetTx->addItem(QString("%1").arg(deviceIndex));
            txIndex++;
        }
    }
}

void LimeRFEUSBDialog::highlightApplyButton(bool highlight)
{
    if (highlight) {
        ui->apply->setStyleSheet("QPushButton { background-color : green; }");
    } else {
        ui->apply->setStyleSheet("QPushButton { background:rgb(79,79,79); }");
    }
}

void LimeRFEUSBDialog::on_modeRx_toggled(bool checked)
{
    int rc;
    ui->statusText->clear();
    m_settings.m_rxOn = checked;

    if (m_rxTxToggle)
    {
        m_settings.m_txOn = !checked;

        if (checked) // Rx on
        {
            rc = m_controller.setTx(m_settings, false); // stop Tx first
            ui->statusText->append(QString("Stop TX: %1").arg(m_controller.getError(rc).c_str()));
        }

        rc = m_controller.setRx(m_settings, m_settings.m_rxOn); // Rx on or off
        ui->statusText->append(QString("RX: %1").arg(m_controller.getError(rc).c_str()));

        if (!checked) // Rx off
        {
            rc = m_controller.setTx(m_settings, true); // start Tx next
            ui->statusText->append(QString("Start TX: %1").arg(m_controller.getError(rc).c_str()));
        }
    }
    else
    {
        rc = m_controller.setRx(m_settings, m_settings.m_rxOn);
        ui->statusText->setText(m_controller.getError(rc).c_str());
    }

    if (m_deviceSetSync) {
        syncRxTx();
    }

    displayMode();
}

void LimeRFEUSBDialog::on_modeTx_toggled(bool checked)
{
    int rc;
    ui->statusText->clear();
    m_settings.m_txOn = checked;

    if (m_rxTxToggle)
    {
        m_settings.m_rxOn = !checked;

        if (checked) // Tx on
        {
            rc = m_controller.setRx(m_settings, false); // stop Rx first
            ui->statusText->append(QString("Stop RX: %1").arg(m_controller.getError(rc).c_str()));
        }

        rc = m_controller.setTx(m_settings, m_settings.m_txOn); // Tx on or off
        ui->statusText->append(QString("TX: %1").arg(m_controller.getError(rc).c_str()));

        if (!checked) // Tx off
        {
            rc = m_controller.setRx(m_settings, true); // start Rx next
            ui->statusText->append(QString("Start RX: %1").arg(m_controller.getError(rc).c_str()));
        }
    }
    else
    {
        rc = m_controller.setTx(m_settings, m_settings.m_txOn);
        ui->statusText->setText(m_controller.getError(rc).c_str());
    }

    if (m_deviceSetSync) {
        syncRxTx();
    }

    displayMode();
}

void LimeRFEUSBDialog::on_rxTxToggle_clicked()
{
    m_rxTxToggle = ui->rxTxToggle->isChecked();

    if (m_rxTxToggle && m_settings.m_rxOn && m_settings.m_txOn)
    {
        m_settings.m_txOn = false;
        int rc = m_controller.setTx(m_settings, m_settings.m_txOn);
        ui->statusText->setText(m_controller.getError(rc).c_str());
        displayMode();

        if (m_deviceSetSync) {
            syncRxTx();
        }
    }
}

void LimeRFEUSBDialog::on_attenuation_currentIndexChanged(int index)
{
    m_settings.m_attenuationFactor = index;
    highlightApplyButton(true);
}

void LimeRFEUSBDialog::on_amFmNotchFilter_clicked()
{
    m_settings.m_amfmNotch = ui->amFmNotchFilter->isChecked();
    highlightApplyButton(true);
}

void LimeRFEUSBDialog::on_apply_clicked()
{
    ui->statusText->clear();
    m_controller.settingsToState(m_settings);
    int rc = m_controller.configure();
    ui->statusText->setText(m_controller.getError(rc).c_str());
    highlightApplyButton(false);
}

void LimeRFEUSBDialog::tick()
{
    refreshPower();
}
