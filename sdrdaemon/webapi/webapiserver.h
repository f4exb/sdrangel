///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon Swagger server adapter interface                                    //
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

#ifndef SDRDAEMON_WEBAPI_WEBAPISERVER_H_
#define SDRDAEMON_WEBAPI_WEBAPISERVER_H_

#include <QString>
#include "export.h"

namespace qtwebapp
{
    class HttpListener;
    class HttpListenerSettings;
}

namespace SDRDaemon {

class WebAPIRequestMapper;

class SDRBASE_API WebAPIServer
{
public:
    WebAPIServer(const QString& host, uint16_t port, WebAPIRequestMapper *requestMapper);
    ~WebAPIServer();

    void start();
    void stop();

    void setHostAndPort(const QString& host, uint16_t port);
    const QString& getHost() const { return m_settings.host; }
    int getPort() const { return m_settings.port; }

private:
    WebAPIRequestMapper *m_requestMapper;
    qtwebapp::HttpListener *m_listener;
    qtwebapp::HttpListenerSettings m_settings;
};

} // namespace SDRdaemon

#endif /* SDRDAEMON_WEBAPI_WEBAPISERVER_H_ */
