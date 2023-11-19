///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include <QFontMetrics>
#include <QPainter>
#include <QDebug>

#include <cmath>


#include "logslider.h"

LogSlider::LogSlider(QWidget *parent) :
    QSlider(Qt::Horizontal, parent)
{
    setRange(0, 1000);
    connect(this, &QSlider::valueChanged, this, &LogSlider::handleValueChanged);
    setPageStep(1);
    setTickPosition(QSlider::TicksAbove);
    setTickInterval(m_stepsPerPower);
}

void LogSlider::setRange(double min, double max)
{
    m_start = floor(log10(min));
    m_stop = ceil(log10(max));
    m_steps = m_stop - m_start;
    setMinimum(0);
    setMaximum(m_steps * m_stepsPerPower);
}

void LogSlider::handleValueChanged(int value)
{
    double v = pow(10.0, value/(double)m_stepsPerPower + m_start);
    emit logValueChanged(v);
}

void LogSlider::setValue(double value)
{
    int v = (int)((log10(value) - m_start) * 100);
    QSlider::setValue(v);
}
