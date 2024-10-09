///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "wrappingdial.h"

#include <QWheelEvent>

WrappingDial::WrappingDial(QWidget *parent) :
    QDial(parent),
    m_wheelEvent(false),
    m_wheelUp(false)
{
    setWrapping(true);

    connect(this, &QDial::actionTriggered, this, &WrappingDial::on_actionTriggered);
}

void WrappingDial::on_actionTriggered(int action)
{
    if (wrapping())
    {
        if (   (   (action == QAbstractSlider::SliderSingleStepSub)
                || (action == QAbstractSlider::SliderPageStepSub)
                || ((action == QAbstractSlider::SliderMove) && m_wheelEvent && !m_wheelUp)
               )
            && (value() < sliderPosition()))
        {
            emit wrapDown();
        }
        if (   (   (action == QAbstractSlider::SliderSingleStepAdd)
                || (action == QAbstractSlider::SliderPageStepAdd)
                || ((action == QAbstractSlider::SliderMove) && m_wheelEvent && m_wheelUp)
                )
            && (value() > sliderPosition()))
        {
            emit wrapUp();
        }
    }
}

// QAbstractSlider just generates SliderMove actions for wheel events, so we can't distinguish between
// wheel and dial being clicked to a new position - so we set a flag here, before passing up the event
void WrappingDial::wheelEvent(QWheelEvent *e)
{
    m_wheelEvent = true;
    m_wheelUp = e->angleDelta().y() > 0;
    QDial::wheelEvent(e);
    m_wheelEvent = false;
}
