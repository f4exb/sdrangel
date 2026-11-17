///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef INCLUDE_RADIOPAGINGDEMODNOTIFICATIONDIALOG_H
#define INCLUDE_RADIOPAGINGDEMODNOTIFICATIONDIALOG_H

#include <QDialog>
#include <QList>

#include "radiopagingdemodsettings.h"

class TableColorChooser;

namespace Ui {
    class RadioPagingDemodNotificationDialog;
}

// The notifications of the messages received: a regular expression on a field of the message, and what is
// done when it matches (highlight, speech, command, plot on the Map)
class RadioPagingDemodNotificationDialog : public QDialog {
    Q_OBJECT

public:
    explicit RadioPagingDemodNotificationDialog(RadioPagingDemodSettings *settings, QWidget *parent = nullptr);
    ~RadioPagingDemodNotificationDialog();

private:
    enum NotificationCol {
        COL_MATCH,
        COL_REG_EXP,
        COL_SPEECH,
        COL_COMMAND,
        COL_HIGHLIGHT,
        COL_PLOT_ON_MAP
    };

    Ui::RadioPagingDemodNotificationDialog *ui;
    RadioPagingDemodSettings *m_settings;
    QList<TableColorChooser *> m_colorGUIs;

    void resizeTable();
    void addRow(const RadioPagingDemodSettings::NotificationSettings *settings = nullptr);
    void removeSelected();

private slots:
    void accept() override;
};

#endif // INCLUDE_RADIOPAGINGDEMODNOTIFICATIONDIALOG_H
