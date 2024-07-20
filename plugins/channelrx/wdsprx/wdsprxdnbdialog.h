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
#ifndef INCLUDE_WDSPRXDNBDIALOG_H
#define INCLUDE_WDSPRXDNBDIALOG_H

#include <QDialog>

#include "wdsprxsettings.h"

namespace Ui {
    class WDSPRxDNBDialog;
}

class WDSPRxDNBDialog : public QDialog {
    Q_OBJECT
public:
    enum ValueChanged {
        ChangedNB,
        ChangedNB2Mode,
        ChangedNBSlewTime,
        ChangedNBLeadTime,
        ChangedNBLagTime,
        ChangedNBThreshold,
        ChangedNBAvgTime,
    };

    explicit WDSPRxDNBDialog(QWidget* parent = nullptr);
    ~WDSPRxDNBDialog() override;

    void setNBScheme(WDSPRxProfile::WDSPRxNBScheme scheme);
    void setNB2Mode(WDSPRxProfile::WDSPRxNB2Mode mode);
    void setNBSlewTime(double time);
    void setNBLeadTime(double time);
    void setNBLagTime(double time);
    void setNBThreshold(int threshold);
    void setNBAvgTime(double time);

    WDSPRxProfile::WDSPRxNBScheme getNBScheme() const { return m_nbScheme; }
    WDSPRxProfile::WDSPRxNB2Mode getNB2Mode() const { return m_nb2Mode; }
    double getNBSlewTime() const { return m_nbSlewTime; }
    double getNBLeadTime() const { return m_nbLeadTime; }
    double getNBLagTime() const { return m_nbLagTime; }
    int getNBThreshold() const { return m_nbThreshold; }
    double getNBAvgTime() const { return m_nbAvgTime; }

signals:
    void valueChanged(int valueChanged);

private:
    Ui::WDSPRxDNBDialog *ui;
    WDSPRxProfile::WDSPRxNBScheme m_nbScheme;
    WDSPRxProfile::WDSPRxNB2Mode m_nb2Mode;
    double m_nbSlewTime;
    double m_nbLeadTime;
    double m_nbLagTime;
    int m_nbThreshold;
    double m_nbAvgTime;

private slots:
    void on_nb_currentIndexChanged(int index);
    void on_nb2Mode_currentIndexChanged(int index);
    void on_nbSlewTime_valueChanged(double value);
    void on_nbLeadTime_valueChanged(double value);
    void on_nbLagTime_valueChanged(double value);
    void on_nbThreshold_valueChanged(int value);
    void on_nbAvgTime_valueChanged(double value);
};

#endif // INCLUDE_WDSPRXDNBDIALOG_H
