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

#include "webapirequestmapper.h"
#include "SWGInstanceSummaryResponse.h"
#include "SWGErrorResponse.h"

WebAPIRequestMapper::WebAPIRequestMapper(QObject* parent) :
    HttpRequestHandler(parent),
    m_adapter(0)
{ }

void WebAPIRequestMapper::service(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    if (m_adapter == 0) // format service unavailable if adapter is null
    {
        response.write("Service not available");
        response.setStatus(500,"Service not available");
    }
    else // normal processing
    {
        QByteArray path=request.getPath();

        if (path == WebAPIAdapterInterface::instanceSummaryURL)
        {
            if (request.getMethod() == "GET")
            {
                Swagger::SWGInstanceSummaryResponse normalResponse;
                Swagger::SWGErrorResponse errorResponse;

                int status = m_adapter->instanceSummary(normalResponse, errorResponse);

                if (status == 200) {
                    response.write(normalResponse.asJson().toUtf8());
                } else {
                    response.write(errorResponse.asJson().toUtf8());
                }

                response.setStatus(status);
            }
            else
            {
                response.write("Invalid HTTP method");
                response.setStatus(405,"Invalid HTTP method");
            }
        }
        else
        {
            response.setStatus(404,"Not found");
        }
    }
}
