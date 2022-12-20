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

#include <QDebug>
#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QResizeEvent>

#include "dialogpositioner.h"

DialogPositioner::DialogPositioner(QWidget *dialog, bool center) :
    QObject(dialog),
    m_dialog(dialog),
    m_center(center)
{
    connect(dialog->screen(), &QScreen::orientationChanged, this, &DialogPositioner::orientationChanged);
    dialog->screen()->setOrientationUpdateMask(Qt::PortraitOrientation
                                                | Qt::LandscapeOrientation
                                                | Qt::InvertedPortraitOrientation
                                                | Qt::InvertedLandscapeOrientation);
    if (m_center) {
        DialogPositioner::centerDialog(m_dialog);
    } else {
        DialogPositioner::positionDialog(m_dialog);
    }
    dialog->installEventFilter(this);
}

void DialogPositioner::orientationChanged(Qt::ScreenOrientation orientation)
{
    (void) orientation;

    // Need a delay before geometry() reflects new orientation
    // https://bugreports.qt.io/browse/QTBUG-109127
    QTimer::singleShot(200, [this]() {
        if (m_center) {
            DialogPositioner::centerDialog(m_dialog);
        } else {
            DialogPositioner::positionDialog(m_dialog);
        }
    });
}

void DialogPositioner::centerDialog(QWidget *dialog)
{
    // Restrict size of dialog to size of desktop
    DialogPositioner::sizeToDesktop(dialog);

    // Position in center of screen
    QRect desktop = dialog->screen()->availableGeometry();
    QSize size = dialog->size();
    QPoint pos;
    pos.setX((desktop.width() - size.width()) / 2);
    pos.setY((desktop.height() - size.height()) / 2);
    dialog->move(pos);
}

void DialogPositioner::positionDialog(QWidget *dialog)
{
    // Restrict size of dialog to size of desktop
    DialogPositioner::sizeToDesktop(dialog);

    // Position so fully on screen
    QRect desktop = dialog->screen()->availableGeometry();
    QSize size = dialog->size();
    QPoint pos = dialog->pos();

    bool move = false;
    if (pos.x() + size.width() > desktop.width())
    {
        pos.setX(desktop.width() - size.width());
        move = true;
    }
    if (pos.y() + size.height() > desktop.height())
    {
        pos.setY(desktop.height() - size.height());
        move = true;
    }
    if (move) {
        dialog->move(pos);
    }
}

void DialogPositioner::sizeToDesktop(QWidget *widget)
{
    QRect desktop = widget->screen()->availableGeometry();
    QSize size = widget->size();

    bool resize = false;
    if (size.width() > desktop.width())
    {
        size.setWidth(desktop.width());
        resize = true;
    }
    if (size.height() > desktop.height())
    {
        size.setHeight(desktop.height());
        resize = true;
    }
    if (resize) {
        widget->resize(size);
    }
}

bool DialogPositioner::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Show)
    {
        if (m_center) {
            DialogPositioner::centerDialog(m_dialog);
        } else {
            DialogPositioner::positionDialog(m_dialog);
        }
    }
    return QObject::eventFilter(obj, event);
}
