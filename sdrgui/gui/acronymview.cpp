///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include <QEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QDebug>

#include "acronymview.h"

AcronymView::AcronymView(QWidget* parent) :
    QPlainTextEdit(parent)
{
    setMouseTracking(true);
    setReadOnly(true);
}

bool AcronymView::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
        QTextCursor cursor = cursorForPosition(helpEvent->pos());
        cursor.select(QTextCursor::WordUnderCursor);
        QString text = cursor.selectedText();
        // Remove trailing digits from METAR
        while (text.size() > 0 && text.right(1)[0].isDigit()) {
            text = text.left(text.size() - 1);
        }
        if (!text.isEmpty() && m_acronym.contains(text))
        {
            QToolTip::showText(helpEvent->globalPos(), QString("%1 - %2").arg(text).arg(m_acronym.value(text)));
        }
        else
        {
            if (!text.isEmpty()) {
                qDebug() << "AcronymView::event: No tooltip for " << text;
            }
            QToolTip::hideText();
        }
        return true;
    }
    return QPlainTextEdit::event(event);
}

void AcronymView::addAcronym(const QString& acronym, const QString& explanation)
{
    m_acronym.insert(acronym, explanation);
}

void AcronymView::addAcronyms(const QHash<QString, QString>& acronyms)
{
    m_acronym.insert(acronyms);
}
