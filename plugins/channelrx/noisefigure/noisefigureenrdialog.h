///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_NOISEFIGUREENRDIALOG_H
#define INCLUDE_NOISEFIGUREENRDIALOG_H

#include <QtCharts>

#include "ui_noisefigureenrdialog.h"
#include "noisefiguresettings.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;
#endif

class NoiseFigureENRDialog : public QDialog {
    Q_OBJECT

public:
    explicit NoiseFigureENRDialog(NoiseFigureSettings *settings, QWidget* parent = 0);
    ~NoiseFigureENRDialog();

    NoiseFigureSettings *m_settings;

    enum ENRCol {
        ENR_COL_FREQ,
        ENR_COL_ENR
    };

private slots:
    void accept();
    void on_addRow_clicked();
    void on_deleteRow_clicked();
    void on_enr_cellChanged(int row, int column);
    void on_start_valueChanged(int value);
    void on_stop_valueChanged(int value);

private:
    QChart *m_chart;
    Ui::NoiseFigureENRDialog* ui;
    void addRow(double freq, double enr);
    void plotChart();
};

#endif // INCLUDE_NOISEFIGUREENRDIALOG_H
