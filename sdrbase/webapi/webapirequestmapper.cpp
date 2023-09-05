///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
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

#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonArray>

#include <boost/lexical_cast.hpp>

#include "httpdocrootsettings.h"
#include "webapirequestmapper.h"
#include "webapiutils.h"
#include "SWGInstanceSummaryResponse.h"
#include "SWGInstanceConfigResponse.h"
#include "SWGInstanceDevicesResponse.h"
#include "SWGInstanceChannelsResponse.h"
#include "SWGInstanceFeaturesResponse.h"
#include "SWGAudioDevices.h"
#include "SWGLocationInformation.h"
#include "SWGAMBEDevices.h"
#include "SWGLimeRFEDevices.h"
#include "SWGLimeRFESettings.h"
#include "SWGPresets.h"
#include "SWGPresetTransfer.h"
#include "SWGPresetIdentifier.h"
#include "SWGPresetImport.h"
#include "SWGPresetExport.h"
#include "SWGBase64Blob.h"
#include "SWGFilePath.h"
#include "SWGConfigurations.h"
#include "SWGConfigurationIdentifier.h"
#include "SWGConfigurationImportExport.h"
#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGDeviceActions.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelsDetail.h"
#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"
#include "SWGSuccessResponse.h"
#include "SWGErrorResponse.h"
#include "SWGFeaturePresets.h"
#include "SWGFeaturePresetIdentifier.h"
#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"
#include "SWGGLSpectrum.h"
#include "SWGSpectrumServer.h"

WebAPIRequestMapper::WebAPIRequestMapper(QObject* parent) :
    HttpRequestHandler(parent),
    m_adapter(0)
{
#ifndef _MSC_VER
    Q_INIT_RESOURCE(webapi);
#endif
    qtwebapp::HttpDocrootSettings docrootSettings;
    docrootSettings.path = ":/webapi";
    m_staticFileController = new qtwebapp::StaticFileController(docrootSettings, parent);
}

WebAPIRequestMapper::~WebAPIRequestMapper()
{
    delete m_staticFileController;
#ifndef _MSC_VER
    Q_CLEANUP_RESOURCE(webapi);
#endif
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

        if (path == WebAPIAdapterInterface::instanceSummaryURL) {
            instanceSummaryService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceConfigURL) {
            instanceConfigService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceDevicesURL) {
            instanceDevicesService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceChannelsURL) {
            instanceChannelsService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceFeaturesURL) {
            instanceFeaturesService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceLoggingURL) {
            instanceLoggingService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceAudioURL) {
            instanceAudioService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceAudioInputParametersURL) {
            instanceAudioInputParametersService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceAudioOutputParametersURL) {
            instanceAudioOutputParametersService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceAudioInputCleanupURL) {
            instanceAudioInputCleanupService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceAudioOutputCleanupURL) {
            instanceAudioOutputCleanupService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceLocationURL) {
            instanceLocationService(request, response);
        } else if (path == WebAPIAdapterInterface::instancePresetsURL) {
            instancePresetsService(request, response);
        } else if (path == WebAPIAdapterInterface::instancePresetURL) {
            instancePresetService(request, response);
        } else if (path == WebAPIAdapterInterface::instancePresetFileURL) {
            instancePresetFileService(request, response);
        } else if (path == WebAPIAdapterInterface::instancePresetBlobURL) {
            instancePresetBlobService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceConfigurationsURL) {
            instanceConfigurationsService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceConfigurationURL) {
            instanceConfigurationService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceConfigurationFileURL) {
            instanceConfigurationFileService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceConfigurationBlobURL) {
            instanceConfigurationBlobService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceFeaturePresetsURL) {
            instanceFeaturePresetsService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceFeaturePresetURL) {
            instanceFeaturePresetService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceDeviceSetsURL) {
            instanceDeviceSetsService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceDeviceSetURL) {
            instanceDeviceSetService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceWorkspaceURL) {
            instanceWorkspaceService(request, response);
        } else if (path == WebAPIAdapterInterface::featuresetURL) {
            featuresetService(request, response);
        } else if (path == WebAPIAdapterInterface::featuresetFeatureURL) {
            featuresetFeatureService(request, response);
        } else if (path == WebAPIAdapterInterface::featuresetPresetURL) {
            featuresetPresetService(request, response);
        }
        else
        {
            std::smatch desc_match;
            std::string pathStr(path.constData(), path.length());

            if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetURLRe)) {
                devicesetService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetDeviceURLRe)) {
                devicesetDeviceService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetSpectrumSettingsURLRe)) {
                devicesetSpectrumSettingsService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetSpectrumServerURLRe)) {
                devicesetSpectrumServerService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetSpectrumWorkspaceURLRe)) {
                devicesetSpectrumWorkspaceService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetDeviceSettingsURLRe)) {
                devicesetDeviceSettingsService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetDeviceRunURLRe)) {
                devicesetDeviceRunService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetDeviceSubsystemRunURLRe)) {
                devicesetDeviceSubsystemRunService(std::string(desc_match[1]), std::string(desc_match[2]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetDeviceReportURLRe)) {
                devicesetDeviceReportService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetDeviceActionsURLRe)) {
                devicesetDeviceActionsService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetDeviceWorkspaceURLRe)) {
                devicesetDeviceWorkspaceService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetChannelsReportURLRe)) {
                devicesetChannelsReportService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetChannelURLRe)) {
                devicesetChannelService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetChannelIndexURLRe)) {
                devicesetChannelIndexService(std::string(desc_match[1]), std::string(desc_match[2]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetChannelSettingsURLRe)) {
                devicesetChannelSettingsService(std::string(desc_match[1]), std::string(desc_match[2]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetChannelReportURLRe)) {
                devicesetChannelReportService(std::string(desc_match[1]), std::string(desc_match[2]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetChannelActionsURLRe)) {
                devicesetChannelActionsService(std::string(desc_match[1]), std::string(desc_match[2]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetChannelWorkspaceURLRe)) {
                devicesetChannelWorkspaceService(std::string(desc_match[1]), std::string(desc_match[2]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::featuresetFeatureIndexURLRe)) {
                featuresetFeatureIndexService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::featuresetFeatureRunURLRe)) {
                featuresetFeatureRunService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::featuresetFeatureSettingsURLRe)) {
                featuresetFeatureSettingsService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::featuresetFeatureReportURLRe)) {
                featuresetFeatureReportService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::featuresetFeatureActionsURLRe)) {
                featuresetFeatureActionsService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::featuresetFeatureWorkspaceURLRe)) {
                featuresetFeatureWorkspaceService(std::string(desc_match[1]), request, response);
            }
            else // serve static documentation pages
            {
                m_staticFileController->service(request, response);
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
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGInstanceSummaryResponse normalResponse;

        int status = m_adapter->instanceSummary(normalResponse, errorResponse);
        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "DELETE")
    {
        SWGSDRangel::SWGSuccessResponse normalResponse;

        int status = m_adapter->instanceDelete(normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceConfigService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGInstanceConfigResponse query;
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGInstanceConfigResponse normalResponse;
        int status = m_adapter->instanceConfigGet(normalResponse, errorResponse);
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
            WebAPIAdapterInterface::ConfigKeys configKeys;
            SWGSDRangel::SWGInstanceConfigResponse query;
            SWGSDRangel::SWGSuccessResponse normalResponse;
            query.init();

            if (validateConfig(query, jsonObject, configKeys))
            {
                int status = m_adapter->instanceConfigPutPatch(
                    request.getMethod() == "PUT",
                    query,
                    configKeys,
                    normalResponse,
                    errorResponse
                );
                response.setStatus(status);
                qDebug("WebAPIRequestMapper::instanceConfigService: %s: %d",
                    request.getMethod() == "PUT" ? "PUT" : "PATCH", status);

                if (status/100 == 2)
                {
                    normalResponse.setMessage(new QString("Configuration updated successfully"));
                    response.write(normalResponse.asJson().toUtf8());
                }
                else
                {
                    normalResponse.setMessage(new QString("Error occurred while updating configuration"));
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

void WebAPIRequestMapper::instanceDevicesService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGInstanceDevicesResponse normalResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        QByteArray dirStr = request.getParameter("direction");
        int direction = 0;

        if (dirStr.length() != 0)
        {
            bool ok;
            int tmp = dirStr.toInt(&ok);
            if (ok) {
                direction = tmp;
            }
        }

        int status = m_adapter->instanceDevices(direction, normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceChannelsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGInstanceChannelsResponse normalResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        QByteArray dirStr = request.getParameter("direction");
        int direction = 0;

        if (dirStr.length() != 0)
        {
            bool ok;
            int tmp = dirStr.toInt(&ok);
            if (ok) {
                direction = tmp;
            }
        }

        int status = m_adapter->instanceChannels(direction, normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceFeaturesService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGInstanceFeaturesResponse normalResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        int status = m_adapter->instanceFeatures(normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceLoggingService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGLoggingInfo query;
    SWGSDRangel::SWGLoggingInfo normalResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        int status = m_adapter->instanceLoggingGet(normalResponse, errorResponse);
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
            int status = m_adapter->instanceLoggingPut(query, normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceAudioService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGAudioDevices normalResponse;

        int status = m_adapter->instanceAudioGet(normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceAudioInputParametersService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    QString jsonStr = request.getBody();
    QJsonObject jsonObject;

    if (parseJsonBody(jsonStr, jsonObject, response))
    {
        SWGSDRangel::SWGAudioInputDevice normalResponse;
        resetAudioInputDevice(normalResponse);
        QStringList audioInputDeviceKeys;

        if (validateAudioInputDevice(normalResponse, jsonObject, audioInputDeviceKeys))
        {
            if (request.getMethod() == "PATCH")
            {
                int status = m_adapter->instanceAudioInputPatch(
                        normalResponse,
                        audioInputDeviceKeys,
                        errorResponse);
                response.setStatus(status);

                if (status/100 == 2) {
                    response.write(normalResponse.asJson().toUtf8());
                } else {
                    response.write(errorResponse.asJson().toUtf8());
                }
            }
            else if (request.getMethod() == "DELETE")
            {
                int status = m_adapter->instanceAudioInputDelete(
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
                response.setStatus(405,"Invalid HTTP method");
                errorResponse.init();
                *errorResponse.getMessage() = "Invalid HTTP method";
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

void WebAPIRequestMapper::instanceAudioOutputParametersService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    QString jsonStr = request.getBody();
    QJsonObject jsonObject;

    if (parseJsonBody(jsonStr, jsonObject, response))
    {
        SWGSDRangel::SWGAudioOutputDevice normalResponse;
        resetAudioOutputDevice(normalResponse);
        QStringList audioOutputDeviceKeys;

        if (validateAudioOutputDevice(normalResponse, jsonObject, audioOutputDeviceKeys))
        {
            if (request.getMethod() == "PATCH")
            {
                int status = m_adapter->instanceAudioOutputPatch(
                        normalResponse,
                        audioOutputDeviceKeys,
                        errorResponse);
                response.setStatus(status);

                if (status/100 == 2) {
                    response.write(normalResponse.asJson().toUtf8());
                } else {
                    response.write(errorResponse.asJson().toUtf8());
                }
            }
            else if (request.getMethod() == "DELETE")
            {
                int status = m_adapter->instanceAudioOutputDelete(
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
                response.setStatus(405,"Invalid HTTP method");
                errorResponse.init();
                *errorResponse.getMessage() = "Invalid HTTP method";
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

void WebAPIRequestMapper::instanceAudioInputCleanupService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "PATCH")
    {
        SWGSDRangel::SWGSuccessResponse normalResponse;

        int status = m_adapter->instanceAudioInputCleanupPatch(normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceAudioOutputCleanupService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "PATCH")
    {
        SWGSDRangel::SWGSuccessResponse normalResponse;

        int status = m_adapter->instanceAudioOutputCleanupPatch(normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceLocationService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGLocationInformation normalResponse;

        int status = m_adapter->instanceLocationGet(normalResponse, errorResponse);
        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "PUT")
    {
        SWGSDRangel::SWGLocationInformation normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            normalResponse.fromJson(jsonStr);
            int status = m_adapter->instanceLocationPut(normalResponse, errorResponse);
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

void WebAPIRequestMapper::instancePresetsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGPresets normalResponse;
        int status = m_adapter->instancePresetsGet(normalResponse, errorResponse);
        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
}

void WebAPIRequestMapper::instancePresetService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "PATCH")
    {
        SWGSDRangel::SWGPresetTransfer query;
        SWGSDRangel::SWGPresetIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);

            if (validatePresetTransfer(query))
            {
                int status = m_adapter->instancePresetPatch(query, normalResponse, errorResponse);
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
    else if (request.getMethod() == "PUT")
    {
        SWGSDRangel::SWGPresetTransfer query;
        SWGSDRangel::SWGPresetIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);

            if (validatePresetTransfer(query))
            {
                int status = m_adapter->instancePresetPut(query, normalResponse, errorResponse);
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
    else if (request.getMethod() == "POST")
    {
        SWGSDRangel::SWGPresetTransfer query;
        SWGSDRangel::SWGPresetIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);

            if (validatePresetTransfer(query))
            {
                int status = m_adapter->instancePresetPost(query, normalResponse, errorResponse);
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
    else if (request.getMethod() == "DELETE")
    {
        SWGSDRangel::SWGPresetIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            normalResponse.fromJson(jsonStr);

            if (validatePresetIdentifer(normalResponse))
            {
                int status = m_adapter->instancePresetDelete(normalResponse, errorResponse);
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

void WebAPIRequestMapper::instancePresetFileService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "PUT")
    {
        SWGSDRangel::SWGFilePath query;
        SWGSDRangel::SWGPresetIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);

            if (query.getFilePath())
            {
                int status = m_adapter->instancePresetFilePut(query, normalResponse, errorResponse);
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
    else if (request.getMethod() == "POST")
    {
        SWGSDRangel::SWGPresetExport query;
        SWGSDRangel::SWGPresetIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);

            if (validatePresetExport(query))
            {
                int status = m_adapter->instancePresetFilePost(query, normalResponse, errorResponse);
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

void WebAPIRequestMapper::instancePresetBlobService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "PUT")
    {
        SWGSDRangel::SWGBase64Blob query;
        SWGSDRangel::SWGPresetIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);

            if (query.getBlob())
            {
                int status = m_adapter->instancePresetBlobPut(query, normalResponse, errorResponse);
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
    else if (request.getMethod() == "POST")
    {
        SWGSDRangel::SWGPresetIdentifier query;
        SWGSDRangel::SWGBase64Blob normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);

            if (validatePresetIdentifer(query))
            {
                int status = m_adapter->instancePresetBlobPost(query, normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceConfigurationsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGConfigurations normalResponse;
        int status = m_adapter->instanceConfigurationsGet(normalResponse, errorResponse);
        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
}

void WebAPIRequestMapper::instanceConfigurationService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "PATCH")
    {
        SWGSDRangel::SWGConfigurationIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            normalResponse.fromJson(jsonStr);

            if (validateConfigurationIdentifier(normalResponse))
            {
                int status = m_adapter->instanceConfigurationPatch(normalResponse, errorResponse);
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
    else if (request.getMethod() == "PUT")
    {
        SWGSDRangel::SWGConfigurationIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            normalResponse.fromJson(jsonStr);

            if (validateConfigurationIdentifier(normalResponse))
            {
                int status = m_adapter->instanceConfigurationPut(normalResponse, errorResponse);
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
    else if (request.getMethod() == "POST")
    {
        SWGSDRangel::SWGConfigurationIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            normalResponse.fromJson(jsonStr);

            if (validateConfigurationIdentifier(normalResponse))
            {
                int status = m_adapter->instanceConfigurationPost(normalResponse, errorResponse);
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
    else if (request.getMethod() == "DELETE")
    {
        SWGSDRangel::SWGConfigurationIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            normalResponse.fromJson(jsonStr);

            if (validateConfigurationIdentifier(normalResponse))
            {
                int status = m_adapter->instanceConfigurationDelete(normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceConfigurationFileService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "PUT")
    {
        SWGSDRangel::SWGFilePath query;
        SWGSDRangel::SWGConfigurationIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);

            if (query.getFilePath())
            {
                int status = m_adapter->instanceConfigurationFilePut(query, normalResponse, errorResponse);
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
    else if (request.getMethod() == "POST")
    {
        SWGSDRangel::SWGConfigurationImportExport query;
        SWGSDRangel::SWGConfigurationIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);

            if (query.getFilePath() && query.getConfiguration() && validateConfigurationIdentifier(*query.getConfiguration()))
            {
                int status = m_adapter->instanceConfigurationFilePost(query, normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceConfigurationBlobService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "PUT")
    {
        SWGSDRangel::SWGBase64Blob query;
        SWGSDRangel::SWGConfigurationIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);

            if (query.getBlob())
            {
                int status = m_adapter->instanceConfigurationBlobPut(query, normalResponse, errorResponse);
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
    else if (request.getMethod() == "POST")
    {
        SWGSDRangel::SWGConfigurationIdentifier query;
        SWGSDRangel::SWGBase64Blob normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);

            if (validateConfigurationIdentifier(query))
            {
                int status = m_adapter->instanceConfigurationBlobPost(query, normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceFeaturePresetsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGFeaturePresets normalResponse;
        int status = m_adapter->instanceFeaturePresetsGet(normalResponse, errorResponse);
        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
}

void WebAPIRequestMapper::instanceFeaturePresetService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "DELETE")
    {
        SWGSDRangel::SWGFeaturePresetIdentifier normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            normalResponse.fromJson(jsonStr);

            if (validateFeaturePresetIdentifer(normalResponse))
            {
                int status = m_adapter->instanceFeaturePresetDelete(normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceDeviceSetsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGDeviceSetList normalResponse;
        int status = m_adapter->instanceDeviceSetsGet(normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceDeviceSetService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "POST")
    {
        SWGSDRangel::SWGSuccessResponse normalResponse;
        QByteArray dirStr = request.getParameter("direction");
        int direction = 0;

        if (dirStr.length() != 0)
        {
            bool ok;
            int tmp = dirStr.toInt(&ok);
            if (ok) {
                direction = tmp;
            }
        }

        int status = m_adapter->instanceDeviceSetPost(direction, normalResponse, errorResponse);
        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "DELETE")
    {
        SWGSDRangel::SWGSuccessResponse normalResponse;
        int status = m_adapter->instanceDeviceSetDelete(normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceWorkspaceService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "POST")
    {
        SWGSDRangel::SWGSuccessResponse normalResponse;
        int status = m_adapter->instanceWorkspacePost(normalResponse, errorResponse);
        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "DELETE")
    {
        SWGSDRangel::SWGSuccessResponse normalResponse;
        int status = m_adapter->instanceWorkspaceDelete(normalResponse, errorResponse);
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

void WebAPIRequestMapper::devicesetService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        try
        {
            SWGSDRangel::SWGDeviceSet normalResponse;
            int deviceSetIndex = boost::lexical_cast<int>(indexStr);
            int status = m_adapter->devicesetGet(deviceSetIndex, normalResponse, errorResponse);
            response.setStatus(status);

            if (status/100 == 2) {
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
        errorResponse.init();
        *errorResponse.getMessage() = "Invalid HTTP method";
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetSpectrumSettingsService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(indexStr);

        if ((request.getMethod() == "PUT") || (request.getMethod() == "PATCH"))
        {
            QString jsonStr = request.getBody();
            QJsonObject jsonObject;

            if (parseJsonBody(jsonStr, jsonObject, response))
            {
                SWGSDRangel::SWGGLSpectrum normalResponse;
                resetSpectrumSettings(normalResponse);
                QStringList spectrumSettingsKeys;

                if (validateSpectrumSettings(normalResponse, jsonObject, spectrumSettingsKeys))
                {
                    int status = m_adapter->devicesetSpectrumSettingsPutPatch(
                            deviceSetIndex,
                            (request.getMethod() == "PUT"), // force settings on PUT
                            spectrumSettingsKeys,
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
            SWGSDRangel::SWGGLSpectrum normalResponse;
            resetSpectrumSettings(normalResponse);
            int status = m_adapter->devicesetSpectrumSettingsGet(deviceSetIndex, normalResponse, errorResponse);
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
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on device set index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetSpectrumServerService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(indexStr);

        if (request.getMethod() == "GET")
        {
            SWGSDRangel::SWGSpectrumServer normalResponse;
            int status = m_adapter->devicesetSpectrumServerGet(deviceSetIndex, normalResponse, errorResponse);

            response.setStatus(status);

            if (status/100 == 2) {
                response.write(normalResponse.asJson().toUtf8());
            } else {
                response.write(errorResponse.asJson().toUtf8());
            }
        }
        else if (request.getMethod() == "POST")
        {
            SWGSDRangel::SWGSuccessResponse normalResponse;
            int status = m_adapter->devicesetSpectrumServerPost(deviceSetIndex, normalResponse, errorResponse);

            response.setStatus(status);

            if (status/100 == 2) {
                response.write(normalResponse.asJson().toUtf8());
            } else {
                response.write(errorResponse.asJson().toUtf8());
            }
        }
        else if (request.getMethod() == "DELETE")
        {
            SWGSDRangel::SWGSuccessResponse normalResponse;
            int status = m_adapter->devicesetSpectrumServerDelete(deviceSetIndex, normalResponse, errorResponse);

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
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on device set index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetSpectrumWorkspaceService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(indexStr);

        if (request.getMethod() == "GET")
        {
            SWGSDRangel::SWGWorkspaceInfo normalResponse;
            int status = m_adapter->devicesetSpectrumWorkspaceGet(deviceSetIndex, normalResponse, errorResponse);
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
                SWGSDRangel::SWGWorkspaceInfo query;
                SWGSDRangel::SWGSuccessResponse normalResponse;

                if (validateWorkspaceInfo(query, jsonObject))
                {
                    int status = m_adapter->devicesetSpectrumWorkspacePut(
                        deviceSetIndex,
                        query,
                        normalResponse,
                        errorResponse
                    );
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
    }
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on device set index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetDeviceService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(indexStr);

        if (request.getMethod() == "PUT")
        {
            QString jsonStr = request.getBody();
            QJsonObject jsonObject;

            if (parseJsonBody(jsonStr, jsonObject, response))
            {
                SWGSDRangel::SWGDeviceListItem query;
                SWGSDRangel::SWGDeviceListItem normalResponse;

                if (validateDeviceListItem(query, jsonObject))
                {
                    int status = m_adapter->devicesetDevicePut(deviceSetIndex, query, normalResponse, errorResponse);
                    response.setStatus(status);

                    if (status/100 == 2) {
                        response.write(normalResponse.asJson().toUtf8());
                    } else {
                        response.write(errorResponse.asJson().toUtf8());
                    }
                }
                else
                {
                    response.setStatus(400,"Missing device identification");
                    errorResponse.init();
                    *errorResponse.getMessage() = "Missing device identification";
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
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on device set index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetDeviceSettingsService(
    const std::string& indexStr,
    qtwebapp::HttpRequest& request,
    qtwebapp::HttpResponse& response
)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(indexStr);

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
                    int status = m_adapter->devicesetDeviceSettingsPutPatch(
                            deviceSetIndex,
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
            int status = m_adapter->devicesetDeviceSettingsGet(deviceSetIndex, normalResponse, errorResponse);
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
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on device set index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetDeviceRunService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(indexStr);

        if (request.getMethod() == "GET")
        {
            SWGSDRangel::SWGDeviceState normalResponse;
            int status = m_adapter->devicesetDeviceRunGet(deviceSetIndex, normalResponse, errorResponse);

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
            int status = m_adapter->devicesetDeviceRunPost(deviceSetIndex, normalResponse, errorResponse);

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
            int status = m_adapter->devicesetDeviceRunDelete(deviceSetIndex, normalResponse, errorResponse);

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
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on device set index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetDeviceSubsystemRunService(const std::string& indexStr, const std::string& subsystemIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(indexStr);
        int subsystemIndex = boost::lexical_cast<int>(subsystemIndexStr);

        if (request.getMethod() == "GET")
        {
            SWGSDRangel::SWGDeviceState normalResponse;
            int status = m_adapter->devicesetDeviceSubsystemRunGet(deviceSetIndex, subsystemIndex, normalResponse, errorResponse);

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
            int status = m_adapter->devicesetDeviceSubsystemRunPost(deviceSetIndex, subsystemIndex, normalResponse, errorResponse);

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
            int status = m_adapter->devicesetDeviceSubsystemRunDelete(deviceSetIndex, subsystemIndex, normalResponse, errorResponse);

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
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on device set index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetDeviceReportService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        try
        {
            SWGSDRangel::SWGDeviceReport normalResponse;
            resetDeviceReport(normalResponse);
            int deviceSetIndex = boost::lexical_cast<int>(indexStr);
            int status = m_adapter->devicesetDeviceReportGet(deviceSetIndex, normalResponse, errorResponse);
            response.setStatus(status);

            if (status/100 == 2) {
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
        errorResponse.init();
        *errorResponse.getMessage() = "Invalid HTTP method";
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetDeviceWorkspaceService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(indexStr);

        if (request.getMethod() == "GET")
        {
            SWGSDRangel::SWGWorkspaceInfo normalResponse;
            int status = m_adapter->devicesetDeviceWorkspaceGet(deviceSetIndex, normalResponse, errorResponse);
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
                SWGSDRangel::SWGWorkspaceInfo query;
                SWGSDRangel::SWGSuccessResponse normalResponse;

                if (validateWorkspaceInfo(query, jsonObject))
                {
                    int status = m_adapter->devicesetDeviceWorkspacePut(
                        deviceSetIndex,
                        query,
                        normalResponse,
                        errorResponse
                    );
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
    }
    catch(const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on device set index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetDeviceActionsService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(indexStr);

        if (request.getMethod() == "POST")
        {
            QString jsonStr = request.getBody();
            QJsonObject jsonObject;

            if (parseJsonBody(jsonStr, jsonObject, response))
            {
                SWGSDRangel::SWGDeviceActions query;
                SWGSDRangel::SWGSuccessResponse normalResponse;
                resetDeviceActions(query);
                QStringList deviceActionsKeys;

                if (validateDeviceActions(query, jsonObject, deviceActionsKeys))
                {
                    int status = m_adapter->devicesetDeviceActionsPost(
                        deviceSetIndex,
                        deviceActionsKeys,
                        query,
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
    catch(const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on device set index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetChannelsReportService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        try
        {
            SWGSDRangel::SWGChannelsDetail normalResponse;
            int deviceSetIndex = boost::lexical_cast<int>(indexStr);
            int status = m_adapter->devicesetChannelsReportGet(deviceSetIndex, normalResponse, errorResponse);
            response.setStatus(status);

            if (status/100 == 2) {
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
        errorResponse.init();
        *errorResponse.getMessage() = "Invalid HTTP method";
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetChannelService(
        const std::string& deviceSetIndexStr,
        qtwebapp::HttpRequest& request,
        qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(deviceSetIndexStr);

        if (request.getMethod() == "POST")
        {
            QString jsonStr = request.getBody();
            QJsonObject jsonObject;

            if (parseJsonBody(jsonStr, jsonObject, response))
            {
                SWGSDRangel::SWGChannelSettings query;
                SWGSDRangel::SWGSuccessResponse normalResponse;
                resetChannelSettings(query);

                if (jsonObject.contains("direction")) {
                    query.setDirection(jsonObject["direction"].toInt());
                } else {
                    query.setDirection(0); // assume Rx
                }

                if (jsonObject.contains("channelType") && jsonObject["channelType"].isString())
                {
                    query.setChannelType(new QString(jsonObject["channelType"].toString()));

                    int status = m_adapter->devicesetChannelPost(deviceSetIndex, query, normalResponse, errorResponse);

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
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetChannelIndexService(
        const std::string& deviceSetIndexStr,
        const std::string& channelIndexStr,
        qtwebapp::HttpRequest& request,
        qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(deviceSetIndexStr);
        int channelIndex = boost::lexical_cast<int>(channelIndexStr);

        if (request.getMethod() == "DELETE")
        {
            SWGSDRangel::SWGSuccessResponse normalResponse;
            int status = m_adapter->devicesetChannelDelete(deviceSetIndex, channelIndex, normalResponse, errorResponse);

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
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetChannelSettingsService(
        const std::string& deviceSetIndexStr,
        const std::string& channelIndexStr,
        qtwebapp::HttpRequest& request,
        qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(deviceSetIndexStr);
        int channelIndex = boost::lexical_cast<int>(channelIndexStr);

        if (request.getMethod() == "GET")
        {
            SWGSDRangel::SWGChannelSettings normalResponse;
            resetChannelSettings(normalResponse);
            int status = m_adapter->devicesetChannelSettingsGet(deviceSetIndex, channelIndex, normalResponse, errorResponse);
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
                    int status = m_adapter->devicesetChannelSettingsPutPatch(
                            deviceSetIndex,
                            channelIndex,
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
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetChannelReportService(
        const std::string& deviceSetIndexStr,
        const std::string& channelIndexStr,
        qtwebapp::HttpRequest& request,
        qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(deviceSetIndexStr);
        int channelIndex = boost::lexical_cast<int>(channelIndexStr);

        if (request.getMethod() == "GET")
        {
            SWGSDRangel::SWGChannelReport normalResponse;
            resetChannelReport(normalResponse);
            int status = m_adapter->devicesetChannelReportGet(deviceSetIndex, channelIndex, normalResponse, errorResponse);
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
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::devicesetChannelActionsService(
        const std::string& deviceSetIndexStr,
        const std::string& channelIndexStr,
        qtwebapp::HttpRequest& request,
        qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(deviceSetIndexStr);
        int channelIndex = boost::lexical_cast<int>(channelIndexStr);

        if (request.getMethod() == "POST")
        {
            QString jsonStr = request.getBody();
            QJsonObject jsonObject;

            if (parseJsonBody(jsonStr, jsonObject, response))
            {
                SWGSDRangel::SWGChannelActions query;
                SWGSDRangel::SWGSuccessResponse normalResponse;
                resetChannelActions(query);
                QStringList channelActionsKeys;

                if (validateChannelActions(query, jsonObject, channelActionsKeys))
                {
                    int status = m_adapter->devicesetChannelActionsPost(
                        deviceSetIndex,
                        channelIndex,
                        channelActionsKeys,
                        query,
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
    catch(const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}


void WebAPIRequestMapper::devicesetChannelWorkspaceService(
        const std::string& deviceSetIndexStr,
        const std::string& channelIndexStr,
        qtwebapp::HttpRequest& request,
        qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(deviceSetIndexStr);
        int channelIndex = boost::lexical_cast<int>(channelIndexStr);

        if (request.getMethod() == "GET")
        {
            SWGSDRangel::SWGWorkspaceInfo normalResponse;
            int status = m_adapter->devicesetChannelWorkspaceGet(deviceSetIndex, channelIndex, normalResponse, errorResponse);
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
                SWGSDRangel::SWGWorkspaceInfo query;
                SWGSDRangel::SWGSuccessResponse normalResponse;

                if (validateWorkspaceInfo(query, jsonObject))
                {
                    int status = m_adapter->devicesetChannelWorkspacePut(
                        deviceSetIndex,
                        channelIndex,
                        query,
                        normalResponse,
                        errorResponse
                    );
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
    catch(const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::featuresetService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGFeatureSet normalResponse;
        int status = m_adapter->featuresetGet(0, normalResponse, errorResponse);
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

void WebAPIRequestMapper::featuresetFeatureService(
        qtwebapp::HttpRequest& request,
        qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "POST")
    {
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            SWGSDRangel::SWGFeatureSettings query;
            SWGSDRangel::SWGSuccessResponse normalResponse;
            resetFeatureSettings(query);

            if (jsonObject.contains("featureType") && jsonObject["featureType"].isString())
            {
                query.setFeatureType(new QString(jsonObject["featureType"].toString()));

                int status = m_adapter->featuresetFeaturePost(0, query, normalResponse, errorResponse);

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

void WebAPIRequestMapper::featuresetPresetService(
    qtwebapp::HttpRequest& request,
    qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "PATCH")
    {
        SWGSDRangel::SWGFeaturePresetIdentifier query;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);

            if (validateFeaturePresetIdentifer(query))
            {
                int status = m_adapter->featuresetPresetPatch(0, query, errorResponse);
                response.setStatus(status);

                if (status/100 == 2) {
                    response.write(query.asJson().toUtf8());
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
    else if (request.getMethod() == "PUT")
    {
        SWGSDRangel::SWGFeaturePresetIdentifier query;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);

            if (validateFeaturePresetIdentifer(query))
            {
                int status = m_adapter->featuresetPresetPut(0, query, errorResponse);
                response.setStatus(status);

                if (status/100 == 2) {
                    response.write(query.asJson().toUtf8());
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
    else if (request.getMethod() == "POST")
    {
        SWGSDRangel::SWGFeaturePresetIdentifier query;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            query.fromJson(jsonStr);

            if (validateFeaturePresetIdentifer(query))
            {
                int status = m_adapter->featuresetPresetPost(0, query, errorResponse);
                response.setStatus(status);

                if (status/100 == 2) {
                    response.write(query.asJson().toUtf8());
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

void WebAPIRequestMapper::featuresetFeatureIndexService(
        const std::string& featureIndexStr,
        qtwebapp::HttpRequest& request,
        qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int featureIndex = boost::lexical_cast<int>(featureIndexStr);

        if (request.getMethod() == "DELETE")
        {
            SWGSDRangel::SWGSuccessResponse normalResponse;
            int status = m_adapter->featuresetFeatureDelete(0, featureIndex, normalResponse, errorResponse);

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
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::featuresetFeatureRunService(
        const std::string& featureIndexStr,
        qtwebapp::HttpRequest& request,
        qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int featureIndex = boost::lexical_cast<int>(featureIndexStr);

        if (request.getMethod() == "GET")
        {
            SWGSDRangel::SWGDeviceState normalResponse;
            int status = m_adapter->featuresetFeatureRunGet(0, featureIndex, normalResponse, errorResponse);

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
            int status = m_adapter->featuresetFeatureRunPost(0, featureIndex, normalResponse, errorResponse);

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
            int status = m_adapter->featuresetFeatureRunDelete(0, featureIndex, normalResponse, errorResponse);

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
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::featuresetFeatureSettingsService(
        const std::string& featureIndexStr,
        qtwebapp::HttpRequest& request,
        qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int featureIndex = boost::lexical_cast<int>(featureIndexStr);

        if (request.getMethod() == "GET")
        {
            SWGSDRangel::SWGFeatureSettings normalResponse;
            resetFeatureSettings(normalResponse);
            int status = m_adapter->featuresetFeatureSettingsGet(0, featureIndex, normalResponse, errorResponse);
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
                SWGSDRangel::SWGFeatureSettings normalResponse;
                resetFeatureSettings(normalResponse);
                QStringList featureSettingsKeys;

                if (validateFeatureSettings(normalResponse, jsonObject, featureSettingsKeys))
                {
                    int status = m_adapter->featuresetFeatureSettingsPutPatch(
                            0,
                            featureIndex,
                            (request.getMethod() == "PUT"), // force settings on PUT
                            featureSettingsKeys,
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
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::featuresetFeatureReportService(
        const std::string& featureIndexStr,
        qtwebapp::HttpRequest& request,
        qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int featureIndex = boost::lexical_cast<int>(featureIndexStr);

        if (request.getMethod() == "GET")
        {
            SWGSDRangel::SWGFeatureReport normalResponse;
            resetFeatureReport(normalResponse);
            int status = m_adapter->featuresetFeatureReportGet(0, featureIndex, normalResponse, errorResponse);
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
    catch (const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::featuresetFeatureActionsService(
        const std::string& featureIndexStr,
        qtwebapp::HttpRequest& request,
        qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int featureIndex = boost::lexical_cast<int>(featureIndexStr);

        if (request.getMethod() == "POST")
        {
            QString jsonStr = request.getBody();
            QJsonObject jsonObject;

            if (parseJsonBody(jsonStr, jsonObject, response))
            {
                SWGSDRangel::SWGFeatureActions query;
                SWGSDRangel::SWGSuccessResponse normalResponse;
                resetFeatureActions(query);
                QStringList featureActionsKeys;

                if (validateFeatureActions(query, jsonObject, featureActionsKeys))
                {
                    int status = m_adapter->featuresetFeatureActionsPost(
                        0,
                        featureIndex,
                        featureActionsKeys,
                        query,
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
    catch(const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

void WebAPIRequestMapper::featuresetFeatureWorkspaceService(
        const std::string& featureIndexStr,
        qtwebapp::HttpRequest& request,
        qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int featureIndex = boost::lexical_cast<int>(featureIndexStr);

        if (request.getMethod() == "GET")
        {
            SWGSDRangel::SWGWorkspaceInfo normalResponse;
            int status = m_adapter->featuresetFeatureWorkspaceGet(featureIndex, normalResponse, errorResponse);
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
                SWGSDRangel::SWGWorkspaceInfo query;
                SWGSDRangel::SWGSuccessResponse normalResponse;

                if (validateWorkspaceInfo(query, jsonObject))
                {
                    int status = m_adapter->featuresetFeatureWorkspacePut(
                        featureIndex,
                        query,
                        normalResponse,
                        errorResponse
                    );
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
    catch(const boost::bad_lexical_cast &e)
    {
        errorResponse.init();
        *errorResponse.getMessage() = "Wrong integer conversion on index";
        response.setStatus(400,"Invalid data");
        response.write(errorResponse.asJson().toUtf8());
    }
}

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

bool WebAPIRequestMapper::validateConfigurationIdentifier(SWGSDRangel::SWGConfigurationIdentifier& configruationIdentifier)
{
    return configruationIdentifier.getGroupName() && configruationIdentifier.getName();
}

bool WebAPIRequestMapper::validateFeaturePresetIdentifer(SWGSDRangel::SWGFeaturePresetIdentifier& presetIdentifier)
{
    return (presetIdentifier.getGroupName() && presetIdentifier.getDescription());
}

bool WebAPIRequestMapper::validatePresetExport(SWGSDRangel::SWGPresetExport& presetExport)
{
    if (presetExport.getFilePath() == 0) {
        return false;
    }

    SWGSDRangel::SWGPresetIdentifier *presetIdentifier =  presetExport.getPreset();

    if (presetIdentifier == 0) {
        return false;
    }

    return validatePresetIdentifer(*presetIdentifier);
}

bool WebAPIRequestMapper::validateConfigurationImportExport(SWGSDRangel::SWGConfigurationImportExport& configurationImportExport)
{
    if (configurationImportExport.getFilePath() == nullptr) {
        return false;
    }

    SWGSDRangel::SWGConfigurationIdentifier *congfigurationIdentifier =  configurationImportExport.getConfiguration();

    if (congfigurationIdentifier == nullptr) {
        return false;
    }

    return validateConfigurationIdentifier(*congfigurationIdentifier);
}

bool WebAPIRequestMapper::validateDeviceListItem(SWGSDRangel::SWGDeviceListItem& deviceListItem, QJsonObject& jsonObject)
{
    if (jsonObject.contains("direction")) {
        deviceListItem.setDirection(jsonObject["direction"].toInt());
    } else {
        deviceListItem.setDirection(0); // assume Rx
    }

    bool identified = false;

    if (jsonObject.contains("displayedName") && jsonObject["displayedName"].isString())
    {
        deviceListItem.setDisplayedName(new QString(jsonObject["displayedName"].toString()));
        identified = true;
    } else {
        deviceListItem.setDisplayedName(0);
    }


    if (jsonObject.contains("hwType") && jsonObject["hwType"].isString())
    {
        deviceListItem.setHwType(new QString(jsonObject["hwType"].toString()));
        identified = true;
    } else {
        deviceListItem.setHwType(0);
    }

    if (jsonObject.contains("serial") && jsonObject["serial"].isString())
    {
        deviceListItem.setSerial(new QString(jsonObject["serial"].toString()));
        identified = true;
    } else {
        deviceListItem.setSerial(0);
    }

    if (jsonObject.contains("index")) {
        deviceListItem.setIndex(jsonObject["index"].toInt(-1));
    } else {
        deviceListItem.setIndex(-1);
    }

    if (jsonObject.contains("sequence")){
        deviceListItem.setSequence(jsonObject["sequence"].toInt(-1));
    } else {
        deviceListItem.setSequence(-1);
    }

    if (jsonObject.contains("deviceStreamIndex")) {
        deviceListItem.setDeviceStreamIndex(jsonObject["deviceStreamIndex"].toInt(-1));
    } else {
        deviceListItem.setDeviceStreamIndex(-1);
    }

    return identified;
}

bool WebAPIRequestMapper::validateDeviceSettings(
        SWGSDRangel::SWGDeviceSettings& deviceSettings,
        QJsonObject& jsonObject,
        QStringList& deviceSettingsKeys)
{
    if (jsonObject.contains("direction")) {
        deviceSettings.setDirection(jsonObject["direction"].toInt());
    } else {
        deviceSettings.setDirection(0); // assume single Rx
    }

    if (jsonObject.contains("deviceHwType") && jsonObject["deviceHwType"].isString()) {
        deviceSettings.setDeviceHwType(new QString(jsonObject["deviceHwType"].toString()));
    } else {
        return false;
    }

    QString *deviceHwType = deviceSettings.getDeviceHwType();
    QString deviceSettingsKey;

    if (deviceSettings.getDirection() == 0) // source
    {
        if (WebAPIUtils::m_sourceDeviceHwIdToSettingsKey.contains(*deviceHwType)) {
            deviceSettingsKey = WebAPIUtils::m_sourceDeviceHwIdToSettingsKey[*deviceHwType];
        } else {
            return false;
        }
    }
    else if (deviceSettings.getDirection() == 1) // sink
    {
        if (WebAPIUtils::m_sinkDeviceHwIdToSettingsKey.contains(*deviceHwType)) {
            deviceSettingsKey = WebAPIUtils::m_sinkDeviceHwIdToSettingsKey[*deviceHwType];
        } else {
            return false;
        }
    }
    else if (deviceSettings.getDirection() == 2) // MIMO
    {
        if (WebAPIUtils::m_mimoDeviceHwIdToSettingsKey.contains(*deviceHwType)) {
            deviceSettingsKey = WebAPIUtils::m_mimoDeviceHwIdToSettingsKey[*deviceHwType];
        } else {
            return false;
        }
    }
    else
    {
        return false;
    }

    return getDeviceSettings(deviceSettingsKey, &deviceSettings, jsonObject, deviceSettingsKeys);
}

bool WebAPIRequestMapper::validateDeviceActions(
        SWGSDRangel::SWGDeviceActions& deviceActions,
        QJsonObject& jsonObject,
        QStringList& deviceActionsKeys)
{
    if (jsonObject.contains("direction")) {
        deviceActions.setDirection(jsonObject["direction"].toInt());
    } else {
        deviceActions.setDirection(0); // assume single Rx
    }

    if (jsonObject.contains("deviceHwType") && jsonObject["deviceHwType"].isString()) {
        deviceActions.setDeviceHwType(new QString(jsonObject["deviceHwType"].toString()));
    } else {
        return false;
    }

    QString *deviceHwType = deviceActions.getDeviceHwType();
    QString deviceActionsKey;

    if (deviceActions.getDirection() == 0) // source
    {
        if (WebAPIUtils::m_sourceDeviceHwIdToSettingsKey.contains(*deviceHwType)) {
            deviceActionsKey = WebAPIUtils::m_sourceDeviceHwIdToActionsKey[*deviceHwType];
        } else {
            return false;
        }
    }
    else if (deviceActions.getDirection() == 1) // sink
    {
        if (WebAPIUtils::m_sinkDeviceHwIdToSettingsKey.contains(*deviceHwType)) {
            deviceActionsKey = WebAPIUtils::m_sinkDeviceHwIdToActionsKey[*deviceHwType];
        } else {
            return false;
        }
    }
    else if (deviceActions.getDirection() == 2) // MIMO
    {
        if (WebAPIUtils::m_mimoDeviceHwIdToSettingsKey.contains(*deviceHwType)) {
            deviceActionsKey = WebAPIUtils::m_mimoDeviceHwIdToActionsKey[*deviceHwType];
        } else {
            return false;
        }
    }
    else
    {
        return false;
    }

    return getDeviceActions(deviceActionsKey, &deviceActions, jsonObject, deviceActionsKeys);
}

bool WebAPIRequestMapper::validateChannelSettings(
        SWGSDRangel::SWGChannelSettings& channelSettings,
        QJsonObject& jsonObject,
        QStringList& channelSettingsKeys)
{
    if (jsonObject.contains("direction")) {
        channelSettings.setDirection(jsonObject["direction"].toInt());
    } else {
        channelSettings.setDirection(0); // assume single Rx
    }

    if (jsonObject.contains("channelType") && jsonObject["channelType"].isString()) {
        channelSettings.setChannelType(new QString(jsonObject["channelType"].toString()));
    } else {
        return false;
    }

    QString *channelType = channelSettings.getChannelType();

    if (WebAPIUtils::m_channelTypeToSettingsKey.contains(*channelType)) {
        return getChannelSettings(WebAPIUtils::m_channelTypeToSettingsKey[*channelType], &channelSettings, jsonObject, channelSettingsKeys);
    } else {
        return false;
    }
}

bool WebAPIRequestMapper::validateChannelActions(
    SWGSDRangel::SWGChannelActions& channelActions,
    QJsonObject& jsonObject,
    QStringList& channelActionsKeys)
{
    if (jsonObject.contains("direction")) {
        channelActions.setDirection(jsonObject["direction"].toInt());
    } else {
        channelActions.setDirection(0); // assume single Rx
    }

    if (jsonObject.contains("channelType") && jsonObject["channelType"].isString()) {
        channelActions.setChannelType(new QString(jsonObject["channelType"].toString()));
    } else {
        return false;
    }

    QString *channelType = channelActions.getChannelType();

    if (WebAPIUtils::m_channelTypeToActionsKey.contains(*channelType)) {
        return getChannelActions(WebAPIUtils::m_channelTypeToActionsKey[*channelType], &channelActions, jsonObject, channelActionsKeys);
    } else {
        return false;
    }
}

bool WebAPIRequestMapper::validateFeatureSettings(
        SWGSDRangel::SWGFeatureSettings& featureSettings,
        QJsonObject& jsonObject,
        QStringList& featureSettingsKeys)
{
    if (jsonObject.contains("featureType") && jsonObject["featureType"].isString()) {
        featureSettings.setFeatureType(new QString(jsonObject["featureType"].toString()));
    } else {
        return false;
    }

    QString *featureType = featureSettings.getFeatureType();

    if (WebAPIUtils::m_featureTypeToSettingsKey.contains(*featureType)) {
        return getFeatureSettings(WebAPIUtils::m_featureTypeToSettingsKey[*featureType], &featureSettings, jsonObject, featureSettingsKeys);
    } else {
        return false;
    }
}

bool WebAPIRequestMapper::validateFeatureActions(
    SWGSDRangel::SWGFeatureActions& featureActions,
    QJsonObject& jsonObject,
    QStringList& featureActionsKeys)
{
    if (jsonObject.contains("featureType") && jsonObject["featureType"].isString()) {
        featureActions.setFeatureType(new QString(jsonObject["featureType"].toString()));
    } else {
        return false;
    }

    QString *featureType = featureActions.getFeatureType();

    if (WebAPIUtils::m_featureTypeToActionsKey.contains(*featureType)) {
        return getFeatureActions(WebAPIUtils::m_featureTypeToActionsKey[*featureType], &featureActions, jsonObject, featureActionsKeys);
    } else {
        return false;
    }
}

bool WebAPIRequestMapper::validateAudioInputDevice(
        SWGSDRangel::SWGAudioInputDevice& audioInputDevice,
        QJsonObject& jsonObject,
        QStringList& audioInputDeviceKeys)
{
    if (jsonObject.contains("index")) {
        audioInputDevice.setIndex(jsonObject["index"].toInt());
    } else {
        audioInputDevice.setIndex(-1); // assume systam default
    }
    if (jsonObject.contains("sampleRate"))
    {
        audioInputDevice.setSampleRate(jsonObject["sampleRate"].toInt());
        audioInputDeviceKeys.append("sampleRate");
    }
    if (jsonObject.contains("volume"))
    {
        audioInputDevice.setVolume(jsonObject["volume"].toDouble());
        audioInputDeviceKeys.append("volume");
    }
    return true;
}

bool WebAPIRequestMapper::validateAudioOutputDevice(
        SWGSDRangel::SWGAudioOutputDevice& audioOutputDevice,
        QJsonObject& jsonObject,
        QStringList& audioOutputDeviceKeys)
{
    if (jsonObject.contains("index")) {
        audioOutputDevice.setIndex(jsonObject["index"].toInt());
    } else {
        audioOutputDevice.setIndex(-1); // assume systam default
    }
    if (jsonObject.contains("sampleRate"))
    {
        audioOutputDevice.setSampleRate(jsonObject["sampleRate"].toInt());
        audioOutputDeviceKeys.append("sampleRate");
    }
    if (jsonObject.contains("copyToUDP"))
    {
        audioOutputDevice.setCopyToUdp(jsonObject["copyToUDP"].toInt() == 0 ? 0 : 1);
        audioOutputDeviceKeys.append("copyToUDP");
    }
    if (jsonObject.contains("udpUsesRTP"))
    {
        audioOutputDevice.setUdpUsesRtp(jsonObject["udpUsesRTP"].toInt() == 0 ? 0 : 1);
        audioOutputDeviceKeys.append("udpUsesRTP");
    }
    if (jsonObject.contains("udpChannelMode"))
    {
        audioOutputDevice.setUdpChannelMode(jsonObject["udpChannelMode"].toInt());
        audioOutputDeviceKeys.append("udpChannelMode");
    }
    if (jsonObject.contains("udpChannelCodec"))
    {
        audioOutputDevice.setUdpChannelCodec(jsonObject["udpChannelCodec"].toInt());
        audioOutputDeviceKeys.append("udpChannelCodec");
    }
    if (jsonObject.contains("udpDecimationFactor"))
    {
        audioOutputDevice.setUdpDecimationFactor(jsonObject["udpDecimationFactor"].toInt());
        audioOutputDeviceKeys.append("udpDecimationFactor");
    }
    if (jsonObject.contains("udpAddress"))
    {
        audioOutputDevice.setUdpAddress(new QString(jsonObject["udpAddress"].toString()));
        audioOutputDeviceKeys.append("udpAddress");
    }
    if (jsonObject.contains("udpPort"))
    {
        audioOutputDevice.setUdpPort(jsonObject["udpPort"].toInt());
        audioOutputDeviceKeys.append("udpPort");
    }
    if (jsonObject.contains("fileRecordName"))
    {
        audioOutputDevice.setFileRecordName(new QString(jsonObject["fileRecordName"].toString()));
        audioOutputDeviceKeys.append("fileRecordName");
    }
    if (jsonObject.contains("recordToFile"))
    {
        audioOutputDevice.setRecordToFile(jsonObject["recordToFile"].toInt() == 0 ? 0 : 1);
        audioOutputDeviceKeys.append("recordToFile");
    }
    if (jsonObject.contains("recordSilenceTime"))
    {
        audioOutputDevice.setRecordSilenceTime(jsonObject["recordSilenceTime"].toInt());
        audioOutputDeviceKeys.append("recordSilenceTime");
    }
    return true;
}

bool WebAPIRequestMapper::validateSpectrumSettings(SWGSDRangel::SWGGLSpectrum& spectrumSettings, QJsonObject& jsonObject, QStringList& spectrumSettingsKeys)
{
    extractKeys(jsonObject, spectrumSettingsKeys);
    spectrumSettings.init();
    spectrumSettings.fromJsonObject(jsonObject);
    // if (jsonObject.contains("fftSize"))
    // {
    //     spectrumSettings.setFftSize(jsonObject["fftSize"].toInt(1024));
    //     spectrumSettingsKeys.append("fftSize");
    // }
    // if (jsonObject.contains("fftOverlap"))
    // {
    //     spectrumSettings.setFftOverlap(jsonObject["fftOverlap"].toInt(0));
    //     spectrumSettingsKeys.append("fftOverlap");
    // }
    // if (jsonObject.contains("fftWindow"))
    // {
    //     spectrumSettings.setFftWindow(jsonObject["fftWindow"].toInt(0));
    //     spectrumSettingsKeys.append("fftWindow");
    // }
    // if (jsonObject.contains("refLevel"))
    // {
    //     spectrumSettings.setRefLevel(jsonObject["refLevel"].toDouble(0.0));
    //     spectrumSettingsKeys.append("refLevel");
    // }
    // if (jsonObject.contains("powerRange"))
    // {
    //     spectrumSettings.setPowerRange(jsonObject["powerRange"].toDouble(100.0));
    //     spectrumSettingsKeys.append("powerRange");
    // }
    // if (jsonObject.contains("fpsPeriodMs"))
    // {
    //     spectrumSettings.setFpsPeriodMs(jsonObject["fpsPeriodMs"].toInt(50));
    //     spectrumSettingsKeys.append("fpsPeriodMs");
    // }
    // if (jsonObject.contains("displayWaterfall"))
    // {
    //     spectrumSettings.setDisplayWaterfall(jsonObject["displayWaterfall"].toInt(0));
    //     spectrumSettingsKeys.append("displayWaterfall");
    // }
    // if (jsonObject.contains("invertedWaterfall"))
    // {
    //     spectrumSettings.setInvertedWaterfall(jsonObject["invertedWaterfall"].toInt(0));
    //     spectrumSettingsKeys.append("invertedWaterfall");
    // }
    // if (jsonObject.contains("displayHistogram"))
    // {
    //     spectrumSettings.setDisplayHistogram(jsonObject["displayHistogram"].toInt(0));
    //     spectrumSettingsKeys.append("displayHistogram");
    // }
    // if (jsonObject.contains("decay"))
    // {
    //     spectrumSettings.setDecay(jsonObject["decay"].toInt(1));
    //     spectrumSettingsKeys.append("decay");
    // }
    // if (jsonObject.contains("displayGrid"))
    // {
    //     spectrumSettings.setDisplayGrid(jsonObject["displayGrid"].toInt(0));
    //     spectrumSettingsKeys.append("displayGrid");
    // }
    // if (jsonObject.contains("displayGridIntensity"))
    // {
    //     spectrumSettings.setDisplayGridIntensity(jsonObject["displayGridIntensity"].toInt(30));
    //     spectrumSettingsKeys.append("displayGridIntensity");
    // }
    // if (jsonObject.contains("decayDivisor"))
    // {
    //     spectrumSettings.setDecayDivisor(jsonObject["decayDivisor"].toInt(1));
    //     spectrumSettingsKeys.append("decayDivisor");
    // }
    // if (jsonObject.contains("histogramStroke"))
    // {
    //     spectrumSettings.setHistogramStroke(jsonObject["histogramStroke"].toInt(10));
    //     spectrumSettingsKeys.append("histogramStroke");
    // }
    // if (jsonObject.contains("displayCurrent"))
    // {
    //     spectrumSettings.setDisplayCurrent(jsonObject["displayCurrent"].toInt(1));
    //     spectrumSettingsKeys.append("displayCurrent");
    // }
    // if (jsonObject.contains("displayTraceIntensity"))
    // {
    //     spectrumSettings.setDisplayTraceIntensity(jsonObject["displayTraceIntensity"].toInt(50));
    //     spectrumSettingsKeys.append("displayTraceIntensity");
    // }
    // if (jsonObject.contains("waterfallShare"))
    // {
    //     spectrumSettings.setWaterfallShare(jsonObject["waterfallShare"].toDouble(0.5));
    //     spectrumSettingsKeys.append("waterfallShare");
    // }
    // if (jsonObject.contains("averagingMode"))
    // {
    //     spectrumSettings.setAveragingMode(jsonObject["averagingMode"].toInt(0));
    //     spectrumSettingsKeys.append("averagingMode");
    // }
    // if (jsonObject.contains("averagingValue"))
    // {
    //     spectrumSettings.setAveragingValue(jsonObject["averagingValue"].toInt(0));
    //     spectrumSettingsKeys.append("averagingValue");
    // }
    // if (jsonObject.contains("linear"))
    // {
    //     spectrumSettings.setLinear(jsonObject["linear"].toInt(0));
    //     spectrumSettingsKeys.append("linear");
    // }
    // if (jsonObject.contains("ssb"))
    // {
    //     spectrumSettings.setSsb(jsonObject["ssb"].toInt(0));
    //     spectrumSettingsKeys.append("ssb");
    // }
    // if (jsonObject.contains("usb"))
    // {
    //     spectrumSettings.setUsb(jsonObject["usb"].toInt(1));
    //     spectrumSettingsKeys.append("usb");
    // }
    // if (jsonObject.contains("wsSpectrumAddress") && jsonObject["wsSpectrumAddress"].isString())
    // {
    //     spectrumSettings.setWsSpectrumAddress(new QString(jsonObject["wsSpectrumAddress"].toString()));
    //     spectrumSettingsKeys.append("wsSpectrumAddress");
    // }
    // if (jsonObject.contains("wsSpectrumPort"))
    // {
    //     spectrumSettings.setWsSpectrumPort(jsonObject["wsSpectrumPort"].toInt(8887));
    //     spectrumSettingsKeys.append("wsSpectrumPort");
    // }

    return true;
}

bool WebAPIRequestMapper::validateWorkspaceInfo(SWGSDRangel::SWGWorkspaceInfo& workspaceInfo, QJsonObject& jsonObject)
{
    if (jsonObject.contains("index"))
    {
        workspaceInfo.setIndex(jsonObject["index"].toInt());
        return true;
    }

    return false;
}

bool WebAPIRequestMapper::validateConfig(
    SWGSDRangel::SWGInstanceConfigResponse& config,
    QJsonObject& jsonObject,
    WebAPIAdapterInterface::ConfigKeys& configKeys)
{
    if (jsonObject.contains("preferences"))
    {
        SWGSDRangel::SWGPreferences *preferences = new SWGSDRangel::SWGPreferences();
        config.setPreferences(preferences);
        QJsonObject preferencesJson = jsonObject["preferences"].toObject();
        configKeys.m_preferencesKeys = preferencesJson.keys();
        preferences->fromJsonObject(preferencesJson);
    }

    if (jsonObject.contains("commands"))
    {
        QList<SWGSDRangel::SWGCommand *> *commands = new QList<SWGSDRangel::SWGCommand *>();
        config.setCommands(commands);
        QJsonArray commandsJson = jsonObject["commands"].toArray();
        QJsonArray::const_iterator commandsIt = commandsJson.begin();

        for (; commandsIt != commandsJson.end(); ++commandsIt)
        {
            QJsonObject commandJson = commandsIt->toObject();
            commands->append(new SWGSDRangel::SWGCommand());
            configKeys.m_commandKeys.append(WebAPIAdapterInterface::CommandKeys());
            configKeys.m_commandKeys.back().m_keys = commandJson.keys();
            commands->back()->fromJsonObject(commandJson);
        }
    }

    if (jsonObject.contains("presets"))
    {
        QList<SWGSDRangel::SWGPreset *> *presets = new QList<SWGSDRangel::SWGPreset *>();
        config.setPresets(presets);
        QJsonArray presetsJson = jsonObject["presets"].toArray();
        QJsonArray::const_iterator presetsIt = presetsJson.begin();

        for (; presetsIt != presetsJson.end(); ++presetsIt)
        {
            QJsonObject presetJson = presetsIt->toObject();
            SWGSDRangel::SWGPreset *preset = new SWGSDRangel::SWGPreset();
            presets->append(preset);
            configKeys.m_presetKeys.append(WebAPIAdapterInterface::PresetKeys());
            appendPresetKeys(preset, presetJson, configKeys.m_presetKeys.back());
        }
    }

    if (jsonObject.contains("featuresetpresets"))
    {
        QList<SWGSDRangel::SWGFeatureSetPreset *> *featureSetPresets = new QList<SWGSDRangel::SWGFeatureSetPreset *>();
        config.setFeaturesetpresets(featureSetPresets);
        QJsonArray presetsJson = jsonObject["featuresetpresets"].toArray();
        QJsonArray::const_iterator featureSetPresetsIt = presetsJson.begin();

        for (; featureSetPresetsIt != presetsJson.end(); ++featureSetPresetsIt)
        {
            QJsonObject presetJson = featureSetPresetsIt->toObject();
            SWGSDRangel::SWGFeatureSetPreset *featureSetPreset = new SWGSDRangel::SWGFeatureSetPreset();
            featureSetPresets->append(featureSetPreset);
            configKeys.m_featureSetPresetKeys.append(WebAPIAdapterInterface::FeatureSetPresetKeys());
            appendFeatureSetPresetKeys(featureSetPreset, presetJson, configKeys.m_featureSetPresetKeys.back());
        }
    }

    if (jsonObject.contains("workingPreset"))
    {
        SWGSDRangel::SWGPreset *preset = new SWGSDRangel::SWGPreset();
        config.setWorkingPreset(preset);
        QJsonObject presetJson = jsonObject["workingPreset"].toObject();
        appendPresetKeys(preset, presetJson, configKeys.m_workingPresetKeys);
    }

    if (jsonObject.contains("workingFeatureSetPreset"))
    {
        SWGSDRangel::SWGFeatureSetPreset *preset = new SWGSDRangel::SWGFeatureSetPreset();
        config.setWorkingFeatureSetPreset(preset);
        QJsonObject presetJson = jsonObject["workingFeatureSetPreset"].toObject();
        appendFeatureSetPresetKeys(preset, presetJson, configKeys.m_workingFeatureSetPresetKeys);
    }

    return true;
}

bool WebAPIRequestMapper::appendFeatureSetPresetKeys(
    SWGSDRangel::SWGFeatureSetPreset *preset,
    const QJsonObject& presetJson,
    WebAPIAdapterInterface::FeatureSetPresetKeys& featureSetPresetKeys
)
{
    if (presetJson.contains("description"))
    {
        preset->setDescription(new QString(presetJson["description"].toString()));
        featureSetPresetKeys.m_keys.append("description");
    }
    if (presetJson.contains("group"))
    {
        preset->setGroup(new QString(presetJson["group"].toString()));
        featureSetPresetKeys.m_keys.append("group");
    }
    if (presetJson.contains("featureConfigs"))
    {
        QJsonArray featuresJson = presetJson["featureConfigs"].toArray();
        QJsonArray::const_iterator featuresIt = featuresJson.begin();
        QList<SWGSDRangel::SWGFeatureConfig*> *features = new QList<SWGSDRangel::SWGFeatureConfig*>();
        preset->setFeatureConfigs(features);

        for (; featuresIt != featuresJson.end(); ++featuresIt)
        {
            QJsonObject featureJson = featuresIt->toObject();
            SWGSDRangel::SWGFeatureConfig *featureConfig = new SWGSDRangel::SWGFeatureConfig();
            featureSetPresetKeys.m_featureKeys.append(WebAPIAdapterInterface::FeatureKeys());

            if (appendPresetFeatureKeys(featureConfig, featureJson, featureSetPresetKeys.m_featureKeys.back()))
            {
                features->append(featureConfig);
            }
            else
            {
                delete featureConfig;
                featureSetPresetKeys.m_featureKeys.takeLast(); // remove channel keys
            }
        }
    }

    return true;
}

bool WebAPIRequestMapper::appendPresetKeys(
    SWGSDRangel::SWGPreset *preset,
    const QJsonObject& presetJson,
    WebAPIAdapterInterface::PresetKeys& presetKeys
)
{
    if (presetJson.contains("centerFrequency"))
    {
        preset->setCenterFrequency(presetJson["centerFrequency"].toInt());
        presetKeys.m_keys.append("centerFrequency");
    }
    if (presetJson.contains("dcOffsetCorrection"))
    {
        preset->setDcOffsetCorrection(presetJson["dcOffsetCorrection"].toInt());
        presetKeys.m_keys.append("dcOffsetCorrection");
    }
    if (presetJson.contains("iqImbalanceCorrection"))
    {
        preset->setIqImbalanceCorrection(presetJson["iqImbalanceCorrection"].toInt());
        presetKeys.m_keys.append("iqImbalanceCorrection");
    }
    if (presetJson.contains("iqImbalanceCorrection"))
    {
        preset->setPresetType(presetJson["presetType"].toInt());
        presetKeys.m_keys.append("presetType");
    }
    if (presetJson.contains("description"))
    {
        preset->setDescription(new QString(presetJson["description"].toString()));
        presetKeys.m_keys.append("description");
    }
    if (presetJson.contains("group"))
    {
        preset->setGroup(new QString(presetJson["group"].toString()));
        presetKeys.m_keys.append("group");
    }
    if (presetJson.contains("layout"))
    {
        preset->setLayout(new QString(presetJson["layout"].toString()));
        presetKeys.m_keys.append("layout");
    }

    if (presetJson.contains("spectrumConfig"))
    {
        QJsonObject spectrumJson = presetJson["spectrumConfig"].toObject();
        presetKeys.m_spectrumKeys = spectrumJson.keys();
        SWGSDRangel::SWGGLSpectrum *spectrum = new SWGSDRangel::SWGGLSpectrum();
        preset->setSpectrumConfig(spectrum);
        spectrum->fromJsonObject(spectrumJson);
    }

    if (presetJson.contains("channelConfigs"))
    {
        QJsonArray channelsJson = presetJson["channelConfigs"].toArray();
        QJsonArray::const_iterator channelsIt = channelsJson.begin();
        QList<SWGSDRangel::SWGChannelConfig*> *channels = new QList<SWGSDRangel::SWGChannelConfig*>();
        preset->setChannelConfigs(channels);

        for (; channelsIt != channelsJson.end(); ++channelsIt)
        {
            QJsonObject channelJson = channelsIt->toObject();
            SWGSDRangel::SWGChannelConfig *channelConfig = new SWGSDRangel::SWGChannelConfig();
            presetKeys.m_channelsKeys.append(WebAPIAdapterInterface::ChannelKeys());

            if (appendPresetChannelKeys(channelConfig, channelJson, presetKeys.m_channelsKeys.back()))
            {
                channels->append(channelConfig);
            }
            else
            {
                delete channelConfig;
                presetKeys.m_channelsKeys.takeLast(); // remove channel keys
            }
        }
    }

    if (presetJson.contains("deviceConfigs"))
    {
        QJsonArray devicesJson = presetJson["deviceConfigs"].toArray();
        QJsonArray::const_iterator devicesIt = devicesJson.begin();
        QList<SWGSDRangel::SWGDeviceConfig*> *devices = new QList<SWGSDRangel::SWGDeviceConfig*>();
        preset->setDeviceConfigs(devices);

        for (; devicesIt != devicesJson.end(); ++devicesIt)
        {
            QJsonObject deviceJson = devicesIt->toObject();
            SWGSDRangel::SWGDeviceConfig *deviceConfig = new SWGSDRangel::SWGDeviceConfig();
            presetKeys.m_devicesKeys.append(WebAPIAdapterInterface::DeviceKeys());

            if (appendPresetDeviceKeys(deviceConfig, deviceJson, presetKeys.m_devicesKeys.back()))
            {
                devices->append(deviceConfig);
            }
            else
            {
                delete deviceConfig;
                presetKeys.m_devicesKeys.takeLast(); // remove device keys
            }
        }
    }

    return true;
}

bool WebAPIRequestMapper::appendPresetFeatureKeys(
        SWGSDRangel::SWGFeatureConfig *feature,
        const QJsonObject& featureSettingsJson,
        WebAPIAdapterInterface::FeatureKeys& featureKeys
)
{
    if (featureSettingsJson.contains("featureIdURI"))
    {
        QString *featureURI = new QString(featureSettingsJson["featureIdURI"].toString());
        feature->setFeatureIdUri(featureURI);
        featureKeys.m_keys.append("featureIdURI");

        if (featureSettingsJson.contains("config") && WebAPIUtils::m_featureURIToSettingsKey.contains(*featureURI))
        {
            SWGSDRangel::SWGFeatureSettings *featureSettings = new SWGSDRangel::SWGFeatureSettings();
            feature->setConfig(featureSettings);
            return getFeatureSettings(
                WebAPIUtils::m_channelURIToSettingsKey[*featureURI],
                featureSettings,
                featureSettingsJson["config"].toObject(),
                featureKeys.m_featureKeys
            );
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

bool WebAPIRequestMapper::appendPresetChannelKeys(
        SWGSDRangel::SWGChannelConfig *channel,
        const QJsonObject& channelSettingsJson,
        WebAPIAdapterInterface::ChannelKeys& channelKeys
)
{
    if (channelSettingsJson.contains("channelIdURI"))
    {
        QString *channelURI = new QString(channelSettingsJson["channelIdURI"].toString());
        channel->setChannelIdUri(channelURI);
        channelKeys.m_keys.append("channelIdURI");

        if (channelSettingsJson.contains("config") && WebAPIUtils::m_channelURIToSettingsKey.contains(*channelURI))
        {
            SWGSDRangel::SWGChannelSettings *channelSettings = new SWGSDRangel::SWGChannelSettings();
            channel->setConfig(channelSettings);
            return getChannelSettings(WebAPIUtils::m_channelURIToSettingsKey[*channelURI], channelSettings, channelSettingsJson["config"].toObject(), channelKeys.m_channelKeys);
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

bool WebAPIRequestMapper::getChannelSettings(
    const QString& channelSettingsKey,
    SWGSDRangel::SWGChannelSettings *channelSettings,
    const QJsonObject& channelSettingsJson,
    QStringList& channelSettingsKeys
)
{
    QStringList channelKeys = channelSettingsJson.keys();

    if (channelKeys.contains(channelSettingsKey) && channelSettingsJson[channelSettingsKey].isObject())
    {
        QJsonObject settingsJsonObject = channelSettingsJson[channelSettingsKey].toObject();
        extractKeys(settingsJsonObject, channelSettingsKeys);
        qDebug() << "WebAPIRequestMapper::getChannelSettings:"
            << " channelSettingsKey: " << channelSettingsKey
            << " channelSettingsKeys: " << channelSettingsKeys;

        if (channelSettingsKey == "ADSBDemodSettings")
        {
            channelSettings->setAdsbDemodSettings(new SWGSDRangel::SWGADSBDemodSettings());
            channelSettings->getAdsbDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "AISDemodSettings")
        {
            channelSettings->setAisDemodSettings(new SWGSDRangel::SWGAISDemodSettings());
            channelSettings->getAisDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "AISModSettings")
        {
            channelSettings->setAisModSettings(new SWGSDRangel::SWGAISModSettings());
            channelSettings->getAisModSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "AMDemodSettings")
        {
            channelSettings->setAmDemodSettings(new SWGSDRangel::SWGAMDemodSettings());
            channelSettings->getAmDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "AMModSettings")
        {
            channelSettings->setAmModSettings(new SWGSDRangel::SWGAMModSettings());
            channelSettings->getAmModSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "APTDemodSettings")
        {
            channelSettings->setAptDemodSettings(new SWGSDRangel::SWGAPTDemodSettings());
            channelSettings->getAptDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "ATVDemodSettings")
        {
            channelSettings->setAtvDemodSettings(new SWGSDRangel::SWGATVDemodSettings());
            channelSettings->getAtvDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "ATVModSettings")
        {
            channelSettings->setAtvModSettings(new SWGSDRangel::SWGATVModSettings());
            channelSettings->getAtvModSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "BeamSteeringCWModSettings")
        {
            channelSettings->setBeamSteeringCwModSettings(new SWGSDRangel::SWGBeamSteeringCWModSettings());
            channelSettings->getBeamSteeringCwModSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "BFMDemodSettings")
        {
            channelSettings->setBfmDemodSettings(new SWGSDRangel::SWGBFMDemodSettings());
            channelSettings->getBfmDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "ChannelAnalyzerSettings")
        {
            channelSettings->setChannelAnalyzerSettings(new SWGSDRangel::SWGChannelAnalyzerSettings());
            channelSettings->getChannelAnalyzerSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "ChirpChatDemodSettings")
        {
            channelSettings->setChirpChatDemodSettings(new SWGSDRangel::SWGChirpChatDemodSettings());
            channelSettings->getChirpChatDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "ChirpChatModSettings")
        {
            channelSettings->setChirpChatModSettings(new SWGSDRangel::SWGChirpChatModSettings());
            channelSettings->getChirpChatModSettings()->init(); // contains a list of strings
            channelSettings->getChirpChatModSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "DATVDemodSettings")
        {
            channelSettings->setDatvDemodSettings(new SWGSDRangel::SWGDATVDemodSettings());
            channelSettings->getDatvDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "DATVModSettings")
        {
            channelSettings->setDatvModSettings(new SWGSDRangel::SWGDATVModSettings());
            channelSettings->getDatvModSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "DABDemodSettings")
        {
            channelSettings->setDabDemodSettings(new SWGSDRangel::SWGDABDemodSettings());
            channelSettings->getDabDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "DOA2Settings")
        {
            channelSettings->setDoa2Settings(new SWGSDRangel::SWGDOA2Settings());
            channelSettings->getDoa2Settings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "DSCDemodSettings")
        {
            channelSettings->setDscDemodSettings(new SWGSDRangel::SWGDSCDemodSettings());
            channelSettings->getDscDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "DSDDemodSettings")
        {
            channelSettings->setDsdDemodSettings(new SWGSDRangel::SWGDSDDemodSettings());
            channelSettings->getDsdDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "FileSinkSettings")
        {
            channelSettings->setFileSinkSettings(new SWGSDRangel::SWGFileSinkSettings());
            channelSettings->getFileSinkSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "FileSourceSettings")
        {
            channelSettings->setFileSourceSettings(new SWGSDRangel::SWGFileSourceSettings());
            channelSettings->getFileSourceSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "FreeDVDemodSettings")
        {
            channelSettings->setFreeDvDemodSettings(new SWGSDRangel::SWGFreeDVDemodSettings());
            channelSettings->getFreeDvDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "FreeDVModSettings")
        {
            channelSettings->setFreeDvModSettings(new SWGSDRangel::SWGFreeDVModSettings());
            channelSettings->getFreeDvModSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "FreqTrackerSettings")
        {
            channelSettings->setFreqTrackerSettings(new SWGSDRangel::SWGFreqTrackerSettings());
            channelSettings->getFreqTrackerSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "FT8DemodSettings")
        {
            channelSettings->setFt8DemodSettings(new SWGSDRangel::SWGFT8DemodSettings());
            channelSettings->getFt8DemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "HeatMapSettings")
        {
            channelSettings->setHeatMapSettings(new SWGSDRangel::SWGHeatMapSettings());
            channelSettings->getHeatMapSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "IEEE_802_15_4_ModSettings")
        {
            channelSettings->setIeee802154ModSettings(new SWGSDRangel::SWGIEEE_802_15_4_ModSettings());
            channelSettings->getIeee802154ModSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "ILSDemodSettings")
        {
            channelSettings->setIlsDemodSettings(new SWGSDRangel::SWGILSDemodSettings());
            channelSettings->getIlsDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "InterferometerSettings")
        {
            channelSettings->setInterferometerSettings(new SWGSDRangel::SWGInterferometerSettings());
            channelSettings->getInterferometerSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "NavtexDemodSettings")
        {
            channelSettings->setNavtexDemodSettings(new SWGSDRangel::SWGNavtexDemodSettings());
            channelSettings->getNavtexDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "M17DemodSettings")
        {
            channelSettings->setM17DemodSettings(new SWGSDRangel::SWGM17DemodSettings());
            channelSettings->getM17DemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "M17ModSettings")
        {
            channelSettings->setM17ModSettings(new SWGSDRangel::SWGM17ModSettings());
            channelSettings->getM17ModSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "NFMDemodSettings")
        {
            channelSettings->setNfmDemodSettings(new SWGSDRangel::SWGNFMDemodSettings());
            channelSettings->getNfmDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "NFMModSettings")
        {
            channelSettings->setNfmModSettings(new SWGSDRangel::SWGNFMModSettings());
            channelSettings->getNfmModSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "NoiseFigureSettings")
        {
            channelSettings->setNoiseFigureSettings(new SWGSDRangel::SWGNoiseFigureSettings());
            channelSettings->getNoiseFigureSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "LocalSinkSettings")
        {
            channelSettings->setLocalSinkSettings(new SWGSDRangel::SWGLocalSinkSettings());
            channelSettings->getLocalSinkSettings()->init(); // contains a QList
            channelSettings->getLocalSinkSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "LocalSourceSettings")
        {
            channelSettings->setLocalSourceSettings(new SWGSDRangel::SWGLocalSourceSettings());
            channelSettings->getLocalSourceSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "PacketDemodSettings")
        {
            channelSettings->setPacketDemodSettings(new SWGSDRangel::SWGPacketDemodSettings());
            channelSettings->getPacketDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "PacketModSettings")
        {
            channelSettings->setPacketModSettings(new SWGSDRangel::SWGPacketModSettings());
            channelSettings->getPacketModSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "PagerDemodSettings")
        {
            channelSettings->setPagerDemodSettings(new SWGSDRangel::SWGPagerDemodSettings());
            channelSettings->getPagerDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "RadioAstronomySettings")
        {
            channelSettings->setRadioAstronomySettings(new SWGSDRangel::SWGRadioAstronomySettings());
            channelSettings->getRadioAstronomySettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "RadioClockSettings")
        {
            channelSettings->setRadioClockSettings(new SWGSDRangel::SWGRadioClockSettings());
            channelSettings->getRadioClockSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "RadiosondeDemodSettings")
        {
            channelSettings->setRadiosondeDemodSettings(new SWGSDRangel::SWGRadiosondeDemodSettings());
            channelSettings->getRadiosondeDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "RemoteSinkSettings")
        {
            channelSettings->setRemoteSinkSettings(new SWGSDRangel::SWGRemoteSinkSettings());
            channelSettings->getRemoteSinkSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "RemoteSourceSettings")
        {
            channelSettings->setRemoteSourceSettings(new SWGSDRangel::SWGRemoteSourceSettings());
            channelSettings->getRemoteSourceSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "RemoteTCPSinkSettings")
        {
            channelSettings->setRemoteTcpSinkSettings(new SWGSDRangel::SWGRemoteTCPSinkSettings());
            channelSettings->getRemoteTcpSinkSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "RTTYDemodSettings")
        {
            channelSettings->setRttyDemodSettings(new SWGSDRangel::SWGRTTYDemodSettings());
            channelSettings->getRttyDemodSettings()->fromJsonObject(settingsJsonObject);
            }
        else if (channelSettingsKey == "RTTYModSettings")
        {
            channelSettings->setRttyModSettings(new SWGSDRangel::SWGRTTYModSettings());
            channelSettings->getRttyModSettings()->fromJsonObject(settingsJsonObject);
            }
        else if (channelSettingsKey == "SigMFFileSinkSettings")
        {
            channelSettings->setSigMfFileSinkSettings(new SWGSDRangel::SWGSigMFFileSinkSettings());
            channelSettings->getSigMfFileSinkSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "SSBDemodSettings")
        {
            channelSettings->setSsbDemodSettings(new SWGSDRangel::SWGSSBDemodSettings());
            channelSettings->getSsbDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "SSBModSettings")
        {
            channelSettings->setSsbModSettings(new SWGSDRangel::SWGSSBModSettings());
            channelSettings->getSsbModSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "UDPSourceSettings")
        {
            channelSettings->setUdpSourceSettings(new SWGSDRangel::SWGUDPSourceSettings());
            channelSettings->getUdpSourceSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "UDPSinkSettings")
        {
            channelSettings->setUdpSinkSettings(new SWGSDRangel::SWGUDPSinkSettings());
            channelSettings->getUdpSinkSettings()->fromJsonObject(settingsJsonObject);
        }
        // else if (channelSettingsKey == "VORDemodMCSettings")
        // {
        //     channelSettings->setVorDemodMcSettings(new SWGSDRangel::SWGVORDemodMCSettings());
        //     channelSettings->getVorDemodMcSettings()->fromJsonObject(settingsJsonObject);
        // }
        else if (channelSettingsKey == "VORDemodSettings")
        {
            channelSettings->setVorDemodSettings(new SWGSDRangel::SWGVORDemodSettings());
            channelSettings->getVorDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "WFMDemodSettings")
        {
            channelSettings->setWfmDemodSettings(new SWGSDRangel::SWGWFMDemodSettings());
            channelSettings->getWfmDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "WFMModSettings")
        {
            channelSettings->setWfmModSettings(new SWGSDRangel::SWGWFMModSettings());
            channelSettings->getWfmModSettings()->fromJsonObject(settingsJsonObject);
        }
        else
        {
            return false;
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool WebAPIRequestMapper::getChannelActions(
    const QString& channelActionsKey,
    SWGSDRangel::SWGChannelActions *channelActions,
    const QJsonObject& channelActionsJson,
    QStringList& channelActionsKeys
)
{
    QStringList channelKeys = channelActionsJson.keys();

    if (channelKeys.contains(channelActionsKey) && channelActionsJson[channelActionsKey].isObject())
    {
        QJsonObject actionsJsonObject = channelActionsJson[channelActionsKey].toObject();
        channelActionsKeys = actionsJsonObject.keys();

        if (channelActionsKey == "AISModActions")
        {
            channelActions->setAisModActions(new SWGSDRangel::SWGAISModActions());
            channelActions->getAisModActions()->fromJsonObject(actionsJsonObject);
        }
        else if (channelActionsKey == "APTDemodActions")
        {
            channelActions->setAptDemodActions(new SWGSDRangel::SWGAPTDemodActions());
            channelActions->getAptDemodActions()->fromJsonObject(actionsJsonObject);
        }
        else if (channelActionsKey == "FileSinkActions")
        {
            channelActions->setFileSinkActions(new SWGSDRangel::SWGFileSinkActions());
            channelActions->getFileSinkActions()->fromJsonObject(actionsJsonObject);
        }
        else if (channelActionsKey == "FileSourceActions")
        {
            channelActions->setFileSourceActions(new SWGSDRangel::SWGFileSourceActions());
            channelActions->getFileSourceActions()->fromJsonObject(actionsJsonObject);
        }
        else if (channelActionsKey == "IEEE_802_15_4_ModActions")
        {
            channelActions->setIeee802154ModActions(new SWGSDRangel::SWGIEEE_802_15_4_ModActions());
            channelActions->getIeee802154ModActions()->fromJsonObject(actionsJsonObject);
        }
        else if (channelActionsKey == "RadioAstronomyActions")
        {
            channelActions->setRadioAstronomyActions(new SWGSDRangel::SWGRadioAstronomyActions());
            channelActions->getRadioAstronomyActions()->fromJsonObject(actionsJsonObject);
        }
        else if (channelActionsKey == "PacketModActions")
        {
            channelActions->setPacketModActions(new SWGSDRangel::SWGPacketModActions());
            channelActions->getPacketModActions()->fromJsonObject(actionsJsonObject);
        }
        else if (channelActionsKey == "RTTYModActions")
        {
            channelActions->setRttyModActions(new SWGSDRangel::SWGRTTYModActions());
            channelActions->getRttyModActions()->fromJsonObject(actionsJsonObject);
        }
        else if (channelActionsKey == "SigMFFileSinkActions")
        {
            channelActions->setSigMfFileSinkActions(new SWGSDRangel::SWGSigMFFileSinkActions());
            channelActions->getSigMfFileSinkActions()->fromJsonObject(actionsJsonObject);
        }
        else
        {
            return false;
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool WebAPIRequestMapper::appendPresetDeviceKeys(
        SWGSDRangel::SWGDeviceConfig *device,
        const QJsonObject& deviceSettngsJson,
        WebAPIAdapterInterface::DeviceKeys& devicelKeys
)
{
    if (deviceSettngsJson.contains("deviceId"))
    {
        QString *deviceId = new QString(deviceSettngsJson["deviceId"].toString());
        device->setDeviceId(deviceId);
        devicelKeys.m_keys.append("deviceId");

        if (deviceSettngsJson.contains("deviceSerial"))
        {
            device->setDeviceSerial(new QString(deviceSettngsJson["deviceSerial"].toString()));
            devicelKeys.m_keys.append("deviceSerial");
        }

        if (deviceSettngsJson.contains("deviceSequence"))
        {
            device->setDeviceSequence(deviceSettngsJson["deviceSequence"].toInt());
            devicelKeys.m_keys.append("deviceSequence");
        }

        if (deviceSettngsJson.contains("config") && WebAPIUtils::m_deviceIdToSettingsKey.contains(*deviceId))
        {
            SWGSDRangel::SWGDeviceSettings *deviceSettings = new SWGSDRangel::SWGDeviceSettings();
            device->setConfig(deviceSettings);
            return getDeviceSettings(
                WebAPIUtils::m_deviceIdToSettingsKey[*deviceId],
                deviceSettings,
                deviceSettngsJson["config"].toObject(),
                devicelKeys.m_deviceKeys
            );
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

bool WebAPIRequestMapper::getDeviceSettings(
        const QString& deviceSettingsKey,
        SWGSDRangel::SWGDeviceSettings *deviceSettings,
        const QJsonObject& deviceSettingsJson,
        QStringList& deviceSettingsKeys
)
{
    QStringList deviceKeys = deviceSettingsJson.keys();

    if (deviceKeys.contains(deviceSettingsKey) && deviceSettingsJson[deviceSettingsKey].isObject())
    {
        QJsonObject settingsJsonObject = deviceSettingsJson[deviceSettingsKey].toObject();
        extractKeys(settingsJsonObject, deviceSettingsKeys);
        qDebug() << "WebAPIRequestMapper::getDeviceSettings: deviceSettingsKeys: " << deviceSettingsKeys;

        if (deviceSettingsKey == "aaroniaRTSASettings")
        {
            deviceSettings->setAaroniaRtsaSettings(new SWGSDRangel::SWGAaroniaRTSASettings());
            deviceSettings->getAaroniaRtsaSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "aaroniaRTSAOutputSettings")
        {
            deviceSettings->setAaroniaRtsaOutputSettings(new SWGSDRangel::SWGAaroniaRTSAOutputSettings());
            deviceSettings->getAaroniaRtsaOutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "airspySettings")
        {
            deviceSettings->setAirspySettings(new SWGSDRangel::SWGAirspySettings());
            deviceSettings->getAirspySettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "airspyHFSettings")
        {
            deviceSettings->setAirspyHfSettings(new SWGSDRangel::SWGAirspyHFSettings());
            deviceSettings->getAirspyHfSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "audioCATSISOSettings")
        {
            deviceSettings->setAudioCatsisoSettings(new SWGSDRangel::SWGAudioCATSISOSettings());
            deviceSettings->getAudioCatsisoSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "audioInputSettings")
        {
            deviceSettings->setAudioInputSettings(new SWGSDRangel::SWGAudioInputSettings());
            deviceSettings->getAudioInputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "audioOutputSettings")
        {
            deviceSettings->setAudioOutputSettings(new SWGSDRangel::SWGAudioOutputSettings());
            deviceSettings->getAudioOutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "bladeRF1InputSettings")
        {
            deviceSettings->setBladeRf1InputSettings(new SWGSDRangel::SWGBladeRF1InputSettings());
            deviceSettings->getBladeRf1InputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "bladeRF1OutputSettings")
        {
            deviceSettings->setBladeRf1OutputSettings(new SWGSDRangel::SWGBladeRF1OutputSettings());
            deviceSettings->getBladeRf1OutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "bladeRF2InputSettings")
        {
            deviceSettings->setBladeRf2InputSettings(new SWGSDRangel::SWGBladeRF2InputSettings());
            deviceSettings->getBladeRf2InputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "bladeRF2OutputSettings")
        {
            deviceSettings->setBladeRf2OutputSettings(new SWGSDRangel::SWGBladeRF2OutputSettings());
            deviceSettings->getBladeRf2OutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "bladeRF2MIMOSettings")
        {
            deviceSettings->setBladeRf2MimoSettings(new SWGSDRangel::SWGBladeRF2MIMOSettings());
            deviceSettings->getBladeRf2MimoSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "fcdProSettings")
        {
            deviceSettings->setFcdProSettings(new SWGSDRangel::SWGFCDProSettings());
            deviceSettings->getFcdProSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "fcdProPlusSettings")
        {
            deviceSettings->setFcdProPlusSettings(new SWGSDRangel::SWGFCDProPlusSettings());
            deviceSettings->getFcdProPlusSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "fileInputSettings")
        {
            deviceSettings->setFileInputSettings(new SWGSDRangel::SWGFileInputSettings());
            deviceSettings->getFileInputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "fileOutputSettings")
        {
            deviceSettings->setFileOutputSettings(new SWGSDRangel::SWGFileOutputSettings());
            deviceSettings->getFileOutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "hackRFInputSettings")
        {
            deviceSettings->setHackRfInputSettings(new SWGSDRangel::SWGHackRFInputSettings());
            deviceSettings->getHackRfInputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "hackRFOutputSettings")
        {
            deviceSettings->setHackRfOutputSettings(new SWGSDRangel::SWGHackRFOutputSettings());
            deviceSettings->getHackRfOutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "kiwiSDRSettings")
        {
            deviceSettings->setKiwiSdrSettings(new SWGSDRangel::SWGKiwiSDRSettings());
            deviceSettings->getKiwiSdrSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "limeSdrInputSettings")
        {
            deviceSettings->setLimeSdrInputSettings(new SWGSDRangel::SWGLimeSdrInputSettings());
            deviceSettings->getLimeSdrInputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "limeSdrOutputSettings")
        {
            deviceSettings->setLimeSdrOutputSettings(new SWGSDRangel::SWGLimeSdrOutputSettings());
            deviceSettings->getLimeSdrOutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "limeSdrMIMOSettings")
        {
            deviceSettings->setLimeSdrMimoSettings(new SWGSDRangel::SWGLimeSdrMIMOSettings());
            deviceSettings->getLimeSdrMimoSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "metisMISOSettings")
        {
            deviceSettings->setMetisMisoSettings(new SWGSDRangel::SWGMetisMISOSettings());
            deviceSettings->getMetisMisoSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "perseusSettings")
        {
            deviceSettings->setPerseusSettings(new SWGSDRangel::SWGPerseusSettings());
            deviceSettings->getPerseusSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "plutoSdrInputSettings")
        {
            deviceSettings->setPlutoSdrInputSettings(new SWGSDRangel::SWGPlutoSdrInputSettings());
            deviceSettings->getPlutoSdrInputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "plutoSdrOutputSettings")
        {
            deviceSettings->setPlutoSdrOutputSettings(new SWGSDRangel::SWGPlutoSdrOutputSettings());
            deviceSettings->getPlutoSdrOutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "rtlSdrSettings")
        {
            deviceSettings->setRtlSdrSettings(new SWGSDRangel::SWGRtlSdrSettings());
            deviceSettings->getRtlSdrSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "sdrPlaySettings")
        {
            deviceSettings->setSdrPlaySettings(new SWGSDRangel::SWGSDRPlaySettings());
            deviceSettings->getSdrPlaySettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "sdrPlayV3Settings")
        {
            deviceSettings->setSdrPlayV3Settings(new SWGSDRangel::SWGSDRPlayV3Settings());
            deviceSettings->getSdrPlayV3Settings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "sigMFFileInputSettings")
        {
            deviceSettings->setSigMfFileInputSettings(new SWGSDRangel::SWGSigMFFileInputSettings());
            deviceSettings->getSigMfFileInputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "soapySDRInputSettings")
        {
            deviceSettings->setSoapySdrInputSettings(new SWGSDRangel::SWGSoapySDRInputSettings());
            deviceSettings->getSoapySdrInputSettings()->init(); // contains complex objects
            deviceSettings->getSoapySdrInputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "soapySDROutputSettings")
        {
            deviceSettings->setSoapySdrOutputSettings(new SWGSDRangel::SWGSoapySDROutputSettings());
            deviceSettings->getSoapySdrOutputSettings()->init(); // contains complex objects
            deviceSettings->getSoapySdrOutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "testSourceSettings")
        {
            deviceSettings->setTestSourceSettings(new SWGSDRangel::SWGTestSourceSettings());
            deviceSettings->getTestSourceSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "testMISettings")
        {
            deviceSettings->setTestMiSettings(new SWGSDRangel::SWGTestMISettings());
            deviceSettings->getTestMiSettings()->init();
            deviceSettings->getTestMiSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "testMOSyncSettings")
        {
            deviceSettings->setTestMoSyncSettings(new SWGSDRangel::SWGTestMOSyncSettings());
            deviceSettings->getTestMoSyncSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "usrpInputSettings")
        {
            deviceSettings->setUsrpInputSettings(new SWGSDRangel::SWGUSRPInputSettings());
            deviceSettings->getUsrpInputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "usrpOutputSettings")
        {
            deviceSettings->setUsrpOutputSettings(new SWGSDRangel::SWGUSRPOutputSettings());
            deviceSettings->getUsrpOutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "xtrxInputSettings")
        {
            deviceSettings->setXtrxInputSettings(new SWGSDRangel::SWGXtrxInputSettings());
            deviceSettings->getXtrxInputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "xtrxOutputSettings")
        {
            deviceSettings->setXtrxOutputSettings(new SWGSDRangel::SWGXtrxOutputSettings());
            deviceSettings->getXtrxOutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "XtrxMIMOSettings")
        {
            deviceSettings->setXtrxMimoSettings(new SWGSDRangel::SWGXtrxMIMOSettings());
            deviceSettings->getXtrxMimoSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "remoteInputSettings")
        {
            deviceSettings->setRemoteInputSettings(new SWGSDRangel::SWGRemoteInputSettings());
            deviceSettings->getRemoteInputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "remoteTCPInputSettings")
        {
            deviceSettings->setRemoteTcpInputSettings(new SWGSDRangel::SWGRemoteTCPInputSettings());
            deviceSettings->getRemoteTcpInputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "localInputSettings")
        {
            deviceSettings->setLocalInputSettings(new SWGSDRangel::SWGLocalInputSettings());
            deviceSettings->getLocalInputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "remoteOutputSettings")
        {
            deviceSettings->setRemoteOutputSettings(new SWGSDRangel::SWGRemoteOutputSettings());
            deviceSettings->getRemoteOutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "localOutputSettings")
        {
            deviceSettings->setLocalOutputSettings(new SWGSDRangel::SWGLocalOutputSettings());
            deviceSettings->getLocalOutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else
        {
            return false;
        }

        return true;
    }
    else
    {
        return false;
    }

}

bool WebAPIRequestMapper::getDeviceActions(
        const QString& deviceActionsKey,
        SWGSDRangel::SWGDeviceActions *deviceActions,
        const QJsonObject& deviceActionsJson,
        QStringList& deviceActionsKeys
)
{
    QStringList deviceKeys = deviceActionsJson.keys();

    if (deviceKeys.contains(deviceActionsKey) && deviceActionsJson[deviceActionsKey].isObject())
    {
        QJsonObject actionsJsonObject = deviceActionsJson[deviceActionsKey].toObject();
        deviceActionsKeys = actionsJsonObject.keys();

        if (deviceActionsKey == "SigMFFileInputActions")
        {
            deviceActions->setSigMfFileInputActions(new SWGSDRangel::SWGSigMFFileInputActions());
            deviceActions->getSigMfFileInputActions()->fromJsonObject(actionsJsonObject);
        }
        else
        {
            return false;
        }

        return true;
    }
    else
    {
        return false;
    }

}

bool WebAPIRequestMapper::getFeatureSettings(
    const QString& featureSettingsKey,
    SWGSDRangel::SWGFeatureSettings *featureSettings,
    const QJsonObject& featureSettingsJson,
    QStringList& featureSettingsKeys
)
{
    QStringList featureKeys = featureSettingsJson.keys();

    if (featureKeys.contains(featureSettingsKey) && featureSettingsJson[featureSettingsKey].isObject())
    {
        QJsonObject settingsJsonObject = featureSettingsJson[featureSettingsKey].toObject();
        extractKeys(settingsJsonObject, featureSettingsKeys);
        qDebug() << "WebAPIRequestMapper::getFeatureSettings: featureSettingsKeys: " << featureSettingsKeys;

        if (featureSettingsKey == "AFCSettings")
        {
            featureSettings->setAfcSettings(new SWGSDRangel::SWGAFCSettings());
            featureSettings->getAfcSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "AISSSettings")
        {
            featureSettings->setAisSettings(new SWGSDRangel::SWGAISSettings());
            featureSettings->getAisSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "AntennaToolsSettings")
        {
            featureSettings->setAntennaToolsSettings(new SWGSDRangel::SWGAntennaToolsSettings());
            featureSettings->getAntennaToolsSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "APRSSettings")
        {
            featureSettings->setAprsSettings(new SWGSDRangel::SWGAPRSSettings());
            featureSettings->getAprsSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "DemodAnalyzerSettings")
        {
            featureSettings->setDemodAnalyzerSettings(new SWGSDRangel::SWGDemodAnalyzerSettings());
            featureSettings->getDemodAnalyzerSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "JogdialControllerSettings")
        {
            featureSettings->setJogdialControllerSettings(new SWGSDRangel::SWGJogdialControllerSettings());
            featureSettings->getJogdialControllerSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "GS232ControllerSettings")
        {
            featureSettings->setGs232ControllerSettings(new SWGSDRangel::SWGGS232ControllerSettings());
            featureSettings->getGs232ControllerSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "LimeRFESettings")
        {
            featureSettings->setLimeRfeSettings(new SWGSDRangel::SWGLimeRFESettings());
            featureSettings->getLimeRfeSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "MapSettings")
        {
            featureSettings->setMapSettings(new SWGSDRangel::SWGMapSettings());
            featureSettings->getMapSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "PERTesterSettings")
        {
            featureSettings->setPerTesterSettings(new SWGSDRangel::SWGPERTesterSettings());
            featureSettings->getPerTesterSettings()->init();
            featureSettings->getPerTesterSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "SatelliteTrackerSettings")
        {
            featureSettings->setSatelliteTrackerSettings(new SWGSDRangel::SWGSatelliteTrackerSettings());
            featureSettings->getSatelliteTrackerSettings()->init();
            featureSettings->getSatelliteTrackerSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "SimplePTTSettings")
        {
            featureSettings->setSimplePttSettings(new SWGSDRangel::SWGSimplePTTSettings());
            featureSettings->getSimplePttSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "StarTrackerSettings")
        {
            featureSettings->setStarTrackerSettings(new SWGSDRangel::SWGStarTrackerSettings());
            featureSettings->getStarTrackerSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "RadiosondeSettings")
        {
            featureSettings->setRadiosondeSettings(new SWGSDRangel::SWGRadiosondeSettings());
            featureSettings->getRadiosondeSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "RigCtlServerSettings")
        {
            featureSettings->setRigCtlServerSettings(new SWGSDRangel::SWGRigCtlServerSettings());
            featureSettings->getRigCtlServerSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (featureSettingsKey == "VORLocalizerSettings")
        {
            featureSettings->setVorLocalizerSettings(new SWGSDRangel::SWGVORLocalizerSettings());
            featureSettings->getVorLocalizerSettings()->fromJsonObject(settingsJsonObject);
        }
        else
        {
            return false;
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool WebAPIRequestMapper::getFeatureActions(
    const QString& featureActionsKey,
    SWGSDRangel::SWGFeatureActions *featureActions,
    const QJsonObject& featureActionsJson,
    QStringList& featureActionsKeys
)
{
    QStringList featureKeys = featureActionsJson.keys();

    if (featureKeys.contains(featureActionsKey) && featureActionsJson[featureActionsKey].isObject())
    {
        QJsonObject actionsJsonObject = featureActionsJson[featureActionsKey].toObject();
        featureActionsKeys = actionsJsonObject.keys();
        qDebug("WebAPIRequestMapper::getFeatureActions: %s", qPrintable(featureActionsKey));

        if (featureActionsKey == "AFCActions")
        {
            featureActions->setAfcActions(new SWGSDRangel::SWGAFCActions());
            featureActions->getAfcActions()->fromJsonObject(actionsJsonObject);
        }
        else if (featureActionsKey == "AMBEActions")
        {
            featureActions->setAmbeActions(new SWGSDRangel::SWGAMBEActions());
            featureActions->getAmbeActions()->fromJsonObject(actionsJsonObject);
        }
        else if (featureActionsKey == "GS232ControllerActions")
        {
            featureActions->setGs232ControllerActions(new SWGSDRangel::SWGGS232ControllerActions());
            featureActions->getGs232ControllerActions()->fromJsonObject(actionsJsonObject);
        }
        else if (featureActionsKey == "LimeRFEActions")
        {
            featureActions->setLimeRfeActions(new SWGSDRangel::SWGLimeRFEActions());
            featureActions->getLimeRfeActions()->fromJsonObject(actionsJsonObject);
        }
        else if (featureActionsKey == "MapActions")
        {
            featureActions->setMapActions(new SWGSDRangel::SWGMapActions());
            featureActions->getMapActions()->fromJsonObject(actionsJsonObject);
        }
        else if (featureActionsKey == "PERTesterActions")
        {
            featureActions->setPerTesterActions(new SWGSDRangel::SWGPERTesterActions());
            featureActions->getPerTesterActions()->fromJsonObject(actionsJsonObject);
        }
        else if (featureActionsKey == "RigCtlServerActions")
        {
            featureActions->setRigCtlServerActions(new SWGSDRangel::SWGRigCtlServerActions());
            featureActions->getRigCtlServerActions()->fromJsonObject(actionsJsonObject);
        }
        else if (featureActionsKey == "SatelliteTrackerActions")
        {
            featureActions->setSatelliteTrackerActions(new SWGSDRangel::SWGSatelliteTrackerActions());
            featureActions->getSatelliteTrackerActions()->fromJsonObject(actionsJsonObject);
        }
        else if (featureActionsKey == "SimplePTTActions")
        {
            featureActions->setSimplePttActions(new SWGSDRangel::SWGSimplePTTActions());
            featureActions->getSimplePttActions()->fromJsonObject(actionsJsonObject);
        }
        else if (featureActionsKey == "StarTrackerActions")
        {
            featureActions->setStarTrackerActions(new SWGSDRangel::SWGStarTrackerActions());
            featureActions->getStarTrackerActions()->fromJsonObject(actionsJsonObject);
        }
        else if (featureActionsKey == "VORLocalizerActions")
        {
            featureActions->setVorLocalizerActions(new SWGSDRangel::SWGVORLocalizerActions());
            featureActions->getVorLocalizerActions()->fromJsonObject(actionsJsonObject);
        }
        else
        {
            return false;
        }

        return true;
    }
    else
    {
        return false;
    }
}

void WebAPIRequestMapper::extractKeys(
    const QJsonObject& rootJsonObject,
    QStringList& keyList
)
{
    QStringList rootKeys = rootJsonObject.keys();

    for (auto rootKey : rootKeys)
    {
        keyList.append(rootKey);

        if (rootJsonObject[rootKey].isObject())
        {
            QStringList subKeys;
            extractKeys(rootJsonObject[rootKey].toObject(), subKeys);

            for (auto subKey : subKeys) {
                keyList.append(tr("%1.%2").arg(rootKey).arg(subKey));
            }
        }
        else if (rootJsonObject[rootKey].isArray())
        {
            QJsonArray arrayJson = rootJsonObject[rootKey].toArray();

            for (int arrayIndex = 0; arrayIndex < arrayJson.count(); arrayIndex++)
            {
                QStringList subKeys;
                extractKeys(arrayJson.at(arrayIndex).toObject(), subKeys);
                keyList.append(tr("%1[%2]").arg(rootKey).arg(arrayIndex));

                for (auto subKey : subKeys) {
                    keyList.append(tr("%1[%2].%3").arg(rootKey).arg(arrayIndex).arg(subKey));
                }
            }
        }
    }
}

void WebAPIRequestMapper::resetSpectrumSettings(SWGSDRangel::SWGGLSpectrum& spectrumSettings)
{
    spectrumSettings.cleanup();
}

void WebAPIRequestMapper::resetDeviceSettings(SWGSDRangel::SWGDeviceSettings& deviceSettings)
{
    deviceSettings.cleanup();
    deviceSettings.setDeviceHwType(nullptr);
    deviceSettings.setAirspySettings(nullptr);
    deviceSettings.setAirspyHfSettings(nullptr);
    deviceSettings.setAudioInputSettings(nullptr);
    deviceSettings.setBladeRf1InputSettings(nullptr);
    deviceSettings.setBladeRf1OutputSettings(nullptr);
    deviceSettings.setFcdProPlusSettings(nullptr);
    deviceSettings.setFcdProSettings(nullptr);
    deviceSettings.setFileInputSettings(nullptr);
    deviceSettings.setFileOutputSettings(nullptr);
    deviceSettings.setHackRfInputSettings(nullptr);
    deviceSettings.setHackRfOutputSettings(nullptr);
    deviceSettings.setLimeSdrInputSettings(nullptr);
    deviceSettings.setLimeSdrOutputSettings(nullptr);
    deviceSettings.setPerseusSettings(nullptr);
    deviceSettings.setPlutoSdrInputSettings(nullptr);
    deviceSettings.setPlutoSdrOutputSettings(nullptr);
    deviceSettings.setRtlSdrSettings(nullptr);
    deviceSettings.setRemoteOutputSettings(nullptr);
    deviceSettings.setRemoteInputSettings(nullptr);
    deviceSettings.setRemoteTcpInputSettings(nullptr);
    deviceSettings.setSdrPlaySettings(nullptr);
    deviceSettings.setSdrPlayV3Settings(nullptr);
    deviceSettings.setTestSourceSettings(nullptr);
    deviceSettings.setUsrpInputSettings(nullptr);
    deviceSettings.setUsrpOutputSettings(nullptr);
}

void WebAPIRequestMapper::resetDeviceReport(SWGSDRangel::SWGDeviceReport& deviceReport)
{
    deviceReport.cleanup();
    deviceReport.setDeviceHwType(nullptr);
    deviceReport.setAirspyHfReport(nullptr);
    deviceReport.setAirspyReport(nullptr);
    deviceReport.setFileInputReport(nullptr);
    deviceReport.setLimeSdrInputReport(nullptr);
    deviceReport.setLimeSdrOutputReport(nullptr);
    deviceReport.setPerseusReport(nullptr);
    deviceReport.setPlutoSdrInputReport(nullptr);
    deviceReport.setPlutoSdrOutputReport(nullptr);
    deviceReport.setRtlSdrReport(nullptr);
    deviceReport.setRemoteOutputReport(nullptr);
    deviceReport.setRemoteInputReport(nullptr);
    deviceReport.setRemoteTcpInputReport(nullptr);
    deviceReport.setSdrPlayReport(nullptr);
    deviceReport.setSdrPlayV3Report(nullptr);
    deviceReport.setUsrpOutputReport(nullptr);
}

void WebAPIRequestMapper::resetDeviceActions(SWGSDRangel::SWGDeviceActions& deviceActions)
{
    deviceActions.cleanup();
    deviceActions.setDeviceHwType(nullptr);
    // deviceActions.setXtrxInputActions(nullptr);
}

void WebAPIRequestMapper::resetChannelSettings(SWGSDRangel::SWGChannelSettings& channelSettings)
{
    channelSettings.cleanup();
    channelSettings.setChannelType(nullptr);
    channelSettings.setAdsbDemodSettings(nullptr);
    channelSettings.setAisDemodSettings(nullptr);
    channelSettings.setAisModSettings(nullptr);
    channelSettings.setAmDemodSettings(nullptr);
    channelSettings.setAmModSettings(nullptr);
    channelSettings.setAptDemodSettings(nullptr);
    channelSettings.setAtvModSettings(nullptr);
    channelSettings.setBfmDemodSettings(nullptr);
    channelSettings.setDatvModSettings(nullptr);
    channelSettings.setDabDemodSettings(nullptr);
    channelSettings.setDsdDemodSettings(nullptr);
    channelSettings.setHeatMapSettings(nullptr);
    channelSettings.setIeee802154ModSettings(nullptr);
    channelSettings.setIlsDemodSettings(nullptr);
    channelSettings.setNavtexDemodSettings(nullptr);
    channelSettings.setNfmDemodSettings(nullptr);
    channelSettings.setNfmModSettings(nullptr);
    channelSettings.setNoiseFigureSettings(nullptr);
    channelSettings.setPacketDemodSettings(nullptr);
    channelSettings.setPacketModSettings(nullptr);
    channelSettings.setPagerDemodSettings(nullptr);
    channelSettings.setRadioAstronomySettings(nullptr);
    channelSettings.setRadioClockSettings(nullptr);
    channelSettings.setRadiosondeDemodSettings(nullptr);
    channelSettings.setRemoteSinkSettings(nullptr);
    channelSettings.setRemoteSourceSettings(nullptr);
    channelSettings.setRemoteTcpSinkSettings(nullptr);
    channelSettings.setRttyDemodSettings(nullptr);
    channelSettings.setRttyModSettings(nullptr);
    channelSettings.setSsbDemodSettings(nullptr);
    channelSettings.setSsbModSettings(nullptr);
    channelSettings.setUdpSourceSettings(nullptr);
    channelSettings.setUdpSinkSettings(nullptr);
    // channelSettings.setVorDemodMcSettings(nullptr);
    channelSettings.setVorDemodSettings(nullptr);
    channelSettings.setWfmDemodSettings(nullptr);
    channelSettings.setWfmModSettings(nullptr);
}

void WebAPIRequestMapper::resetChannelReport(SWGSDRangel::SWGChannelReport& channelReport)
{
    channelReport.cleanup();
    channelReport.setChannelType(nullptr);
    channelReport.setAdsbDemodReport(nullptr);
    channelReport.setAisDemodReport(nullptr);
    channelReport.setAisModReport(nullptr);
    channelReport.setAmDemodReport(nullptr);
    channelReport.setAmModReport(nullptr);
    channelReport.setAtvModReport(nullptr);
    channelReport.setBfmDemodReport(nullptr);
    channelReport.setDatvModReport(nullptr);
    channelReport.setDsdDemodReport(nullptr);
    channelReport.setHeatMapReport(nullptr);
    channelReport.setIlsDemodReport(nullptr);
    channelReport.setNavtexDemodReport(nullptr);
    channelReport.setNfmDemodReport(nullptr);
    channelReport.setNfmModReport(nullptr);
    channelReport.setNoiseFigureReport(nullptr);
    channelReport.setIeee802154ModReport(nullptr);
    channelReport.setPacketModReport(nullptr);
    channelReport.setRadioAstronomyReport(nullptr);
    channelReport.setRadioClockReport(nullptr);
    channelReport.setRadiosondeDemodReport(nullptr);
    channelReport.setRemoteSourceReport(nullptr);
    channelReport.setRttyDemodReport(nullptr);
    channelReport.setRttyModReport(nullptr);
    channelReport.setSsbDemodReport(nullptr);
    channelReport.setSsbModReport(nullptr);
    channelReport.setUdpSourceReport(nullptr);
    channelReport.setUdpSinkReport(nullptr);
    // channelReport.setVorDemodMcReport(nullptr);
    channelReport.setVorDemodReport(nullptr);
    channelReport.setWfmDemodReport(nullptr);
    channelReport.setWfmModReport(nullptr);
}

void WebAPIRequestMapper::resetChannelActions(SWGSDRangel::SWGChannelActions& channelActions)
{
    channelActions.cleanup();
    channelActions.setAisModActions(nullptr);
    channelActions.setAptDemodActions(nullptr);
    channelActions.setChannelType(nullptr);
    channelActions.setFileSourceActions(nullptr);
    channelActions.setIeee802154ModActions(nullptr);
    channelActions.setRadioAstronomyActions(nullptr);
    channelActions.setPacketModActions(nullptr);
    channelActions.setRttyModActions(nullptr);
}

void WebAPIRequestMapper::resetAudioInputDevice(SWGSDRangel::SWGAudioInputDevice& audioInputDevice)
{
    audioInputDevice.cleanup();
    audioInputDevice.setName(nullptr);
}

void WebAPIRequestMapper::resetAudioOutputDevice(SWGSDRangel::SWGAudioOutputDevice& audioOutputDevice)
{
    audioOutputDevice.cleanup();
    audioOutputDevice.setName(nullptr);
    audioOutputDevice.setUdpAddress(nullptr);
}

void WebAPIRequestMapper::resetFeatureSettings(SWGSDRangel::SWGFeatureSettings& featureSettings)
{
    featureSettings.cleanup();
    featureSettings.setFeatureType(nullptr);
    featureSettings.setAisSettings(nullptr);
    featureSettings.setAntennaToolsSettings(nullptr);
    featureSettings.setAprsSettings(nullptr);
    featureSettings.setGs232ControllerSettings(nullptr);
    featureSettings.setMapSettings(nullptr);
    featureSettings.setPerTesterSettings(nullptr);
    featureSettings.setSatelliteTrackerSettings(nullptr);
    featureSettings.setSimplePttSettings(nullptr);
    featureSettings.setStarTrackerSettings(nullptr);
    featureSettings.setRadiosondeSettings(nullptr);
    featureSettings.setRigCtlServerSettings(nullptr);
}

void WebAPIRequestMapper::resetFeatureReport(SWGSDRangel::SWGFeatureReport& featureReport)
{
    featureReport.cleanup();
    featureReport.setFeatureType(nullptr);
    featureReport.setAfcReport(nullptr);
    featureReport.setGs232ControllerReport(nullptr);
    featureReport.setPerTesterReport(nullptr);
    featureReport.setRigCtlServerReport(nullptr);
    featureReport.setMapReport(nullptr);
    featureReport.setSatelliteTrackerReport(nullptr);
    featureReport.setSimplePttReport(nullptr);
    featureReport.setStarTrackerReport(nullptr);
    featureReport.setVorLocalizerReport(nullptr);
}

void WebAPIRequestMapper::resetFeatureActions(SWGSDRangel::SWGFeatureActions& featureActions)
{
    featureActions.cleanup();
    featureActions.setFeatureType(nullptr);
    featureActions.setAfcActions(nullptr);
    featureActions.setGs232ControllerActions(nullptr);
    featureActions.setMapActions(nullptr);
    featureActions.setPerTesterActions(nullptr);
    featureActions.setRigCtlServerActions(nullptr);
    featureActions.setSatelliteTrackerActions(nullptr);
    featureActions.setSimplePttActions(nullptr);
    featureActions.setStarTrackerActions(nullptr);
    featureActions.setVorLocalizerActions(nullptr);
}

