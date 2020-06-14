#include "recordinfodialog.h"
#include "ui_recordinfodialog.h"

RecordInfoDialog::RecordInfoDialog(const QString& text, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::RecordInfoDialog)
{
	ui->setupUi(this);
    ui->infoText->setText(text);
}

RecordInfoDialog::~RecordInfoDialog()
{
	delete ui;
}
