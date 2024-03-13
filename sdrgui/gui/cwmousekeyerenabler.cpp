///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include <QEvent>
#include <QMouseEvent>

#include "cwmousekeyerenabler.h"

CWMouseKeyerEnabler::CWMouseKeyerEnabler(QWidget *widget) :
    QObject(widget),
    m_widget(widget)
{
    m_widget->installEventFilter(this);
}

bool CWMouseKeyerEnabler::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        auto mouseEvent = (QMouseEvent*) event;

        if (mouseEvent->button() == Qt::RightButton)
        {
            emit rightButtonPress(mouseEvent->globalPos());
            mouseEvent->setAccepted(true);
            return true;
        }

        if (mouseEvent->button() == Qt::LeftButton)
        {
            emit leftButtonPress(mouseEvent->globalPos());
            mouseEvent->setAccepted(true);
            return true;
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        auto mouseEvent = (QMouseEvent*) event;

        if (mouseEvent->button() == Qt::RightButton)
        {
            emit rightButtonRelease(mouseEvent->globalPos());
            mouseEvent->setAccepted(true);
            return true;
        }

        if (mouseEvent->button() == Qt::LeftButton)
        {
            emit leftButtonRelease(mouseEvent->globalPos());
            mouseEvent->setAccepted(true);
            return true;
        }
    }

    return QObject::eventFilter(obj, event);
}
