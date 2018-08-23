///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRDaemon Swagger server adapter interface                                    //
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

#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonArray>

#include <boost/lexical_cast.hpp>

#include "httpdocrootsettings.h"
#include "webapirequestmapper.h"
#include "SWGDaemonSummaryResponse.h"
#include "SWGInstanceDevicesResponse.h"
#include "SWGChannelSettings.h"
#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGSuccessResponse.h"
#include "SWGErrorResponse.h"
#include "SWGLoggingInfo.h"

#include "webapirequestmapper.h"
#include "webapiadapterdaemon.h"

namespace SDRDaemon
{

WebAPIRequestMapper::WebAPIRequestMapper(QObject* parent) :
    HttpRequestHandler(parent),
    m_adapter(0)
{
    qtwebapp::HttpDocrootSettings docrootSettings;
    docrootSettings.path = ":/webapi";
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
        SWGSDRangel::SWGErrorResponse errorResponse;
        response.setHeader("Content-Type", "application/json");
        response.setHeader("Access-Control-Allow-Origin", "*");
        response.setStatus(500,"Service not available");

        errorResponse.init();
        *errorResponse.getMessage() = "Service not available";
        response.write(errorResponse.asJson().toUtf8());
    }
    else // normal processing
    {
        QByteArray path=request.getPath();

        // Handle pre-flight requests
        if (request.getMethod() == "OPTIONS")
        {
            qDebug("WebAPIRequestMapper::service: method OPTIONS: assume pre-flight");
            response.setHeader("Access-Control-Allow-Origin", "*");
            response.setHeader("Access-Control-Allow-Headers", "*");
            response.setHeader("Access-Control-Allow-Methods", "*");
            response.setStatus(200, "OK");
            return;
        }

        if (path.startsWith("/sdrangel") && (path != "/sdrangel_logo.png"))
        {
            SWGSDRangel::SWGErrorResponse errorResponse;
            response.setStatus(501,"Not implemented");
            errorResponse.init();
            *errorResponse.getMessage() = "Not implemented";
            response.write(errorResponse.asJson().toUtf8());
            return;
        }

        if (path == WebAPIAdapterDaemon::daemonInstanceSummaryURL) {
            daemonInstanceSummaryService(request, response);
        } else if (path == WebAPIAdapterDaemon::daemonInstanceLoggingURL) {
            daemonInstanceLoggingService(request, response);
        } else if (path == WebAPIAdapterDaemon::daemonChannelSettingsURL) {
            daemonChannelSettingsService(request, response);
        } else if (path == WebAPIAdapterDaemon::daemonDeviceSettingsURL) {
            daemonDeviceSettingsService(request, response);
        } else if (path == WebAPIAdapterDaemon::daemonDeviceReportURL) {
            daemonDeviceReportService(request, response);
        } else if (path == WebAPIAdapterDaemon::daemonRunURL) {
            daemonRunService(request, response);
        } else {
            m_staticFileController->service(request, response); // serve static pages
        }
    }
}

void WebAPIRequestMapper::daemonInstanceSummaryService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGDaemonSummaryResponse normalResponse;

        int status = m_adapter->daemonInstanceSummary(normalResponse, errorResponse);
        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        errorResponse.init();
        *errorResponse.getMessage() = "Invalid HTTP method";
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::daemonInstanceLoggingService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGLoggingInfo query;
    SWGSDRangel::SWGLoggingInfo normalResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        int status = m_adapter->daemonInstanceLoggingGet(normalResponse, errorResponse);
        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "PUT")
    {
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);
            int status = m_adapter->daemonInstanceLoggingPut(query, normalResponse, errorResponse);
            response.setStatus(status);

            if (status/100 == 2) {
                response.write(normalResponse.asJson().toUtf8());
            } else {
                response.write(errorResponse.asJson().toUtf8());
            }
        }
        else
        {
            response.setStatus(400,"Invalid JSON format");
            errorResponse.init();
            *errorResponse.getMessage() = "Invalid JSON format";
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        errorResponse.init();
        *errorResponse.getMessage() = "Invalid HTTP method";
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::daemonChannelSettingsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGChannelSettings normalResponse;
        resetChannelSettings(normalResponse);
        int status = m_adapter->daemonChannelSettingsGet(normalResponse, errorResponse);
        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if ((request.getMethod() == "PUT") || (request.getMethod() == "PATCH"))
    {
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            SWGSDRangel::SWGChannelSettings normalResponse;
            resetChannelSettings(normalResponse);
            QStringList channelSettingsKeys;

            if (validateChannelSettings(normalResponse, jsonObject, channelSettingsKeys))
            {
                int status = m_adapter->daemonChannelSettingsPutPatch(
                        (request.getMethod() == "PUT"), // force settings on PUT
                        channelSettingsKeys,
                        normalResponse,
                        errorResponse);
                response.setStatus(status);

                if (status/100 == 2) {
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
        else
        {
            response.setStatus(400,"Invalid JSON format");
            errorResponse.init();
            *errorResponse.getMessage() = "Invalid JSON format";
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        errorResponse.init();
        *errorResponse.getMessage() = "Invalid HTTP method";
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::daemonDeviceSettingsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if ((request.getMethod() == "PUT") || (request.getMethod() == "PATCH"))
    {
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            SWGSDRangel::SWGDeviceSettings normalResponse;
            resetDeviceSettings(normalResponse);
            QStringList deviceSettingsKeys;

            if (validateDeviceSettings(normalResponse, jsonObject, deviceSettingsKeys))
            {
                int status = m_adapter->daemonDeviceSettingsPutPatch(
                        (request.getMethod() == "PUT"), // force settings on PUT
                        deviceSettingsKeys,
                        normalResponse,
                        errorResponse);
                response.setStatus(status);

                if (status/100 == 2) {
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
        else
        {
            response.setStatus(400,"Invalid JSON format");
            errorResponse.init();
            *errorResponse.getMessage() = "Invalid JSON format";
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGDeviceSettings normalResponse;
        resetDeviceSettings(normalResponse);
        int status = m_adapter->daemonDeviceSettingsGet(normalResponse, errorResponse);
        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        errorResponse.init();
        *errorResponse.getMessage() = "Invalid HTTP method";
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::daemonDeviceReportService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGDeviceReport normalResponse;
        resetDeviceReport(normalResponse);
        int status = m_adapter->daemonReportGet(normalResponse, errorResponse);
        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        errorResponse.init();
        *errorResponse.getMessage() = "Invalid HTTP method";
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::daemonRunService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGDeviceState normalResponse;
        int status = m_adapter->daemonRunGet(normalResponse, errorResponse);

        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "POST")
    {
        SWGSDRangel::SWGDeviceState normalResponse;
        int status = m_adapter->daemonRunPost(normalResponse, errorResponse);

        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "DELETE")
    {
        SWGSDRangel::SWGDeviceState normalResponse;
        int status = m_adapter->daemonRunDelete(normalResponse, errorResponse);

        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else
    {
        response.setStatus(405,"Invalid HTTP method");
        errorResponse.init();
        *errorResponse.getMessage() = "Invalid HTTP method";
        response.write(errorResponse.asJson().toUtf8());
    }
}

// TODO: put in library in common with SDRangel. Can be static.
bool WebAPIRequestMapper::validateChannelSettings(
        SWGSDRangel::SWGChannelSettings& channelSettings,
        QJsonObject& jsonObject,
        QStringList& channelSettingsKeys)
{
    if (jsonObject.contains("tx")) {
        channelSettings.setTx(jsonObject["tx"].toInt());
    } else {
        channelSettings.setTx(0); // assume Rx
    }

    if (jsonObject.contains("channelType") && jsonObject["channelType"].isString()) {
        channelSettings.setChannelType(new QString(jsonObject["channelType"].toString()));
    } else {
        return false;
    }

    QString *channelType = channelSettings.getChannelType();

    if (*channelType == "AMDemod")
    {
        if (channelSettings.getTx() == 0)
        {
            QJsonObject amDemodSettingsJsonObject = jsonObject["AMDemodSettings"].toObject();
            channelSettingsKeys = amDemodSettingsJsonObject.keys();
            channelSettings.setAmDemodSettings(new SWGSDRangel::SWGAMDemodSettings());
            channelSettings.getAmDemodSettings()->fromJsonObject(amDemodSettingsJsonObject);
            return true;
        }
        else {
            return false;
        }
    }
    else if (*channelType == "AMMod")
    {
        if (channelSettings.getTx() != 0)
        {
            QJsonObject amModSettingsJsonObject = jsonObject["AMModSettings"].toObject();
            channelSettingsKeys = amModSettingsJsonObject.keys();

            if (channelSettingsKeys.contains("cwKeyer"))
            {
                QJsonObject cwKeyerSettingsJsonObject;
                appendSettingsSubKeys(amModSettingsJsonObject, cwKeyerSettingsJsonObject, "cwKeyer", channelSettingsKeys);
            }

            channelSettings.setAmModSettings(new SWGSDRangel::SWGAMModSettings());
            channelSettings.getAmModSettings()->fromJsonObject(amModSettingsJsonObject);
            return true;
        }
        else {
            return false;
        }
    }
    else if (*channelType == "ATVMod")
    {
        if (channelSettings.getTx() != 0)
        {
            QJsonObject atvModSettingsJsonObject = jsonObject["ATVModSettings"].toObject();
            channelSettingsKeys = atvModSettingsJsonObject.keys();
            channelSettings.setAtvModSettings(new SWGSDRangel::SWGATVModSettings());
            channelSettings.getAtvModSettings()->fromJsonObject(atvModSettingsJsonObject);
            return true;
        }
        else {
            return false;
        }
    }
    else if (*channelType == "BFMDemod")
    {
        if (channelSettings.getTx() == 0)
        {
            QJsonObject bfmDemodSettingsJsonObject = jsonObject["BFMDemodSettings"].toObject();
            channelSettingsKeys = bfmDemodSettingsJsonObject.keys();
            channelSettings.setBfmDemodSettings(new SWGSDRangel::SWGBFMDemodSettings());
            channelSettings.getBfmDemodSettings()->fromJsonObject(bfmDemodSettingsJsonObject);
            return true;
        }
        else {
            return false;
        }
    }
    else if (*channelType == "DSDDemod")
    {
        if (channelSettings.getTx() == 0)
        {
            QJsonObject dsdDemodSettingsJsonObject = jsonObject["DSDDemodSettings"].toObject();
            channelSettingsKeys = dsdDemodSettingsJsonObject.keys();
            channelSettings.setDsdDemodSettings(new SWGSDRangel::SWGDSDDemodSettings());
            channelSettings.getDsdDemodSettings()->fromJsonObject(dsdDemodSettingsJsonObject);
            return true;
        }
        else {
            return false;
        }
    }
    else if (*channelType == "NFMDemod")
    {
        if (channelSettings.getTx() == 0)
        {
            QJsonObject nfmDemodSettingsJsonObject = jsonObject["NFMDemodSettings"].toObject();
            channelSettingsKeys = nfmDemodSettingsJsonObject.keys();
            channelSettings.setNfmDemodSettings(new SWGSDRangel::SWGNFMDemodSettings());
            channelSettings.getNfmDemodSettings()->fromJsonObject(nfmDemodSettingsJsonObject);
            return true;
        }
        else {
            return false;
        }
    }
    else if (*channelType == "NFMMod")
    {
        if (channelSettings.getTx() != 0)
        {
            QJsonObject nfmModSettingsJsonObject = jsonObject["NFMModSettings"].toObject();
            channelSettingsKeys = nfmModSettingsJsonObject.keys();

            if (channelSettingsKeys.contains("cwKeyer"))
            {
                QJsonObject cwKeyerSettingsJsonObject;
                appendSettingsSubKeys(nfmModSettingsJsonObject, cwKeyerSettingsJsonObject, "cwKeyer", channelSettingsKeys);
            }

            channelSettings.setNfmModSettings(new SWGSDRangel::SWGNFMModSettings());
            channelSettings.getNfmModSettings()->fromJsonObject(nfmModSettingsJsonObject);
            return true;
        }
        else {
            return false;
        }
    }
    else if (*channelType == "SDRDaemonChannelSink")
    {
        if (channelSettings.getTx() == 0)
        {
            QJsonObject sdrDaemonChannelSinkSettingsJsonObject = jsonObject["SDRDaemonChannelSinkSettings"].toObject();
            channelSettingsKeys = sdrDaemonChannelSinkSettingsJsonObject.keys();
            channelSettings.setSdrDaemonChannelSinkSettings(new SWGSDRangel::SWGSDRDaemonChannelSinkSettings());
            channelSettings.getSdrDaemonChannelSinkSettings()->fromJsonObject(sdrDaemonChannelSinkSettingsJsonObject);
            return true;
        }
        else {
            return false;
        }
    }
    else if (*channelType == "SSBDemod")
    {
        if (channelSettings.getTx() == 0)
        {
            QJsonObject ssbDemodSettingsJsonObject = jsonObject["SSBDemodSettings"].toObject();
            channelSettingsKeys = ssbDemodSettingsJsonObject.keys();
            channelSettings.setSsbDemodSettings(new SWGSDRangel::SWGSSBDemodSettings());
            channelSettings.getSsbDemodSettings()->fromJsonObject(ssbDemodSettingsJsonObject);
            return true;
        }
        else {
            return false;
        }
    }
    else if (*channelType == "SSBMod")
    {
        if (channelSettings.getTx() != 0)
        {
            QJsonObject ssbModSettingsJsonObject = jsonObject["SSBModSettings"].toObject();
            channelSettingsKeys = ssbModSettingsJsonObject.keys();

            if (channelSettingsKeys.contains("cwKeyer"))
            {
                QJsonObject cwKeyerSettingsJsonObject;
                appendSettingsSubKeys(ssbModSettingsJsonObject, cwKeyerSettingsJsonObject, "cwKeyer", channelSettingsKeys);
            }

            channelSettings.setSsbModSettings(new SWGSDRangel::SWGSSBModSettings());
            channelSettings.getSsbModSettings()->fromJsonObject(ssbModSettingsJsonObject);
            return true;
        }
        else {
            return false;
        }
    }
    else if (*channelType == "UDPSink")
    {
        if (channelSettings.getTx() != 0)
        {
            QJsonObject udpSinkSettingsJsonObject = jsonObject["UDPSinkSettings"].toObject();
            channelSettingsKeys = udpSinkSettingsJsonObject.keys();
            channelSettings.setUdpSinkSettings(new SWGSDRangel::SWGUDPSinkSettings());
            channelSettings.getUdpSinkSettings()->fromJsonObject(udpSinkSettingsJsonObject);
            return true;
        }
        else {
            return false;
        }
    }
    else if (*channelType == "UDPSrc")
    {
        if (channelSettings.getTx() == 0)
        {
            QJsonObject udpSrcSettingsJsonObject = jsonObject["UDPSrcSettings"].toObject();
            channelSettingsKeys = udpSrcSettingsJsonObject.keys();
            channelSettings.setUdpSrcSettings(new SWGSDRangel::SWGUDPSrcSettings());
            channelSettings.getUdpSrcSettings()->fromJsonObject(udpSrcSettingsJsonObject);
            return true;
        }
        else {
            return false;
        }
    }
    else if (*channelType == "WFMDemod")
    {
        if (channelSettings.getTx() == 0)
        {
            QJsonObject wfmDemodSettingsJsonObject = jsonObject["WFMDemodSettings"].toObject();
            channelSettingsKeys = wfmDemodSettingsJsonObject.keys();
            channelSettings.setWfmDemodSettings(new SWGSDRangel::SWGWFMDemodSettings());
            channelSettings.getWfmDemodSettings()->fromJsonObject(wfmDemodSettingsJsonObject);
            return true;
        }
        else {
            return false;
        }
    }
    else if (*channelType == "WFMMod")
    {
        if (channelSettings.getTx() != 0)
        {
            QJsonObject wfmModSettingsJsonObject = jsonObject["WFMModSettings"].toObject();
            channelSettingsKeys = wfmModSettingsJsonObject.keys();

            if (channelSettingsKeys.contains("cwKeyer"))
            {
                QJsonObject cwKeyerSettingsJsonObject;
                appendSettingsSubKeys(wfmModSettingsJsonObject, cwKeyerSettingsJsonObject, "cwKeyer", channelSettingsKeys);
            }

            channelSettings.setWfmModSettings(new SWGSDRangel::SWGWFMModSettings());
            channelSettings.getWfmModSettings()->fromJsonObject(wfmModSettingsJsonObject);
            return true;
        }
        else {
            return false;
        }
    }
    else
    {
        return false;
    }
}

// TODO: put in library in common with SDRangel. Can be static.
bool WebAPIRequestMapper::validateDeviceSettings(SWGSDRangel::SWGDeviceSettings& deviceSettings, QJsonObject& jsonObject, QStringList& deviceSettingsKeys)
{
    if (jsonObject.contains("tx")) {
        deviceSettings.setTx(jsonObject["tx"].toInt());
    } else {
        deviceSettings.setTx(0); // assume Rx
    }

    if (jsonObject.contains("deviceHwType") && jsonObject["deviceHwType"].isString()) {
        deviceSettings.setDeviceHwType(new QString(jsonObject["deviceHwType"].toString()));
    } else {
        return false;
    }

    QString *deviceHwType = deviceSettings.getDeviceHwType();

    if ((*deviceHwType == "Airspy") && (deviceSettings.getTx() == 0))
    {
        if (jsonObject.contains("airspySettings") && jsonObject["airspySettings"].isObject())
        {
            QJsonObject airspySettingsJsonObject = jsonObject["airspySettings"].toObject();
            deviceSettingsKeys = airspySettingsJsonObject.keys();
            deviceSettings.setAirspySettings(new SWGSDRangel::SWGAirspySettings());
            deviceSettings.getAirspySettings()->fromJsonObject(airspySettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if ((*deviceHwType == "AirspyHF") && (deviceSettings.getTx() == 0))
    {
        if (jsonObject.contains("airspyHFSettings") && jsonObject["airspyHFSettings"].isObject())
        {
            QJsonObject airspyHFSettingsJsonObject = jsonObject["airspyHFSettings"].toObject();
            deviceSettingsKeys = airspyHFSettingsJsonObject.keys();
            deviceSettings.setAirspyHfSettings(new SWGSDRangel::SWGAirspyHFSettings());
            deviceSettings.getAirspyHfSettings()->fromJsonObject(airspyHFSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if ((*deviceHwType == "BladeRF") && (deviceSettings.getTx() == 0))
    {
        if (jsonObject.contains("bladeRFInputSettings") && jsonObject["bladeRFInputSettings"].isObject())
        {
            QJsonObject bladeRFInputSettingsJsonObject = jsonObject["bladeRFInputSettings"].toObject();
            deviceSettingsKeys = bladeRFInputSettingsJsonObject.keys();
            deviceSettings.setBladeRfInputSettings(new SWGSDRangel::SWGBladeRFInputSettings());
            deviceSettings.getBladeRfInputSettings()->fromJsonObject(bladeRFInputSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if ((*deviceHwType == "BladeRF") && (deviceSettings.getTx() != 0))
    {
        if (jsonObject.contains("bladeRFOutputSettings") && jsonObject["bladeRFOutputSettings"].isObject())
        {
            QJsonObject bladeRFOutputSettingsJsonObject = jsonObject["bladeRFOutputSettings"].toObject();
            deviceSettingsKeys = bladeRFOutputSettingsJsonObject.keys();
            deviceSettings.setBladeRfOutputSettings(new SWGSDRangel::SWGBladeRFOutputSettings());
            deviceSettings.getBladeRfOutputSettings()->fromJsonObject(bladeRFOutputSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if (*deviceHwType == "FCDPro")
    {
        if (jsonObject.contains("fcdProSettings") && jsonObject["fcdProSettings"].isObject())
        {
            QJsonObject fcdProSettingsJsonObject = jsonObject["fcdProSettings"].toObject();
            deviceSettingsKeys = fcdProSettingsJsonObject.keys();
            deviceSettings.setFcdProSettings(new SWGSDRangel::SWGFCDProSettings());
            deviceSettings.getFcdProSettings()->fromJsonObject(fcdProSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if (*deviceHwType == "FCDProPlus")
    {
        if (jsonObject.contains("fcdProPlusSettings") && jsonObject["fcdProPlusSettings"].isObject())
        {
            QJsonObject fcdProPlusSettingsJsonObject = jsonObject["fcdProPlusSettings"].toObject();
            deviceSettingsKeys = fcdProPlusSettingsJsonObject.keys();
            deviceSettings.setFcdProPlusSettings(new SWGSDRangel::SWGFCDProPlusSettings());
            deviceSettings.getFcdProPlusSettings()->fromJsonObject(fcdProPlusSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if (*deviceHwType == "FileSource")
    {
        if (jsonObject.contains("fileSourceSettings") && jsonObject["fileSourceSettings"].isObject())
        {
            QJsonObject fileSourceSettingsJsonObject = jsonObject["fileSourceSettings"].toObject();
            deviceSettingsKeys = fileSourceSettingsJsonObject.keys();
            deviceSettings.setFileSourceSettings(new SWGSDRangel::SWGFileSourceSettings());
            deviceSettings.getFileSourceSettings()->fromJsonObject(fileSourceSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if ((*deviceHwType == "HackRF") && (deviceSettings.getTx() == 0))
    {
        if (jsonObject.contains("hackRFInputSettings") && jsonObject["hackRFInputSettings"].isObject())
        {
            QJsonObject hackRFInputSettingsJsonObject = jsonObject["hackRFInputSettings"].toObject();
            deviceSettingsKeys = hackRFInputSettingsJsonObject.keys();
            deviceSettings.setHackRfInputSettings(new SWGSDRangel::SWGHackRFInputSettings());
            deviceSettings.getHackRfInputSettings()->fromJsonObject(hackRFInputSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if ((*deviceHwType == "HackRF") && (deviceSettings.getTx() != 0))
    {
        if (jsonObject.contains("hackRFOutputSettings") && jsonObject["hackRFOutputSettings"].isObject())
        {
            QJsonObject hackRFOutputSettingsJsonObject = jsonObject["hackRFOutputSettings"].toObject();
            deviceSettingsKeys = hackRFOutputSettingsJsonObject.keys();
            deviceSettings.setHackRfOutputSettings(new SWGSDRangel::SWGHackRFOutputSettings());
            deviceSettings.getHackRfOutputSettings()->fromJsonObject(hackRFOutputSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if ((*deviceHwType == "LimeSDR") && (deviceSettings.getTx() == 0))
    {
        if (jsonObject.contains("limeSdrInputSettings") && jsonObject["limeSdrInputSettings"].isObject())
        {
            QJsonObject limeSdrInputSettingsJsonObject = jsonObject["limeSdrInputSettings"].toObject();
            deviceSettingsKeys = limeSdrInputSettingsJsonObject.keys();
            deviceSettings.setLimeSdrInputSettings(new SWGSDRangel::SWGLimeSdrInputSettings());
            deviceSettings.getLimeSdrInputSettings()->fromJsonObject(limeSdrInputSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if ((*deviceHwType == "LimeSDR") && (deviceSettings.getTx() != 0))
    {
        if (jsonObject.contains("limeSdrOutputSettings") && jsonObject["limeSdrOutputSettings"].isObject())
        {
            QJsonObject limeSdrOutputSettingsJsonObject = jsonObject["limeSdrOutputSettings"].toObject();
            deviceSettingsKeys = limeSdrOutputSettingsJsonObject.keys();
            deviceSettings.setLimeSdrOutputSettings(new SWGSDRangel::SWGLimeSdrOutputSettings());
            deviceSettings.getLimeSdrOutputSettings()->fromJsonObject(limeSdrOutputSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if (*deviceHwType == "Perseus")
    {
        if (jsonObject.contains("perseusSettings") && jsonObject["perseusSettings"].isObject())
        {
            QJsonObject perseusSettingsJsonObject = jsonObject["perseusSettings"].toObject();
            deviceSettingsKeys = perseusSettingsJsonObject.keys();
            deviceSettings.setPerseusSettings(new SWGSDRangel::SWGPerseusSettings());
            deviceSettings.getPerseusSettings()->fromJsonObject(perseusSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if ((*deviceHwType == "PlutoSDR") && (deviceSettings.getTx() == 0))
    {
        if (jsonObject.contains("plutoSdrInputSettings") && jsonObject["plutoSdrInputSettings"].isObject())
        {
            QJsonObject plutoSdrInputSettingsJsonObject = jsonObject["plutoSdrInputSettings"].toObject();
            deviceSettingsKeys = plutoSdrInputSettingsJsonObject.keys();
            deviceSettings.setPlutoSdrInputSettings(new SWGSDRangel::SWGPlutoSdrInputSettings());
            deviceSettings.getPlutoSdrInputSettings()->fromJsonObject(plutoSdrInputSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if ((*deviceHwType == "PlutoSDR") && (deviceSettings.getTx() != 0))
    {
        if (jsonObject.contains("plutoSdrOutputSettings") && jsonObject["plutoSdrOutputSettings"].isObject())
        {
            QJsonObject plutoSdrOutputSettingsJsonObject = jsonObject["plutoSdrOutputSettings"].toObject();
            deviceSettingsKeys = plutoSdrOutputSettingsJsonObject.keys();
            deviceSettings.setPlutoSdrOutputSettings(new SWGSDRangel::SWGPlutoSdrOutputSettings());
            deviceSettings.getPlutoSdrOutputSettings()->fromJsonObject(plutoSdrOutputSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if (*deviceHwType == "RTLSDR")
    {
        if (jsonObject.contains("rtlSdrSettings") && jsonObject["rtlSdrSettings"].isObject())
        {
            QJsonObject rtlSdrSettingsJsonObject = jsonObject["rtlSdrSettings"].toObject();
            deviceSettingsKeys = rtlSdrSettingsJsonObject.keys();
            deviceSettings.setRtlSdrSettings(new SWGSDRangel::SWGRtlSdrSettings());
            deviceSettings.getRtlSdrSettings()->fromJsonObject(rtlSdrSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else if (*deviceHwType == "TestSource")
    {
        if (jsonObject.contains("testSourceSettings") && jsonObject["testSourceSettings"].isObject())
        {
            QJsonObject testSourceSettingsJsonObject = jsonObject["testSourceSettings"].toObject();
            deviceSettingsKeys = testSourceSettingsJsonObject.keys();
            deviceSettings.setTestSourceSettings(new SWGSDRangel::SWGTestSourceSettings());
            deviceSettings.getTestSourceSettings()->fromJsonObject(testSourceSettingsJsonObject);
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

// TODO: put in library in common with SDRangel. Can be static.
void WebAPIRequestMapper::appendSettingsSubKeys(
        const QJsonObject& parentSettingsJsonObject,
        QJsonObject& childSettingsJsonObject,
        const QString& parentKey,
        QStringList& keyList)
{
    childSettingsJsonObject = parentSettingsJsonObject[parentKey].toObject();
    QStringList childSettingsKeys = childSettingsJsonObject.keys();

    for (int i = 0; i < childSettingsKeys.size(); i++) {
        keyList.append(parentKey + QString(".") + childSettingsKeys.at(i));
    }
}

// TODO: put in library in common with SDRangel. Can be static.
bool WebAPIRequestMapper::parseJsonBody(QString& jsonStr, QJsonObject& jsonObject, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;

    try
    {
        QByteArray jsonBytes(jsonStr.toStdString().c_str());
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &error);

        if (error.error == QJsonParseError::NoError)
        {
            jsonObject = doc.object();
        }
        else
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

// TODO: put in library in common with SDRangel. Can be static.
void WebAPIRequestMapper::resetChannelSettings(SWGSDRangel::SWGChannelSettings& channelSettings)
{
    channelSettings.cleanup();
    channelSettings.setChannelType(0);
    channelSettings.setAmDemodSettings(0);
    channelSettings.setAmModSettings(0);
    channelSettings.setAtvModSettings(0);
    channelSettings.setBfmDemodSettings(0);
    channelSettings.setDsdDemodSettings(0);
    channelSettings.setNfmDemodSettings(0);
    channelSettings.setNfmModSettings(0);
    channelSettings.setSdrDaemonChannelSinkSettings(0);
    channelSettings.setSsbDemodSettings(0);
    channelSettings.setSsbModSettings(0);
    channelSettings.setUdpSinkSettings(0);
    channelSettings.setUdpSrcSettings(0);
    channelSettings.setWfmDemodSettings(0);
    channelSettings.setWfmModSettings(0);
}

// TODO: put in library in common with SDRangel. Can be static.
void WebAPIRequestMapper::resetDeviceSettings(SWGSDRangel::SWGDeviceSettings& deviceSettings)
{
    deviceSettings.cleanup();
    deviceSettings.setDeviceHwType(0);
    deviceSettings.setAirspySettings(0);
    deviceSettings.setAirspyHfSettings(0);
    deviceSettings.setBladeRfInputSettings(0);
    deviceSettings.setBladeRfOutputSettings(0);
    deviceSettings.setFcdProPlusSettings(0);
    deviceSettings.setFcdProSettings(0);
    deviceSettings.setFileSourceSettings(0);
    deviceSettings.setHackRfInputSettings(0);
    deviceSettings.setHackRfOutputSettings(0);
    deviceSettings.setLimeSdrInputSettings(0);
    deviceSettings.setLimeSdrOutputSettings(0);
    deviceSettings.setPerseusSettings(0);
    deviceSettings.setPlutoSdrInputSettings(0);
    deviceSettings.setPlutoSdrOutputSettings(0);
    deviceSettings.setRtlSdrSettings(0);
    deviceSettings.setSdrDaemonSinkSettings(0);
    deviceSettings.setSdrDaemonSourceSettings(0);
    deviceSettings.setSdrPlaySettings(0);
    deviceSettings.setTestSourceSettings(0);
}

// TODO: put in library in common with SDRangel. Can be static.
void WebAPIRequestMapper::resetDeviceReport(SWGSDRangel::SWGDeviceReport& deviceReport)
{
    deviceReport.cleanup();
    deviceReport.setDeviceHwType(0);
    deviceReport.setAirspyHfReport(0);
    deviceReport.setAirspyReport(0);
    deviceReport.setFileSourceReport(0);
    deviceReport.setLimeSdrInputReport(0);
    deviceReport.setLimeSdrOutputReport(0);
    deviceReport.setPerseusReport(0);
    deviceReport.setPlutoSdrInputReport(0);
    deviceReport.setPlutoSdrOutputReport(0);
    deviceReport.setRtlSdrReport(0);
    deviceReport.setSdrDaemonSinkReport(0);
    deviceReport.setSdrDaemonSourceReport(0);
    deviceReport.setSdrPlayReport(0);
}

} // namespace SDRDaemon


