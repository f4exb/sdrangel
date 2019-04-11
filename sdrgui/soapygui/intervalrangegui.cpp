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

#include "ui_intervalrangegui.h"
#include "intervalrangegui.h"

IntervalRangeGUI::IntervalRangeGUI(QWidget* parent) :
    ItemSettingGUI(parent),
    ui(new Ui::IntervalRangeGUI),
    m_nbDigits(7)
{
    ui->setupUi(this);
    ui->value->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
}

IntervalRangeGUI::~IntervalRangeGUI()
{
    delete ui;
}

void IntervalRangeGUI::setLabel(const QString& text)
{
    ui->rangeLabel->setText(text);
}

void IntervalRangeGUI::setUnits(const QString& units)
{
    ui->rangeUnits->setText(units);
}

void IntervalRangeGUI::addInterval(double minimum, double maximum)
{
    ui->rangeInterval->blockSignals(true);
    ui->rangeInterval->addItem(QString("%1").arg(m_minima.size()));
    ui->rangeInterval->blockSignals(false);
    m_minima.push_back(minimum);
    m_maxima.push_back(maximum);
}

void IntervalRangeGUI::reset()
{
    if (m_minima.size() > 0)
    {
        double maxLog = 0.0;

        for (const auto &it : m_maxima)
        {
            if (log10(it) > maxLog) {
                maxLog = log10(it);
            }
        }

        m_nbDigits = maxLog;
        m_nbDigits++;
        ui->rangeInterval->blockSignals(true);
        ui->rangeInterval->setCurrentIndex(0);
        ui->rangeInterval->blockSignals(false);
        ui->value->setValueRange(m_minima[0] >= 0, m_nbDigits, m_minima[0], m_maxima[0]);
    }

    if (m_minima.size() == 1) {
        ui->rangeInterval->setDisabled(true);
    }
}

double IntervalRangeGUI::getCurrentValue()
{
    return ui->value->getValueNew();
}

void IntervalRangeGUI::setValue(double value)
{
    ui->value->setValue(value);
}

void IntervalRangeGUI::on_value_changed(qint64 value)
{
    emit ItemSettingGUI::valueChanged(value);
}

void IntervalRangeGUI::on_rangeInterval_currentIndexChanged(int index)
{
    ui->value->setValueRange(m_minima[index] >= 0, m_nbDigits, m_minima[index], m_maxima[index]);
    emit ItemSettingGUI::valueChanged(ui->value->getValueNew());
}

