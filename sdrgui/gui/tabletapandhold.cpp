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

#include <QTableWidget>
#include <QHeaderView>
#include <QGestureEvent>
#include <QGesture>
#include <QTapAndHoldGesture>

#include "tabletapandhold.h"

TableTapAndHold::TableTapAndHold(QTableWidget *table) :
    QObject(table),
    m_table(table)
{
    m_table->installEventFilter(this);
    m_table->grabGesture(Qt::TapAndHoldGesture);
};

bool TableTapAndHold::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Gesture)
    {
        QGestureEvent *gestureEvent = static_cast<QGestureEvent *>(event);
        if (QTapAndHoldGesture *tapAndHoldGesture = static_cast<QTapAndHoldGesture *>(gestureEvent->gesture(Qt::TapAndHoldGesture)))
        {
            // Map from global position to item position
            QPoint point = m_table->mapFromGlobal(tapAndHoldGesture->position().toPoint());
            QHeaderView *hHeader = m_table->horizontalHeader();
            QHeaderView *vHeader = m_table->verticalHeader();
            if (hHeader) {
                point.setY(point.y() - hHeader->height());
            }
            if (vHeader) {
                point.setX(point.x() - vHeader->width());
            }
            emit tapAndHold(point);
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}
