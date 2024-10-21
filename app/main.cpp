///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2018 Jason Gerecke <killertofu@gmail.com>                       //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2023 Daniele Forsi <iu5hkx@gmail.com>                           //
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
#include <QProxyStyle>
#include <QStyleFactory>
#include <QFontDatabase>
#include <QSysInfo>
#include <QSettings>
#ifdef __APPLE__
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QGLFormat>
#endif
#include <QSurfaceFormat>
#endif
#ifdef ANDROID
#include "util/android.h"
#endif
#if (QT_VERSION >= QT_VERSION_CHECK(6, 4, 0))
#include <QQuickWindow>
#endif

#include "loggerwithfile.h"
#include "mainwindow.h"
#include "remotetcpsinkstarter.h"
#include "dsp/dsptypes.h"

static int runQtApplication(int argc, char* argv[], qtwebapp::LoggerWithFile *logger)
{
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
#if (QT_VERSION >= QT_VERSION_CHECK(6, 4, 0))
    // Only use OpenGL, to easily combine QOpenGLWidget, QQuickWidget and QWebEngine
    // in a single window
    // See https://www.qt.io/blog/qt-quick-and-widgets-qt-6.4-edition
    // This prevents Direct3D/Vulcan being used on Windows/Mac though for QQuickWidget
    // and QWebEngine, so possibly should be reviewed in the future
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#endif
#ifndef ANDROID
     QApplication::setAttribute(Qt::AA_DontUseNativeDialogs); // Don't use on Android, otherwise we can't access files on internal storage
#endif

    // Set UI scale factor for High DPI displays
    QSettings settings;
    QString uiScaleFactor = "graphics.ui_scale_factor";
    if (settings.contains(uiScaleFactor))
    {
        QString scaleFactor = settings.value(uiScaleFactor).toString();
        qputenv("QT_SCALE_FACTOR", scaleFactor.toLatin1());
    }

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

#ifdef ANDROID
    // Default sized sliders can be hard to move using touch GUIs, so increase size
    // FIXME: How can we do a double border around the handle, as Fusion style seems to use?
    // Dialog borders are hard to see as is (perhaps as Android doesn't have a title bar), so use same color as for MDI
    qApp->setStyleSheet("QSlider {min-height: 20px; } "
                        "QSlider::groove:horizontal { border: 1px solid #2e2e2e; height: 1px; background: #444444; margin: 1px 0;}"
                        "QSlider::handle:horizontal { background: #585858; border: 1px double  #676767; width: 16px; margin: -8px 0px; border-radius: 3px;}"
                        "QSlider::sub-page {background: #ff8c00; border: 1px solid #2e2e2e;border-top-right-radius: 0px;border-bottom-right-radius: 0px;border-top-left-radius: 5px;border-bottom-left-radius: 5px;}"
                        "QSlider::add-page {background: #444444; border: 1px solid #2e2e2e;border-top-right-radius: 5px;border-bottom-right-radius: 5px;border-top-left-radius: 0px;border-bottom-left-radius: 0px;}"
                        "QDialog { border: 1px solid #ff8c00;}"
                        );
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
            qPrintable(qApp->applicationVersion()),
            qPrintable(QString(QT_VERSION_STR)),
            QT_POINTER_SIZE*8,
            SDR_RX_SAMP_SZ,
            SDR_TX_SAMP_SZ,
            applicationPid);
#endif

    if (parser.getListDevices())
    {
        // Disable log on console, so we can more easily see device list
        logger->setConsoleMinMessageLevel(QtFatalMsg);
        // Don't pass logger to MainWindow, otherwise it can re-enable log output
        logger = nullptr;
    }

	MainWindow w(logger, parser);

    if (parser.getListDevices())
    {
        // List available physical devices and exit
        RemoteTCPSinkStarter::listAvailableDevices();
        return EXIT_SUCCESS;
    }

    if (parser.getRemoteTCPSink()) {
        RemoteTCPSinkStarter::start(parser);
    }

	w.show();

	return a.exec();
}

int main(int argc, char* argv[])
{
#ifdef __APPLE__
    // Request OpenGL 3.3 context, needed for glspectrum and 3D Map feature
    // Note that Mac only supports CoreProfile, so any deprecated OpenGL 2 features
    // will not work. Because of this, we have two versions of the shaders:
    // OpenGL 2 versions for compatibility with older drivers and OpenGL 3.3
    // versions for newer drivers
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QGLFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QGLFormat::CoreProfile);
    QGLFormat::setDefaultFormat(fmt);
#endif
    QSurfaceFormat sfc;
    sfc.setVersion(3, 3);
    sfc.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(sfc);

    // Request authorization for access to camera and microphone (mac/auth.mm)
    extern int authCameraAndMic();
    if (authCameraAndMic() < 0) {
        qWarning("Failed to authorize access to camera and microphone. Enable access in System Settings > Privacy & Security");
    }
#endif

#ifdef ANDROID
    qtwebapp::LoggerWithFile *logger = nullptr;
    qInstallMessageHandler(Android::messageHandler);
#else
    qtwebapp::LoggerWithFile *logger = new qtwebapp::LoggerWithFile(qApp);
    logger->installMsgHandler();
#endif

	int res = runQtApplication(argc, argv, logger);

    if (logger) {
        delete logger;
    }

	qWarning("SDRangel quit.");
	return res;
}
