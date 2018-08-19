///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon instance                                                            //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
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

#include <QSysInfo>
#include <QResource>
#include <unistd.h>

#include "dsp/dspengine.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "device/deviceenumerator.h"
#include "plugin/pluginmanager.h"
#include "util/message.h"
#include "loggerwithfile.h"

#include "webapi/webapiadapterdaemon.h"
#include "webapi/webapirequestmapper.h"
#include "webapi/webapiserver.h"
#include "channel/sdrdaemonchannelsink.h"
#include "sdrdaemonparser.h"
#include "sdrdaemonmain.h"

SDRDaemonMain *SDRDaemonMain::m_instance = 0;

SDRDaemonMain::SDRDaemonMain(qtwebapp::LoggerWithFile *logger, const SDRDaemonParser& parser, QObject *parent) :
    QObject(parent),
    m_logger(logger),
    m_settings(),
    m_dspEngine(DSPEngine::instance()),
    m_lastEngineState(DSPDeviceSourceEngine::StNotStarted)
{
    qDebug() << "SDRDaemonMain::SDRDaemonMain: start";

    m_instance = this;

    m_pluginManager = new PluginManager(this);
    m_pluginManager->loadPluginsPart(QString("pluginssrv/samplesink"));
    m_pluginManager->loadPluginsPart(QString("pluginssrv/samplesource"));
    m_pluginManager->loadPluginsFinal();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()), Qt::QueuedConnection);
    m_masterTimer.start(50);

    loadSettings();

    QString applicationDirPath = QCoreApplication::instance()->applicationDirPath();

    if (QResource::registerResource(applicationDirPath + "/sdrbase.rcc")) {
        qDebug("SDRDaemonMain::SDRDaemonMain: registered resource file %s/%s", qPrintable(applicationDirPath), "sdrbase.rcc");
    } else {
        qWarning("SDRDaemonMain::SDRDaemonMain: could not register resource file %s/%s", qPrintable(applicationDirPath), "sdrbase.rcc");
    }

    m_apiAdapter = new WebAPIAdapterDaemon(*this);
    m_requestMapper = new SDRDaemon::WebAPIRequestMapper(this);
    m_requestMapper->setAdapter(m_apiAdapter);
    m_apiServer = new SDRDaemon::WebAPIServer(parser.getServerAddress(), parser.getServerPort(), m_requestMapper);
    m_apiServer->start();

    m_tx = parser.getTx();
    m_deviceType = parser.getDeviceType();
    m_deviceSerial = parser.hasSerial() ? parser.getSerial() : "";
    m_deviceSequence = parser.hasSequence() ? parser.getSequence() : -1;
    m_deviceSourceEngine = 0;
    m_deviceSinkEngine = 0;
    m_deviceSourceAPI = 0;
    m_deviceSinkAPI = 0;
    m_channelSink = 0;

    if (m_tx)
    {
        if (addSinkDevice())
        {
            QString msg(tr("SDRDaemonMain::SDRDaemonMain: set sink %1").arg(m_deviceType));
            if (m_deviceSerial.length() > 0) {
                msg += tr(" ser: %1").arg(m_deviceSerial);
            } else if (m_deviceSequence >= 0) {
                msg += tr(" seq: %1").arg(m_deviceSequence);
            } else {
                msg += " first device";
            }
            QDebug info = qInfo();
            info.noquote();
            info << msg;
        }
        else
        {
            qCritical("SDRDaemonMain::SDRDaemonMain: sink device not found aborting");
            emit finished();
        }
    }
    else
    {
        if (addSourceDevice())
        {
            QString msg(tr("SDRDaemonMain::SDRDaemonMain: set source %1").arg(m_deviceType));
            if (m_deviceSerial.length() > 0) {
                msg += tr(" ser: %1").arg(m_deviceSerial);
            } else if (m_deviceSequence >= 0) {
                msg += tr(" seq: %1").arg(m_deviceSequence);
            } else {
                msg += " first device";
            }
            QDebug info = qInfo();
            info.noquote();
            info << msg;
            m_channelSink = new SDRDaemonChannelSink(m_deviceSourceAPI);
        }
        else
        {
            qCritical("SDRDaemonMain::SDRDaemonMain: source device not found aborting");
            emit finished();
        }
    }

    qDebug() << "SDRDaemonMain::SDRDaemonMain: end";
}

SDRDaemonMain::~SDRDaemonMain()
{
    removeDevice();
    m_apiServer->stop();
    m_settings.save();
    delete m_apiServer;
    delete m_requestMapper;
    delete m_apiAdapter;

    delete m_pluginManager;

    qDebug() << "SDRDaemonMain::~SDRDaemonMain: end";
    delete m_logger;
}

void SDRDaemonMain::loadSettings()
{
    qDebug() << "SDRDaemonMain::loadSettings";

    m_settings.load();
    setLoggingOptions();
}

void SDRDaemonMain::setLoggingOptions()
{
    m_logger->setConsoleMinMessageLevel(m_settings.getConsoleMinLogLevel());

    if (m_settings.getUseLogFile())
    {
        qtwebapp::FileLoggerSettings fileLoggerSettings; // default values

        if (m_logger->hasFileLogger()) {
            fileLoggerSettings = m_logger->getFileLoggerSettings(); // values from file logger if it exists
        }

        fileLoggerSettings.fileName = m_settings.getLogFileName(); // put new values
        m_logger->createOrSetFileLogger(fileLoggerSettings, 2000); // create file logger if it does not exist and apply settings in any case
    }

    if (m_logger->hasFileLogger()) {
        m_logger->setFileMinMessageLevel(m_settings.getFileMinLogLevel());
    }

    m_logger->setUseFileLogger(m_settings.getUseLogFile());

    if (m_settings.getUseLogFile())
    {
#if QT_VERSION >= 0x050400
        QString appInfoStr(tr("%1 %2 Qt %3 %4b %5 %6 DSP Rx:%7b Tx:%8b PID %9")
                .arg(QCoreApplication::applicationName())
                .arg(QCoreApplication::applicationVersion())
                .arg(QT_VERSION_STR)
                .arg(QT_POINTER_SIZE*8)
                .arg(QSysInfo::currentCpuArchitecture())
                .arg(QSysInfo::prettyProductName())
                .arg(SDR_RX_SAMP_SZ)
                .arg(SDR_TX_SAMP_SZ)
                .arg(QCoreApplication::applicationPid()));
#else
        QString appInfoStr(tr("%1 %2 Qt %3 %4b DSP Rx:%5b Tx:%6b PID %7")
                .arg(QCoreApplication::applicationName())
                .arg(QCoreApplication::applicationVersion())
                .arg(QT_VERSION_STR)
                .arg(QT_POINTER_SIZE*8)
                .arg(SDR_RX_SAMP_SZ)
                .arg(SDR_RX_SAMP_SZ)
                .arg(QCoreApplication::applicationPid());
 #endif
        m_logger->logToFile(QtInfoMsg, appInfoStr);
    }
}

bool SDRDaemonMain::addSinkDevice()
{
    DSPDeviceSinkEngine *dspDeviceSinkEngine = m_dspEngine->addDeviceSinkEngine();
    dspDeviceSinkEngine->start();

    uint dspDeviceSinkEngineUID =  dspDeviceSinkEngine->getUID();
    char uidCStr[16];
    sprintf(uidCStr, "UID:%d", dspDeviceSinkEngineUID);

    m_deviceSinkEngine = dspDeviceSinkEngine;

    m_deviceSinkAPI = new DeviceSinkAPI(0, dspDeviceSinkEngine);
    int deviceIndex = getDeviceIndex();

    if (deviceIndex >= 0)
    {
        PluginInterface::SamplingDevice samplingDevice = DeviceEnumerator::instance()->getTxSamplingDevice(deviceIndex);
        m_deviceSinkAPI->setSampleSinkSequence(samplingDevice.sequence);
        m_deviceSinkAPI->setNbItems(samplingDevice.deviceNbItems);
        m_deviceSinkAPI->setItemIndex(samplingDevice.deviceItemIndex);
        m_deviceSinkAPI->setHardwareId(samplingDevice.hardwareId);
        m_deviceSinkAPI->setSampleSinkId(samplingDevice.id);
        m_deviceSinkAPI->setSampleSinkSerial(samplingDevice.serial);
        m_deviceSinkAPI->setSampleSinkDisplayName(samplingDevice.displayedName);
        m_deviceSinkAPI->setSampleSinkPluginInterface(DeviceEnumerator::instance()->getTxPluginInterface(deviceIndex));

        DeviceSampleSink *sink = m_deviceSinkAPI->getPluginInterface()->createSampleSinkPluginInstanceOutput(
                m_deviceSinkAPI->getSampleSinkId(), m_deviceSinkAPI);
        m_deviceSinkAPI->setSampleSink(sink);
        return true;
    }

    return false;
}

bool SDRDaemonMain::addSourceDevice()
{
    DSPDeviceSourceEngine *dspDeviceSourceEngine = m_dspEngine->addDeviceSourceEngine();
    dspDeviceSourceEngine->start();

    uint dspDeviceSourceEngineUID =  dspDeviceSourceEngine->getUID();
    char uidCStr[16];
    sprintf(uidCStr, "UID:%d", dspDeviceSourceEngineUID);

    m_deviceSourceEngine = dspDeviceSourceEngine;

    m_deviceSourceAPI = new DeviceSourceAPI(0, dspDeviceSourceEngine);
    int deviceIndex = getDeviceIndex();

    if (deviceIndex >= 0)
    {
        PluginInterface::SamplingDevice samplingDevice = DeviceEnumerator::instance()->getRxSamplingDevice(deviceIndex);
        m_deviceSourceAPI->setSampleSourceSequence(samplingDevice.sequence);
        m_deviceSourceAPI->setNbItems(samplingDevice.deviceNbItems);
        m_deviceSourceAPI->setItemIndex(samplingDevice.deviceItemIndex);
        m_deviceSourceAPI->setHardwareId(samplingDevice.hardwareId);
        m_deviceSourceAPI->setSampleSourceId(samplingDevice.id);
        m_deviceSourceAPI->setSampleSourceSerial(samplingDevice.serial);
        m_deviceSourceAPI->setSampleSourceDisplayName(samplingDevice.displayedName);
        m_deviceSourceAPI->setSampleSourcePluginInterface(DeviceEnumerator::instance()->getRxPluginInterface(deviceIndex));

        DeviceSampleSource *source = m_deviceSourceAPI->getPluginInterface()->createSampleSourcePluginInstanceInput(
                m_deviceSourceAPI->getSampleSourceId(), m_deviceSourceAPI);
        m_deviceSourceAPI->setSampleSource(source);
        return true;
    }

    return false;
}

void SDRDaemonMain::removeDevice()
{
    if (m_deviceSourceEngine) // source set
    {
        m_deviceSourceEngine->stopAcquistion();

        // deletes old UI and input object

        if (m_channelSink) {
            m_channelSink->destroy();
        }

        m_deviceSourceAPI->resetSampleSourceId();
        m_deviceSourceAPI->getPluginInterface()->deleteSampleSourcePluginInstanceInput(
                m_deviceSourceAPI->getSampleSource());
        m_deviceSourceAPI->clearBuddiesLists(); // clear old API buddies lists

        m_deviceSourceEngine->stop();
        m_dspEngine->removeLastDeviceSourceEngine();

        delete m_deviceSourceAPI;
        m_deviceSourceAPI = 0;
    }
    else if (m_deviceSinkEngine) // sink set
    {
        m_deviceSinkEngine->stopGeneration();

        // deletes old UI and output object
        m_deviceSinkAPI->resetSampleSinkId();
        m_deviceSinkAPI->getPluginInterface()->deleteSampleSinkPluginInstanceOutput(
                m_deviceSinkAPI->getSampleSink());
        m_deviceSinkAPI->clearBuddiesLists(); // clear old API buddies lists

        m_deviceSinkEngine->stop();
        m_dspEngine->removeLastDeviceSinkEngine();

        delete m_deviceSinkAPI;
        m_deviceSinkAPI = 0;
    }
}

int SDRDaemonMain::getDeviceIndex()
{
    int nbSamplingDevices = m_tx ? DeviceEnumerator::instance()->getNbTxSamplingDevices() : DeviceEnumerator::instance()->getNbRxSamplingDevices();

    for (int i = 0; i < nbSamplingDevices; i++)
    {
        PluginInterface::SamplingDevice samplingDevice = m_tx ? DeviceEnumerator::instance()->getTxSamplingDevice(i) : DeviceEnumerator::instance()->getRxSamplingDevice(i);
        if (samplingDevice.hardwareId == m_deviceType)
        {
            if (m_deviceSerial.length() > 0)
            {
                if (samplingDevice.serial == m_deviceSerial) {
                    return i;
                } else {
                    continue;
                }
            }
            else if (m_deviceSequence >= 0)
            {
                if (samplingDevice.sequence == m_deviceSequence) {
                    return i;
                } else {
                    continue;
                }
            }
            else
            {
                return i;
            }
        }
    }

    return -1; // not found
}

bool SDRDaemonMain::handleMessage(const Message& cmd __attribute__((unused)))
{
    return false;
}

void SDRDaemonMain::handleMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("SDRDaemonMain::handleMessages: message: %s", message->getIdentifier());
        handleMessage(*message);
        delete message;
    }
}
