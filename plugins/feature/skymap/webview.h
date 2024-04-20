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

#ifndef INCLUDE_FEATURE_WEBVIEW_H_
#define INCLUDE_FEATURE_WEBVIEW_H_

#include <QWebEngineView>

class QTabWidget;

class WebView : public QWebEngineView
{
    Q_OBJECT

public:

    WebView(QWidget *parent = nullptr);

    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;

    void setTabs(QTabWidget *tabs) { m_tabs = tabs; }

private slots:
    void on_titleChanged(const QString& title);

private:
    QTabWidget *m_tabs;
};

#endif // INCLUDE_FEATURE_WEBVIEW_H_
