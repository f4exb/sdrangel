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

#ifndef SDRBASE_WEBAPI_WEBAPIREQUESTMAPPER_H_
#define SDRBASE_WEBAPI_WEBAPIREQUESTMAPPER_H_

#include <QJsonParseError>

#include "httprequesthandler.h"
#include "httprequest.h"
#include "httpresponse.h"
#include "staticfilecontroller.h"
#include "webapiadapterinterface.h"

namespace SWGSDRangel
{
    class SWGPresetTransfer;
    class SWGPresetIdentifier;
}

class WebAPIRequestMapper : public qtwebapp::HttpRequestHandler {
    Q_OBJECT
public:
    WebAPIRequestMapper(QObject* parent=0);
    ~WebAPIRequestMapper();
    void service(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void setAdapter(WebAPIAdapterInterface *adapter) { m_adapter = adapter; }

private:
    WebAPIAdapterInterface *m_adapter;
    qtwebapp::StaticFileController *m_staticFileController;

    void instanceSummaryService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceDevicesService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceChannelsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceLoggingService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceAudioService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceLocationService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceDVSerialService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instancePresetsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instancePresetService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instancePresetFileService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceDeviceSetsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void instanceDeviceSetService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);

    void devicesetService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetFocusService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetDeviceService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetDeviceSettingsService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetDeviceRunService(const std::string& indexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetChannelService(const std::string& deviceSetIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetChannelIndexService(const std::string& deviceSetIndexStr, const std::string& channelIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void devicesetChannelSettingsService(const std::string& deviceSetIndexStr, const std::string& channelIndexStr, qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);

    bool validatePresetTransfer(SWGSDRangel::SWGPresetTransfer& presetTransfer);
    bool validatePresetIdentifer(SWGSDRangel::SWGPresetIdentifier& presetIdentifier);
    bool validatePresetExport(SWGSDRangel::SWGPresetExport& presetExport);
    bool validateDeviceListItem(SWGSDRangel::SWGDeviceListItem& deviceListItem, QJsonObject& jsonObject);
    bool validateDeviceSettings(SWGSDRangel::SWGDeviceSettings& deviceSettings, QJsonObject& jsonObject, QStringList& deviceSettingsKeys);
    bool validateChannelSettings(SWGSDRangel::SWGChannelSettings& deviceSettings, QJsonObject& jsonObject, QStringList& channelSettingsKeys);

    void appendSettingsSubKeys(
            const QJsonObject& parentSettingsJsonObject,
            QJsonObject& childSettingsJsonObject,
            const QString& parentKey,
            QStringList& keyList);

    bool parseJsonBody(QString& jsonStr, QJsonObject& jsonObject, qtwebapp::HttpResponse& response);

    void resetDeviceSettings(SWGSDRangel::SWGDeviceSettings& deviceSettings);
    void resetChannelSettings(SWGSDRangel::SWGChannelSettings& deviceSettings);
};

#endif /* SDRBASE_WEBAPI_WEBAPIREQUESTMAPPER_H_ */
