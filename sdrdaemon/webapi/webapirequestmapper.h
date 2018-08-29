///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
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

#ifndef SDRDAEMON_WEBAPI_WEBAPIREQUESTMAPPER_H_
#define SDRDAEMON_WEBAPI_WEBAPIREQUESTMAPPER_H_

#include <QJsonParseError>

#include "httprequesthandler.h"
#include "httprequest.h"
#include "httpresponse.h"
#include "staticfilecontroller.h"

#include "export.h"

namespace SWGSDRangel
{
    class SWGChannelSettings;
    class SWGChannelReport;
    class SWGDeviceSettings;
    class SWGDeviceReport;
}

class WebAPIAdapterDaemon;

namespace SDRDaemon
{

class SDRBASE_API WebAPIRequestMapper : public qtwebapp::HttpRequestHandler {
    Q_OBJECT
public:
    WebAPIRequestMapper(QObject* parent=0);
    ~WebAPIRequestMapper();
    void service(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void setAdapter(WebAPIAdapterDaemon *adapter) { m_adapter = adapter; }

private:
    WebAPIAdapterDaemon *m_adapter;
    qtwebapp::StaticFileController *m_staticFileController;

    void daemonInstanceSummaryService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void daemonInstanceLoggingService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void daemonChannelSettingsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void daemonDeviceSettingsService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void daemonRunService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void daemonDeviceReportService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void daemonChannelReportService(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);

    bool validateChannelSettings(SWGSDRangel::SWGChannelSettings& channelSettings, QJsonObject& jsonObject, QStringList& channelSettingsKeys);
    bool validateDeviceSettings(SWGSDRangel::SWGDeviceSettings& deviceSettings, QJsonObject& jsonObject, QStringList& deviceSettingsKeys);

    void appendSettingsSubKeys(
            const QJsonObject& parentSettingsJsonObject,
            QJsonObject& childSettingsJsonObject,
            const QString& parentKey,
            QStringList& keyList);

    bool parseJsonBody(QString& jsonStr, QJsonObject& jsonObject, qtwebapp::HttpResponse& response);

    void resetChannelSettings(SWGSDRangel::SWGChannelSettings& channelSettings);
    void resetChannelReport(SWGSDRangel::SWGChannelReport& deviceSettings);
    void resetDeviceSettings(SWGSDRangel::SWGDeviceSettings& deviceSettings);
    void resetDeviceReport(SWGSDRangel::SWGDeviceReport& deviceReport);
};

} // namespace SDRdaemon

#endif /* SDRDAEMON_WEBAPI_WEBAPIREQUESTMAPPER_H_ */
