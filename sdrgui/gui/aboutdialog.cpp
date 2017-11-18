#include "gui/aboutdialog.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget* parent) :
	QDialog(parent),
	ui(new Ui::AboutDialog)
{
	ui->setupUi(this);
	ui->version->setText(QString("Version %1 - Copyright (C) 2015-2017 Edouard Griffiths, F4EXB.").arg(qApp->applicationVersion()));
}

AboutDialog::~AboutDialog()
{
	delete ui;
}
