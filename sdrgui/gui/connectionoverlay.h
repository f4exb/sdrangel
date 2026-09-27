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
#include <QWidget>

class QMdiArea;
class QMdiSubWindow;

// A non-interactive drawing layer above one workspace's MDI windows, showing connections between windows.
class ConnectionOverlay : public QWidget
{
    Q_OBJECT
public:
    enum Kind { DeviceLink, PipeLink };
    enum Direction { NoDirection, Forward, Reverse, BothDirections };

    struct Connection {
        QPointer<QMdiSubWindow> source;
        QPointer<QMdiSubWindow> target;
        QString sourceLabel;
        QString targetLabel;
        QString flowLabel;
        int sourceWorkspace = -1;
        int targetWorkspace = -1;
        Kind kind = DeviceLink;
        Direction direction = Forward;
        QColor color;
    };

    explicit ConnectionOverlay(QMdiArea *mdi);
    void setConnections(const QList<Connection>& connections);
    void setOverlayVisible(bool visible);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    // Geometry for one drawn connection, cached until windows move or connections change.
    struct Route {
        QPainterPath path;
        qreal length = 0;
        QRect bounds;
        QPointF from;
        QPointF to;
        QColor color;
        Kind kind = DeviceLink;
        Direction direction = Forward;
        QString flowLabel;
        QRectF flowLabelRect;
        QString marker;
        QRectF markerRect;
        // Inputs to connectionPath(), kept so a stub can be rerouted to follow its label.
        int fromSide = 0;
        int toSide = 0;
        QRectF sourceBox;
        QRectF targetBox;
        int stubTip = 0;        // Cross-workspace stub: 1 if to is its tip, -1 if from is
    };
    struct Pulse {
        QPointF tail;
        QPointF head;
        QColor color;
    };

    QMdiArea *m_mdi;
    QList<Connection> m_connections;
    QHash<QMdiSubWindow*, QPointer<QWidget>> m_indexLabels;
    QList<QPointer<QObject>> m_watched;
    QVector<Route> m_routes;
    QRegion m_clipRegion;       // Everything except the title ID labels
    QRegion m_pulseRegion;      // Area covered by the pulses last painted
    qreal m_phase = 0;
    bool m_layoutDirty = true;       // Cached routes no longer match the windows
    bool m_relayoutPending = false;  // A queued relayout() is outstanding
    QElapsedTimer m_clock;
    QTimer m_animationTimer;

    static bool sameConnections(const QList<Connection>& a, const QList<Connection>& b);
    void watchWindows();
    void invalidateLayout();
    void relayout();
    bool computeLayout(); // Returns true if the mask changed
    QVector<Pulse> pulses() const;
    QRegion pulsesRegion(const QVector<Pulse>& pulses) const;
    void animate();
};

#endif // SDRGUI_GUI_CONNECTIONOVERLAY_H_
