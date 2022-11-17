///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#include <QApplication>
#include <QTextCodec>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QFontDatabase>
#include <QSysInfo>
#ifdef __APPLE__
#include <QGLFormat>
#include <QSurfaceFormat>
#endif

#include "loggerwithfile.h"
#include "mainwindow.h"
#include "dsp/dsptypes.h"

static int runQtApplication(int argc, char* argv[], qtwebapp::LoggerWithFile *logger)
{
/*
	QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
	QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
*/
	QCoreApplication::setOrganizationName(COMPANY);
	QCoreApplication::setApplicationName(APPLICATION_NAME);
    QCoreApplication::setApplicationVersion(SDRANGEL_VERSION);

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling); // DPI support
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps); //HiDPI pixmaps
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts); // Needed for WebGL in QWebEngineView and MainWindow::openGLVersion
#endif
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)) && (QT_VERSION <= QT_VERSION_CHECK(6, 0, 0))
    QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif

	QApplication a(argc, argv);

#if 1
    qApp->setStyle(QStyleFactory::create("fusion"));

	QPalette palette;
	palette.setColor(QPalette::Window, QColor(64,64,64));
	palette.setColor(QPalette::WindowText, Qt::white);
	palette.setColor(QPalette::Base, QColor(25,25,25));
	palette.setColor(QPalette::AlternateBase, QColor(53,53,53));
	palette.setColor(QPalette::ToolTipBase, Qt::white);
	palette.setColor(QPalette::ToolTipText, Qt::black);
	palette.setColor(QPalette::Text, Qt::white);
	palette.setColor(QPalette::Button, QColor(64,64,64));
	palette.setColor(QPalette::ButtonText, Qt::white);
	palette.setColor(QPalette::BrightText, Qt::red);

	palette.setColor(QPalette::Light, QColor(64,64,64).lighter(125).lighter());
	palette.setColor(QPalette::Mid, QColor(64,64,64).lighter(125));
	palette.setColor(QPalette::Dark, QColor(64,64,64).lighter(125).darker());

	palette.setColor(QPalette::Link, QColor(0,0xa0,0xa0));
	palette.setColor(QPalette::LinkVisited, QColor(0,0xa0,0xa0).lighter());
	palette.setColor(QPalette::Highlight, QColor(0xff, 0x8c, 0x00));
	palette.setColor(QPalette::HighlightedText, Qt::black);

	palette.setColor(QPalette::Disabled, QPalette::WindowText, Qt::gray);
	palette.setColor(QPalette::Disabled, QPalette::Text, Qt::gray);
	palette.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::gray);
	qApp->setPalette(palette);

#if 0
	if(QFontDatabase::addApplicationFont("/tmp/Cuprum.otf") >= 0) {
		QFont font("CuprumFFU");
		font.setPointSize(10);
		qApp->setFont(font);
	}
#endif
#if 0
	if(QFontDatabase::addApplicationFont("/tmp/PTN57F.ttf") >= 0) {
		QFont font("PT Sans Narrow");
		font.setPointSize(10);
		qApp->setFont(font);
	}
#endif
#if 0
	if(QFontDatabase::addApplicationFont("/tmp/PTS55F.ttf") >= 0) {
		QFont font("PT Sans");
		font.setPointSize(10);
		qApp->setFont(font);
	}
#endif
#if 0
	{
		QFont font("Ubuntu Condensed");
		font.setPointSize(10);
		qApp->setFont(font);
	}
#endif

#endif
	MainParser parser;
	parser.parse(*qApp);

#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
	qInfo("%s %s Qt %s %db %s %s DSP Rx:%db Tx:%db PID %lld",
	        qPrintable(qApp->applicationName()),
	        qPrintable(qApp->applicationVersion()),
	        qPrintable(QString(QT_VERSION_STR)),
	        QT_POINTER_SIZE*8,
	        qPrintable(QSysInfo::currentCpuArchitecture()),
	        qPrintable(QSysInfo::prettyProductName()),
	        SDR_RX_SAMP_SZ,
	        SDR_TX_SAMP_SZ,
	        qApp->applicationPid());
#else
    qInfo("%s %s Qt %s %db DSP Rx:%db Tx:%db PID: %lld",
            qPrintable(qApp->applicationName()),
            qPrintable((qApp->applicationVersion()),
            qPrintable(QString(QT_VERSION_STR)),
            QT_POINTER_SIZE*8,
            SDR_RX_SAMP_SZ,
            SDR_TX_SAMP_SZ,
            applicationPid);
#endif

	MainWindow w(logger, parser);
	w.show();

	return a.exec();
}

int main(int argc, char* argv[])
{
#ifdef __APPLE__
    // Request OpenGL 3.3 context, needed for glspectrum and 3D Map feature
    // Note that Mac only supports CoreProfile, so any deprecated OpenGL 2 features
    // will not work. Because of this, we have two versions of the shaders:
    // OpenGL 2 versions for compatiblity with older drivers and OpenGL 3.3
    // versions for newer drivers
    QGLFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QGLFormat::CoreProfile);
    QGLFormat::setDefaultFormat(fmt);
    QSurfaceFormat sfc;
    sfc.setVersion(3, 3);
    sfc.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(sfc);
#endif

	qtwebapp::LoggerWithFile *logger = new qtwebapp::LoggerWithFile(qApp);
    logger->installMsgHandler();
	int res = runQtApplication(argc, argv, logger);
	delete logger;
	qWarning("SDRangel quit.");
	return res;
}
