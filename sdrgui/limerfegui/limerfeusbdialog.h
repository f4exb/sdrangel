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

#include <QDialog>

#include "limerfe/limerfecontroller.h"
#include "export.h"

namespace Ui {
    class LimeRFEUSBDialog;
}

class SDRGUI_API LimeRFEUSBDialog : public QDialog {
    Q_OBJECT

public:
    explicit LimeRFEUSBDialog(QWidget* parent = nullptr);
    ~LimeRFEUSBDialog();

private:
    void displaySettings();
    void displayMode();
    void setRxChannels();
    void setTxChannels();

    Ui::LimeRFEUSBDialog* ui;
    LimeRFEController m_controller;
    LimeRFEController::LimeRFESettings m_settings;
    bool m_rxTxToggle;

private slots:
    void on_openDevice_clicked();
    void on_closeDevice_clicked();
    void on_deviceToGUI_clicked();
    void on_rxChannelGroup_currentIndexChanged(int index);
    void on_rxChannel_currentIndexChanged(int index);
    void on_rxPort_currentIndexChanged(int index);
    void on_txFollowsRx_clicked();
    void on_txChannelGroup_currentIndexChanged(int index);
    void on_txChannel_currentIndexChanged(int index);
    void on_txPort_currentIndexChanged(int index);
    void on_modeRx_toggled(bool checked);
    void on_modeTx_toggled(bool checked);
    void on_rxTxToggle_clicked();
    void on_apply_clicked();
};

#endif // SDRGUI_LIMERFEGUI_LIMERFEUSBDIALOG_H_
