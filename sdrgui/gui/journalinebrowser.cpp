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

#include <QScrollBar>

#include "journalinebrowser.h"

JournalineBrowser::JournalineBrowser(QWidget *parent) :
    QTextBrowser(parent),
    m_object(0)
{
    setOpenLinks(false);
    connect(this, &QTextBrowser::anchorClicked, this, &JournalineBrowser::openLink);
    showPage(true);
}

QString JournalineBrowser::html(const Objects& objects, int objectId)
{
    const auto found = objects.find(objectId);

    if (found == objects.end()) {
        return tr("<p><i>Waiting for this page...</i></p>");
    }

    const modemdr::JournalineObject& object = found->second;
    const auto text = [](const std::string& utf8) {
        return QString::fromUtf8(utf8.data(), static_cast<int>(utf8.size())).toHtmlEscaped().replace('\n', QStringLiteral("<br>"));
    };
    QString html = QStringLiteral("<h3>%1</h3>").arg(text(object.title));

    if (object.type == modemdr::JournalineObject::Menu)
    {
        html += QStringLiteral("<ul>");

        for (const auto& item : object.items)
        {
            // A link once the object it opens has arrived
            if (objects.count(item.linkId)) {
                html += QStringLiteral("<li><a href=\"jml:%1\">%2</a></li>").arg(QString::number(item.linkId), text(item.text));
            } else {
                html += QStringLiteral("<li><font color=\"gray\">%1</font></li>").arg(text(item.text));
            }
        }

        html += QStringLiteral("</ul>");
    }
    else if (object.type == modemdr::JournalineObject::List)
    {
        html += QStringLiteral("<table cellpadding=\"2\">");
        bool open = false;

        for (const auto& item : object.items)
        {
            if (item.newRow || !open)
            {
                html += open ? QStringLiteral("</tr><tr>") : QStringLiteral("<tr>");
                open = true;
            }

            html += QStringLiteral("<td>%1</td>").arg(text(item.text));
        }

        html += open ? QStringLiteral("</tr></table>") : QStringLiteral("</table>");
    }
    else
    {
        for (const auto& item : object.items) {
            html += QStringLiteral("<p>%1</p>").arg(text(item.text));
        }
    }

    return html;
}

void JournalineBrowser::setObjects(std::shared_ptr<const Objects> objects, bool anotherService)
{
    m_objects = objects;
    if (anotherService)
    {
        m_object = 0;
        m_history.clear();
    }
    showPage(anotherService);
}

void JournalineBrowser::showPage(bool top)
{
    const QString html = (!m_objects || m_objects->empty())
        ? tr("<p><i>No Journaline service</i></p>") : JournalineBrowser::html(*m_objects, m_object);

    // Updates arrive every few seconds: redraw only a page that changed, keeping the reader's
    // place (and selection)
    if (html != m_shownHtml)
    {
        const int scroll = verticalScrollBar()->value();
        setHtml(html);
        verticalScrollBar()->setValue(top ? 0 : scroll);
        m_shownHtml = html;
    }
    else if (top)
    {
        verticalScrollBar()->setValue(0);
    }
    emit navigationChanged();
}

void JournalineBrowser::openLink(const QUrl& link)
{
    bool ok = false;
    const int objectId = link.path().toInt(&ok);

    if ((link.scheme() != QLatin1String("jml")) || !ok) {
        return;
    }

    m_history.push_back(m_object);
    m_object = objectId;
    showPage(true);
}

void JournalineBrowser::goHome()
{
    if (m_object != 0)
    {
        m_history.push_back(m_object);
        m_object = 0;
        showPage(true);
    }
}

void JournalineBrowser::goBack()
{
    if (!m_history.empty())
    {
        m_object = m_history.back();
        m_history.pop_back();
        showPage(true);
    }
}
