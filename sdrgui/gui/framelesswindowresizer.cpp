///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#include <QGuiApplication>
#include <QLayout>

#include "framelesswindowresizer.h"

FramelessWindowResizer::FramelessWindowResizer(QWidget *widget) :
    m_widget(widget),
    m_vResizing(false),
    m_hResizing(false),
    m_vMove(false),
    m_hMove(false),
    m_cursor(nullptr),
    m_vCursor(Qt::SizeVerCursor),
    m_hCursor(Qt::SizeHorCursor),
    m_bCursor(Qt::SizeBDiagCursor),
    m_fCursor(Qt::SizeFDiagCursor)
{
}

void FramelessWindowResizer::enableChildMouseTracking()
{
    QList<QWidget *> widgets = m_widget->findChildren<QWidget *>();
    for (auto widget : widgets) {
        widget->setMouseTracking(true);
    }
}

bool FramelessWindowResizer::mouseOnTopBorder(QPoint pos) const
{
    return ((pos.y() >= 0) && (pos.y() < m_gripSize)
         && (m_widget->sizePolicy().verticalPolicy() != QSizePolicy::Fixed));
}

bool FramelessWindowResizer::mouseOnBottomBorder(QPoint pos) const
{
    return ((pos.y() > m_widget->height() - 1 - m_gripSize) && (pos.y() < m_widget->height())
         && (m_widget->sizePolicy().verticalPolicy() != QSizePolicy::Fixed));
}

bool FramelessWindowResizer::mouseOnLeftBorder(QPoint pos) const
{
    return ((pos.x() >= 0) && (pos.x() < m_gripSize)
         && (m_widget->sizePolicy().horizontalPolicy() != QSizePolicy::Fixed));
}

bool FramelessWindowResizer::mouseOnRightBorder(QPoint pos) const
{
    return ((pos.x() > m_widget->width() - 1 - m_gripSize) && (pos.x() < m_widget->width())
         && (m_widget->sizePolicy().horizontalPolicy() != QSizePolicy::Fixed));
}

bool FramelessWindowResizer::mouseOnBorder(QPoint pos) const
{
    return mouseOnTopBorder(pos) || mouseOnBottomBorder(pos)
        || mouseOnLeftBorder(pos) || mouseOnRightBorder(pos);
}

void FramelessWindowResizer::setCursor(const QCursor &cursor)
{
    if (m_cursor != &cursor)
    {
        if (m_cursor != nullptr) {
            QGuiApplication::restoreOverrideCursor();
        }
        QGuiApplication::setOverrideCursor(cursor);
        m_cursor = &cursor;
    }
}

void FramelessWindowResizer::clearCursor()
{
    if (m_cursor)
    {
        QGuiApplication::restoreOverrideCursor();
        m_cursor = nullptr;
    }
}

void FramelessWindowResizer::mousePressEvent(QMouseEvent* event)
{
    if ((event->buttons() & Qt::LeftButton) && mouseOnBorder(event->pos()))
    {
        if (mouseOnTopBorder(event->pos()) || mouseOnBottomBorder(event->pos())) {
            m_vResizing = true;
        }
        if (mouseOnLeftBorder(event->pos()) || mouseOnRightBorder(event->pos())) {
            m_hResizing = true;
        }
        if (mouseOnTopBorder(event->pos()))
        {
            m_vMove = true;
            // Calculate difference between mouse position and top left corner of widget
            m_mouseOffsetToPos = event->globalPos() - m_widget->pos();
        }
        if (mouseOnLeftBorder(event->pos()))
        {
            m_hMove = true;
            m_mouseOffsetToPos = event->globalPos() - m_widget->pos();
        }
        // Save coords of bottom/right of widget
        m_origBottomRight.setX(m_widget->pos().x() + m_widget->width());
        m_origBottomRight.setY(m_widget->pos().y() + m_widget->height());

        m_initialMousePos = event->globalPos();
        m_initRect = m_widget->rect();

        event->accept();
    }
}

void FramelessWindowResizer::mouseReleaseEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton))
    {
        m_vResizing = false;
        m_hResizing = false;
        m_vMove = false;
        m_hMove = false;
    }
}

void FramelessWindowResizer::leaveEvent(QEvent*)
{
    clearCursor();
}

void FramelessWindowResizer::mouseMoveEvent(QMouseEvent* event)
{
    if (m_vResizing || m_hResizing)
    {

        int x, y;

        // Calculate resize requested
        int w = m_initRect.width();
        int h = m_initRect.height();
        QPoint delta = event->globalPos() - m_initialMousePos;

        x = delta.x();
        y = delta.y();
        if (m_hMove) {
            x = -x;
        }
        if (m_vMove) {
            y = -y;
        }
        if (m_hResizing) {
            w += x;
        }
        if (m_vResizing) {
            h += y;
        }
        QSize reqSize(w, h);

        // Get min and max size we can resize to
        QSize minSize, maxSize;

        if (m_widget->layout())
        {
            //minSize = m_widget->layout()->minimumSize();
            maxSize = m_widget->layout()->maximumSize();
        }
        else
        {
            //minSize = m_widget->minimumSize();
            maxSize = m_widget->maximumSize();
        }
        minSize = m_widget->minimumSizeHint(); // Need to use minimumSizeHint for FlowLayout to work

        // Limit requested to size to allowed min/max
        QSize size = reqSize;
        size = size.expandedTo(minSize);
        size = size.boundedTo(maxSize);

        // Prevent vertical expansion of vertically fixed widgets
        if (m_widget->sizePolicy().verticalPolicy() == QSizePolicy::Fixed) {
            size.setHeight(m_widget->sizeHint().height());
        }

        // Prevent horizontal expansion of horizontal fixed widgets
        if (m_widget->sizePolicy().horizontalPolicy() == QSizePolicy::Fixed) {
            size.setWidth(m_widget->sizeHint().width());
        }

        // Move
        if (m_vMove || m_hMove)
        {
            if (m_hMove)
            {
                x = event->globalPos().x() - m_mouseOffsetToPos.x();

                if (x + minSize.width() > m_origBottomRight.x()) {
                    x = m_origBottomRight.x() - minSize.width();
                }
            }
            else
            {
                x = m_widget->pos().x();
            }
            if (m_vMove)
            {
                y = event->globalPos().y() - m_mouseOffsetToPos.y();

                if (y + minSize.height() > m_origBottomRight.y()) {
                    y = m_origBottomRight.y() - minSize.height();
                }
            }
            else
            {
                y = m_widget->pos().y();
            }
            m_widget->move(x, y);
        }

        m_widget->resize(size);
        event->accept();
    }
    else
    {
        QPoint pos = event->pos();
        if (mouseOnBorder(pos))
        {
            // Set cursor according to edge or corner mouse is over
            if (mouseOnTopBorder(pos) && mouseOnRightBorder(pos)) {
                setCursor(m_bCursor);
            } else if (mouseOnTopBorder(pos) && mouseOnLeftBorder(pos)) {
                setCursor(m_fCursor);
            } else if (mouseOnBottomBorder(pos) && mouseOnRightBorder(pos)) {
                setCursor(m_fCursor);
            } else if (mouseOnBottomBorder(pos) && mouseOnLeftBorder(pos)) {
                setCursor(m_bCursor);
            } else if (mouseOnTopBorder(pos) || mouseOnBottomBorder(pos)) {
                setCursor(m_vCursor);
            } else if (mouseOnLeftBorder(pos) || mouseOnRightBorder(pos)) {
                setCursor(m_hCursor);
            }
        }
        else
        {
            clearCursor();
        }
    }
}
