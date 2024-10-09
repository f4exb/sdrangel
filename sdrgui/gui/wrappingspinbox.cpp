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

#include "wrappingspinbox.h"

#include <QWheelEvent>

WrappingSpinBox::WrappingSpinBox(QWidget *parent) :
    QSpinBox(parent),
    m_wheelEvent(false),
    m_wheelUp(false)
{
    setWrapping(true);
}

void WrappingSpinBox::stepBy(int steps)
{
    int v = value();
    QSpinBox::stepBy(steps);
    if (wrapping())
    {
        if (v + steps > maximum()) {
            emit wrapUp();
        }
        if (v + steps < minimum()) {
            emit wrapDown();
        }
    }
}

// QAbstractSlider just generates SliderMove actions for wheel events, so we can't distinguish between
// wheel and dial being clicked to a new position - so we set a flag here, before passing up the event
void WrappingSpinBox::wheelEvent(QWheelEvent *e)
{
    m_wheelEvent = true;
    m_wheelUp = e->angleDelta().y() > 0;
    QSpinBox::wheelEvent(e);
    m_wheelEvent = false;
}
