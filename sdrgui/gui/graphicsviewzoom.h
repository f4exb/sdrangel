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

#ifndef SDRGUI_GUI_GRAPHICSVIEWZOOM_H
#define SDRGUI_GUI_GRAPHICSVIEWZOOM_H

#include <QObject>
#include <QGraphicsView>

#include "export.h"

// GraphicsView that allows scroll wheel and pinch gesture to be used for zoom
// https://stackoverflow.com/questions/19113532/qgraphicsview-zooming-in-and-out-under-mouse-position-using-mouse-wheel
class SDRGUI_API GraphicsViewZoom : public QObject {
    Q_OBJECT

public:
    GraphicsViewZoom(QGraphicsView* view);
    void gentleZoom(double factor);
    void setModifiers(Qt::KeyboardModifiers modifiers);
    void setZoomFactorBase(double value);

private:
    QGraphicsView* m_view;
    Qt::KeyboardModifiers m_modifiers;
    double m_zoomFactorBase;
    QPointF m_targetScenePos, m_targetViewportPos;
    bool eventFilter(QObject* object, QEvent* event) override;

signals:
    void zoomed();
};

#endif // SDRGUI_GUI_GRAPHICSVIEWZOOM_H
