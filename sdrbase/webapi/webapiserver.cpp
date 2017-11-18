///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QCoreApplication>

#include "httplistener.h"
#include "webapirequestmapper.h"
#include "webapiserver.h"

WebAPIServer::WebAPIServer(const QString& host, uint16_t port, WebAPIRequestMapper *requestMapper) :
    m_requestMapper(requestMapper),
    m_listener(0)
{
    m_settings.host = host;
    m_settings.port = port;
}

WebAPIServer::~WebAPIServer()
{
    if (m_listener) { delete m_listener; }
}

void WebAPIServer::start()
{
    if (!m_listener)
    {
        m_listener = new qtwebapp::HttpListener(m_settings, m_requestMapper, qApp);
        qInfo("WebAPIServer::start: starting web API server at http://%s:%d", qPrintable(m_settings.host), m_settings.port);
    }
}

void WebAPIServer::stop()
{
    if (m_listener)
    {
        delete m_listener;
        m_listener = 0;
        qInfo("WebAPIServer::stop: stopped web API server at http://%s:%d", qPrintable(m_settings.host), m_settings.port);
    }
}

void WebAPIServer::setHostAndPort(const QString& host, uint16_t port)
{
    stop();
    m_settings.host = host;
    m_settings.port = port;
    m_listener = new qtwebapp::HttpListener(m_settings, m_requestMapper, qApp);
}
