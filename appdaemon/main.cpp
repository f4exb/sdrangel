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

#include <QCoreApplication>
#include <QSysInfo>

#include <signal.h>
#include <unistd.h>
#include <vector>

#include "dsp/dsptypes.h"
#include "loggerwithfile.h"
#include "sdrdaemonparser.h"
#include "sdrdaemonmain.h"

void handler(int sig) {
    fprintf(stderr, "quit the application by signal(%d).\n", sig);
    QCoreApplication::quit();
}

void catchUnixSignals(const std::vector<int>& quitSignals) {
    sigset_t blocking_mask;
    sigemptyset(&blocking_mask);

    for (std::vector<int>::const_iterator it = quitSignals.begin(); it != quitSignals.end(); ++it) {
        sigaddset(&blocking_mask, *it);
    }

    struct sigaction sa;
    sa.sa_handler = handler;
    sa.sa_mask    = blocking_mask;
    sa.sa_flags   = 0;

    for (std::vector<int>::const_iterator it = quitSignals.begin(); it != quitSignals.end(); ++it) {
        sigaction(*it, &sa, 0);
    }
}

static int runQtApplication(int argc, char* argv[], qtwebapp::LoggerWithFile *logger)
{
    QCoreApplication a(argc, argv);

    QCoreApplication::setOrganizationName("f4exb");
    QCoreApplication::setApplicationName("SDRdaemonSrv");
    QCoreApplication::setApplicationVersion("4.1.0");

    int catchSignals[] = {SIGQUIT, SIGINT, SIGTERM, SIGHUP};
    std::vector<int> vsig(catchSignals, catchSignals + sizeof(catchSignals) / sizeof(int));
    catchUnixSignals(vsig);

    SDRDaemonParser parser;
    parser.parse(a);

#if QT_VERSION >= 0x050400
    qInfo("%s %s Qt %s %db %s %s DSP Rx:%db Tx:%db PID %lld",
            qPrintable(QCoreApplication::applicationName()),
            qPrintable(QCoreApplication::applicationVersion()),
            qPrintable(QString(QT_VERSION_STR)),
            QT_POINTER_SIZE*8,
            qPrintable(QSysInfo::currentCpuArchitecture()),
            qPrintable(QSysInfo::prettyProductName()),
            SDR_RX_SAMP_SZ,
            SDR_TX_SAMP_SZ,
            QCoreApplication::applicationPid());
#else
    qInfo("%s %s Qt %s %db DSP Rx:%db Tx:%db PID %lld",
            qPrintable(QCoreApplication::applicationName()),
            qPrintable((QCoreApplication::>applicationVersion()),
            qPrintable(QString(QT_VERSION_STR)),
            QT_POINTER_SIZE*8,
            SDR_RX_SAMP_SZ,
            SDR_TX_SAMP_SZ,
            QCoreApplication::applicationPid());
#endif

    SDRDaemonMain m(logger, parser, &a);

    if (m.doAbort())
    {
        return -1;
    }
    else
    {
        // This will cause the application to exit when SDRdaemon is finished
        QObject::connect(&m, SIGNAL(finished()), &a, SLOT(quit()));

        return a.exec();
    }
}

int main(int argc, char* argv[])
{
    qtwebapp::LoggerWithFile *logger = new qtwebapp::LoggerWithFile(qApp);
    logger->installMsgHandler();
    int res = runQtApplication(argc, argv, logger);
    qWarning("SDRdaemon quit.");
    return res;
}





