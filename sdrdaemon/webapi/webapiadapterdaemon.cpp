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

#include <QCoreApplication>

#include "SWGDaemonSummaryResponse.h"
#include "SWGLoggingInfo.h"
#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGChannelSettings.h"
#include "SWGErrorResponse.h"

#include "dsp/dsptypes.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "dsp/devicesamplesink.h"
#include "dsp/devicesamplesource.h"
#include "webapiadapterdaemon.h"
#include "sdrdaemonmain.h"
#include "loggerwithfile.h"

QString WebAPIAdapterDaemon::daemonInstanceSummaryURL = "/sdrdaemon";
QString WebAPIAdapterDaemon::daemonInstanceLoggingURL = "/sdrdaemon/logging";
QString WebAPIAdapterDaemon::daemonChannelSettingsURL = "/sdrdaemon/channel/settings";
QString WebAPIAdapterDaemon::daemonDeviceSettingsURL = "/sdrdaemon/device/settings";
QString WebAPIAdapterDaemon::daemonDeviceReportURL = "/sdrdaemon/device/report";
QString WebAPIAdapterDaemon::daemonRunURL = "/sdrdaemon/run";

WebAPIAdapterDaemon::WebAPIAdapterDaemon(SDRDaemonMain& sdrDaemonMain) :
    m_sdrDaemonMain(sdrDaemonMain)
{
}

WebAPIAdapterDaemon::~WebAPIAdapterDaemon()
{
}

int WebAPIAdapterDaemon::daemonInstanceSummary(
        SWGSDRangel::SWGDaemonSummaryResponse& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    response.init();
    *response.getAppname() = QCoreApplication::applicationName();
    *response.getVersion() = QCoreApplication::applicationVersion();
    *response.getQtVersion() = QString(QT_VERSION_STR);
    response.setDspRxBits(SDR_RX_SAMP_SZ);
    response.setDspTxBits(SDR_TX_SAMP_SZ);
    response.setPid(QCoreApplication::applicationPid());
#if QT_VERSION >= 0x050400
    *response.getArchitecture() = QString(QSysInfo::currentCpuArchitecture());
    *response.getOs() = QString(QSysInfo::prettyProductName());
#endif

    SWGSDRangel::SWGLoggingInfo *logging = response.getLogging();
    logging->init();
    logging->setDumpToFile(m_sdrDaemonMain.m_logger->getUseFileLogger() ? 1 : 0);

    if (logging->getDumpToFile()) {
        m_sdrDaemonMain.m_logger->getLogFileName(*logging->getFileName());
        m_sdrDaemonMain.m_logger->getFileMinMessageLevelStr(*logging->getFileLevel());
    }

    m_sdrDaemonMain.m_logger->getConsoleMinMessageLevelStr(*logging->getConsoleLevel());

    SWGSDRangel::SWGSamplingDevice *samplingDevice = response.getSamplingDevice();
    samplingDevice->setTx(m_sdrDaemonMain.m_tx ? 1 : 0);
    samplingDevice->setHwType(new QString(m_sdrDaemonMain.m_deviceType));
    samplingDevice->setIndex(0);

    if (m_sdrDaemonMain.m_tx)
    {
        QString state;
        m_sdrDaemonMain.m_deviceSinkAPI->getDeviceEngineStateStr(state);
        samplingDevice->setState(new QString(state));
        samplingDevice->setSerial(new QString(m_sdrDaemonMain.m_deviceSinkAPI->getSampleSinkSerial()));
        samplingDevice->setSequence(m_sdrDaemonMain.m_deviceSinkAPI->getSampleSinkSequence());
        samplingDevice->setNbStreams(m_sdrDaemonMain.m_deviceSinkAPI->getNbItems());
        samplingDevice->setStreamIndex(m_sdrDaemonMain.m_deviceSinkAPI->getItemIndex());
        DeviceSampleSink *sampleSink = m_sdrDaemonMain.m_deviceSinkEngine->getSink();

        if (sampleSink) {
            samplingDevice->setCenterFrequency(sampleSink->getCenterFrequency());
            samplingDevice->setBandwidth(sampleSink->getSampleRate());
        }
    }
    else
    {
        QString state;
        m_sdrDaemonMain.m_deviceSourceAPI->getDeviceEngineStateStr(state);
        samplingDevice->setState(new QString(state));
        samplingDevice->setSerial(new QString(m_sdrDaemonMain.m_deviceSourceAPI->getSampleSourceSerial()));
        samplingDevice->setSequence(m_sdrDaemonMain.m_deviceSourceAPI->getSampleSourceSequence());
        samplingDevice->setNbStreams(m_sdrDaemonMain.m_deviceSourceAPI->getNbItems());
        samplingDevice->setStreamIndex(m_sdrDaemonMain.m_deviceSourceAPI->getItemIndex());
        DeviceSampleSource *sampleSource = m_sdrDaemonMain.m_deviceSourceEngine->getSource();

        if (sampleSource) {
            samplingDevice->setCenterFrequency(sampleSource->getCenterFrequency());
            samplingDevice->setBandwidth(sampleSource->getSampleRate());
        }
    }

    return 200;
}

int WebAPIAdapterDaemon::daemonInstanceLoggingGet(
        SWGSDRangel::SWGLoggingInfo& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    response.init();
    response.setDumpToFile(m_sdrDaemonMain.m_logger->getUseFileLogger() ? 1 : 0);

    if (response.getDumpToFile()) {
        m_sdrDaemonMain.m_logger->getLogFileName(*response.getFileName());
        m_sdrDaemonMain.m_logger->getFileMinMessageLevelStr(*response.getFileLevel());
    }

    m_sdrDaemonMain.m_logger->getConsoleMinMessageLevelStr(*response.getConsoleLevel());

    return 200;
}

int WebAPIAdapterDaemon::daemonInstanceLoggingPut(
        SWGSDRangel::SWGLoggingInfo& query,
        SWGSDRangel::SWGLoggingInfo& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    // response input is the query actually
    bool dumpToFile = (query.getDumpToFile() != 0);
    QString* consoleLevel = query.getConsoleLevel();
    QString* fileLevel = query.getFileLevel();
    QString* fileName = query.getFileName();

    // perform actions
    if (consoleLevel) {
        m_sdrDaemonMain.m_settings.setConsoleMinLogLevel(getMsgTypeFromString(*consoleLevel));
    }

    if (fileLevel) {
        m_sdrDaemonMain.m_settings.setFileMinLogLevel(getMsgTypeFromString(*fileLevel));
    }

    m_sdrDaemonMain.m_settings.setUseLogFile(dumpToFile);

    if (fileName) {
        m_sdrDaemonMain.m_settings.setLogFileName(*fileName);
    }

    m_sdrDaemonMain.setLoggingOptions();

    // build response
    response.init();
    getMsgTypeString(m_sdrDaemonMain.m_settings.getConsoleMinLogLevel(), *response.getConsoleLevel());
    response.setDumpToFile(m_sdrDaemonMain.m_settings.getUseLogFile() ? 1 : 0);
    getMsgTypeString(m_sdrDaemonMain.m_settings.getFileMinLogLevel(), *response.getFileLevel());
    *response.getFileName() = m_sdrDaemonMain.m_settings.getLogFileName();

    return 200;
}

int WebAPIAdapterDaemon::daemonChannelSettingsGet(
        SWGSDRangel::SWGChannelSettings& response __attribute__((unused)),
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();
    *error.getMessage() = "Not implemented";
    return 501;
}

int WebAPIAdapterDaemon::daemonChannelSettingsPutPatch(
        bool force __attribute__((unused)),
        const QStringList& channelSettingsKeys __attribute__((unused)),
        SWGSDRangel::SWGChannelSettings& response __attribute__((unused)),
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();
    *error.getMessage() = "Not implemented";
    return 501;
}

int WebAPIAdapterDaemon::daemonDeviceSettingsGet(
        SWGSDRangel::SWGDeviceSettings& response __attribute__((unused)),
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if (m_sdrDaemonMain.m_deviceSourceEngine) // Rx
    {
        response.setDeviceHwType(new QString(m_sdrDaemonMain.m_deviceSourceAPI->getHardwareId()));
        response.setTx(0);
        DeviceSampleSource *source = m_sdrDaemonMain.m_deviceSourceAPI->getSampleSource();
        return source->webapiSettingsGet(response, *error.getMessage());
    }
    else if (m_sdrDaemonMain.m_deviceSinkEngine) // Tx
    {
        response.setDeviceHwType(new QString(m_sdrDaemonMain.m_deviceSinkAPI->getHardwareId()));
        response.setTx(1);
        DeviceSampleSink *sink = m_sdrDaemonMain.m_deviceSinkAPI->getSampleSink();
        return sink->webapiSettingsGet(response, *error.getMessage());
    }
    else
    {
        *error.getMessage() = QString("Device error");
        return 500;
    }
}

int WebAPIAdapterDaemon::daemonDeviceSettingsPutPatch(
        bool force,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if (m_sdrDaemonMain.m_deviceSourceEngine) // Rx
    {
        if (response.getTx() != 0)
        {
            *error.getMessage() = QString("Rx device found but Tx device requested");
            return 400;
        }
        if (m_sdrDaemonMain.m_deviceSourceAPI->getHardwareId() != *response.getDeviceHwType())
        {
            *error.getMessage() = QString("Device mismatch. Found %1 input").arg(m_sdrDaemonMain.m_deviceSourceAPI->getHardwareId());
            return 400;
        }
        else
        {
            DeviceSampleSource *source = m_sdrDaemonMain.m_deviceSourceAPI->getSampleSource();
            return source->webapiSettingsPutPatch(force, deviceSettingsKeys, response, *error.getMessage());
        }
    }
    else if (m_sdrDaemonMain.m_deviceSinkEngine) // Tx
    {
        if (response.getTx() == 0)
        {
            *error.getMessage() = QString("Tx device found but Rx device requested");
            return 400;
        }
        else if (m_sdrDaemonMain.m_deviceSinkAPI->getHardwareId() != *response.getDeviceHwType())
        {
            *error.getMessage() = QString("Device mismatch. Found %1 output").arg(m_sdrDaemonMain.m_deviceSinkAPI->getHardwareId());
            return 400;
        }
        else
        {
            DeviceSampleSink *sink = m_sdrDaemonMain.m_deviceSinkAPI->getSampleSink();
            return sink->webapiSettingsPutPatch(force, deviceSettingsKeys, response, *error.getMessage());
        }
    }
    else
    {
        *error.getMessage() = QString("DeviceSet error");
        return 500;
    }
}

int WebAPIAdapterDaemon::daemonRunGet(
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if (m_sdrDaemonMain.m_deviceSourceEngine) // Rx
    {
        DeviceSampleSource *source = m_sdrDaemonMain.m_deviceSourceAPI->getSampleSource();
        response.init();
        return source->webapiRunGet(response, *error.getMessage());
    }
    else if (m_sdrDaemonMain.m_deviceSinkEngine) // Tx
    {
        DeviceSampleSink *sink = m_sdrDaemonMain.m_deviceSinkAPI->getSampleSink();
        response.init();
        return sink->webapiRunGet(response, *error.getMessage());
    }
    else
    {
        *error.getMessage() = QString("DeviceSet error");
        return 500;
    }
}

int WebAPIAdapterDaemon::daemonRunPost(
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if (m_sdrDaemonMain.m_deviceSourceEngine) // Rx
    {
        DeviceSampleSource *source = m_sdrDaemonMain.m_deviceSourceAPI->getSampleSource();
        response.init();
        return source->webapiRun(true, response, *error.getMessage());
    }
    else if (m_sdrDaemonMain.m_deviceSinkEngine) // Tx
    {
        DeviceSampleSink *sink = m_sdrDaemonMain.m_deviceSinkAPI->getSampleSink();
        response.init();
        return sink->webapiRun(true, response, *error.getMessage());
    }
    else
    {
        *error.getMessage() = QString("DeviceSet error");
        return 500;
    }
}

int WebAPIAdapterDaemon::daemonRunDelete(
        SWGSDRangel::SWGDeviceState& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if (m_sdrDaemonMain.m_deviceSourceEngine) // Rx
    {
        DeviceSampleSource *source = m_sdrDaemonMain.m_deviceSourceAPI->getSampleSource();
        response.init();
        return source->webapiRun(false, response, *error.getMessage());
   }
    else if (m_sdrDaemonMain.m_deviceSinkEngine) // Tx
    {
        DeviceSampleSink *sink = m_sdrDaemonMain.m_deviceSinkAPI->getSampleSink();
        response.init();
        return sink->webapiRun(false, response, *error.getMessage());
    }
    else
    {
        *error.getMessage() = QString("DeviceSet error");
        return 500;
    }
}

int WebAPIAdapterDaemon::daemonReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    error.init();

    if (m_sdrDaemonMain.m_deviceSourceEngine) // Rx
    {
        response.setDeviceHwType(new QString(m_sdrDaemonMain.m_deviceSourceAPI->getHardwareId()));
        response.setTx(0);
        DeviceSampleSource *source = m_sdrDaemonMain.m_deviceSourceAPI->getSampleSource();
        return source->webapiReportGet(response, *error.getMessage());
    }
    else if (m_sdrDaemonMain.m_deviceSinkEngine) // Tx
    {
        response.setDeviceHwType(new QString(m_sdrDaemonMain.m_deviceSinkAPI->getHardwareId()));
        response.setTx(1);
        DeviceSampleSink *sink = m_sdrDaemonMain.m_deviceSinkAPI->getSampleSink();
        return sink->webapiReportGet(response, *error.getMessage());
    }
    else
    {
        *error.getMessage() = QString("DeviceSet error");
        return 500;
    }
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
