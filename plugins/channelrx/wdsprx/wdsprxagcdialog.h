///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#ifndef INCLUDE_WDSPRXAGCDIALOG_H
#define INCLUDE_WDSPRXAGCDIALOG_H

#include <QDialog>

#include "wdsprxsettings.h"

namespace Ui {
    class WDSPRxAGCDialog;
}

class WDSPRxAGCDialog : public QDialog {
    Q_OBJECT
public:
    enum ValueChanged {
        ChangedMode,
        ChangedSlope,
        ChangedHangThreshold,
    };

    explicit WDSPRxAGCDialog(QWidget* parent = nullptr);
    ~WDSPRxAGCDialog();

    void setAGCMode(WDSPRxProfile::WDSPRxAGCMode mode);
    void setAGCSlope(int slope);
    void setAGCHangThreshold(int hangThreshold);

    WDSPRxProfile::WDSPRxAGCMode getAGCMode() const { return m_agcMode; }
    int getAGCSlope() const { return m_agcSlope; }
    int getAGCHangThreshold() const { return m_agcHangThreshold; }

signals:
    void valueChanged(int valueChanged);

private:
    Ui::WDSPRxAGCDialog *ui;
    WDSPRxProfile::WDSPRxAGCMode m_agcMode;
    int m_agcSlope;         // centi-Bels 0 - 200
    int m_agcHangThreshold; // 0 - 100

private slots:
    void on_agcMode_currentIndexChanged(int index);
    void on_agcSlope_valueChanged(int value);
    void on_agcHangThreshold_valueChanged(int value);
};

#endif // INCLUDE_WDSPRXAGCDIALOG_H

