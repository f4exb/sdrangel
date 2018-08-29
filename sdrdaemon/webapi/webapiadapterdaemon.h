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

#ifndef SDRDAEMON_WEBAPI_WEBAPIADAPTERDAEMON_H_
#define SDRDAEMON_WEBAPI_WEBAPIADAPTERDAEMON_H_

#include <QtGlobal>
#include <QStringList>

namespace SWGSDRangel
{
    class SWGDaemonSummaryResponse;
    class SWGDeviceSet;
    class SWGDeviceListItem;
    class SWGDeviceSettings;
    class SWGDeviceState;
    class SWGDeviceReport;
    class SWGChannelReport;
    class SWGSuccessResponse;
    class SWGErrorResponse;
    class SWGLoggingInfo;
    class SWGChannelSettings;
}

class SDRDaemonMain;

class WebAPIAdapterDaemon
{
public:
    WebAPIAdapterDaemon(SDRDaemonMain& sdrDaemonMain);
    ~WebAPIAdapterDaemon();

    int daemonInstanceSummary(
            SWGSDRangel::SWGDaemonSummaryResponse& response,
            SWGSDRangel::SWGErrorResponse& error);

    int daemonInstanceLoggingGet(
            SWGSDRangel::SWGLoggingInfo& response,
            SWGSDRangel::SWGErrorResponse& error);

    int daemonInstanceLoggingPut(
            SWGSDRangel::SWGLoggingInfo& query,
            SWGSDRangel::SWGLoggingInfo& response,
            SWGSDRangel::SWGErrorResponse& error);

    int daemonChannelSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            SWGSDRangel::SWGErrorResponse& error);

    int daemonChannelSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            SWGSDRangel::SWGErrorResponse& error);

    int daemonDeviceSettingsGet(
            SWGSDRangel::SWGDeviceSettings& response,
            SWGSDRangel::SWGErrorResponse& error);

    int daemonDeviceSettingsPutPatch(
            bool force,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response,
            SWGSDRangel::SWGErrorResponse& error);

    int daemonRunGet(
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error);

    int daemonRunPost(
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error);

    int daemonRunDelete(
            SWGSDRangel::SWGDeviceState& response,
            SWGSDRangel::SWGErrorResponse& error);

    int daemonDeviceReportGet(
            SWGSDRangel::SWGDeviceReport& response,
            SWGSDRangel::SWGErrorResponse& error);

    int daemonChannelReportGet(
            SWGSDRangel::SWGChannelReport& response,
            SWGSDRangel::SWGErrorResponse& error);

    static QString daemonInstanceSummaryURL;
    static QString daemonInstanceLoggingURL;
    static QString daemonChannelSettingsURL;
    static QString daemonDeviceSettingsURL;
    static QString daemonDeviceReportURL;
    static QString daemonChannelReportURL;
    static QString daemonRunURL;

private:
    SDRDaemonMain& m_sdrDaemonMain;

    static QtMsgType getMsgTypeFromString(const QString& msgTypeString);
    static void getMsgTypeString(const QtMsgType& msgType, QString& level);
};

#endif /* SDRDAEMON_WEBAPI_WEBAPIADAPTERDAEMON_H_ */
