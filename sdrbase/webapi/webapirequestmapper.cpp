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

//#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonArray>

#include <boost/lexical_cast.hpp>

#include "httpdocrootsettings.h"
#include "webapirequestmapper.h"
#include "SWGInstanceSummaryResponse.h"
#include "SWGInstanceDevicesResponse.h"
#include "SWGInstanceChannelsResponse.h"
#include "SWGAudioDevices.h"
#include "SWGAudioDevicesSelect.h"
#include "SWGLocationInformation.h"
#include "SWGDVSeralDevices.h"
#include "SWGPresets.h"
#include "SWGPresetTransfer.h"
#include "SWGPresetIdentifier.h"
#include "SWGDeviceSettings.h"
#include "SWGErrorResponse.h"

WebAPIRequestMapper::WebAPIRequestMapper(QObject* parent) :
    HttpRequestHandler(parent),
    m_adapter(0)
{
    qtwebapp::HttpDocrootSettings docrootSettings;
    docrootSettings.path = ":/";
    m_staticFileController = new qtwebapp::StaticFileController(docrootSettings, parent);
}

WebAPIRequestMapper::~WebAPIRequestMapper()
{
    delete m_staticFileController;
}

void WebAPIRequestMapper::service(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    if (m_adapter == 0) // format service unavailable if adapter is null
    {
        response.setStatus(500,"Service not available");
        response.write("Service not available");
    }
    else // normal processing
    {
        QByteArray path=request.getPath();

        if (path == WebAPIAdapterInterface::instanceSummaryURL) {
            instanceSummaryService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceDevicesURL) {
            instanceDevicesService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceChannelsURL) {
            instanceChannelsService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceLoggingURL) {
            instanceLoggingService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceAudioURL) {
            instanceAudioService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceLocationURL) {
            instanceLocationService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceDVSerialURL) {
            instanceDVSerialService(request, response);
        } else if (path == WebAPIAdapterInterface::instancePresetURL) {
            instancePresetService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceDeviceSetsURL) {
            instanceDeviceSetsService(request, response);
        }
        else
        {
            std::smatch desc_match;
            std::string pathStr(path.constData(), path.length());

            if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetURLRe)) {
                devicesetService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetDeviceURLRe)) {
                devicesetDeviceService(std::string(desc_match[1]), request, response);
            }
            else
            {
                QByteArray path = "/index.html";
                m_staticFileController->service(path, response);
            }

//            QDirIterator it(":", QDirIterator::Subdirectories);
//            while (it.hasNext()) {
//                qDebug() << "WebAPIRequestMapper::service: " << it.next();
//            }

        }
    }
}

void WebAPIRequestMapper::instanceSummaryService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGInstanceSummaryResponse normalResponse;
        SWGSDRangel::SWGErrorResponse errorResponse;

        int status = m_adapter->instanceSummary(normalResponse, errorResponse);
        response.setStatus(status);

        if (status == 200) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        response.write("Invalid HTTP method");
    }
}

void WebAPIRequestMapper::instanceDevicesService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGInstanceDevicesResponse normalResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;

    if (request.getMethod() == "GET")
    {
        QByteArray txStr = request.getParameter("tx");
        bool tx = false;

        if (txStr.length() != 0) {
            tx = !(txStr == "0");
        }

        int status = m_adapter->instanceDevices(tx, normalResponse, errorResponse);
        response.setStatus(status);

        if (status == 200) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        response.write("Invalid HTTP method");
    }
}

void WebAPIRequestMapper::instanceChannelsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGInstanceChannelsResponse normalResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;

    if (request.getMethod() == "GET")
    {
        QByteArray txStr = request.getParameter("tx");
        bool tx = false;

        if (txStr.length() != 0) {
            tx = !(txStr == "0");
        }

        int status = m_adapter->instanceChannels(tx, normalResponse, errorResponse);
        response.setStatus(status);

        if (status == 200) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        response.write("Invalid HTTP method");
    }
}

void WebAPIRequestMapper::instanceLoggingService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGLoggingInfo normalResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;

    if (request.getMethod() == "GET")
    {
        int status = m_adapter->instanceLoggingGet(normalResponse, errorResponse);
        response.setStatus(status);

        if (status == 200) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "PUT")
    {
        QString jsonStr = request.getBody();

        if (parseJsonBody(jsonStr, response))
        {
            normalResponse.fromJson(jsonStr);
            int status = m_adapter->instanceLoggingPut(normalResponse, errorResponse);
            response.setStatus(status);

            if (status == 200) {
                response.write(normalResponse.asJson().toUtf8());
            } else {
                response.write(errorResponse.asJson().toUtf8());
            }
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        response.write("Invalid HTTP method");
    }
}

void WebAPIRequestMapper::instanceAudioService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGAudioDevices normalResponse;

        int status = m_adapter->instanceAudioGet(normalResponse, errorResponse);
        response.setStatus(status);

        if (status == 200) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "PATCH")
    {
        SWGSDRangel::SWGAudioDevicesSelect normalResponse;
        QString jsonStr = request.getBody();

        if (parseJsonBody(jsonStr, response))
        {
            normalResponse.fromJson(jsonStr);
            int status = m_adapter->instanceAudioPatch(normalResponse, errorResponse);
            response.setStatus(status);

            if (status == 200) {
                response.write(normalResponse.asJson().toUtf8());
            } else {
                response.write(errorResponse.asJson().toUtf8());
            }
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        response.write("Invalid HTTP method");
    }
}

void WebAPIRequestMapper::instanceLocationService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGLocationInformation normalResponse;

        int status = m_adapter->instanceLocationGet(normalResponse, errorResponse);
        response.setStatus(status);

        if (status == 200) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "PUT")
    {
        SWGSDRangel::SWGLocationInformation normalResponse;
        QString jsonStr = request.getBody();

        if (parseJsonBody(jsonStr, response))
        {
            normalResponse.fromJson(jsonStr);
            int status = m_adapter->instanceLocationPut(normalResponse, errorResponse);
            response.setStatus(status);

            if (status == 200) {
                response.write(normalResponse.asJson().toUtf8());
            } else {
                response.write(errorResponse.asJson().toUtf8());
            }
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        response.write("Invalid HTTP method");
    }
}

void WebAPIRequestMapper::instanceDVSerialService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;

    if (request.getMethod() == "PATCH")
    {
        QByteArray dvserialStr = request.getParameter("dvserial");
        bool dvserial = false;

        if (dvserialStr.length() != 0) {
            dvserial = !(dvserialStr == "0");
        }

        SWGSDRangel::SWGDVSeralDevices normalResponse;

        int status = m_adapter->instanceDVSerialPatch(dvserial, normalResponse, errorResponse);
        response.setStatus(status);

        if (status == 200) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        response.write("Invalid HTTP method");
    }
}

void WebAPIRequestMapper::instancePresetService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGPresets normalResponse;
        int status = m_adapter->instancePresetGet(normalResponse, errorResponse);
        response.setStatus(status);

        if (status == 200) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "PATCH")
    {
        SWGSDRangel::SWGPresetTransfer query;
        SWGSDRangel::SWGPresetIdentifier normalResponse;
        QString jsonStr = request.getBody();

        if (parseJsonBody(jsonStr, response))
        {
            query.fromJson(jsonStr);

            if (validatePresetTransfer(query))
            {
                int status = m_adapter->instancePresetPatch(query, normalResponse, errorResponse);
                response.setStatus(status);

                if (status == 200) {
                    response.write(normalResponse.asJson().toUtf8());
                } else {
                    response.write(errorResponse.asJson().toUtf8());
                }
            }
            else
            {
                response.setStatus(400,"Invalid JSON request");
                errorResponse.init();
                *errorResponse.getMessage() = "Invalid JSON request";
                response.write(errorResponse.asJson().toUtf8());
            }
        }
    }
    else if (request.getMethod() == "PUT")
    {
        SWGSDRangel::SWGPresetTransfer query;
        SWGSDRangel::SWGPresetIdentifier normalResponse;
        QString jsonStr = request.getBody();

        if (parseJsonBody(jsonStr, response))
        {
            query.fromJson(jsonStr);

            if (validatePresetTransfer(query))
            {
                int status = m_adapter->instancePresetPut(query, normalResponse, errorResponse);
                response.setStatus(status);

                if (status == 200) {
                    response.write(normalResponse.asJson().toUtf8());
                } else {
                    response.write(errorResponse.asJson().toUtf8());
                }
            }
            else
            {
                response.setStatus(400,"Invalid JSON request");
                errorResponse.init();
                *errorResponse.getMessage() = "Invalid JSON request";
                response.write(errorResponse.asJson().toUtf8());
            }
        }
    }
    else if (request.getMethod() == "POST")
    {
        SWGSDRangel::SWGPresetTransfer query;
        SWGSDRangel::SWGPresetIdentifier normalResponse;
        QString jsonStr = request.getBody();

        if (parseJsonBody(jsonStr, response))
        {
            query.fromJson(jsonStr);

            if (validatePresetTransfer(query))
            {
                int status = m_adapter->instancePresetPost(query, normalResponse, errorResponse);
                response.setStatus(status);

                if (status == 200) {
                    response.write(normalResponse.asJson().toUtf8());
                } else {
                    response.write(errorResponse.asJson().toUtf8());
                }
            }
            else
            {
                response.setStatus(400,"Invalid JSON request");
                errorResponse.init();
                *errorResponse.getMessage() = "Invalid JSON request";
                response.write(errorResponse.asJson().toUtf8());
            }
        }
    }
    else if (request.getMethod() == "DELETE")
    {
        SWGSDRangel::SWGPresetIdentifier normalResponse;
        QString jsonStr = request.getBody();

        if (parseJsonBody(jsonStr, response))
        {
            normalResponse.fromJson(jsonStr);

            if (validatePresetIdentifer(normalResponse))
            {
                int status = m_adapter->instancePresetDelete(normalResponse, errorResponse);
                response.setStatus(status);

                if (status == 200) {
                    response.write(normalResponse.asJson().toUtf8());
                } else {
                    response.write(errorResponse.asJson().toUtf8());
                }
            }
            else
            {
                response.setStatus(400,"Invalid JSON request");
                errorResponse.init();
                *errorResponse.getMessage() = "Invalid JSON request";
                response.write(errorResponse.asJson().toUtf8());
            }
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        response.write("Invalid HTTP method");
    }
}

void WebAPIRequestMapper::instanceDeviceSetsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGDeviceSetList normalResponse;
        int status = m_adapter->instanceDeviceSetsGet(normalResponse, errorResponse);
        response.setStatus(status);

        if (status == 200) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "POST")
    {
        SWGSDRangel::SWGDeviceSet normalResponse;
        QByteArray txStr = request.getParameter("tx");
        bool tx = false;

        if (txStr.length() != 0) {
            tx = !(txStr == "0");
        }

        int status = m_adapter->instanceDeviceSetsPost(tx, normalResponse, errorResponse);
        response.setStatus(status);

        if (status == 200) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "DELETE")
    {
        SWGSDRangel::SWGDeviceSetList normalResponse;
        int status = m_adapter->instanceDeviceSetsDelete(normalResponse, errorResponse);
        response.setStatus(status);

        if (status == 200) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        response.write("Invalid HTTP method");
    }
}

void WebAPIRequestMapper::devicesetService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;

    if (request.getMethod() == "GET")
    {
        try
        {
            SWGSDRangel::SWGDeviceSet normalResponse;
            int deviceSetIndex = boost::lexical_cast<int>(indexStr);
            int status = m_adapter->devicesetGet(deviceSetIndex, normalResponse, errorResponse);
            response.setStatus(status);

            if (status == 200) {
                response.write(normalResponse.asJson().toUtf8());
            } else {
                response.write(errorResponse.asJson().toUtf8());
            }
        }
        catch (const boost::bad_lexical_cast &e)
        {
            errorResponse.init();
            *errorResponse.getMessage() = "Wrong integer conversion on device set index";
            response.setStatus(400,"Invalid data");
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        response.write("Invalid HTTP method");
    }
}

void WebAPIRequestMapper::devicesetDeviceService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(indexStr);

        if (request.getMethod() == "PUT")
        {
            SWGSDRangel::SWGDeviceListItem normalResponse;
            QString jsonStr = request.getBody();

            if (parseJsonBody(jsonStr, response))
            {
                normalResponse.fromJson(jsonStr);

                int status = m_adapter->devicesetDevicePut(deviceSetIndex, normalResponse, errorResponse);
                response.setStatus(status);

                if (status == 200) {
                    response.write(normalResponse.asJson().toUtf8());
                } else {
                    response.write(errorResponse.asJson().toUtf8());
                }
            }
        }
        else if (request.getMethod() == "GET")
        {
            SWGSDRangel::SWGDeviceSettings normalResponse;
            int status = m_adapter->devicesetDeviceGet(deviceSetIndex, normalResponse, errorResponse);
            response.setStatus(status);

            if (status == 200) {
                response.write(normalResponse.asJson().toUtf8());
            } else {
                response.write(errorResponse.asJson().toUtf8());
            }
        }
        else
        {
            response.setStatus(405,"Invalid HTTP method");
            response.write("Invalid HTTP method");
        }
    }
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on device set index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

bool WebAPIRequestMapper::parseJsonBody(QString& jsonStr, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;

    try
    {
        QByteArray jsonBytes(jsonStr.toStdString().c_str());
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &error);

        if (error.error != QJsonParseError::NoError)
        {
            QString errorMsg = QString("Input JSON error: ") + error.errorString() + QString(" at offset ") + QString::number(error.offset);
            errorResponse.init();
            *errorResponse.getMessage() = errorMsg;
            response.setStatus(400, errorMsg.toUtf8());
            response.write(errorResponse.asJson().toUtf8());
        }

        return (error.error == QJsonParseError::NoError);
    }
    catch (const std::exception& ex)
    {
        QString errorMsg = QString("Error parsing request: ") + ex.what();
        errorResponse.init();
        *errorResponse.getMessage() = errorMsg;
        response.setStatus(500, errorMsg.toUtf8());
        response.write(errorResponse.asJson().toUtf8());

        return false;
    }
}

bool WebAPIRequestMapper::validatePresetTransfer(SWGSDRangel::SWGPresetTransfer& presetTransfer)
{
    SWGSDRangel::SWGPresetIdentifier *presetIdentifier = presetTransfer.getPreset();

    if (presetIdentifier == 0) {
        return false;
    }

    return validatePresetIdentifer(*presetIdentifier);
}

bool WebAPIRequestMapper::validatePresetIdentifer(SWGSDRangel::SWGPresetIdentifier& presetIdentifier)
{
    return (presetIdentifier.getGroupName() && presetIdentifier.getName() && presetIdentifier.getType());
}


