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
#ifndef INCLUDE_WDSPRXEQDIALOG_H
#define INCLUDE_WDSPRXEQDIALOG_H

#include <array>
#include <QDialog>

#include "wdsprxsettings.h"

namespace Ui {
    class WDSPRxEqDialog;
}

class WDSPRxEqDialog : public QDialog {
    Q_OBJECT
public:
    enum ValueChanged {
        ChangedFrequency,
        ChangedGain,
    };

    explicit WDSPRxEqDialog(QWidget* parent = nullptr);
    ~WDSPRxEqDialog() override;

    void setEqF(const std::array<float, 11>& eqF);
    void setEqG(const std::array<float, 11>& eqG);

    const std::array<float, 11>& getEqF() const { return m_eqF; }
    const std::array<float, 11>& getEqG() const { return m_eqG; }

signals:
    void valueChanged(int valueChanged);

private:
    Ui::WDSPRxEqDialog *ui;
    std::array<float, 11> m_eqF;
    std::array<float, 11> m_eqG;

private slots:
    void on_f1_valueChanged(int value);
    void on_f2_valueChanged(int value);
    void on_f3_valueChanged(int value);
    void on_f4_valueChanged(int value);
    void on_f5_valueChanged(int value);
    void on_f6_valueChanged(int value);
    void on_f7_valueChanged(int value);
    void on_f8_valueChanged(int value);
    void on_f9_valueChanged(int value);
    void on_f10_valueChanged(int value);
    void on_preampGain_valueChanged(int value);
    void on_f1Gain_valueChanged(int value);
    void on_f2Gain_valueChanged(int value);
    void on_f3Gain_valueChanged(int value);
    void on_f4Gain_valueChanged(int value);
    void on_f5Gain_valueChanged(int value);
    void on_f6Gain_valueChanged(int value);
    void on_f7Gain_valueChanged(int value);
    void on_f8Gain_valueChanged(int value);
    void on_f9Gain_valueChanged(int value);
    void on_f10Gain_valueChanged(int value);
};

#endif
