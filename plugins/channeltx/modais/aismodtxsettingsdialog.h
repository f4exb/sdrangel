///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_AISMODTXSETTINGSDIALOG_H
#define INCLUDE_AISMODTXSETTINGSDIALOG_H

#include "ui_aismodtxsettingsdialog.h"

class AISModTXSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit AISModTXSettingsDialog(int rampUpBits, int rampDownBits, int rampRange,
                        int baud, int symbolSpan,
                        bool rfNoise, bool writeToFile,
                        QWidget* parent = 0);
    ~AISModTXSettingsDialog();

   int m_rampUpBits;
   int m_rampDownBits;
   int m_rampRange;
   int m_baud;
   int m_symbolSpan;
   bool m_rfNoise;
   bool m_writeToFile;

private slots:
    void accept();

private:
    Ui::AISModTXSettingsDialog* ui;
};

#endif // INCLUDE_AISMODTXSETTINGSDIALOG_H
