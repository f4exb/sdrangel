///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <QWidget>
#include <QDebug>
#include <QDataStream>
#include <QIODevice>

#include "mdiutils.h"

// QWidget::save/restoreGeometry doesn't work properly for MDI sub-windows
// so we have this code

QByteArray MDIUtils::saveMDIGeometry(QWidget *widget)
{
    QByteArray array;
    QDataStream stream(&array, QIODevice::WriteOnly);
    quint16 version = 1;
    stream << version
           << widget->x()
           << widget->y()
           << widget->width()
           << widget->height()
           << widget->isMaximized()
           << widget->isFullScreen();
    return array;
}

bool MDIUtils::restoreMDIGeometry(QWidget *widget, const QByteArray& geometry)
{
    if (geometry.size() == 66)
    {
        // Older versions of SDRangel used save/restoreGeometry
        return widget->restoreGeometry(geometry);
    }
    else if (geometry.size() < 4)
    {
        qDebug() << "MDIUtils::restoreMDIGeometry: geometry is invalid";
        return false;
    }
    else
    {
        QDataStream stream(geometry);
        quint16 version = 0;
        stream >> version;
        if (version != 1)
        {
            qDebug() << "MDIUtils::restoreMDIGeometry: Unsupported version" << version;
            return false;
        }

        // Restore window position
        qint32 x, y, width, height;
        stream >> x >> y >> width >> height;
        widget->move(x, y);
        widget->resize(width, height);

        // Restore window state
        // After restoring the geometry for a number of MDI windows, if one was maximized
        // but not the last to have geometry restored for, we end up with the wrong
        // window being maximized, so we don't bother trying for now
        bool maximized, fullscreen;
        stream >> maximized >> fullscreen;
        /*if (fullscreen) {
            widget->showFullScreen();
        } else if (maximized) {
            widget->showMaximized();
        } else {
            widget->showNormal();
        }*/

        return true;
    }
}
