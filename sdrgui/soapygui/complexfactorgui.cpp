///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include "ui_complexfactorgui.h"
#include "complexfactorgui.h"

ComplexFactorGUI::ComplexFactorGUI(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ComplexFactorGUI)
{
    ui->setupUi(this);
    ui->automatic->setChecked(false);
}

ComplexFactorGUI::~ComplexFactorGUI()
{
    delete ui;
}

double ComplexFactorGUI::getModule() const
{
    return ui->module->value() / 100.0;
}

double ComplexFactorGUI::getArgument() const
{
    return ui->arg->value() * 1.0;
}

bool ComplexFactorGUI::getAutomatic() const
{
    return ui->automatic->isChecked();
}

void ComplexFactorGUI::setModule(double value)
{
    double modValue = value < -1.0 ? -1.0 : value > 1.0 ? 1.0 : value;
    ui->module->setValue((int) modValue*100.0f);
    ui->moduleText->setText(tr("%1").arg(modValue, 0, 'f', 2));
}

void ComplexFactorGUI::setArgument(double value)
{
    int argValue = (int) (value < -180.0 ? -180.0 : value > 180.0 ? 180.0 : value);
    ui->arg->setValue(argValue);
    ui->argText->setText(tr("%1").arg(value));
}

void ComplexFactorGUI::setAutomatic(bool automatic)
{
    ui->automatic->setChecked(automatic);
}

void ComplexFactorGUI::setAutomaticEnable(bool enable)
{
    ui->automatic->setEnabled(enable);
}

void ComplexFactorGUI::setLabel(const QString& text)
{
    ui->label->setText(text);
}

void ComplexFactorGUI::setToolTip(const QString& text)
{
    ui->label->setToolTip(text);
}

void ComplexFactorGUI::on_automatic_toggled(bool set)
{
    ui->module->setEnabled(!set);
    ui->arg->setEnabled(!set);
    emit automaticChanged(set);
}

void ComplexFactorGUI::on_module_valueChanged(int value)
{
    ui->moduleText->setText(tr("%1").arg(value/100.0f, 0, 'f', 2));
    emit moduleChanged(value/100.0f);
}

void ComplexFactorGUI::on_arg_valueChanged(int value)
{
    ui->argText->setText(tr("%1").arg(value));
    emit argumentChanged(value);
}
