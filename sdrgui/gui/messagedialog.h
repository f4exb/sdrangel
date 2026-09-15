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

#ifndef SDRGUI_GUI_MESSAGEDIALOG_H
#define SDRGUI_GUI_MESSAGEDIALOG_H

#include <QMessageBox>
#include <QString>

#include <functional>

#include "export.h"

class QWidget;

// Non-blocking replacements for the QMessageBox static functions, for use in device,
// channel and feature GUIs.
//
// QMessageBox::information() and friends build the message box on the stack, parent it to
// the calling widget and run a nested event loop in exec(). If the parent is closed 
// via the Web API while the box is open, the parent deletes the box and its children, 
// which corrupts the heap.
//
// The trade-off is that these return immediately, so they cannot report which button was
// pressed. Use question() when the answer is needed; it delivers it to a callback.
class SDRGUI_API MessageDialog
{
public:
    static void information(QWidget *parent, const QString& title, const QString& text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok);
    static void warning(QWidget *parent, const QString& title, const QString& text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok);
    static void critical(QWidget *parent, const QString& title, const QString& text,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok);

    //!< Calls onAnswer with the button pressed. Not called if the box is destroyed unanswered
    static void question(QWidget *parent, const QString& title, const QString& text,
        QMessageBox::StandardButtons buttons, std::function<void (QMessageBox::StandardButton)> onAnswer);

private:
    static void show(QWidget *parent, QMessageBox::Icon icon, const QString& title,
        const QString& text, QMessageBox::StandardButtons buttons,
        std::function<void (QMessageBox::StandardButton)> onAnswer);
};

#endif // SDRGUI_GUI_MESSAGEDIALOG_H
