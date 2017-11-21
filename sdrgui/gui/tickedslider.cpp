///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QStylePainter>
#include <QStyleOptionSlider>

#include <math.h>

#include "tickedslider.h"

TickedSlider::TickedSlider(QWidget* parent) :
    QSlider(parent),
    m_tickColor(Qt::white)
{ }

void TickedSlider::paintEvent(QPaintEvent *ev __attribute__((unused)))
{

    QStylePainter p(this);
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    // draw tick marks
    // do this manually because they are very badly behaved with style sheets
    int interval = tickInterval();
    if (interval == 0)
    {
        interval = pageStep();
    }

    if (tickPosition() != NoTicks)
    {
        for (int i = minimum(); i <= maximum(); i += interval)
        {
            int x = roundf((double)((double)((double)(i - this->minimum()) / (double)(this->maximum() - this->minimum())) * (double)(this->width() - handle.width()) + (double)(handle.width() / 2.0))) - 1;
            int h = 4;
            p.setPen(m_tickColor);
            if (tickPosition() == TicksBothSides || tickPosition() == TicksAbove)
            {
                int y = this->rect().top();
                p.drawLine(x, y, x, y + h);
            }
            if (tickPosition() == TicksBothSides || tickPosition() == TicksBelow)
            {
                int y = this->rect().bottom();
                p.drawLine(x, y, x, y - h);
            }

        }
    }

    // draw the slider (this is basically copy/pasted from QSlider::paintEvent)
    opt.subControls = QStyle::SC_SliderGroove;
    p.drawComplexControl(QStyle::CC_Slider, opt);

    // draw the slider handle
    opt.subControls = QStyle::SC_SliderHandle;
    p.drawComplexControl(QStyle::CC_Slider, opt);
}




