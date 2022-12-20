///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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
#include <QApplication>
#include <QScrollBar>
#include <QGestureEvent>
#include <QPinchGesture>

#include <qmath.h>

#include "graphicsviewzoom.h"

GraphicsViewZoom::GraphicsViewZoom(QGraphicsView* view) :
    QObject(view),
    m_view(view),
    m_modifiers(Qt::NoModifier),
    m_zoomFactorBase(1.0015)
{
    m_view->viewport()->installEventFilter(this);
    m_view->setMouseTracking(true);
    m_view->viewport()->grabGesture(Qt::PinchGesture);
}

void GraphicsViewZoom::gentleZoom(double factor)
{
    m_view->scale(factor, factor);
    m_view->centerOn(m_targetScenePos);
    QPointF deltaViewportPos = m_targetViewportPos - QPointF(m_view->viewport()->width() / 2.0,
                                                             m_view->viewport()->height() / 2.0);
    QPointF viewportCenter = m_view->mapFromScene(m_targetScenePos) - deltaViewportPos;
    m_view->centerOn(m_view->mapToScene(viewportCenter.toPoint()));
    emit zoomed();
}

void GraphicsViewZoom::setModifiers(Qt::KeyboardModifiers modifiers)
{
    m_modifiers = modifiers;
}

void GraphicsViewZoom::setZoomFactorBase(double value)
{
    m_zoomFactorBase = value;
}

bool GraphicsViewZoom::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::MouseMove)
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QPointF delta = m_targetViewportPos - mouseEvent->pos();
        if (qAbs(delta.x()) > 5 || qAbs(delta.y()) > 5)
        {
            m_targetViewportPos = mouseEvent->pos();
            m_targetScenePos = m_view->mapToScene(mouseEvent->pos());
        }
    }
    else if (event->type() == QEvent::Wheel)
    {
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
        if (QApplication::keyboardModifiers() == m_modifiers)
        {
            if (wheelEvent->angleDelta().y() != 0)
            {
                double angle = wheelEvent->angleDelta().y();
                double factor = qPow(m_zoomFactorBase, angle);
                gentleZoom(factor);
                return true;
            }
        }
    }
    else if (event->type() == QEvent::Gesture)
    {
        QGestureEvent *gestureEvent = static_cast<QGestureEvent *>(event);
        if (QPinchGesture *pinchGesture = static_cast<QPinchGesture *>(gestureEvent->gesture(Qt::PinchGesture)))
        {
            if (pinchGesture->changeFlags() & QPinchGesture::ScaleFactorChanged)
            {
                m_view->scale(pinchGesture->scaleFactor(), pinchGesture->scaleFactor());
                emit zoomed();
            }
            return true;
        }
    }

    Q_UNUSED(object)
    return false;
}
