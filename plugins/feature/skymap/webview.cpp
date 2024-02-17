///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "webview.h"

#include <QTabWidget>
#include <QWebEngineView>
#include <QHBoxLayout>

WebView::WebView(QWidget *parent) :
    QWebEngineView(parent)
{
}

QWebEngineView *WebView::createWindow(QWebEnginePage::WebWindowType type)
{
    (void) type;
    QWebEngineView *view = new QWebEngineView();
    connect(view, &QWebEngineView::titleChanged, this, &WebView::on_titleChanged);

    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget(view);

    int tab = m_tabs->addTab(view, "Web");
    m_tabs->setCurrentIndex(tab);

    return view;
}

void WebView::on_titleChanged(const QString& title)
{
    QWebEngineView *view = qobject_cast<QWebEngineView *>(sender());
    for (int i = 0; i < m_tabs->count(); i++)
    {
        if (m_tabs->widget(i) == view) {
            m_tabs->setTabText(i, title);
        }
    }
}
