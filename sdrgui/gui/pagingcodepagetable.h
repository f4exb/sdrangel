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

#ifndef SDRGUI_GUI_PAGINGCODEPAGETABLE_H_
#define SDRGUI_GUI_PAGINGCODEPAGETABLE_H_

#include <QFrame>
#include <QString>

#include "export.h"
#include "radiopagingcharset.h"

class QLabel;
class QTableWidget;

// The table of a pager codepage as the pager manuals print it: a column for each high part of the code (0x00-0x0F,
// 0x10-0x1F, ...), a row for each low 4 bits. A cell shows the glyph of its code, or its hex code when it has none;
// its tooltip the code, the glyph and the other characters taken for it. A click gives the text of the code: the
// glyph, or <0xNN> for a code without one (and for the controls CR and LF, which a text line does not hold).
class SDRGUI_API PagingCodepageTable : public QFrame
{
    Q_OBJECT
public:
    explicit PagingCodepageTable(QWidget *parent = nullptr);

    void setCodepage(const RadioPagingCharset::Codepage& codepage);
    //!< A popup under the widget
    void popup(QWidget *below);

    //!< The text of a code for a line of text: the glyph, or <0xNN>
    static QString codeText(const RadioPagingCharset::Codepage& codepage, int code);

signals:
    void codeSelected(const QString& text);

private:
    QLabel *m_title;
    QTableWidget *m_table;
    const RadioPagingCharset::Codepage *m_codepage;
};

#endif // SDRGUI_GUI_PAGINGCODEPAGETABLE_H_
