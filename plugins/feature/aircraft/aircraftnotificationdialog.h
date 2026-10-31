///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_AIRCRAFTNOTIFICATIONDIALOG_H
#define INCLUDE_AIRCRAFTNOTIFICATIONDIALOG_H

#include "ui_aircraftnotificationdialog.h"
#include "aircraftsettings.h"

class AircraftNotificationDialog : public QDialog {
    Q_OBJECT

public:
    explicit AircraftNotificationDialog(AircraftSettings* settings, QWidget* parent = 0);
    ~AircraftNotificationDialog();

private:
    void resizeTable();

private slots:
    void accept();
    void on_add_clicked();
    void on_remove_clicked();
    void addRow(AircraftSettings::NotificationSettings *settings=nullptr);

private:
    Ui::AircraftNotificationDialog* ui;
    AircraftSettings *m_settings;

    enum NotificationCol {
        NOTIFICATION_COL_MATCH,
        NOTIFICATION_COL_REG_EXP,
        NOTIFICATION_COL_SPEECH,
        NOTIFICATION_COL_COMMAND
    };
};

#endif // INCLUDE_AIRCRAFTNOTIFICATIONDIALOG_H
