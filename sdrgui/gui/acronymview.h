///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>               //
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

class QMouseEvent;

// Displays text like a QPlainTextEdit, with acronym tooltips and links for
// Maidenhead locators, amateur radio callsigns, and web URLs.
class SDRGUI_API AcronymView : public QPlainTextEdit {
    Q_OBJECT

    QHash<QString, QString> m_acronym;
    QString m_pressedUrl;
    QString m_pressedMaidenhead;
    QString m_pressedCallsign;

    QString wordAt(const QPoint& position) const;
    QString urlAt(const QPoint& position) const;
    QString maidenheadAt(const QPoint& position) const;
    QString callsignAt(const QPoint& position, QString* country = nullptr) const;

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

public:

    AcronymView(QWidget* parent = nullptr);
    bool event(QEvent* event);
    void addAcronym(const QString& acronym, const QString& explanation);
    void addAcronyms(const QHash<QString, QString>& acronyms);

};

#endif // INCLUDE_GUI_ACRONYMVIEW_H
