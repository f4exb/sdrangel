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

#ifndef INCLUDE_ADSBDEMODNOTIFICATIONDIALOG_H
#define INCLUDE_ADSBDEMODNOTIFICATIONDIALOG_H

#include <QHash>

#include "ui_adsbdemodnotificationdialog.h"
#include "adsbdemodsettings.h"

class ADSBDemodNotificationDialog : public QDialog {
    Q_OBJECT

public:
    explicit ADSBDemodNotificationDialog(ADSBDemodSettings* settings, QWidget* parent = 0);
    ~ADSBDemodNotificationDialog();

private:
    void resizeTable();

private slots:
    void accept();
    void on_add_clicked();
    void on_remove_clicked();
    void addRow(ADSBDemodSettings::NotificationSettings *settings=nullptr);

private:
    Ui::ADSBDemodNotificationDialog* ui;
    ADSBDemodSettings *m_settings;

    enum NotificationCol {
        NOTIFICATION_COL_MATCH,
        NOTIFICATION_COL_REG_EXP,
        NOTIFICATION_COL_SPEECH,
        NOTIFICATION_COL_COMMAND,
        NOTIFICATION_COL_AUTOTARGET
    };

    static std::vector<int> m_columnMap;
};

#endif // INCLUDE_ADSBDEMODNOTIFICATIONDIALOG_H
