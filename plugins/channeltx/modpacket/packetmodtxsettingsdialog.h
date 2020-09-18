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

#ifndef INCLUDE_PACKETMODTXSETTINGSDIALOG_H
#define INCLUDE_PACKETMODTXSETTINGSDIALOG_H

#include "ui_packetmodtxsettingsdialog.h"

class PacketModTXSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit PacketModTXSettingsDialog(int rampUpBits, int rampDownBits, int rampRange,
                        bool modulateWhileRamping,
                        int markFrequency, int spaceFrequency,
                        int ax25PreFlags, int ax25PostFlags,
                        int ax25Control, int ax25PID,
                        int lpfTaps, bool bbNoise, bool rfNoise, bool writeToFile,
                        QWidget* parent = 0);
    ~PacketModTXSettingsDialog();

   int m_rampUpBits;
   int m_rampDownBits;
   int m_rampRange;
   bool m_modulateWhileRamping;
   int m_markFrequency;
   int m_spaceFrequency;
   int m_ax25PreFlags;
   int m_ax25PostFlags;
   int m_ax25Control;
   int m_ax25PID;
   int m_lpfTaps;
   bool m_bbNoise;
   bool m_rfNoise;
   bool m_writeToFile;

private slots:
    void accept();

private:
    Ui::PacketModTXSettingsDialog* ui;
};

#endif // INCLUDE_PACKETMODTXSETTINGSDIALOG_H
