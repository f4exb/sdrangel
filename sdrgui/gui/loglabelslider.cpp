///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include <QDebug>

#include <cmath>

#include "loglabelslider.h"

LogLabelSlider::LogLabelSlider(QWidget *parent) :
    QWidget(parent)
{
    m_vLayout = new QVBoxLayout(this);
    m_hLayout = new QHBoxLayout();
    m_slider = new LogSlider();
    connect(m_slider, &LogSlider::logValueChanged, this, &LogLabelSlider::handleLogValueChanged);

    m_vLayout->addLayout(m_hLayout);
    m_vLayout->addWidget(m_slider);
}

void LogLabelSlider::setRange(double min, double max)
{
    m_slider->setRange(min, max);
    double start = floor(log10(min));
    double stop = ceil(log10(max));
    double steps = stop - start;

    qDeleteAll(m_labels);
    m_labels.clear();

    double v = pow(10.0, start);
    for (int i = 0; i <= steps; i++)
    {
        QString t = QString("%1").arg(v);

        QLabel *label = new QLabel(t);

        if (i == 0) {
            label->setAlignment(Qt::AlignLeft);
        } else if (i == steps) {
            label->setAlignment(Qt::AlignRight);
        } else {
            label->setAlignment(Qt::AlignCenter);
        }


        m_labels.append(label);
        m_hLayout->addWidget(label);

        v *= 10.0;
    }
}

void LogLabelSlider::handleLogValueChanged(double value)
{
    emit logValueChanged(value);
}
