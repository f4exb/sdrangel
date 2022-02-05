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

#ifndef INCLUDE_APTDEMODSETTINGSDIALOG_H
#define INCLUDE_APTDEMODSETTINGSDIALOG_H

#include "ui_aptdemodsettingsdialog.h"
#include "aptdemodsettings.h"

class APTDemodSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit APTDemodSettingsDialog(APTDemodSettings *settings, QWidget* parent = 0);
    ~APTDemodSettingsDialog();

    APTDemodSettings *m_settings;

private slots:
    void accept();
    void on_autoSavePathBrowse_clicked();
    void on_autoSave_clicked(bool checked);
    void on_addPalette_clicked();
    void on_removePalette_clicked();

private:
    Ui::APTDemodSettingsDialog* ui;
};

#endif // INCLUDE_APTDEMODSETTINGSDIALOG_H
