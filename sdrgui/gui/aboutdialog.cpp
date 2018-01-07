#include "gui/aboutdialog.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget* parent) :
	QDialog(parent),
	ui(new Ui::AboutDialog)
{
	ui->setupUi(this);
	ui->version->setText(QString("Version %1 - Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.").arg(qApp->applicationVersion()));
	ui->build->setText(QString("Build info: Qt %1 %2 bits").arg(QT_VERSION_STR).arg(QT_POINTER_SIZE*8));
}

AboutDialog::~AboutDialog()
{
	delete ui;
}
