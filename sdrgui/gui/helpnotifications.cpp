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

#include "helpnotifications.h"

#include <QCheckBox>
#include <QEvent>
#include <QSettings>
#include <QTimer>

HelpNotifications::HelpNotifications(QWidget *parent) :
    QMessageBox(parent),
    m_checkBox(nullptr)
{
    setWindowModality(Qt::NonModal);
    setAttribute(Qt::WA_DeleteOnClose);
    setStandardButtons(QMessageBox::Ok);
}

// Global settings
//
// Controls whether transient notifications are enabled globally and
// provides a way to reset individual notification dismissals.

void HelpNotifications::setHelpNotificationsEnabled(bool enabled)
{
    QSettings settings;
    settings.beginGroup("HelpNotifications");
    settings.setValue("enabled", enabled);
}

bool HelpNotifications::isHelpNotificationsEnabled()
{
    QSettings settings;
    settings.beginGroup("HelpNotifications");
    return settings.value("enabled", true).toBool();
}

void HelpNotifications::resetAllDontRemind()
{
    QSettings settings;
    settings.beginGroup("HelpNotifications");
    settings.remove("Dismissed");
    settings.setValue("enabled", true);
}

// Helper functions for notification checkbox
//
// Manages the "Don't remind me again" checkbox displayed in each
// transient notification dialog.

void HelpNotifications::setDontRemindCheckBox(const QString& text)
{
    m_checkBox = new QCheckBox(text);
    setCheckBox(m_checkBox);
}

bool HelpNotifications::dontRemindAgain() const
{
    return m_checkBox && m_checkBox->isChecked();
}

// Dialog lifecycle
//
// Closes transient notifications when they lose window activation

bool HelpNotifications::event(QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate)
    {
        close();
        return true;
    }

    return QMessageBox::event(event);
}

// Display a notification
//
// Displays a non-modal warning unless notifications are globally
// disabled or the user has previously dismissed that specific warning.
// Individual dismissals are saved in QSettings when the user clicks OK
// with "Don't show again" selected.
// Dialogs close automatically after 30 seconds or when another window
// becomes active.
// Callers must ensure settingsKey is a unique, stable, hierarchical key.
// Use "Component/NotificationName", using PascalCase.
// Returns true if the notification was displayed; false if it was suppressed.

bool HelpNotifications::display(
    QWidget *parent,
    const QString& title,
    const QString& text,
    const QString& settingsKey
)
{
    QSettings settings;

    settings.beginGroup("HelpNotifications");
    const QString dismissedKey = "Dismissed/" + settingsKey;

    if (!settings.value("enabled", true).toBool() || settings.value(dismissedKey, false).toBool()) {
        return false;
    }

    auto *messageBox = new HelpNotifications(parent);

    messageBox->setIcon(QMessageBox::Warning);
    messageBox->setWindowTitle(title);
    messageBox->setText(text);
    messageBox->setDontRemindCheckBox(tr("Don't show again"));

    QObject::connect(
        messageBox,
        &QDialog::finished,
        messageBox,
        [messageBox, settingsKey](int result)
        {
            if (result == QMessageBox::Ok &&
                messageBox->dontRemindAgain())
            {
                QSettings settings;
                settings.beginGroup("HelpNotifications");
                settings.setValue("Dismissed/" + settingsKey, true);
            }
        }
    );

    messageBox->show();

    QTimer::singleShot(30000, messageBox, [messageBox]()
    {
        messageBox->close();
    });

    return true;
}
