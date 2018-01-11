#include "gui/aboutdialog.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(const QString& apiHost, int apiPort, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::AboutDialog)
{
	ui->setupUi(this);
	ui->version->setText(QString("Version %1 - Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.").arg(qApp->applicationVersion()));
	ui->build->setText(QString("Build info: Qt %1 %2 bits").arg(QT_VERSION_STR).arg(QT_POINTER_SIZE*8));
	ui->restApiUrl->setText(QString("REST API base URL: http://%1:%2/sdrangel").arg(apiHost).arg(apiPort));
}

AboutDialog::~AboutDialog()
{
	delete ui;
}
