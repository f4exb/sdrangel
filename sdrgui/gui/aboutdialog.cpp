///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2017-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>              //
// Copyright (C) 2022 AsciiWolf <mail@asciiwolf.com>                                 //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#include "gui/aboutdialog.h"
#include "ui_aboutdialog.h"
#include "dsp/dsptypes.h"
#include "settings/mainsettings.h"

AboutDialog::AboutDialog(const QString& apiHost, int apiPort, const MainSettings& mainSettings, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::AboutDialog)
{
	ui->setupUi(this);
	ui->version->setText(QString("Version %1 - Copyright (C) 2015-2022 Edouard Griffiths, F4EXB.").arg(qApp->applicationVersion()));
	ui->build->setText(QString("Build info: Qt %1 %2 bits").arg(QT_VERSION_STR).arg(QT_POINTER_SIZE*8));
	ui->dspBits->setText(QString("DSP Rx %1 bits Tx %2 bits").arg(SDR_RX_SAMP_SZ).arg(SDR_TX_SAMP_SZ));
	ui->pid->setText(QString("PID: %1").arg(qApp->applicationPid()));
	QString apiUrl = QString("http://%1:%2/").arg(apiHost).arg(apiPort);
	ui->restApiUrl->setText(QString("REST API documentation: <a href=\"%1\">%2</a>").arg(apiUrl).arg(apiUrl));
	ui->restApiUrl->setOpenExternalLinks(true);
	ui->settingsFile->setText(QString("Settings: %1").arg(mainSettings.getFileLocation()));
}

AboutDialog::~AboutDialog()
{
	delete ui;
}
