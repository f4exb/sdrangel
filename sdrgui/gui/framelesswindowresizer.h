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

#ifndef SDRGUI_FRAMELESSWINDOWRESIZER_H_
#define SDRGUI_FRAMELESSWINDOWRESIZER_H_

#include "export.h"

#include <QEvent>
#include <QMouseEvent>
#include <QWidget>

// Class to allow frameless windows (Qt::FramelessWindowHint) to be resized
// by clicking and draging on the border
// The window needs to forward the mousePressEvent, mouseReleaseEvent, mouseMoveEvent
// and leaveEvent events to this class
// Child widgets should have mouse tracking enabled, so cursor can be controlled properly
// This can be achieved by calling enableChildMouseTracking
class SDRGUI_API FramelessWindowResizer
{
private:
    QWidget *m_widget;        // Widget to be resized
    bool m_vResizing;         // Whether we are resizing vertically
    bool m_hResizing;
    bool m_vMove;             // Whether we are moving vertically
    bool m_hMove;
    QPoint m_mouseOffsetToPos;// Offset from mouse position to top/left of window
    QPoint m_initialMousePos; // Position of mouse when resize started
    QRect m_initRect;         // Dimensions of widget when resize started
    QPoint m_origBottomRight; // So we do not "push" the widget when resizing from top or left
    const QCursor *m_cursor;  // Current cursor

    const QCursor m_vCursor;  // Ideally static, but when run:
    const QCursor m_hCursor;  // QPixmap: Must construct a QGuiApplication before a QPixmap
    const QCursor m_bCursor;
    const QCursor m_fCursor;

public:
    FramelessWindowResizer(QWidget *widget);
    void enableChildMouseTracking();
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void leaveEvent(QEvent* event);

    const int m_gripSize = 2;   // Size in pixels of the border of the window that can be clicked in to resize it

protected:
    bool mouseOnTopBorder(QPoint pos) const;
    bool mouseOnBottomBorder(QPoint pos) const;
    bool mouseOnLeftBorder(QPoint pos) const;
    bool mouseOnRightBorder(QPoint pos) const;
    bool mouseOnBorder(QPoint pos) const;
    void setCursor(const QCursor &cursor);
    void clearCursor();

};

#endif // SDRGUI_FRAMELESSWINDOWRESIZER_H_
