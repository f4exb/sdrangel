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
#ifndef INCLUDE_WDSPRXCWPEAKDIALOG_H
#define INCLUDE_WDSPRXCWPEAKDIALOG_H

#include <QDialog>

#include "wdsprxsettings.h"

namespace Ui {
    class WDSPRxCWPeakDialog;
}

class WDSPRxCWPeakDialog : public QDialog {
    Q_OBJECT
public:
    enum ValueChanged {
        ChangedCWPeakFrequency,
        ChangedCWBandwidth,
        ChangedCWGain
    };

    explicit WDSPRxCWPeakDialog(QWidget* parent = nullptr);
    ~WDSPRxCWPeakDialog() override;

    void setCWPeakFrequency(double cwPeakFrequency);
    void setCWBandwidth(double cwBandwidth);
    void setCWGain(double cwGain);
    double getCWPeakFrequency() const { return m_cwPeakFrequency; }
    double getCWBandwidth() const { return m_cwBandwidth; }
    double getCWGain() const { return m_cwGain; }

signals:
    void valueChanged(int valueChanged);

private:
    Ui::WDSPRxCWPeakDialog *ui;
    double m_cwPeakFrequency;
    double m_cwBandwidth;
    double m_cwGain;

private slots:
    void on_cwPeakFrequency_valueChanged(double value);
    void on_cwBandwidth_valueChanged(double value);
    void on_cwGain_valueChanged(double value);
};

#endif
