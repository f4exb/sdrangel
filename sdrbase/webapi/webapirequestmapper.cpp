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
#include "SWGInstanceSummaryResponse.h"
#include "SWGInstanceConfigResponse.h"
#include "SWGInstanceDevicesResponse.h"
#include "SWGInstanceChannelsResponse.h"
#include "SWGAudioDevices.h"
#include "SWGLocationInformation.h"
#include "SWGDVSerialDevices.h"
#include "SWGAMBEDevices.h"
#include "SWGLimeRFEDevices.h"
#include "SWGLimeRFESettings.h"
#include "SWGLimeRFEPower.h"
#include "SWGPresets.h"
#include "SWGPresetTransfer.h"
#include "SWGPresetIdentifier.h"
#include "SWGPresetImport.h"
#include "SWGPresetExport.h"
#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGDeviceActions.h"
#include "SWGChannelsDetail.h"
#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"
#include "SWGSuccessResponse.h"
#include "SWGErrorResponse.h"

const QMap<QString, QString> WebAPIRequestMapper::m_channelURIToSettingsKey = {
    {"sdrangel.channel.amdemod", "AMDemodSettings"},
    {"de.maintech.sdrangelove.channel.am", "AMDemodSettings"}, // remap
    {"sdrangel.channeltx.modam", "AMModSettings"},
    {"sdrangel.channeltx.modatv", "ATVModSettings"},
    {"sdrangel.channel.bfm", "BFMDemodSettings"},
    {"sdrangel.channel.chanalyzer", "ChannelAnalyzerSettings"},
    {"sdrangel.channel.chanalyzerng", "ChannelAnalyzerSettings"}, // remap
    {"org.f4exb.sdrangelove.channel.chanalyzer", "ChannelAnalyzerSettings"}, // remap
    {"sdrangel.channel.demodatv", "ATVDemodSettings"},
    {"sdrangel.channel.demoddatv", "DATVDemodSettings"},
    {"sdrangel.channel.dsddemod", "DSDDemodSettings"},
    {"sdrangel.channeltx.filesource", "FileSourceSettings"},
    {"sdrangel.channel.freedvdemod", "FreeDVDemodSettings"},
    {"sdrangel.channeltx.freedvmod", "FreeDVModSettings"},
    {"sdrangel.channel.freqtracker", "FreqTrackerSettings"},
    {"sdrangel.channel.nfmdemod", "NFMDemodSettings"},
    {"de.maintech.sdrangelove.channel.nfm", "NFMDemodSettings"}, // remap
    {"sdrangel.channeltx.modnfm", "NFMModSettings"},
    {"sdrangel.demod.localsink", "LocalSinkSettings"},
    {"sdrangel.channel.localsink", "LocalSinkSettings"}, // remap
    {"sdrangel.channel.localsource", "LocalSourceSettings"},
    {"sdrangel.demod.remotesink", "RemoteSinkSettings"},
    {"sdrangel.channeltx.remotesource", "RemoteSourceSettings"},
    {"sdrangel.channeltx.modssb", "SSBModSettings"},
    {"sdrangel.channel.ssbdemod", "SSBDemodSettings"},
    {"de.maintech.sdrangelove.channel.ssb", "SSBDemodSettings"}, // remap
    {"sdrangel.channeltx.udpsource", "UDPSourceSettings"},
    {"sdrangel.channeltx.udpsink", "UDPSourceSettings"}, // remap
    {"sdrangel.channel.udpsink", "UDPSinkSettings"},
    {"sdrangel.channel.udpsrc", "UDPSinkSettings"}, // remap
    {"sdrangel.channel.wfmdemod", "WFMDemodSettings"},
    {"de.maintech.sdrangelove.channel.wfm", "WFMDemodSettings"}, // remap
    {"sdrangel.channeltx.modwfm", "WFMModSettings"}
};

const QMap<QString, QString> WebAPIRequestMapper::m_deviceIdToSettingsKey = {
    {"sdrangel.samplesource.airspy", "airspySettings"},
    {"sdrangel.samplesource.airspyhf", "airspyHFSettings"},
    {"sdrangel.samplesource.bladerf1input", "bladeRF1InputSettings"},
    {"sdrangel.samplesource.bladerf", "bladeRF1InputSettings"}, // remap
    {"sdrangel.samplesink.bladerf1output", "bladeRF1OutputSettings"},
    {"sdrangel.samplesource.bladerf1output", "bladeRF1OutputSettings"}, // remap
    {"sdrangel.samplesource.bladerfoutput", "bladeRF1OutputSettings"}, // remap
    {"sdrangel.samplesource.bladerf2input", "bladeRF2InputSettings"},
    {"sdrangel.samplesink.bladerf2output", "bladeRF2OutputSettings"},
    {"sdrangel.samplesource.bladerf2output", "bladeRF2OutputSettings"}, // remap
    {"sdrangel.samplesource.fcdpro", "fcdProSettings"},
    {"sdrangel.samplesource.fcdproplus", "fcdProPlusSettings"},
    {"sdrangel.samplesource.fileinput", "fileInputSettings"},
    {"sdrangel.samplesource.filesource", "fileInputSettings"}, // remap
    {"sdrangel.samplesource.hackrf", "hackRFInputSettings"},
    {"sdrangel.samplesink.hackrf", "hackRFOutputSettings"},
    {"sdrangel.samplesource.hackrfoutput", "hackRFOutputSettings"}, // remap
    {"sdrangel.samplesource.kiwisdrsource", "kiwiSDRSettings"},
    {"sdrangel.samplesource.limesdr", "limeSdrInputSettings"},
    {"sdrangel.samplesink.limesdr", "limeSdrOutputSettings"},
    {"sdrangel.samplesource.localinput", "localInputSettings"},
    {"sdrangel.samplesink.localoutput", "localOutputSettings"},
    {"sdrangel.samplesource.localoutput", "localOutputSettings"}, // remap
    {"sdrangel.samplesource.perseus", "perseusSettings"},
    {"sdrangel.samplesource.plutosdr", "plutoSdrInputSettings"},
    {"sdrangel.samplesink.plutosdr", "plutoSdrOutputSettings"},
    {"sdrangel.samplesource.rtlsdr", "rtlSdrSettings"},
    {"sdrangel.samplesource.remoteinput", "remoteInputSettings"},
    {"sdrangel.samplesink.remoteoutput", "remoteOutputSettings"},
    {"sdrangel.samplesource.sdrplay", "sdrPlaySettings"},
    {"sdrangel.samplesource.soapysdrinput", "soapySDRInputSettings"},
    {"sdrangel.samplesink.soapysdroutput", "soapySDROutputSettings"},
    {"sdrangel.samplesource.testsource", "testSourceSettings"},
    {"sdrangel.samplesource.xtrx", "XtrxInputSettings"},
    {"sdrangel.samplesink.xtrx", "XtrxOutputSettings"}
};

const QMap<QString, QString> WebAPIRequestMapper::m_channelTypeToSettingsKey = {
    {"AMDemod", "AMDemodSettings"},
    {"AMMod", "AMModSettings"},
    {"ATVDemod", "ATVDemodSettings"},
    {"ATVMod", "ATVModSettings"},
    {"BFMDemod", "BFMDemodSettings"},
    {"ChannelAnalyzer", "ChannelAnalyzerSettings"},
    {"DATVDemod", "DATVDemodSettings"},
    {"DSDDemod", "DSDDemodSettings"},
    {"FileSource", "FileSourceSettings"},
    {"FreeDVDemod", "FreeDVDemodSettings"},
    {"FreeDVMod", "FreeDVModSettings"},
    {"FreqTracker", "FreqTrackerSettings"},
    {"NFMDemod", "NFMDemodSettings"},
    {"NFMMod", "NFMModSettings"},
    {"LocalSink", "LocalSinkSettings"},
    {"LocalSource", "LocalSourceSettings"},
    {"RemoteSink", "RemoteSinkSettings"},
    {"RemoteSource", "RemoteSourceSettings"},
    {"SSBMod", "SSBModSettings"},
    {"SSBDemod", "SSBDemodSettings"},
    {"UDPSink", "UDPSourceSettings"},
    {"UDPSource", "UDPSinkSettings"},
    {"WFMDemod", "WFMDemodSettings"},
    {"WFMMod", "WFMModSettings"}
};

const QMap<QString, QString> WebAPIRequestMapper::m_channelTypeToActionsKey = {
    {"FileSource", "FileSourceActions"}
};

const QMap<QString, QString> WebAPIRequestMapper::m_sourceDeviceHwIdToSettingsKey = {
    {"Airspy", "airspySettings"},
    {"AirspyHF", "airspyHFSettings"},
    {"BladeRF1", "bladeRF1InputSettings"},
    {"BladeRF2", "bladeRF2InputSettings"},
    {"FCDPro", "fcdProSettings"},
    {"FCDPro+", "fcdProPlusSettings"},
    {"FileInput", "fileInputSettings"},
    {"HackRF", "hackRFInputSettings"},
    {"KiwiSDR", "kiwiSDRSettings"},
    {"LimeSDR", "limeSdrInputSettings"},
    {"LocalInput", "localInputSettings"},
    {"Perseus", "perseusSettings"},
    {"PlutoSDR", "plutoSdrInputSettings"},
    {"RTLSDR", "rtlSdrSettings"},
    {"RemoteInput", "remoteInputSettings"},
    {"SDRplay1", "sdrPlaySettings"},
    {"SoapySDR", "soapySDRInputSettings"},
    {"TestSource", "testSourceSettings"},
    {"XTRX", "XtrxInputSettings"}
};

const QMap<QString, QString> WebAPIRequestMapper::m_sourceDeviceHwIdToActionsKey = {
    {"Airspy", "airspyActions"},
    {"AirspyHF", "airspyHFActions"},
    {"BladeRF1", "bladeRF1InputActions"},
    {"FCDPro", "fcdProActions"},
    {"FCDPro+", "fcdProPlusActions"},
    {"HackRF", "hackRFInputActions"},
    {"KiwiSDR", "kiwiSDRActions"},
    {"LimeSDR", "limeSdrInputActions"},
    {"LocalInput", "localInputActions"},
    {"Perseus", "perseusActions"},
    {"PlutoSDR", "plutoSdrInputActions"},
    {"RemoteInput", "remoteInputActions"},
    {"RTLSDR", "rtlSdrActions"},
    {"SDRplay1", "sdrPlayActions"},
    {"SoapySDR", "soapySDRInputActions"},
    {"TestSource", "testSourceActions"},
    {"XTRX", "xtrxInputActions"}
};

const QMap<QString, QString> WebAPIRequestMapper::m_sinkDeviceHwIdToSettingsKey = {
    {"BladeRF1", "bladeRF1OutputSettings"},
    {"BladeRF2", "bladeRF2OutputSettings"},
    {"HackRF", "hackRFOutputSettings"},
    {"LimeSDR", "limeSdrOutputSettings"},
    {"LocalOutput", "localOutputSettings"},
    {"PlutoSDR", "plutoSdrOutputSettings"},
    {"RemoteOutput", "remoteOutputSettings"},
    {"SoapySDR", "soapySDROutputSettings"},
    {"XTRX", "xtrxOutputSettings"}
};

const QMap<QString, QString> WebAPIRequestMapper::m_sinkDeviceHwIdToActionsKey = {
};

const QMap<QString, QString> WebAPIRequestMapper::m_mimoDeviceHwIdToSettingsKey= {
    {"BladeRF2", "bladeRF2MIMOSettings"},
    {"TestMI", "testMISettings"},
    {"TestMOSync", "testMOSyncSettings"}
};

const QMap<QString, QString> WebAPIRequestMapper::m_mimoDeviceHwIdToActionsKey= {
};

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
        } else if (path == WebAPIAdapterInterface::instanceAMBESerialURL) {
            instanceAMBESerialService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceAMBEDevicesURL) {
            instanceAMBEDevicesService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceLimeRFESerialURL) {
            instanceLimeRFESerialService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceLimeRFEConfigURL) {
            instanceLimeRFEConfigService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceLimeRFERunURL) {
            instanceLimeRFERunService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceLimeRFEPowerURL) {
            instanceLimeRFEPowerService(request, response);
        } else if (path == WebAPIAdapterInterface::instancePresetsURL) {
            instancePresetsService(request, response);
        } else if (path == WebAPIAdapterInterface::instancePresetURL) {
            instancePresetService(request, response);
        } else if (path == WebAPIAdapterInterface::instancePresetFileURL) {
            instancePresetFileService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceDeviceSetsURL) {
            instanceDeviceSetsService(request, response);
        } else if (path == WebAPIAdapterInterface::instanceDeviceSetURL) {
            instanceDeviceSetService(request, response);
        }
        else
        {
            std::smatch desc_match;
            std::string pathStr(path.constData(), path.length());

            if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetURLRe)) {
                devicesetService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetDeviceURLRe)) {
                devicesetDeviceService(std::string(desc_match[1]), request, response);
            } else if (std::regex_match(pathStr, desc_match, WebAPIAdapterInterface::devicesetFocusURLRe)) {
                devicesetFocusService(std::string(desc_match[1]), request, response);
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
                    normalResponse.setMessage(new QString("Error occured while updating configuration"));
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

void WebAPIRequestMapper::instanceAMBESerialService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGDVSerialDevices normalResponse;

        int status = m_adapter->instanceAMBESerialGet(normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceAMBEDevicesService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGAMBEDevices normalResponse;

        int status = m_adapter->instanceAMBEDevicesGet(normalResponse, errorResponse);
        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if ((request.getMethod() == "PATCH") || (request.getMethod() == "PUT"))
    {
        SWGSDRangel::SWGAMBEDevices query;
        SWGSDRangel::SWGAMBEDevices normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            if (validateAMBEDevices(query, jsonObject))
            {
                int status;

                if (request.getMethod() == "PATCH") {
                    status = m_adapter->instanceAMBEDevicesPatch(query, normalResponse, errorResponse);
                } else {
                    status = m_adapter->instanceAMBEDevicesPut(query, normalResponse, errorResponse);
                }

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
        SWGSDRangel::SWGSuccessResponse normalResponse;

        int status = m_adapter->instanceAMBEDevicesDelete(normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceLimeRFESerialService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        SWGSDRangel::SWGLimeRFEDevices normalResponse;

        int status = m_adapter->instanceLimeRFESerialGet(normalResponse, errorResponse);
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

void WebAPIRequestMapper::instanceLimeRFEConfigService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        QByteArray serialStr = request.getParameter("serial");
        SWGSDRangel::SWGLimeRFESettings normalResponse;

        int status = m_adapter->instanceLimeRFEConfigGet(serialStr, normalResponse, errorResponse);
        response.setStatus(status);

        if (status/100 == 2) {
            response.write(normalResponse.asJson().toUtf8());
        } else {
            response.write(errorResponse.asJson().toUtf8());
        }
    }
    else if (request.getMethod() == "PUT")
    {
        SWGSDRangel::SWGLimeRFESettings query;
        SWGSDRangel::SWGSuccessResponse normalResponse;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            QStringList limeRFESettingsKeys;

            if (validateLimeRFEConfig(query, jsonObject, limeRFESettingsKeys))
            {
                if (limeRFESettingsKeys.contains("devicePath"))
                {
                    int status = m_adapter->instanceLimeRFEConfigPut(query, normalResponse, errorResponse);
                    response.setStatus(status);

                    if (status/100 == 2) {
                        response.write(normalResponse.asJson().toUtf8());
                    } else {
                        response.write(errorResponse.asJson().toUtf8());
                    }
                }
                else
                {
                    response.setStatus(400,"LimeRFE device path expected in JSON body");
                    errorResponse.init();
                    *errorResponse.getMessage() = "Invalid request";
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

void WebAPIRequestMapper::instanceLimeRFERunService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "PUT")
    {
        SWGSDRangel::SWGLimeRFESettings query;
        QString jsonStr = request.getBody();
        QJsonObject jsonObject;

        if (parseJsonBody(jsonStr, jsonObject, response))
        {
            QStringList limeRFESettingsKeys;

            if (validateLimeRFEConfig(query, jsonObject, limeRFESettingsKeys))
            {
                if (limeRFESettingsKeys.contains("devicePath"))
                {
                    SWGSDRangel::SWGSuccessResponse normalResponse;
                    int status = m_adapter->instanceLimeRFERunPut(query, normalResponse, errorResponse);
                    response.setStatus(status);

                    if (status/100 == 2) {
                        response.write(normalResponse.asJson().toUtf8());
                    } else {
                        response.write(errorResponse.asJson().toUtf8());
                    }
                }
                else
                {
                    response.setStatus(400,"LimeRFE device path expected in JSON body");
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

void WebAPIRequestMapper::instanceLimeRFEPowerService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.getMethod() == "GET")
    {
        QByteArray serialStr = request.getParameter("serial");
        SWGSDRangel::SWGLimeRFEPower normalResponse;

        int status = m_adapter->instanceLimeRFEPowerGet(serialStr, normalResponse, errorResponse);
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
        SWGSDRangel::SWGPresetImport query;
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

void WebAPIRequestMapper::devicesetFocusService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    SWGSDRangel::SWGErrorResponse errorResponse;
    response.setHeader("Content-Type", "application/json");
    response.setHeader("Access-Control-Allow-Origin", "*");

    try
    {
        int deviceSetIndex = boost::lexical_cast<int>(indexStr);

        if (request.getMethod() == "PATCH")
        {
            SWGSDRangel::SWGSuccessResponse normalResponse;
            int status = m_adapter->devicesetFocusPatch(deviceSetIndex, normalResponse, errorResponse);

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

void WebAPIRequestMapper::devicesetDeviceSettingsService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
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
        if (m_sourceDeviceHwIdToSettingsKey.contains(*deviceHwType)) {
            deviceSettingsKey = m_sourceDeviceHwIdToSettingsKey[*deviceHwType];
        } else {
            return false;
        }
    }
    else if (deviceSettings.getDirection() == 1) // sink
    {
        if (m_sinkDeviceHwIdToSettingsKey.contains(*deviceHwType)) {
            deviceSettingsKey = m_sinkDeviceHwIdToSettingsKey[*deviceHwType];
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
        if (m_sourceDeviceHwIdToSettingsKey.contains(*deviceHwType)) {
            deviceActionsKey = m_sourceDeviceHwIdToActionsKey[*deviceHwType];
        } else {
            return false;
        }
    }
    else if (deviceActions.getDirection() == 1) // sink
    {
        if (m_sinkDeviceHwIdToSettingsKey.contains(*deviceHwType)) {
            deviceActionsKey = m_sinkDeviceHwIdToActionsKey[*deviceHwType];
        } else {
            return false;
        }
    }
    else if (deviceActions.getDirection() == 2) // MIMO
    {
        if (m_mimoDeviceHwIdToSettingsKey.contains(*deviceHwType)) {
            deviceActionsKey = m_mimoDeviceHwIdToActionsKey[*deviceHwType];
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

    if (m_channelTypeToSettingsKey.contains(*channelType)) {
        return getChannelSettings(m_channelTypeToSettingsKey[*channelType], &channelSettings, jsonObject, channelSettingsKeys);
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

    if (m_channelTypeToActionsKey.contains(*channelType)) {
        return getChannelActions(m_channelTypeToActionsKey[*channelType], &channelActions, jsonObject, channelActionsKeys);
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
    return true;
}

bool WebAPIRequestMapper::validateAMBEDevices(SWGSDRangel::SWGAMBEDevices& ambeDevices, QJsonObject& jsonObject)
{
    if (jsonObject.contains("nbDevices"))
    {
        int nbDevices = jsonObject["nbDevices"].toInt();

        if (jsonObject.contains("ambeDevices"))
        {
            QJsonArray ambeDevicesJson = jsonObject["ambeDevices"].toArray();

            if (nbDevices != ambeDevicesJson.size()) {
                return false;
            }

            ambeDevices.init();
            ambeDevices.setNbDevices(nbDevices);
            QList<SWGSDRangel::SWGAMBEDevice *> *ambeList = ambeDevices.getAmbeDevices();

            for (int i = 0; i < nbDevices; i++)
            {
                QJsonObject ambeDeviceJson = ambeDevicesJson.at(i).toObject();
                if (ambeDeviceJson.contains("deviceRef") && ambeDeviceJson.contains("delete"))
                {
                    ambeList->push_back(new SWGSDRangel::SWGAMBEDevice());
                    ambeList->back()->init();
                    ambeList->back()->setDeviceRef(new QString(ambeDeviceJson["deviceRef"].toString()));
                    ambeList->back()->setDelete(ambeDeviceJson["delete"].toInt());
                }
                else
                {
                    return false;
                }
            }

            return true;
        }
    }

    return false;
}

bool WebAPIRequestMapper::validateLimeRFEConfig(SWGSDRangel::SWGLimeRFESettings& limeRFESettings, QJsonObject& jsonObject, QStringList& limeRFESettingsKeys)
{
    if (jsonObject.contains("devicePath"))
    {
        limeRFESettings.setDevicePath(new QString(jsonObject["devicePath"].toString()));
        limeRFESettingsKeys.append("devicePath");
    }
    if (jsonObject.contains("rxChannels"))
    {
        limeRFESettings.setRxChannels(jsonObject["rxChannels"].toInt());
        limeRFESettingsKeys.append("rxChannels");
    }
    if (jsonObject.contains("rxWidebandChannel"))
    {
        limeRFESettings.setRxWidebandChannel(jsonObject["rxWidebandChannel"].toInt());
        limeRFESettingsKeys.append("rxWidebandChannel");
    }
    if (jsonObject.contains("rxHAMChannel"))
    {
        limeRFESettings.setRxHamChannel(jsonObject["rxHAMChannel"].toInt());
        limeRFESettingsKeys.append("rxHAMChannel");
    }
    if (jsonObject.contains("rxCellularChannel"))
    {
        limeRFESettings.setRxCellularChannel(jsonObject["rxCellularChannel"].toInt());
        limeRFESettingsKeys.append("rxCellularChannel");
    }
    if (jsonObject.contains("rxPort"))
    {
        limeRFESettings.setRxPort(jsonObject["rxPort"].toInt());
        limeRFESettingsKeys.append("rxPort");
    }
    if (jsonObject.contains("attenuationFactor"))
    {
        limeRFESettings.setAttenuationFactor(jsonObject["attenuationFactor"].toInt());
        limeRFESettingsKeys.append("attenuationFactor");
    }
    if (jsonObject.contains("amfmNotch"))
    {
        limeRFESettings.setAmfmNotch(jsonObject["amfmNotch"].toInt());
        limeRFESettingsKeys.append("amfmNotch");
    }
    if (jsonObject.contains("txChannels"))
    {
        limeRFESettings.setTxChannels(jsonObject["txChannels"].toInt());
        limeRFESettingsKeys.append("txChannels");
    }
    if (jsonObject.contains("txWidebandChannel"))
    {
        limeRFESettings.setTxWidebandChannel(jsonObject["txWidebandChannel"].toInt());
        limeRFESettingsKeys.append("txWidebandChannel");
    }
    if (jsonObject.contains("txHAMChannel"))
    {
        limeRFESettings.setTxHamChannel(jsonObject["txHAMChannel"].toInt());
        limeRFESettingsKeys.append("txHAMChannel");
    }
    if (jsonObject.contains("txCellularChannel"))
    {
        limeRFESettings.setTxCellularChannel(jsonObject["txCellularChannel"].toInt());
        limeRFESettingsKeys.append("txCellularChannel");
    }
    if (jsonObject.contains("txPort"))
    {
        limeRFESettings.setTxPort(jsonObject["txPort"].toInt());
        limeRFESettingsKeys.append("txPort");
    }
    if (jsonObject.contains("rxOn"))
    {
        limeRFESettings.setRxOn(jsonObject["rxOn"].toInt());
        limeRFESettingsKeys.append("rxOn");
    }
    if (jsonObject.contains("txOn"))
    {
        limeRFESettings.setTxOn(jsonObject["txOn"].toInt());
        limeRFESettingsKeys.append("txOn");
    }
    if (jsonObject.contains("swrEnable"))
    {
        limeRFESettings.setSwrEnable(jsonObject["swrEnable"].toInt());
        limeRFESettingsKeys.append("swrEnable");
    }
    if (jsonObject.contains("swrSource"))
    {
        limeRFESettings.setSwrSource(jsonObject["swrSource"].toInt());
        limeRFESettingsKeys.append("swrSource");
    }

    return true;
}

bool WebAPIRequestMapper::validateConfig(SWGSDRangel::SWGInstanceConfigResponse& config, QJsonObject& jsonObject, WebAPIAdapterInterface::ConfigKeys& configKeys)
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

    if (jsonObject.contains("workingPreset"))
    {
        SWGSDRangel::SWGPreset *preset = new SWGSDRangel::SWGPreset();
        config.setWorkingPreset(preset);
        QJsonObject presetJson = jsonObject["workingPreset"].toObject();
        appendPresetKeys(preset, presetJson, configKeys.m_workingPresetKeys);
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

        if (channelSettingsJson.contains("config") && m_channelURIToSettingsKey.contains(*channelURI))
        {
            SWGSDRangel::SWGChannelSettings *channelSettings = new SWGSDRangel::SWGChannelSettings();
            channel->setConfig(channelSettings);
            return getChannelSettings(m_channelURIToSettingsKey[*channelURI], channelSettings, channelSettingsJson["config"].toObject(), channelKeys.m_channelKeys);
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
        channelSettingsKeys = settingsJsonObject.keys();

        // get possible sub-keys
        if (channelSettingsKeys.contains("cwKeyer"))
        {
            QJsonObject cwJson; // unused
            appendSettingsSubKeys(settingsJsonObject, cwJson, "cwKeyer", channelSettingsKeys);
        }

        if (channelSettingsKey == "AMDemodSettings")
        {
            channelSettings->setAmDemodSettings(new SWGSDRangel::SWGAMDemodSettings());
            channelSettings->getAmDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "AMModSettings")
        {
            channelSettings->setAmModSettings(new SWGSDRangel::SWGAMModSettings());
            channelSettings->getAmModSettings()->fromJsonObject(settingsJsonObject);
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
        else if (channelSettingsKey == "BFMDemodSettings")
        {
            channelSettings->setBfmDemodSettings(new SWGSDRangel::SWGBFMDemodSettings());
            channelSettings->getBfmDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "ChannelAnalyzerSettings")
        {
            processChannelAnalyzerSettings(channelSettings, settingsJsonObject, channelSettingsKeys);
        }
        else if (channelSettingsKey == "DATVDemodSettings")
        {
            channelSettings->setDatvDemodSettings(new SWGSDRangel::SWGDATVDemodSettings());
            channelSettings->getDatvDemodSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "DSDDemodSettings")
        {
            channelSettings->setDsdDemodSettings(new SWGSDRangel::SWGDSDDemodSettings());
            channelSettings->getDsdDemodSettings()->fromJsonObject(settingsJsonObject);
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
        else if (channelSettingsKey == "LocalSinkSettings")
        {
            channelSettings->setLocalSinkSettings(new SWGSDRangel::SWGLocalSinkSettings());
            channelSettings->getLocalSinkSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (channelSettingsKey == "LocalSourceSettings")
        {
            channelSettings->setLocalSourceSettings(new SWGSDRangel::SWGLocalSourceSettings());
            channelSettings->getLocalSourceSettings()->fromJsonObject(settingsJsonObject);
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

        if (channelActionsKey == "FileSourceActions")
        {
            channelActions->setFileSourceActions(new SWGSDRangel::SWGFileSourceActions());
            channelActions->getFileSourceActions()->fromJsonObject(actionsJsonObject);
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

        if (deviceSettngsJson.contains("config") && m_deviceIdToSettingsKey.contains(*deviceId))
        {
            SWGSDRangel::SWGDeviceSettings *deviceSettings = new SWGSDRangel::SWGDeviceSettings();
            device->setConfig(deviceSettings);
            return getDeviceSettings(m_deviceIdToSettingsKey[*deviceId], deviceSettings, deviceSettngsJson["config"].toObject(), devicelKeys.m_deviceKeys);
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
        deviceSettingsKeys = settingsJsonObject.keys();

        if (deviceSettingsKey == "airspySettings")
        {
            deviceSettings->setAirspySettings(new SWGSDRangel::SWGAirspySettings());
            deviceSettings->getAirspySettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "airspyHFSettings")
        {
            deviceSettings->setAirspyHfSettings(new SWGSDRangel::SWGAirspyHFSettings());
            deviceSettings->getAirspyHfSettings()->fromJsonObject(settingsJsonObject);
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
        else if (deviceSettingsKey == "bladeRF2InputSettings")
        {
            deviceSettings->setBladeRf2OutputSettings(new SWGSDRangel::SWGBladeRF2OutputSettings());
            deviceSettings->getBladeRf2OutputSettings()->fromJsonObject(settingsJsonObject);
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
        else if (deviceSettingsKey == "soapySDRInputSettings")
        {
            processSoapySDRSettings(deviceSettings, settingsJsonObject, deviceSettingsKeys, true);
            // deviceSettings->setSoapySdrInputSettings(new SWGSDRangel::SWGSoapySDRInputSettings());
            // deviceSettings->getSoapySdrInputSettings()->init(); // contains complex objects
            // deviceSettings->getSoapySdrInputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "soapySDROutputSettings")
        {
            processSoapySDRSettings(deviceSettings, settingsJsonObject, deviceSettingsKeys, false);
            // deviceSettings->setSoapySdrOutputSettings(new SWGSDRangel::SWGSoapySDROutputSettings());
            // deviceSettings->getSoapySdrOutputSettings()->init(); // contains complex objects
            // deviceSettings->getSoapySdrOutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "testSourceSettings")
        {
            deviceSettings->setTestSourceSettings(new SWGSDRangel::SWGTestSourceSettings());
            deviceSettings->getTestSourceSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "XtrxInputSettings")
        {
            deviceSettings->setXtrxInputSettings(new SWGSDRangel::SWGXtrxInputSettings());
            deviceSettings->getXtrxInputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "XtrxOutputSettings")
        {
            deviceSettings->setXtrxOutputSettings(new SWGSDRangel::SWGXtrxOutputSettings());
            deviceSettings->getXtrxOutputSettings()->fromJsonObject(settingsJsonObject);
        }
        else if (deviceSettingsKey == "remoteInputSettings")
        {
            deviceSettings->setRemoteInputSettings(new SWGSDRangel::SWGRemoteInputSettings());
            deviceSettings->getRemoteInputSettings()->fromJsonObject(settingsJsonObject);
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

        if (deviceActionsKey == "airspyActions")
        {
            deviceActions->setAirspyActions(new SWGSDRangel::SWGAirspyActions());
            deviceActions->getAirspyActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "airspyHFActions")
        {
            deviceActions->setAirspyHfActions(new SWGSDRangel::SWGAirspyHFActions());
            deviceActions->getAirspyHfActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "bladeRF1InputActions")
        {
            deviceActions->setBladeRf1InputActions(new SWGSDRangel::SWGBladeRF1InputActions());
            deviceActions->getBladeRf1InputActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "fcdProActions")
        {
            deviceActions->setFcdProActions(new SWGSDRangel::SWGFCDProActions());
            deviceActions->getFcdProActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "fcdProPlusActions")
        {
            deviceActions->setFcdProPlusActions(new SWGSDRangel::SWGFCDProPlusActions());
            deviceActions->getFcdProPlusActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "hackRFInputActions")
        {
            deviceActions->setHackRfInputActions(new SWGSDRangel::SWGHackRFInputActions());
            deviceActions->getHackRfInputActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "kiwiSDRActions")
        {
            deviceActions->setKiwiSdrActions(new SWGSDRangel::SWGKiwiSDRActions());
            deviceActions->getKiwiSdrActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "limeSdrInputActions")
        {
            deviceActions->setLimeSdrInputActions(new SWGSDRangel::SWGLimeSdrInputActions());
            deviceActions->getLimeSdrInputActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "localInputActions")
        {
            deviceActions->setLocalInputActions(new SWGSDRangel::SWGLocalInputActions());
            deviceActions->getLocalInputActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "perseusActions")
        {
            deviceActions->setPerseusActions(new SWGSDRangel::SWGPerseusActions());
            deviceActions->getPerseusActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "plutoSdrInputActions")
        {
            deviceActions->setPlutoSdrInputActions(new SWGSDRangel::SWGPlutoSdrInputActions());
            deviceActions->getPlutoSdrInputActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "remoteInputActions")
        {
            deviceActions->setRemoteInputActions(new SWGSDRangel::SWGRemoteInputActions());
            deviceActions->getRemoteInputActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "rtlSdrActions")
        {
            deviceActions->setRtlSdrActions(new SWGSDRangel::SWGRtlSdrActions());
            deviceActions->getRtlSdrActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "sdrPlayActions")
        {
            deviceActions->setSdrPlayActions(new SWGSDRangel::SWGSDRPlayActions());
            deviceActions->getSdrPlayActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "soapySDRInputActions")
        {
            deviceActions->setSoapySdrInputActions(new SWGSDRangel::SWGSoapySDRInputActions());
            deviceActions->getSoapySdrInputActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "testSourceActions")
        {
            deviceActions->setTestSourceActions(new SWGSDRangel::SWGTestSourceActions());
            deviceActions->getTestSourceActions()->fromJsonObject(actionsJsonObject);
        }
        else if (deviceActionsKey == "xtrxInputActions")
        {
            deviceActions->setXtrxInputActions(new SWGSDRangel::SWGXtrxInputActions());
            deviceActions->getXtrxInputActions()->fromJsonObject(actionsJsonObject);
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

void WebAPIRequestMapper::appendSettingsArrayKeys(
        const QJsonObject& parentSettingsJsonObject,
        const QString& parentKey,
        QStringList& keyList)
{
    QJsonArray arrayJson = parentSettingsJsonObject[parentKey].toArray();

    for (int arrayIndex = 0; arrayIndex < arrayJson.count(); arrayIndex++)
    {
        QJsonValue v = arrayJson.at(arrayIndex);

        if (v.isObject())
        {
            QJsonObject itemSettingsJsonObject = v.toObject();
            QStringList itemSettingsKeys = itemSettingsJsonObject.keys();
            keyList.append(tr("%1[%2]").arg(parentKey).arg(arrayIndex));

            for (int i = 0; i < itemSettingsKeys.size(); i++) {
                keyList.append(tr("%1[%2].%3").arg(parentKey).arg(arrayIndex).arg(itemSettingsKeys[i]));
            }
        }
    }
}

void WebAPIRequestMapper::resetDeviceSettings(SWGSDRangel::SWGDeviceSettings& deviceSettings)
{
    deviceSettings.cleanup();
    deviceSettings.setDeviceHwType(nullptr);
    deviceSettings.setAirspySettings(nullptr);
    deviceSettings.setAirspyHfSettings(nullptr);
    deviceSettings.setBladeRf1InputSettings(nullptr);
    deviceSettings.setBladeRf1OutputSettings(nullptr);
    deviceSettings.setFcdProPlusSettings(nullptr);
    deviceSettings.setFcdProSettings(nullptr);
    deviceSettings.setFileInputSettings(nullptr);
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
    deviceSettings.setSdrPlaySettings(nullptr);
    deviceSettings.setTestSourceSettings(nullptr);
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
    deviceReport.setSdrPlayReport(nullptr);
}

void WebAPIRequestMapper::resetDeviceActions(SWGSDRangel::SWGDeviceActions& deviceActions)
{
    deviceActions.cleanup();
    deviceActions.setDeviceHwType(nullptr);
    deviceActions.setAirspyActions(nullptr);
    deviceActions.setAirspyHfActions(nullptr);
    deviceActions.setBladeRf1InputActions(nullptr);
    deviceActions.setFcdProActions(nullptr);
    deviceActions.setFcdProPlusActions(nullptr);
    deviceActions.setHackRfInputActions(nullptr);
    deviceActions.setKiwiSdrActions(nullptr);
    deviceActions.setLimeSdrInputActions(nullptr);
    deviceActions.setLocalInputActions(nullptr);
    deviceActions.setPerseusActions(nullptr);
    deviceActions.setPlutoSdrInputActions(nullptr);
    deviceActions.setRemoteInputActions(nullptr);
    deviceActions.setRtlSdrActions(nullptr);
    deviceActions.setSdrPlayActions(nullptr);
    deviceActions.setSoapySdrInputActions(nullptr);
    deviceActions.setTestSourceActions(nullptr);
    deviceActions.setXtrxInputActions(nullptr);
}

void WebAPIRequestMapper::resetChannelSettings(SWGSDRangel::SWGChannelSettings& channelSettings)
{
    channelSettings.cleanup();
    channelSettings.setChannelType(nullptr);
    channelSettings.setAmDemodSettings(nullptr);
    channelSettings.setAmModSettings(nullptr);
    channelSettings.setAtvModSettings(nullptr);
    channelSettings.setBfmDemodSettings(nullptr);
    channelSettings.setDsdDemodSettings(nullptr);
    channelSettings.setNfmDemodSettings(nullptr);
    channelSettings.setNfmModSettings(nullptr);
    channelSettings.setRemoteSinkSettings(nullptr);
    channelSettings.setRemoteSourceSettings(nullptr);
    channelSettings.setSsbDemodSettings(nullptr);
    channelSettings.setSsbModSettings(nullptr);
    channelSettings.setUdpSourceSettings(nullptr);
    channelSettings.setUdpSinkSettings(nullptr);
    channelSettings.setWfmDemodSettings(nullptr);
    channelSettings.setWfmModSettings(nullptr);
}

void WebAPIRequestMapper::resetChannelReport(SWGSDRangel::SWGChannelReport& channelReport)
{
    channelReport.cleanup();
    channelReport.setChannelType(nullptr);
    channelReport.setAmDemodReport(nullptr);
    channelReport.setAmModReport(nullptr);
    channelReport.setAtvModReport(nullptr);
    channelReport.setBfmDemodReport(nullptr);
    channelReport.setDsdDemodReport(nullptr);
    channelReport.setNfmDemodReport(nullptr);
    channelReport.setNfmModReport(nullptr);
    channelReport.setRemoteSourceReport(nullptr);
    channelReport.setSsbDemodReport(nullptr);
    channelReport.setSsbModReport(nullptr);
    channelReport.setUdpSourceReport(nullptr);
    channelReport.setUdpSinkReport(nullptr);
    channelReport.setWfmDemodReport(nullptr);
    channelReport.setWfmModReport(nullptr);
}

void WebAPIRequestMapper::resetChannelActions(SWGSDRangel::SWGChannelActions& channelActions)
{
    channelActions.cleanup();
    channelActions.setChannelType(nullptr);
    channelActions.setFileSourceActions(nullptr);
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

void WebAPIRequestMapper::processChannelAnalyzerSettings(
        SWGSDRangel::SWGChannelSettings *channelSettings,
        const QJsonObject& channelSettingsJson,
        QStringList& channelSettingsKeys
)
{
    SWGSDRangel::SWGChannelAnalyzerSettings *channelAnalyzerSettings = new SWGSDRangel::SWGChannelAnalyzerSettings();
    channelSettings->setChannelAnalyzerSettings(channelAnalyzerSettings);
    channelAnalyzerSettings->init();

    if (channelSettingsJson.contains("bandwidth")) {
        channelAnalyzerSettings->setBandwidth(channelSettingsJson["bandwidth"].toInt());
    }
    if (channelSettingsJson.contains("downSample")) {
        channelAnalyzerSettings->setDownSample(channelSettingsJson["downSample"].toInt());
    }
    if (channelSettingsJson.contains("downSampleRate")) {
        channelAnalyzerSettings->setDownSampleRate(channelSettingsJson["downSampleRate"].toInt());
    }
    if (channelSettingsJson.contains("fll")) {
        channelAnalyzerSettings->setFll(channelSettingsJson["fll"].toInt());
    }
    if (channelSettingsJson.contains("frequency")) {
        channelAnalyzerSettings->setFrequency(channelSettingsJson["frequency"].toInt());
    }
    if (channelSettingsJson.contains("inputType")) {
        channelAnalyzerSettings->setInputType(channelSettingsJson["inputType"].toInt());
    }
    if (channelSettingsJson.contains("lowCutoff")) {
        channelAnalyzerSettings->setLowCutoff(channelSettingsJson["lowCutoff"].toInt());
    }
    if (channelSettingsJson.contains("pll")) {
        channelAnalyzerSettings->setPll(channelSettingsJson["pll"].toInt());
    }
    if (channelSettingsJson.contains("pllPskOrder")) {
        channelAnalyzerSettings->setPllPskOrder(channelSettingsJson["pllPskOrder"].toInt());
    }
    if (channelSettingsJson.contains("rgbColor")) {
        channelAnalyzerSettings->setRgbColor(channelSettingsJson["rgbColor"].toInt());
    }
    if (channelSettingsJson.contains("rrc")) {
        channelAnalyzerSettings->setRrc(channelSettingsJson["rrc"].toInt());
    }
    if (channelSettingsJson.contains("rrcRolloff")) {
        channelAnalyzerSettings->setRrcRolloff(channelSettingsJson["rrcRolloff"].toInt());
    }
    if (channelSettingsJson.contains("spanLog2")) {
        channelAnalyzerSettings->setSpanLog2(channelSettingsJson["spanLog2"].toInt());
    }
    if (channelSettingsJson.contains("ssb")) {
        channelAnalyzerSettings->setSsb(channelSettingsJson["ssb"].toInt());
    }
    if (channelSettingsJson.contains("title")) {
        channelAnalyzerSettings->setTitle(new QString(channelSettingsJson["title"].toString()));
    }

    if (channelSettingsJson.contains("spectrumConfig"))
    {
        SWGSDRangel::SWGGLSpectrum *spectrum = new SWGSDRangel::SWGGLSpectrum();
        spectrum->init();
        channelAnalyzerSettings->setSpectrumConfig(spectrum);
        QJsonObject spectrumJson;
        appendSettingsSubKeys(channelSettingsJson, spectrumJson, "spectrumConfig", channelSettingsKeys);
        spectrum->fromJsonObject(spectrumJson);
    }

    if (channelSettingsJson.contains("scopeConfig") && channelSettingsJson["scopeConfig"].isObject())
    {
        SWGSDRangel::SWGGLScope *scopeConfig = new SWGSDRangel::SWGGLScope();
        scopeConfig->init();
        channelAnalyzerSettings->setScopeConfig(scopeConfig);
        QJsonObject scopeConfigJson;
        appendSettingsSubKeys(channelSettingsJson, scopeConfigJson, "scopeConfig", channelSettingsKeys);
        scopeConfig->fromJsonObject(scopeConfigJson);

        if (scopeConfigJson.contains("tracesData") && scopeConfigJson["tracesData"].isArray())
        {
            QList<SWGSDRangel::SWGTraceData *> *tracesData = new QList<SWGSDRangel::SWGTraceData *>();
            scopeConfig->setTracesData(tracesData);
            QJsonArray tracesJson = scopeConfigJson["tracesData"].toArray();

            for (int i = 0; i < tracesJson.size(); i++)
            {
                SWGSDRangel::SWGTraceData *traceData = new SWGSDRangel::SWGTraceData();
                tracesData->append(traceData);
                QJsonObject traceJson = tracesJson.at(i).toObject();
                traceData->fromJsonObject(traceJson);
            }

            QStringList tracesDataKeys;
            appendSettingsArrayKeys(scopeConfigJson, "tracesData", tracesDataKeys);

            for (int i = 0; i < tracesDataKeys.size(); i++) {
                channelSettingsKeys.append(QString("scopeConfig.") + tracesDataKeys.at(i));
            }
        }

        if (scopeConfigJson.contains("triggersData") && scopeConfigJson["triggersData"].isArray())
        {
            QList<SWGSDRangel::SWGTriggerData *> *triggersData = new QList<SWGSDRangel::SWGTriggerData *>();
            scopeConfig->setTriggersData(triggersData);
            QJsonArray triggersJson = scopeConfigJson["triggersData"].toArray();

            for (int i = 0; i < triggersJson.size(); i++)
            {
                SWGSDRangel::SWGTriggerData *triggerData = new SWGSDRangel::SWGTriggerData();
                triggersData->append(triggerData);
                QJsonObject triggerJson = triggersJson.at(i).toObject();
                triggerData->fromJsonObject(triggerJson);
            }

            QStringList triggersDataKeys;
            appendSettingsArrayKeys(scopeConfigJson, "triggersData", triggersDataKeys);

            for (int i = 0; i < triggersDataKeys.size(); i++) {
                channelSettingsKeys.append(QString("scopeConfig.") + triggersDataKeys.at(i));
            }
        }
    }
}

void WebAPIRequestMapper::processSoapySDRSettings(
        SWGSDRangel::SWGDeviceSettings *deviceSettings,
        QJsonObject& deviceSettingsJson,
        QStringList& deviceSettingsKeys,
        bool inputElseOutput
)
{
    if (inputElseOutput)
    {
        SWGSDRangel::SWGSoapySDRInputSettings *swgSoapySDRInputSettings = new SWGSDRangel::SWGSoapySDRInputSettings();
        deviceSettings->setSoapySdrInputSettings(swgSoapySDRInputSettings);
        swgSoapySDRInputSettings->init();

        if (deviceSettingsJson.contains("centerFrequency")) {
            swgSoapySDRInputSettings->setCenterFrequency(deviceSettingsJson["centerFrequency"].toInt());
        }
        if (deviceSettingsJson.contains("LOppmTenths")) {
            swgSoapySDRInputSettings->setLOppmTenths(deviceSettingsJson["LOppmTenths"].toInt());
        }
        if (deviceSettingsJson.contains("devSampleRate")) {
            swgSoapySDRInputSettings->setDevSampleRate(deviceSettingsJson["devSampleRate"].toInt());
        }
        if (deviceSettingsJson.contains("log2Decim")) {
            swgSoapySDRInputSettings->setLog2Decim(deviceSettingsJson["log2Decim"].toInt());
        }
        if (deviceSettingsJson.contains("fcPos")) {
            swgSoapySDRInputSettings->setFcPos(deviceSettingsJson["fcPos"].toInt());
        }
        if (deviceSettingsJson.contains("softDCCorrection")) {
            swgSoapySDRInputSettings->setSoftDcCorrection(deviceSettingsJson["softDCCorrection"].toInt());
        }
        if (deviceSettingsJson.contains("softIQCorrection")) {
            swgSoapySDRInputSettings->setSoftIqCorrection(deviceSettingsJson["softIQCorrection"].toInt());
        }
        if (deviceSettingsJson.contains("transverterMode")) {
            swgSoapySDRInputSettings->setTransverterMode(deviceSettingsJson["transverterMode"].toInt());
        }
        if (deviceSettingsJson.contains("transverterDeltaFrequency")) {
            swgSoapySDRInputSettings->setTransverterDeltaFrequency(deviceSettingsJson["transverterDeltaFrequency"].toInt());
        }
        if (deviceSettingsJson.contains("fileRecordName")) {
            swgSoapySDRInputSettings->setFileRecordName(new QString(deviceSettingsJson["fileRecordName"].toString()));
        }
        if (deviceSettingsJson.contains("antenna")) {
            swgSoapySDRInputSettings->setAntenna(new QString(deviceSettingsJson["antenna"].toString()));
        }
        if (deviceSettingsJson.contains("bandwidth")) {
            swgSoapySDRInputSettings->setBandwidth(deviceSettingsJson["bandwidth"].toInt());
        }
        if (deviceSettingsJson.contains("globalGain")) {
            swgSoapySDRInputSettings->setGlobalGain(deviceSettingsJson["globalGain"].toInt());
        }
        if (deviceSettingsJson.contains("autoGain")) {
            swgSoapySDRInputSettings->setAutoGain(deviceSettingsJson["autoGain"].toInt());
        }
        if (deviceSettingsJson.contains("autoDCCorrection")) {
            swgSoapySDRInputSettings->setAutoDcCorrection(deviceSettingsJson["autoDCCorrection"].toInt());
        }
        if (deviceSettingsJson.contains("autoIQCorrection")) {
            swgSoapySDRInputSettings->setAutoIqCorrection(deviceSettingsJson["autoIQCorrection"].toInt());
        }
        if (deviceSettingsJson.contains("dcCorrection"))
        {
            SWGSDRangel::SWGComplex *swgComplex = new SWGSDRangel::SWGComplex;
            swgSoapySDRInputSettings->setDcCorrection(swgComplex);
            QJsonObject complexJson = deviceSettingsJson["dcCorrection"].toObject();

            if (complexJson.contains("real")) {
                swgComplex->setReal(complexJson["real"].toDouble());
            }
            if (complexJson.contains("imag")) {
                swgComplex->setImag(complexJson["imag"].toDouble());
            }
        }
        if (deviceSettingsJson.contains("iqCorrection"))
        {
            SWGSDRangel::SWGComplex *swgComplex = new SWGSDRangel::SWGComplex;
            swgSoapySDRInputSettings->setIqCorrection(swgComplex);
            QJsonObject complexJson = deviceSettingsJson["iqCorrection"].toObject();

            if (complexJson.contains("real")) {
                swgComplex->setReal(complexJson["real"].toDouble());
            }
            if (complexJson.contains("imag")) {
                swgComplex->setImag(complexJson["imag"].toDouble());
            }
        }
        if (deviceSettingsJson.contains("useReverseAPI")) {
            swgSoapySDRInputSettings->setUseReverseApi(deviceSettingsJson["useReverseAPI"].toInt());
        }
        if (deviceSettingsJson.contains("reverseAPIAddress")) {
            swgSoapySDRInputSettings->setReverseApiAddress(new QString(deviceSettingsJson["reverseAPIAddress"].toString()));
        }
        if (deviceSettingsJson.contains("reverseAPIPort")) {
            swgSoapySDRInputSettings->setReverseApiPort(deviceSettingsJson["reverseAPIPort"].toInt());
        }
        if (deviceSettingsJson.contains("reverseAPIDeviceIndex")) {
            swgSoapySDRInputSettings->setReverseApiDeviceIndex(deviceSettingsJson["reverseAPIDeviceIndex"].toInt());
        }
    }
    else
    {
        SWGSDRangel::SWGSoapySDROutputSettings *swgSoapySDROutputSettings = new SWGSDRangel::SWGSoapySDROutputSettings();
        deviceSettings->setSoapySdrOutputSettings(swgSoapySDROutputSettings);
        swgSoapySDROutputSettings->init();

        if (deviceSettingsJson.contains("centerFrequency")) {
            swgSoapySDROutputSettings->setCenterFrequency(deviceSettingsJson["centerFrequency"].toInt());
        }
        if (deviceSettingsJson.contains("LOppmTenths")) {
            swgSoapySDROutputSettings->setLOppmTenths(deviceSettingsJson["LOppmTenths"].toInt());
        }
        if (deviceSettingsJson.contains("devSampleRate")) {
            swgSoapySDROutputSettings->setDevSampleRate(deviceSettingsJson["devSampleRate"].toInt());
        }
        if (deviceSettingsJson.contains("log2Interp")) {
            swgSoapySDROutputSettings->setLog2Interp(deviceSettingsJson["log2Interp"].toInt());
        }
        if (deviceSettingsJson.contains("transverterMode")) {
            swgSoapySDROutputSettings->setTransverterMode(deviceSettingsJson["transverterMode"].toInt());
        }
        if (deviceSettingsJson.contains("transverterDeltaFrequency")) {
            swgSoapySDROutputSettings->setTransverterDeltaFrequency(deviceSettingsJson["transverterDeltaFrequency"].toInt());
        }
        if (deviceSettingsJson.contains("antenna")) {
            swgSoapySDROutputSettings->setAntenna(new QString(deviceSettingsJson["antenna"].toString()));
        }
        if (deviceSettingsJson.contains("bandwidth")) {
            swgSoapySDROutputSettings->setBandwidth(deviceSettingsJson["bandwidth"].toInt());
        }
        if (deviceSettingsJson.contains("globalGain")) {
            swgSoapySDROutputSettings->setGlobalGain(deviceSettingsJson["globalGain"].toInt());
        }
        if (deviceSettingsJson.contains("autoGain")) {
            swgSoapySDROutputSettings->setAutoGain(deviceSettingsJson["autoGain"].toInt());
        }
        if (deviceSettingsJson.contains("autoDCCorrection")) {
            swgSoapySDROutputSettings->setAutoDcCorrection(deviceSettingsJson["autoDCCorrection"].toInt());
        }
        if (deviceSettingsJson.contains("autoIQCorrection")) {
            swgSoapySDROutputSettings->setAutoIqCorrection(deviceSettingsJson["autoIQCorrection"].toInt());
        }
        if (deviceSettingsJson.contains("dcCorrection"))
        {
            SWGSDRangel::SWGComplex *swgComplex = new SWGSDRangel::SWGComplex;
            swgSoapySDROutputSettings->setDcCorrection(swgComplex);
            QJsonObject complexJson = deviceSettingsJson["dcCorrection"].toObject();

            if (complexJson.contains("real")) {
                swgComplex->setReal(complexJson["real"].toDouble());
            }
            if (complexJson.contains("imag")) {
                swgComplex->setImag(complexJson["imag"].toDouble());
            }
        }
        if (deviceSettingsJson.contains("iqCorrection"))
        {
            SWGSDRangel::SWGComplex *swgComplex = new SWGSDRangel::SWGComplex;
            swgSoapySDROutputSettings->setIqCorrection(swgComplex);
            QJsonObject complexJson = deviceSettingsJson["iqCorrection"].toObject();

            if (complexJson.contains("real")) {
                swgComplex->setReal(complexJson["real"].toDouble());
            }
            if (complexJson.contains("imag")) {
                swgComplex->setImag(complexJson["imag"].toDouble());
            }
        }
        if (deviceSettingsJson.contains("useReverseAPI")) {
            swgSoapySDROutputSettings->setUseReverseApi(deviceSettingsJson["useReverseAPI"].toInt());
        }
        if (deviceSettingsJson.contains("reverseAPIAddress")) {
            swgSoapySDROutputSettings->setReverseApiAddress(new QString(deviceSettingsJson["reverseAPIAddress"].toString()));
        }
        if (deviceSettingsJson.contains("reverseAPIPort")) {
            swgSoapySDROutputSettings->setReverseApiPort(deviceSettingsJson["reverseAPIPort"].toInt());
        }
        if (deviceSettingsJson.contains("reverseAPIDeviceIndex")) {
            swgSoapySDROutputSettings->setReverseApiDeviceIndex(deviceSettingsJson["reverseAPIDeviceIndex"].toInt());
        }
    }

    if (deviceSettingsKeys.contains("deviceArgSettings"))
    {
        QList<SWGSDRangel::SWGArgValue *> *swgArgSettings = new QList<SWGSDRangel::SWGArgValue *>;
        QJsonArray argsJson = deviceSettingsJson["deviceArgSettings"].toArray();

        if (inputElseOutput) {
            deviceSettings->getSoapySdrInputSettings()->setDeviceArgSettings(swgArgSettings);
        } else {
            deviceSettings->getSoapySdrOutputSettings()->setDeviceArgSettings(swgArgSettings);
        }

        for (int i = 0; i < argsJson.count(); i++)
        {
            SWGSDRangel::SWGArgValue *argValue = new SWGSDRangel::SWGArgValue();
            swgArgSettings->append(argValue);
            QJsonObject argValueJson = argsJson.at(i).toObject();
            argValue->fromJsonObject(argValueJson);
        }

        appendSettingsArrayKeys(deviceSettingsJson, "deviceArgSettings", deviceSettingsKeys);
    }

    if (deviceSettingsKeys.contains("individualGains"))
    {
        QList<SWGSDRangel::SWGArgValue *> *swgIndividualGains = new QList<SWGSDRangel::SWGArgValue *>;
        QJsonArray argsJson = deviceSettingsJson["individualGains"].toArray();

        if (inputElseOutput) {
            deviceSettings->getSoapySdrInputSettings()->setIndividualGains(swgIndividualGains);
        } else {
            deviceSettings->getSoapySdrOutputSettings()->setIndividualGains(swgIndividualGains);
        }

        for (int i = 0; i < argsJson.count(); i++)
        {
            SWGSDRangel::SWGArgValue *argValue = new SWGSDRangel::SWGArgValue();
            swgIndividualGains->append(argValue);
            QJsonObject argValueJson = argsJson.at(i).toObject();
            argValue->fromJsonObject(argValueJson);
        }

        appendSettingsArrayKeys(deviceSettingsJson, "individualGains", deviceSettingsKeys);
    }

    if (deviceSettingsKeys.contains("streamArgSettings"))
    {
        QList<SWGSDRangel::SWGArgValue *> *swgStreamArgSettings = new QList<SWGSDRangel::SWGArgValue *>;
        QJsonArray argsJson = deviceSettingsJson["streamArgSettings"].toArray();

        if (inputElseOutput) {
            deviceSettings->getSoapySdrInputSettings()->setStreamArgSettings(swgStreamArgSettings);
        } else {
            deviceSettings->getSoapySdrOutputSettings()->setStreamArgSettings(swgStreamArgSettings);
        }

        for (int i = 0; i < argsJson.count(); i++)
        {
            SWGSDRangel::SWGArgValue *argValue = new SWGSDRangel::SWGArgValue();
            swgStreamArgSettings->append(argValue);
            QJsonObject argValueJson = argsJson.at(i).toObject();
            argValue->fromJsonObject(argValueJson);
        }

        appendSettingsArrayKeys(deviceSettingsJson, "streamArgSettings", deviceSettingsKeys);
    }

    if (deviceSettingsKeys.contains("tunableElements"))
    {
        QList<SWGSDRangel::SWGArgValue *> *swgTunableElements = new QList<SWGSDRangel::SWGArgValue *>;
        QJsonArray argsJson = deviceSettingsJson["tunableElements"].toArray();

        if (inputElseOutput) {
            deviceSettings->getSoapySdrInputSettings()->setTunableElements(swgTunableElements);
        } else {
            deviceSettings->getSoapySdrOutputSettings()->setTunableElements(swgTunableElements);
        }

        for (int i = 0; i < argsJson.count(); i++)
        {
            SWGSDRangel::SWGArgValue *argValue = new SWGSDRangel::SWGArgValue();
            swgTunableElements->append(argValue);
            QJsonObject argValueJson = argsJson.at(i).toObject();
            argValue->fromJsonObject(argValueJson);
        }

        appendSettingsArrayKeys(deviceSettingsJson, "tunableElements", deviceSettingsKeys);
    }
}