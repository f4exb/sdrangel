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

#ifndef INCLUDE_RADIOASTRONOMYSENSORDIALOG_H
#define INCLUDE_RADIOASTRONOMYSENSORDIALOG_H

#include "ui_radioastronomysensordialog.h"
#include "radioastronomysettings.h"

class RadioAstronomySensorDialog : public QDialog {
    Q_OBJECT

public:
    explicit RadioAstronomySensorDialog(RadioAstronomySettings *settings, QWidget* parent = 0);
    ~RadioAstronomySensorDialog();

    RadioAstronomySettings *m_settings;

private slots:
    void accept();

private:
    Ui::RadioAstronomySensorDialog* ui;
};

#endif // INCLUDE_RADIOASTRONOMYSENSORDIALOG_H
