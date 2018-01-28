#include "gui/aboutdialog.h"
#include "ui_aboutdialog.h"
#include "dsp/dsptypes.h"

AboutDialog::AboutDialog(const QString& apiHost, int apiPort, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::AboutDialog)
{
	ui->setupUi(this);
	ui->version->setText(QString("Version %1 - Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.").arg(qApp->applicationVersion()));
	ui->build->setText(QString("Build info: Qt %1 %2 bits").arg(QT_VERSION_STR).arg(QT_POINTER_SIZE*8));
	ui->dspBits->setText(QString("DSP Rx %1 bits Tx %2 bits").arg(SDR_RX_SAMP_SZ).arg(SDR_TX_SAMP_SZ));
	ui->pid->setText(QString("PID: %1").arg(qApp->applicationPid()));
	QString apiUrl = QString("http://%1:%2/").arg(apiHost).arg(apiPort);
	ui->restApiUrl->setText(QString("REST API documentation: <a href=\"%1\">%2</a>").arg(apiUrl).arg(apiUrl));
	ui->restApiUrl->setOpenExternalLinks(true);
}

AboutDialog::~AboutDialog()
{
	delete ui;
}
