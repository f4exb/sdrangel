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

#ifndef INCLUDE_ADSBDEMODDISPLAYDIALOG_H
#define INCLUDE_ADSBDEMODDISPLAYDIALOG_H

#include "ui_adsbdemoddisplaydialog.h"
#include "adsbdemodsettings.h"

class ADSBDemodDisplayDialog : public QDialog {
    Q_OBJECT

public:
    explicit ADSBDemodDisplayDialog(ADSBDemodSettings *settings, QWidget* parent = 0);
    ~ADSBDemodDisplayDialog();

private slots:
    void accept();
    void on_font_clicked();

private:
    Ui::ADSBDemodDisplayDialog* ui;
    ADSBDemodSettings *m_settings;
    QString m_fontName;
    int m_fontSize;
};

#endif // INCLUDE_ADSBDEMODDISPLAYDIALOG_H
