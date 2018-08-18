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

#include "SWGDaemonSummaryResponse.h"
#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGErrorResponse.h"

#include "webapiadapterdaemon.h"

QString WebAPIAdapterDaemon::daemonInstanceSummaryURL = "/sdrdaemon";
QString WebAPIAdapterDaemon::daemonInstanceLoggingURL = "/sdrdaemon/logging";
QString WebAPIAdapterDaemon::daemonSettingsURL = "/sdrdaemon/settings";
QString WebAPIAdapterDaemon::daemonReportURL = "/sdrdaemon/report";
QString WebAPIAdapterDaemon::daemonRunURL = "/sdrdaemon/run";

WebAPIAdapterDaemon::WebAPIAdapterDaemon(SDRDaemonMain& sdrDaemonMain) :
    m_sdrDaemonMain(sdrDaemonMain)
{
}

WebAPIAdapterDaemon::~WebAPIAdapterDaemon()
{
}

int WebAPIAdapterDaemon::daemonInstanceSummary(
        SWGSDRangel::SWGDaemonSummaryResponse& response __attribute__((unused)),
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();
    *error.getMessage() = "Not implemented";
    return 501;
}

int WebAPIAdapterDaemon::daemonInstanceLoggingGet(
        SWGSDRangel::SWGLoggingInfo& response __attribute__((unused)),
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();
    *error.getMessage() = "Not implemented";
    return 501;
}

int WebAPIAdapterDaemon::daemonInstanceLoggingPut(
        SWGSDRangel::SWGLoggingInfo& query __attribute__((unused)),
        SWGSDRangel::SWGLoggingInfo& response __attribute__((unused)),
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();
    *error.getMessage() = "Not implemented";
    return 501;
}

int WebAPIAdapterDaemon::daemonSettingsGet(
        SWGSDRangel::SWGDeviceSettings& response __attribute__((unused)),
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();
    *error.getMessage() = "Not implemented";
    return 501;
}

int WebAPIAdapterDaemon::daemonSettingsPutPatch(
        bool force __attribute__((unused)),
        const QStringList& deviceSettingsKeys __attribute__((unused)),
        SWGSDRangel::SWGDeviceSettings& response __attribute__((unused)),
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();
    *error.getMessage() = "Not implemented";
    return 501;
}

int WebAPIAdapterDaemon::daemonRunGet(
        SWGSDRangel::SWGDeviceState& response __attribute__((unused)),
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();
    *error.getMessage() = "Not implemented";
    return 501;
}

int WebAPIAdapterDaemon::daemonRunPost(
        SWGSDRangel::SWGDeviceState& response __attribute__((unused)),
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();
    *error.getMessage() = "Not implemented";
    return 501;
}

int WebAPIAdapterDaemon::daemonRunDelete(
        SWGSDRangel::SWGDeviceState& response __attribute__((unused)),
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();
    *error.getMessage() = "Not implemented";
    return 501;
}

int WebAPIAdapterDaemon::daemonReportGet(
        SWGSDRangel::SWGDeviceReport& response __attribute__((unused)),
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();
    *error.getMessage() = "Not implemented";
    return 501;
}

// TODO: put in library in common with SDRangel. Can be static.
QtMsgType WebAPIAdapterDaemon::getMsgTypeFromString(const QString& msgTypeString)
{
    if (msgTypeString == "debug") {
        return QtDebugMsg;
    } else if (msgTypeString == "info") {
        return QtInfoMsg;
    } else if (msgTypeString == "warning") {
        return QtWarningMsg;
    } else if (msgTypeString == "error") {
        return QtCriticalMsg;
    } else {
        return QtDebugMsg;
    }
}

// TODO: put in library in common with SDRangel. Can be static.
void WebAPIAdapterDaemon::getMsgTypeString(const QtMsgType& msgType, QString& levelStr)
{
    switch (msgType)
    {
    case QtDebugMsg:
        levelStr = "debug";
        break;
    case QtInfoMsg:
        levelStr = "info";
        break;
    case QtWarningMsg:
        levelStr = "warning";
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        levelStr = "error";
        break;
    default:
        levelStr = "debug";
        break;
    }
}
