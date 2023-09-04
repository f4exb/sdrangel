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

#ifndef INCLUDE_GUI_ACRONYMVIEW_H
#define INCLUDE_GUI_ACRONYMVIEW_H

#include <QHash>
#include <QPlainTextEdit>

#include "export.h"

// Displays text like a QPlainTextEdit, but adds tooltips for acronyms in the text
class SDRGUI_API AcronymView : public QPlainTextEdit {
    Q_OBJECT

    QHash<QString, QString> m_acronym;

public:

    AcronymView(QWidget* parent = nullptr);
    bool event(QEvent* event);
    void addAcronym(const QString& acronym, const QString& explanation);
    void addAcronyms(const QHash<QString, QString>& acronyms);

};

#endif // INCLUDE_GUI_ACRONYMVIEW_H
