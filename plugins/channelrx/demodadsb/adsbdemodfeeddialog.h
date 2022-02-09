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

#ifndef INCLUDE_ADSBDEMODFEEDDIALOG_H
#define INCLUDE_ADSBDEMODFEEDDIALOG_H

#include "ui_adsbdemodfeeddialog.h"
#include "adsbdemodsettings.h"

class ADSBDemodFeedDialog : public QDialog {
    Q_OBJECT

public:
    explicit ADSBDemodFeedDialog(ADSBDemodSettings *settings, QWidget* parent = 0);
    ~ADSBDemodFeedDialog();

    ADSBDemodSettings *m_settings;

private slots:
    void accept();
    void on_exportClientHost_currentIndexChanged(int value);

private:
    Ui::ADSBDemodFeedDialog* ui;
};

#endif // INCLUDE_ADSBDEMODFEEDDIALOG_H
