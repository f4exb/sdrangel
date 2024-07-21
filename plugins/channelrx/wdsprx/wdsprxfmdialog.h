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
#ifndef INCLUDE_WDSPRXFMDIALOG_H
#define INCLUDE_WDSPRXFMDIALOG_H

#include <QDialog>

#include "wdsprxsettings.h"

namespace Ui {
    class WDSPRxFMDialog;
}

class WDSPRxFMDialog : public QDialog {
    Q_OBJECT
public:
    enum ValueChanged {
        ChangedDeviation,
        ChangedAFLow,
        ChangedAFHigh,
        ChangedAFLimiter,
        ChangedAFLimiterGain,
        ChangedCTCSSNotch,
        ChangedCTCSSNotchFrequency,
    };

    explicit WDSPRxFMDialog(QWidget* parent = nullptr);
    ~WDSPRxFMDialog() override;

    void setDeviation(double deviation);
    void setAFLow(double afLow);
    void setAFHigh(double afHigh);
    void setAFLimiter(bool afLimiter);
    void setAFLimiterGain(double gain);
    void setCTCSSNotch(bool notch);
    void setCTCSSNotchFrequency(double frequency);

    double getDeviation() const { return m_deviation; }
    double getAFLow() const { return m_afLow; }
    double getAFHigh() const { return m_afHigh; }
    bool getAFLimiter() const { return m_afLimiter; }
    double getAFLimiterGain() const { return m_afLimiterGain; }
    bool getCTCSSNotch() const { return m_ctcssNotch; }
    double getCTCSSNotchFrequency() { return m_ctcssNotchFrequency; }

signals:
    void valueChanged(int valueChanged);

private:
    Ui::WDSPRxFMDialog *ui;
    double m_deviation;
    double m_afLow;
    double m_afHigh;
    bool m_afLimiter;
    double m_afLimiterGain;
    bool m_ctcssNotch;
    double m_ctcssNotchFrequency;

private slots:
    void on_deviation_valueChanged(int value);
    void on_afLow_valueChanged(int value);
    void on_afHigh_valueChanged(int value);
    void on_afLimiter_clicked(bool checked);
    void on_afLimiterGain_valueChanged(int value);
    void on_ctcssNotch_clicked(bool checked);
    void on_ctcssNotchFrequency_valueChanged(int value);
};
#endif
