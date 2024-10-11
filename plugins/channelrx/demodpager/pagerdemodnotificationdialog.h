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

#ifndef INCLUDE_PAGERDEMODNOTIFICATIONDIALOG_H
#define INCLUDE_PAGERDEMODNOTIFICATIONDIALOG_H

#include <QHash>

#include "ui_pagerdemodnotificationdialog.h"
#include "pagerdemodsettings.h"

class TableColorChooser;

class PagerDemodNotificationDialog : public QDialog {
    Q_OBJECT

public:
    explicit PagerDemodNotificationDialog(PagerDemodSettings* settings, QWidget* parent = 0);
    ~PagerDemodNotificationDialog();

private:
    void resizeTable();

private slots:
    void accept() override;
    void on_add_clicked();
    void on_remove_clicked();
    void addRow(PagerDemodSettings::NotificationSettings *settings=nullptr);

private:
    Ui::PagerDemodNotificationDialog* ui;
    PagerDemodSettings *m_settings;
    QList<TableColorChooser *> m_colorGUIs;

    enum NotificationCol {
        NOTIFICATION_COL_MATCH,
        NOTIFICATION_COL_REG_EXP,
        NOTIFICATION_COL_SPEECH,
        NOTIFICATION_COL_COMMAND,
        NOTIFICATION_COL_HIGHLIGHT,
        NOTIFICATION_COL_PLOT_ON_MAP
    };

    static std::vector<int> m_columnMap;
};

#endif // INCLUDE_PagerDEMODNOTIFICATIONDIALOG_H
