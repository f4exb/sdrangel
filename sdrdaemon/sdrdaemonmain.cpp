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
#include "plugin/pluginmanager.h"
#include "util/message.h"
#include "loggerwithfile.h"
#include "sdrdaemonmain.h"

#include "webapi/webapiadapterdaemon.h"
#include "webapi/webapirequestmapper.h"
#include "webapi/webapiserver.h"

SDRDaemonMain *SDRDaemonMain::m_instance = 0;

SDRDaemonMain::SDRDaemonMain(qtwebapp::LoggerWithFile *logger, const MainParser& parser, QObject *parent) :
    QObject(parent),
    m_logger(logger),
    m_settings(),
    m_dspEngine(DSPEngine::instance()),
    m_lastEngineState(DSPDeviceSourceEngine::StNotStarted)
{
    qDebug() << "SDRdaemon::SDRdaemon: start";

    m_instance = this;

    m_pluginManager = new PluginManager(this);
    m_pluginManager->loadPlugins(QString("pluginssrv"));

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()), Qt::QueuedConnection);
    m_masterTimer.start(50);

    loadSettings();

    QString applicationDirPath = QCoreApplication::instance()->applicationDirPath();

    if (QResource::registerResource(applicationDirPath + "/sdrbase.rcc")) {
        qDebug("MainCore::MainCore: registered resource file %s/%s", qPrintable(applicationDirPath), "sdrbase.rcc");
    } else {
        qWarning("MainCore::MainCore: could not register resource file %s/%s", qPrintable(applicationDirPath), "sdrbase.rcc");
    }

    m_apiAdapter = new WebAPIAdapterDaemon(*this);
    m_requestMapper = new SDRDaemon::WebAPIRequestMapper(this);
    m_requestMapper->setAdapter(m_apiAdapter);
    m_apiServer = new SDRDaemon::WebAPIServer(parser.getServerAddress(), parser.getServerPort(), m_requestMapper);
    m_apiServer->start();

    qDebug() << "SDRdaemon::SDRdaemon: end";
}

SDRDaemonMain::~SDRDaemonMain()
{
    m_apiServer->stop();
    m_settings.save();
    delete m_apiServer;
    delete m_requestMapper;
    delete m_apiAdapter;

    delete m_pluginManager;

    qDebug() << "SDRdaemon::~SDRdaemon: end";
    delete m_logger;
}

void SDRDaemonMain::loadSettings()
{
    qDebug() << "SDRdaemon::loadSettings";

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

bool SDRDaemonMain::handleMessage(const Message& cmd __attribute__((unused)))
{
    return false;
}

void SDRDaemonMain::handleMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("SDRdaemon::handleMessages: message: %s", message->getIdentifier());
        handleMessage(*message);
        delete message;
    }
}
