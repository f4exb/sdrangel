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

#include "stringrangegui.h"

StringRangeGUI::StringRangeGUI(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::DiscreteRangeGUI)
{
    ui->setupUi(this);
}

StringRangeGUI::~StringRangeGUI()
{
    delete ui;
}

void StringRangeGUI::setLabel(const QString& text)
{
    ui->rangeLabel->setText(text);
}

void StringRangeGUI::setUnits(const QString& units)
{
    ui->rangeUnits->setText(units);
}

void StringRangeGUI::addItem(const QString& itemStr, const std::string& itemValue)
{
    ui->rangeCombo->blockSignals(true);
    ui->rangeCombo->addItem(itemStr);
    itemValues.push_back(itemValue);
    ui->rangeCombo->blockSignals(false);
}

const std::string& StringRangeGUI::getCurrentValue()
{
    return itemValues[ui->rangeCombo->currentIndex()];
}

void StringRangeGUI::setValue(const std::string& value)
{
    int index = 0;

    for (const auto &it : itemValues)
    {
        if (it == value)
        {
            ui->rangeCombo->blockSignals(true);
            ui->rangeCombo->setCurrentIndex(index);
            ui->rangeCombo->blockSignals(false);
            break;
        }

        index++;
    }
}

void StringRangeGUI::on_rangeCombo_currentIndexChanged(int index)
{
    (void) index;
    emit valueChanged();
}

