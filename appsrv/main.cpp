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

#include <QCoreApplication>

#include "loggerwithfile.h"
#include "maincore.h"


static int runQtApplication(int argc, char* argv[], qtwebapp::LoggerWithFile *logger)
{
    QCoreApplication a(argc, argv);

    QCoreApplication::setOrganizationName("f4exb");
    QCoreApplication::setApplicationName("SDRangelSrv");
    QCoreApplication::setApplicationVersion("3.9.0");

    MainParser parser;
    parser.parse(a);
    MainCore m(logger, parser, &a);

    // This will cause the application to exit when the main core is finished
    QObject::connect(&m, SIGNAL(finished()), &a, SLOT(quit()));

    // This will run the main core from the application event loop.
    QTimer::singleShot(0, &m, SLOT(run()));

    return a.exec();
}

int main(int argc, char* argv[])
{
    qtwebapp::LoggerWithFile *logger = new qtwebapp::LoggerWithFile(qApp);
    logger->installMsgHandler();
    int res = runQtApplication(argc, argv, logger);
    qWarning("SDRangel quit.");
    return res;
}


