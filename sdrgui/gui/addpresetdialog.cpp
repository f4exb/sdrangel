///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2015, 2017-2018 Edouard Griffiths, F4EXB <f4exb06@gmail.com>        //
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

void AddPresetDialog::setGroup(const QString& group)
{
    ui->group->lineEdit()->setText(group);
}

void AddPresetDialog::setDescription(const QString& description)
{
    ui->description->setText(description);
}

void AddPresetDialog::showGroupOnly()
{
    ui->description->hide();
    ui->descriptionLabel->hide();
}

void AddPresetDialog::setDialogTitle(const QString& title)
{
    setWindowTitle(title);
}

void AddPresetDialog::setDescriptionBoxTitle(const QString& title)
{
    ui->descriptionBox->setTitle(title);
}
