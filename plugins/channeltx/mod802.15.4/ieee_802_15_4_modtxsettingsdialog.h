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

#ifndef INCLUDE_IEEE_802_15_4_MODTXSETTINGSDIALOG_H
#define INCLUDE_IEEE_802_15_4_MODTXSETTINGSDIALOG_H

#include "ui_ieee_802_15_4_modtxsettingsdialog.h"

class IEEE_802_15_4_ModTXSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit IEEE_802_15_4_ModTXSettingsDialog(int rampUpBits, int rampDownBits, int rampRange,
                        bool modulateWhileRamping, int modulation, int bitRate,
                        int pulseShaping, float beta, int symbolSpan,
                        bool scramble, int polynomial,
                        int lpfTaps, bool bbNoise, bool writeToFile,
                        QWidget* parent = 0);
    ~IEEE_802_15_4_ModTXSettingsDialog();

   int m_rampUpBits;
   int m_rampDownBits;
   int m_rampRange;
   bool m_modulateWhileRamping;
   int m_modulation;
   int m_bitRate;
   int m_pulseShaping;
   float m_beta;
   int m_symbolSpan;
   bool m_scramble;
   int m_polynomial;
   int m_lpfTaps;
   bool m_bbNoise;
   bool m_writeToFile;

private slots:
    void accept();

private:
    Ui::IEEE_802_15_4_ModTXSettingsDialog* ui;
};

#endif // INCLUDE_IEEE_802_15_4_MODTXSETTINGSDIALOG_H
