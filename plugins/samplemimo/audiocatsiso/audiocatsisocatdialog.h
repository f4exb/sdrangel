///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_AUDIOCATSISPCATDIALOG_H
#define INCLUDE_AUDIOCATSISPCATDIALOG_H

#include <QDialog>

#include "ui_audiocatsisocatdialog.h"

class AudioCATSISOSettings;

namespace Ui {
	class AudioCATSISOCATDialog;
}

class AudioCATSISOCATDialog : public QDialog {
	Q_OBJECT

public:
    AudioCATSISOCATDialog(AudioCATSISOSettings& settings, QList<QString>& settingsKeys, QWidget* parent = nullptr);
    ~AudioCATSISOCATDialog();

private slots:
    void accept();
    void on_baudRate_currentIndexChanged(int index);
    void on_handshake_currentIndexChanged(int index);
    void on_dataBits_currentIndexChanged(int index);
    void on_stopBits_currentIndexChanged(int index);
    void on_pttMethod_currentIndexChanged(int index);
    void on_dtrHigh_currentIndexChanged(int index);
    void on_rtsHigh_currentIndexChanged(int index);
    void on_pollingTime_valueChanged(int value);

private:
    Ui::AudioCATSISOCATDialog* ui;
    AudioCATSISOSettings& m_settings;
    QList<QString>& m_settingsKeys;
};

#endif // INCLUDE_AUDIOCATSISPCATDIALOG_H
