#include "gui/addpresetdialog.h"
#include "ui_addpresetdialog.h"

AddPresetDialog::AddPresetDialog(const QStringList& groups, const QString& group, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::AddPresetDialog)
{
	ui->setupUi(this);
	ui->group->addItems(groups);
	ui->group->lineEdit()->setText(group);
}

AddPresetDialog::~AddPresetDialog()
{
	delete ui;
}

QString AddPresetDialog::group() const
{
	return ui->group->lineEdit()->text();
}

QString AddPresetDialog::description() const
{
	return ui->description->text();
}

void AddPresetDialog::setGroup(QString& group)
{
    ui->group->lineEdit()->setText(group);
}

void AddPresetDialog::setDescription(QString& description)
{
    ui->description->setText(description);
}
