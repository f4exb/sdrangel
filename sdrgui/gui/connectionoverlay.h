///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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
 
#ifndef SDRGUI_GUI_CONNECTIONOVERLAY_H_
#define SDRGUI_GUI_CONNECTIONOVERLAY_H_

#include <QElapsedTimer>
#include <QColor>
#include <QHash>
#include <QList>
#include <QPainterPath>
#include <QRegion>
#include <QVector>
#include <QPointer>
#include <QTimer>
#include <QOpenGLWidget>

class QMdiArea;
class QMdiSubWindow;
class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;
class QPainter;

// A non-interactive drawing layer above one workspace's MDI windows, showing connections between windows.
// It is an OpenGL widget stacked on top, so it blends over OpenGL spectra too; a translucent
// ordinary widget shows black where it overlaps them.
class ConnectionOverlay : public QOpenGLWidget
{
    Q_OBJECT
public:
    enum Kind { DeviceLink, PipeLink };
    enum Direction { NoDirection, Forward, Reverse, BothDirections };

    struct Connection {
        QPointer<QMdiSubWindow> m_source;
        QPointer<QMdiSubWindow> m_target;
        QString m_sourceLabel;
        QString m_targetLabel;
        QString m_flowLabel;
        int m_sourceWorkspace = -1;
        int m_targetWorkspace = -1;
        Kind m_kind = DeviceLink;
        Direction m_direction = Forward;
        QColor m_color;
    };

    explicit ConnectionOverlay(QMdiArea *mdi);
    ~ConnectionOverlay() override;
    void setConnections(const QList<Connection>& connections);
    void setOverlayVisible(bool visible);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void initializeGL() override;
    void paintGL() override;

private:
    // Geometry for one drawn connection, cached until windows move or connections change.
    struct Route {
        QPainterPath m_path;
        qreal m_length = 0;
        QPointF m_from;
        QPointF m_to;
        QColor m_color;
        Kind m_kind = DeviceLink;
        Direction m_direction = Forward;
        QString m_flowLabel;
        QRectF m_flowLabelRect;
        QString m_marker;
        QRectF m_markerRect;
        // Inputs to connectionPath(), kept so a stub can be rerouted to follow its label.
        int m_fromSide = 0;
        int m_toSide = 0;
        QRectF m_sourceBox;
        QRectF m_targetBox;
        QVector<QRectF> m_obstacles;  // Other windows the route should avoid
        int m_stubTip = 0;        // Cross-workspace stub: 1 if m_to is its tip, -1 if m_from is
        QMdiSubWindow *m_source = nullptr;    // For hover highlighting; compared, never dereferenced
        QMdiSubWindow *m_target = nullptr;
        bool m_local = false;         // Both ends in this workspace, so it can be rerouted around other links
        QVector<quint32> m_cells;     // Lane cells the route reserves, see Occupancy in the .cpp
    };
    struct Pulse {
        QPointF m_tail;
        QPointF m_head;
        QColor m_color;
    };

    QMdiArea *m_mdi;
    QList<Connection> m_connections;
    QList<QPointer<QMdiSubWindow>> m_windows;  // Windows with links, to watch for moves
    QList<QPointer<QObject>> m_watched;
    QVector<Route> m_routes;
    qreal m_phase = 0;
    QPointer<QMdiSubWindow> m_hovered;  // Window under the mouse, if it has links
    bool m_highlightActive = false;     // Dim links that do not touch m_hovered
    bool m_layoutDirty = true;       // Cached routes no longer match the windows
    bool m_relayoutPending = false;  // A queued relayout() is outstanding
    QElapsedTimer m_clock;
    QTimer m_animationTimer;
    // QPainter draws premultiplied alpha, but Qt composites an OpenGL widget as straight
    // alpha, which darkens anything translucent. So paint offscreen, then copy through a
    // shader that converts to straight alpha.
    QOpenGLFramebufferObject *m_paintBuffer = nullptr;    // Multisampled, painted by QPainter
    QOpenGLFramebufferObject *m_resolveBuffer = nullptr;  // Plain texture the shader reads
    QOpenGLShaderProgram *m_unpremultiply = nullptr;

    static bool sameConnections(const QList<Connection>& a, const QList<Connection>& b);
    void watchWindows();
    void invalidateLayout();
    void relayout();
    void computeLayout();
    QVector<Pulse> pulses() const;
    void animate();
    void setHovered(QMdiSubWindow *window);
    void updateHighlight();
    bool isDimmed(const Route& route) const;
    void paintRoutes(QPainter& painter);
    void releaseGL();
};

#endif // SDRGUI_GUI_CONNECTIONOVERLAY_H_
