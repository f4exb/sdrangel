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

#include <math.h>

#include "ui_intervalslidergui.h"
#include "intervalslidergui.h"

IntervalSliderGUI::IntervalSliderGUI(QWidget* parent) :
    ItemSettingGUI(parent),
    ui(new Ui::IntervalSliderGUI),
    m_minimum(0),
    m_maximum(0)
{
    ui->setupUi(this);
}

IntervalSliderGUI::~IntervalSliderGUI()
{
    delete ui;
}

void IntervalSliderGUI::setLabel(const QString& text)
{
    ui->intervalLabel->setText(text);
}

void IntervalSliderGUI::setUnits(const QString& units)
{
    ui->intervalUnits->setText(units);
}

void IntervalSliderGUI::setInterval(double minimum, double maximum)
{
    ui->intervalSlider->blockSignals(true);
    ui->intervalSlider->setMinimum(minimum);
    ui->intervalSlider->setMaximum(maximum);
    ui->intervalSlider->blockSignals(false);
    m_minimum = minimum;
    m_maximum = maximum;
}

double IntervalSliderGUI::getCurrentValue()
{
    return ui->intervalSlider->value();
}

void IntervalSliderGUI::setValue(double value)
{
    ui->intervalSlider->setValue(value);
    ui->valueText->setText(QString("%1").arg(ui->intervalSlider->value()));
}

void IntervalSliderGUI::on_intervalSlider_valueChanged(int value)
{
    ui->valueText->setText(QString("%1").arg(value));
    emit ItemSettingGUI::valueChanged(value);
}
