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

#ifndef SDRGUI_LIMERFEGUI_LIMERFEUSBDIALOG_H_
#define SDRGUI_LIMERFEGUI_LIMERFEUSBDIALOG_H_

#include <vector>

#include <QDialog>
#include <QTimer>

#include "util/movingaverage.h"
#include "limerfe/limerfecontroller.h"
#include "limerfe/limerfeusbcalib.h"
#include "export.h"

class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;
class MainWindow;

namespace Ui {
    class LimeRFEUSBDialog;
}

class SDRGUI_API LimeRFEUSBDialog : public QDialog {
    Q_OBJECT

public:
    explicit LimeRFEUSBDialog(LimeRFEUSBCalib& limeRFEUSBCalib, MainWindow* mainWindow);
    ~LimeRFEUSBDialog();

private:
    void displaySettings();
    void displayMode();
    void displayPower();
    void refreshPower();
    void setRxChannels();
    void setTxChannels();
    int getPowerCorectionIndex();
    double getPowerCorrection();
    void setPowerCorrection(double dbValue);
    void updateAbsPower(double powerCorrDB);
    void updateDeviceSetList();
    void stopStartRx(bool start);
    void stopStartTx(bool start);
    void syncRxTx();
    void highlightApplyButton(bool highlight);

    Ui::LimeRFEUSBDialog* ui;
    MainWindow *m_mainWindow;
    LimeRFEController m_controller;
    LimeRFEController::LimeRFESettings m_settings;
    LimeRFEUSBCalib& m_limeRFEUSBCalib;
    bool m_rxTxToggle;
    QTimer m_timer;
    double m_currentPowerCorrection;
    bool m_avgPower;
    MovingAverageUtil<double, double, 10> m_powerMovingAverage;
    bool m_deviceSetSync;
    int m_rxDeviceSetSequence;
    int m_txDeviceSetSequence;
    std::vector<DSPDeviceSourceEngine*> m_sourceEngines;
    std::vector<int> m_rxDeviceSetIndex;
    std::vector<DSPDeviceSinkEngine*> m_sinkEngines;
    std::vector<int> m_txDeviceSetIndex;

private slots:
    void on_openDevice_clicked();
    void on_closeDevice_clicked();
    void on_deviceToGUI_clicked();
    void on_rxChannelGroup_currentIndexChanged(int index);
    void on_rxChannel_currentIndexChanged(int index);
    void on_rxPort_currentIndexChanged(int index);
    void on_attenuation_currentIndexChanged(int index);
    void on_amFmNotchFilter_clicked();
    void on_txFollowsRx_clicked();
    void on_txChannelGroup_currentIndexChanged(int index);
    void on_txChannel_currentIndexChanged(int index);
    void on_txPort_currentIndexChanged(int index);
    void on_powerEnable_clicked();
    void on_powerSource_currentIndexChanged(int index);
    void on_powerRefresh_clicked();
    void on_powerAutoRefresh_toggled(bool checked);
    void on_powerAbsAvg_clicked();
    void on_powerCorrValue_textEdited(const QString &text);
    void on_modeRx_toggled(bool checked);
    void on_modeTx_toggled(bool checked);
    void on_rxTxToggle_clicked();
    void on_deviceSetRefresh_clicked();
    void on_deviceSetSync_clicked();
    void on_apply_clicked();
    void tick();
};

#endif // SDRGUI_LIMERFEGUI_LIMERFEUSBDIALOG_H_
