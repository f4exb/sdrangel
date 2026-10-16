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

#ifndef SDRGUI_GUI_JOURNALINEBROWSER_H_
#define SDRGUI_GUI_JOURNALINEBROWSER_H_

#include <map>
#include <memory>
#include <vector>

#include <QTextBrowser>
#include <QUrl>

#include "modemdr/journaline.h"
#include "export.h"

// Shows the Journaline (ETSI TS 102 979) news of a DAB or DRM service, a page at a time: menu
// entries link to the pages that have arrived (as jml:<object Id> URLs), with a history for Back.
class SDRGUI_API JournalineBrowser : public QTextBrowser
{
    Q_OBJECT

public:
    using Objects = std::map<int, modemdr::JournalineObject>;

    explicit JournalineBrowser(QWidget *parent = nullptr);

    // A new snapshot of the service's objects. The page shown is kept, and redrawn only if it
    // changed, keeping the reader's place; another service starts at its main menu.
    void setObjects(std::shared_ptr<const Objects> objects, bool anotherService = false);
    bool canGoBack() const { return !m_history.empty(); }
    bool atHome() const { return m_object == 0; }
    // The HTML of an object, its menu entries linking to the pages that have arrived
    static QString html(const Objects& objects, int objectId);

public slots:
    // A jml: link: its page, the one shown going to the history
    void openLink(const QUrl& link);
    void goHome();
    void goBack();

signals:
    // The page shown changed: Home and Back may now apply
    void navigationChanged();

private:
    void showPage(bool top);

    std::shared_ptr<const Objects> m_objects;
    int m_object;                   // The page shown, 0 the main menu
    std::vector<int> m_history;
    QString m_shownHtml;
};

#endif // SDRGUI_GUI_JOURNALINEBROWSER_H_
