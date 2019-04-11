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

#include "ui_discreterangegui.h"
#include "discreterangegui.h"

DiscreteRangeGUI::DiscreteRangeGUI(QWidget* parent) :
    ItemSettingGUI(parent),
    ui(new Ui::DiscreteRangeGUI)
{
    ui->setupUi(this);
}

DiscreteRangeGUI::~DiscreteRangeGUI()
{
    delete ui;
}

void DiscreteRangeGUI::setLabel(const QString& text)
{
    ui->rangeLabel->setText(text);
}

void DiscreteRangeGUI::setUnits(const QString& units)
{
    ui->rangeUnits->setText(units);
}

void DiscreteRangeGUI::addItem(const QString& itemStr, double itemValue)
{
    ui->rangeCombo->blockSignals(true);
    ui->rangeCombo->addItem(itemStr);
    itemValues.push_back(itemValue);
    ui->rangeCombo->blockSignals(false);
}

double DiscreteRangeGUI::getCurrentValue()
{
    return itemValues[ui->rangeCombo->currentIndex()];
}

void DiscreteRangeGUI::setValue(double value)
{
    int index = 0;

    for (const auto &it : itemValues)
    {
        if (it >= value)
        {
            ui->rangeCombo->blockSignals(true);
            ui->rangeCombo->setCurrentIndex(index);
            ui->rangeCombo->blockSignals(false);
            break;
        }

        index++;
    }
}

void DiscreteRangeGUI::on_rangeCombo_currentIndexChanged(int index)
{
    double newRange = itemValues[index];
    emit ItemSettingGUI::valueChanged(newRange);
}
