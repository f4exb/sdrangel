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

#ifndef INCLUDE_ACARSTEXTFORMAT_H
#define INCLUDE_ACARSTEXTFORMAT_H

#include <QString>
#include <QStringList>
#include <QRegularExpression>

// Message decodes are built as (possibly <br>-separated) sequences of
// "Label: value" lines. The Message Decode COLUMN shows them flattened to one
// semicolon-separated line so more fits; the decode VIEW and Map popups show
// them multi-line, with the label before each colon bolded. Shared between the
// worker (which computes the column and plain forms) and the GUI (which renders
// the HTML form).

// Flatten a multi-line (or <br>-separated HTML) decode for the table column
inline QString acarsDecodeToColumn(const QString& decode)
{
    QString t = decode;
    t.replace("<br>", "; ");
    t.remove(QRegularExpression("<[^>]*>"));
    t.replace("\r\n", "; ");
    t.replace("\n", "; ");
    t.replace("\r", "; ");
    return t.trimmed();
}

// Multi-line plain text from a decode that may be <br>-separated HTML
inline QString acarsDecodeToPlain(const QString& decode)
{
    QString t = decode;
    t.replace("<br>", "\n");
    t.remove(QRegularExpression("<[^>]*>"));
    t.replace("\r\n", "\n");
    t.replace("\r", "\n");
    return t.trimmed();
}

// Render a plain multi-line decode as HTML for the decode view: each line's
// label (the text before the first colon) is bolded, indentation preserved
inline QString acarsDecodeToHtml(const QString& plain)
{
    QStringList out;
    for (const QString& line : acarsDecodeToPlain(plain).split('\n'))
    {
        QString escaped = line.toHtmlEscaped();
        // Keep leading indentation (libacars indents nested layers)
        int indent = 0;
        while ((indent < escaped.size()) && (escaped[indent] == ' ')) {
            indent++;
        }
        QString spaces = QString("&nbsp;").repeated(indent);
        QString body = escaped.mid(indent);
        int colon = body.indexOf(':');
        if ((colon > 0) && (colon <= 40)) {
            body = "<b>" + body.left(colon + 1) + "</b>" + body.mid(colon + 1);
        }
        out.append(spaces + body);
    }
    return out.join("<br>");
}

#endif // INCLUDE_ACARSTEXTFORMAT_H
