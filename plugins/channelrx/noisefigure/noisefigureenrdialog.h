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

#include "ui_noisefigureenrdialog.h"
#include "noisefiguresettings.h"

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

private:
    Ui::NoiseFigureENRDialog* ui;
    void addRow(double freq, double enr);
};

#endif // INCLUDE_NOISEFIGUREENRDIALOG_H
