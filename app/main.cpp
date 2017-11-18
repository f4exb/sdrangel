///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#include <QApplication>
#include <QTextCodec>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QFontDatabase>

#include "loggerwithfile.h"
#include "mainwindow.h"

static int runQtApplication(int argc, char* argv[], qtwebapp::LoggerWithFile *logger)
{
	QApplication a(argc, argv);
/*
	QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
	QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
*/
	QCoreApplication::setOrganizationName("f4exb");
	QCoreApplication::setApplicationName("SDRangel");
	QCoreApplication::setApplicationVersion("3.8.4");

#if 1
	qApp->setStyle(QStyleFactory::create("fusion"));

	QPalette palette;
	palette.setColor(QPalette::Window, QColor(53,53,53));
	palette.setColor(QPalette::WindowText, Qt::white);
	palette.setColor(QPalette::Base, QColor(25,25,25));
	palette.setColor(QPalette::AlternateBase, QColor(53,53,53));
	palette.setColor(QPalette::ToolTipBase, Qt::white);
	palette.setColor(QPalette::ToolTipText, Qt::black);
	palette.setColor(QPalette::Text, Qt::white);
	palette.setColor(QPalette::Button, QColor(0x40, 0x40, 0x40));
	palette.setColor(QPalette::ButtonText, Qt::white);
	palette.setColor(QPalette::BrightText, Qt::red);

	palette.setColor(QPalette::Light, QColor(53,53,53).lighter(125).lighter());
	palette.setColor(QPalette::Mid, QColor(53,53,53).lighter(125));
	palette.setColor(QPalette::Dark, QColor(53,53,53).lighter(125).darker());

	palette.setColor(QPalette::Link, QColor(0,0xa0,0xa0));
	palette.setColor(QPalette::LinkVisited, QColor(0,0xa0,0xa0).lighter());
	palette.setColor(QPalette::Highlight, QColor(0xff, 0x8c, 0x00));
	palette.setColor(QPalette::HighlightedText, Qt::black);
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

	MainWindow w(logger, parser);
	w.show();

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
