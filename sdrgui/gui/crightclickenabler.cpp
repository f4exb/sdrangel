///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QMouseEvent>
#include <QGestureEvent>
#include <QGesture>
#include <QTapAndHoldGesture>

#include "crightclickenabler.h"

CRightClickEnabler::CRightClickEnabler(QWidget *widget) :
    QObject(widget),
    m_widget(widget),
    m_mousePressed(false)
{
    m_widget->installEventFilter(this);
    m_widget->grabGesture(Qt::TapAndHoldGesture);
}

bool CRightClickEnabler::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        auto mouseEvent = (QMouseEvent*) event;

        if (mouseEvent->button() == Qt::RightButton)
        {
            emit rightClick(mouseEvent->globalPos());
            mouseEvent->setAccepted(true);
            return true;
        }

        if (mouseEvent->button() == Qt::LeftButton)
        {
            if (mouseEvent->source() == Qt::MouseEventNotSynthesized) {
                m_mousePressed = true;
            } else {
                m_mousePressed = false; // Mouse event generated from touch event
            }
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        auto mouseEvent = (QMouseEvent*) event;

        if (mouseEvent->button() == Qt::RightButton)
        {
            mouseEvent->setAccepted(true);
            return true;
        }

        if (mouseEvent->button() == Qt::LeftButton) {
            m_mousePressed = false;
        }
    }
    else if (event->type() == QEvent::Gesture)
    {
        // Use tap and hold as right click on touch screens
        if (!m_mousePressed)
        {
            QGestureEvent *gestureEvent = static_cast<QGestureEvent *>(event);
            if (QTapAndHoldGesture *tapAndHold = static_cast<QTapAndHoldGesture *>(gestureEvent->gesture(Qt::TapAndHoldGesture)))
            {
                if (tapAndHold->state() == Qt::GestureFinished) {
                    emit rightClick(tapAndHold->position().toPoint());
                }
                return true;
            }
        }
    }
    else if (event->type() == QEvent::ContextMenu)
    {
        // Filter ContextMenu events, so we don't get popup menus as well
        return true;
    }

    return QObject::eventFilter(obj, event);
}
