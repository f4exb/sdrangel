///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_ACARSMESSGAEVIEW_H
#define INCLUDE_ACARSMESSGAEVIEW_H

#include <QHash>
#include <QTextEdit>

#include "export.h"

class SDRGUI_API AcarsMessageView : public QTextEdit {

    QHash<QString, QString> m_acronym;  //!< Added to this view, on top of the built-in ones and airports

    QString explanation(const QString& text) const;

public:

    AcarsMessageView(QWidget *parent=nullptr);
    bool event(QEvent *event);
    void addAcronym(const QString &acronym, const QString &explanation);

};

#endif // INCLUDE_ACARSMESSGAEVIEW_H
