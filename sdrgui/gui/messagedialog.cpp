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

#include <QWidget>

#include "gui/messagedialog.h"

void MessageDialog::show(QWidget *parent, QMessageBox::Icon icon, const QString& title, const QString& text, QMessageBox::StandardButtons buttons, std::function<void (QMessageBox::StandardButton)> onAnswer)
{
    // Avoid creating new box with same text. Raise the existing one instead.
    if (parent)
    {
        const QList<QMessageBox *> existing = parent->findChildren<QMessageBox *>(
            QString(), Qt::FindDirectChildrenOnly);

        for (QMessageBox *box : existing)
        {
            if (box->isVisible() && (box->text() == text) && (box->windowTitle() == title))
            {
                box->raise();
                return;
            }
        }
    }

    QMessageBox *box = new QMessageBox(icon, title, text, buttons, parent);
    box->setAttribute(Qt::WA_DeleteOnClose);

    if (onAnswer)
    {
        QObject::connect(box, &QMessageBox::finished, box, [box, onAnswer](int) {
            onAnswer(box->standardButton(box->clickedButton()));
        });
    }

    // open() rather than exec(): it returns immediately, so nothing left on stack
    box->open();
}

void MessageDialog::information(QWidget *parent, const QString& title, const QString& text, QMessageBox::StandardButtons buttons)
{
    show(parent, QMessageBox::Information, title, text, buttons, nullptr);
}

void MessageDialog::warning(QWidget *parent, const QString& title, const QString& text, QMessageBox::StandardButtons buttons)
{
    show(parent, QMessageBox::Warning, title, text, buttons, nullptr);
}

void MessageDialog::critical(QWidget *parent, const QString& title, const QString& text, QMessageBox::StandardButtons buttons)
{
    show(parent, QMessageBox::Critical, title, text, buttons, nullptr);
}

void MessageDialog::question(QWidget *parent, const QString& title, const QString& text, QMessageBox::StandardButtons buttons, std::function<void (QMessageBox::StandardButton)> onAnswer)
{
    show(parent, QMessageBox::Question, title, text, buttons, onAnswer);
}
