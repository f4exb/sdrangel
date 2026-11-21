///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026                                                                //
// written by Robin Getz <rgetz503@gmail.com>                                        //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#ifndef INCLUDE_HELPNOTIFICATIONS_H
#define INCLUDE_HELPNOTIFICATIONS_H

#include <QMessageBox>

#include "export.h"

class QCheckBox;
class QEvent;

class SDRGUI_API HelpNotifications : public QMessageBox
{
    Q_OBJECT

public:
    static bool display(
        QWidget *parent,
        const QString& title,
        const QString& text,
        const QString& settingsKey
    );
    static void setHelpNotificationsEnabled(bool enabled);
    static bool isHelpNotificationsEnabled();
    static void resetAllDontRemind();

private:
    explicit HelpNotifications(QWidget *parent = nullptr);
    void setDontRemindCheckBox(const QString& text);
    bool dontRemindAgain() const;
    bool event(QEvent *event) override;

    QCheckBox *m_checkBox;
};

#endif // INCLUDE_HELPNOTIFICATIONS_H
