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

#include "connectionoverlay.h"

#include <QEvent>
#include <QHash>
#include <QPair>
#include <QLineF>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPaintEvent>
#include <QRegion>
#include <QVector>
#include <QtMath>

ConnectionOverlay::ConnectionOverlay(QMdiArea *mdi) :
    QWidget(mdi),
    m_mdi(mdi)
{
    // QMdiArea scrolls its viewport children; keep the overlay fixed above the viewport.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setGeometry(mdi->viewport()->geometry());
    mdi->viewport()->installEventFilter(this);
    m_animationTimer.setInterval(33);
    connect(&m_animationTimer, &QTimer::timeout, this, &ConnectionOverlay::animate);
    m_clock.start();
    hide();
}

bool ConnectionOverlay::sameConnections(const QList<Connection>& a, const QList<Connection>& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (int i = 0; i < a.size(); ++i)
    {
        const Connection& x = a[i];
        const Connection& y = b[i];
        if (x.source.data() != y.source.data() || x.target.data() != y.target.data() ||
            x.sourceLabel != y.sourceLabel || x.targetLabel != y.targetLabel ||
            x.flowLabel != y.flowLabel || x.sourceWorkspace != y.sourceWorkspace ||
            x.targetWorkspace != y.targetWorkspace || x.kind != y.kind ||
            x.direction != y.direction || x.color != y.color) {
            return false;
        }
    }
    return true;
}

void ConnectionOverlay::setConnections(const QList<Connection>& connections)
{
    // Connections are refreshed periodically; only relayout when something changed.
    if (sameConnections(connections, m_connections)) {
        return;
    }
    m_connections = connections;
    m_indexLabels.clear();
    // Cache each title ID widget so paths can anchor beside it and avoid painting over it.
    for (const Connection& connection : m_connections)
    {
        for (QMdiSubWindow *window : {connection.source.data(), connection.target.data()})
        {
            if (window && !m_indexLabels.contains(window)) {
                m_indexLabels.insert(window, window->findChild<QWidget*>("connectionIndexLabel"));
            }
        }
    }
    watchWindows();
    invalidateLayout();
}

void ConnectionOverlay::watchWindows()
{
    // Cached routes depend on the connected windows' geometry and their ID labels.
    for (const QPointer<QObject>& object : m_watched)
    {
        if (object) {
            object->removeEventFilter(this);
        }
    }
    m_watched.clear();
    for (auto it = m_indexLabels.constBegin(); it != m_indexLabels.constEnd(); ++it)
    {
        for (QObject *object : {static_cast<QObject*>(it.key()), static_cast<QObject*>(it.value().data())})
        {
            if (object)
            {
                object->installEventFilter(this);
                m_watched.append(object);
            }
        }
    }
}

void ConnectionOverlay::setOverlayVisible(bool visible)
{
    setVisible(visible);
    if (visible)
    {
        setGeometry(m_mdi->viewport()->geometry());
        raise();
        m_layoutDirty = true;
        relayout();
        m_animationTimer.start();
    }
    else
    {
        m_animationTimer.stop();
        m_pulseRegion = QRegion();
    }
}

bool ConnectionOverlay::eventFilter(QObject *object, QEvent *event)
{
    const QEvent::Type type = event->type();
    if (object == m_mdi->viewport())
    {
        if (type == QEvent::Resize || type == QEvent::Move || type == QEvent::Show)
        {
            setGeometry(m_mdi->viewport()->geometry());
            if (isVisible()) {
                raise();
            }
            invalidateLayout();
        }
        else if (type == QEvent::ChildAdded || type == QEvent::ChildRemoved)
        {
            invalidateLayout(); // A window joined or left this workspace.
        }
    }
    else if (type == QEvent::Move || type == QEvent::Resize || type == QEvent::Show ||
             type == QEvent::Hide || type == QEvent::WindowStateChange || type == QEvent::ParentChange)
    {
        invalidateLayout(); // A connected window or its ID label moved, e.g. when scrolling.
    }
    return QWidget::eventFilter(object, event);
}

void ConnectionOverlay::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange || event->type() == QEvent::PaletteChange) {
        invalidateLayout();
    }
    QWidget::changeEvent(event);
}

void ConnectionOverlay::invalidateLayout()
{
    // Remember the cache is stale even while hidden (e.g. in a background dock tab),
    // so showEvent() can relayout. Coalesce bursts (e.g. scrolling) into one relayout.
    m_layoutDirty = true;
    if (!isVisible() || m_relayoutPending) {
        return;
    }
    m_relayoutPending = true;
    QMetaObject::invokeMethod(this, [this]() { relayout(); }, Qt::QueuedConnection);
}

void ConnectionOverlay::relayout()
{
    m_relayoutPending = false;
    if (!isVisible() || !m_layoutDirty) {
        return;
    }
    computeLayout();
    update();
}

void ConnectionOverlay::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // Changes while hidden were not laid out. The viewport's own Show event arrives
    // before this widget is visible, so it cannot trigger the relayout itself.
    if (m_layoutDirty)
    {
        computeLayout();
        update();
    }
}

static QPointF viewportEdge(const QRectF& bounds, const QPointF& outside)
{
    return QPointF(qBound(bounds.left(), outside.x(), bounds.right()),
        qBound(bounds.top(), outside.y(), bounds.bottom()));
}

static QString scrollArrow(const QRectF& bounds, const QPointF& outside)
{
    if (outside.y() < bounds.top()) {
        return QString::fromUtf8("↑");
    }
    if (outside.y() > bounds.bottom()) {
        return QString::fromUtf8("↓");
    }
    if (outside.x() < bounds.left()) {
        return QString::fromUtf8("←");
    }
    return QString::fromUtf8("→");
}

static QPainterPath roundedRoute(const QVector<QPointF>& points)
{
    QPainterPath path(points.first());
    for (int i = 1; i + 1 < points.size(); ++i) 
    {
        const QPointF incoming = points[i] - points[i - 1];
        const QPointF outgoing = points[i + 1] - points[i];
        const qreal incomingLength = QLineF(points[i - 1], points[i]).length();
        const qreal outgoingLength = QLineF(points[i], points[i + 1]).length();
        if (incomingLength < 1 || outgoingLength < 1 ||
            qAbs(incoming.x() * outgoing.y() - incoming.y() * outgoing.x()) < 0.01) 
        {
            path.lineTo(points[i]);
            continue;
        }
        const qreal radius = qMin<qreal>(18, qMin(incomingLength, outgoingLength) / 2);
        path.lineTo(points[i] - incoming * (radius / incomingLength));
        path.quadTo(points[i], points[i] + outgoing * (radius / outgoingLength));
    }
    path.lineTo(points.last());
    return path;
}

static int crossingCost(const QPainterPath& path, const QRectF& sourceBox, const QRectF& targetBox)
{
    // Count samples drawn over a window body. An endpoint can be inside a window,
    // e.g. the source's own title bar, or an ID label covered by an overlapping
    // window. The route may then leave that window at the start, or arrive in it
    // at the end, but any other part inside a window is a crossing.
    constexpr int samples = 80;
    QPointF points[samples + 1];
    for (int i = 0; i <= samples; ++i) {
        points[i] = path.pointAtPercent(qreal(i) / samples);
    }
    int cost = 0;
    for (const QRectF& box : {sourceBox, targetBox})
    {
        if (box.isEmpty()) {
            continue;
        }
        const QRectF body = box.adjusted(-2, -2, 2, 2);
        int first = 0;
        while (first <= samples && body.contains(points[first])) {
            ++first;    // Leading run inside, leaving the start window
        }
        int last = samples;
        while (last >= first && body.contains(points[last])) {
            --last;     // Trailing run inside, arriving in the end window
        }
        for (int i = first; i <= last; ++i)
        {
            if (body.contains(points[i])) {
                ++cost;
            }
        }
    }
    return cost;
}

static QPainterPath connectionPath(const QPointF& from, const QPointF& to,
                                   int fromSide, int toSide, const QRectF& sourceBox,
                                   const QRectF& targetBox, const QRectF& bounds)
{
    constexpr qreal labelLead = 14;
    const QPointF curveFrom = from + QPointF(fromSide * labelLead, 0);
    const QPointF curveTo = to + QPointF(toSide * labelLead, 0);
    const QPointF delta = curveTo - curveFrom;
    const qreal handle = qBound<qreal>(32, qAbs(delta.x()) * 0.35 + qAbs(delta.y()) * 0.3, 180);
    const QPointF firstControl = fromSide == 0 ? curveFrom + delta * 0.3 :
        curveFrom + QPointF(fromSide * handle, 0);
    const QPointF lastControl = toSide == 0 ? curveTo - delta * 0.3 :
        curveTo + QPointF(toSide * handle, 0);
    QPainterPath direct(from);
    if (fromSide != 0) {
        direct.lineTo(curveFrom);
    }
    direct.cubicTo(firstControl, lastControl, curveTo);
    if (toSide != 0) {
        direct.lineTo(to);
    }
    if (sourceBox.isEmpty() || targetBox.isEmpty()) {
        return direct;
    }
    // Prefer the route that crosses windows least, then the shortest. Crossing can
    // be unavoidable (e.g. an endpoint covered by the other window), and then the
    // direct route is usually clearer than a long detour around both windows.
    QPainterPath bestRoute = direct;
    int bestCost = crossingCost(direct, sourceBox, targetBox);
    qreal bestLength = direct.length();
    if (bestCost == 0) {
        return direct;
    }
    auto consider = [&](const QPainterPath& candidate) {
        const qreal length = candidate.length();
        if (bestCost == 0 && length >= bestLength) {
            return; // Cannot beat a shorter clear route
        }
        const int cost = crossingCost(candidate, sourceBox, targetBox);
        if (cost < bestCost || (cost == bestCost && length < bestLength))
        {
            bestRoute = candidate;
            bestCost = cost;
            bestLength = length;
        }
    };

    // Try an outer route around both windows, and nearer horizontal lanes.
    constexpr qreal clearance = 24;
    const bool separatedBelow = sourceBox.bottom() + 2 * clearance < targetBox.top();
    const bool separatedAbove = targetBox.bottom() + 2 * clearance < sourceBox.top();
    const qreal laneY = separatedBelow ? (sourceBox.bottom() + targetBox.top()) / 2 :
        (separatedAbove ? (targetBox.bottom() + sourceBox.top()) / 2 :
            qMax(sourceBox.bottom(), targetBox.bottom()) + clearance);
    const qreal rightX = qMax(sourceBox.right(),
        separatedBelow || separatedAbove ? sourceBox.right() : targetBox.right()) + clearance;
    const qreal leftX = qMin(to.x() - 30, targetBox.left() - clearance);
    QVector<QPointF> route;
    route.append(from);
    const qreal topY = qMin(sourceBox.top(), targetBox.top()) - clearance;
    if (topY >= bounds.top() + 4) 
    {
        route.append(QPointF(from.x() + clearance, from.y()));
        route.append(QPointF(from.x() + clearance, topY));
        route.append(QPointF(rightX, topY));
    } 
    else 
    {
        route.append(QPointF(rightX, from.y()));
    }
    route.append(QPointF(rightX, laneY));
    route.append(QPointF(leftX, laneY));
    route.append(QPointF(leftX, to.y()));
    route.append(to);
    consider(roundedRoute(route));
    const qreal exitXs[] = {
        from.x() + clearance,
        qMax(from.x() + clearance, targetBox.right() + clearance),
        qMax(from.x() + clearance, sourceBox.right() + clearance)
    };
    const qreal laneYs[] = {
        topY, sourceBox.top() - clearance, targetBox.top() - clearance,
        laneY, sourceBox.bottom() + clearance, targetBox.bottom() + clearance
    };
    for (qreal candidateY : laneYs) 
    {
        if (candidateY < bounds.top() + 4 || candidateY > bounds.bottom() - 4 ||
            qAbs(candidateY - from.y()) < 6) {
            continue;
        }
        for (qreal exitX : exitXs) 
        {
            // A downward exit within the source would run through its controls.
            const bool insideSourceX = exitX > sourceBox.left() && exitX < sourceBox.right();
            const bool throughSourceBody = insideSourceX &&
                qMax(qMin(from.y(), candidateY), sourceBox.top() + 26) <
                    qMin(qMax(from.y(), candidateY), sourceBox.bottom());
            if (throughSourceBody) {
                continue;
            }
            consider(roundedRoute({
                from,
                QPointF(exitX, from.y()),
                QPointF(exitX, candidateY),
                QPointF(leftX, candidateY),
                QPointF(leftX, to.y()),
                to
            }));
        }
    }
    // A source directly below its target has no upper lane. Turn just below
    // the source title and leave on the left, instead of circling both windows.
    if (targetBox.bottom() <= sourceBox.top() + clearance) {
        const qreal leftExitX = qMin(sourceBox.left(), targetBox.left()) - clearance;
        const qreal turnY = qMax(from.y() + clearance, targetBox.bottom() + clearance);
        if (leftExitX >= bounds.left() + 4 && turnY <= bounds.bottom() - 4) {
            consider(roundedRoute({
                from,
                QPointF(from.x() + clearance, from.y()),
                QPointF(from.x() + clearance, turnY),
                QPointF(leftExitX, turnY),
                QPointF(leftExitX, to.y()),
                to
            }));
        }
    }
    return bestRoute;
}

bool ConnectionOverlay::computeLayout()
{
    m_layoutDirty = false;
    m_routes.clear();
    const QList<QMdiSubWindow*> localWindows = m_mdi->subWindowList();
    const QRectF bounds = rect().adjusted(8, 8, -8, -8);
    const QPoint viewportOffset = m_mdi->viewport()->pos() - pos();
    const QFontMetrics metrics(font());
    m_clipRegion = QRegion(rect());
    QRegion paintedRegion;
    QVector<QRectF> labelObstacles; // Title ID labels, then each label as it is placed
    // Stubs to other workspaces that leave the same anchor fan out vertically, so
    // their lines and labels do not sit on top of each other.
    QHash<QPair<QMdiSubWindow*, int>, int> stubCounts;
    const qreal stubSpacing = metrics.height() + 14;
    auto fanOffset = [&](QMdiSubWindow *window, int side) -> qreal {
        const int n = stubCounts[qMakePair(window, side)]++;
        const int step = (n + 1) / 2;       // 0, 1, 1, 2, 2, ...
        return (n % 2 ? step : -step) * stubSpacing;   // 0, +1, -1, +2, -2, ...
    };
    QPainterPathStroker maskStroker;
    maskStroker.setWidth(20);
    maskStroker.setCapStyle(Qt::RoundCap);
    maskStroker.setJoinStyle(Qt::RoundJoin);
    for (auto it = m_indexLabels.constBegin(); it != m_indexLabels.constEnd(); ++it)
    {
        QMdiSubWindow *window = it.key();
        QWidget *indexLabel = it.value().data();
        if (!window || !localWindows.contains(window) || !indexLabel || !indexLabel->isVisible()) {
            continue;
        }
        const QPoint labelTopLeft = window->geometry().topLeft() +
            indexLabel->mapTo(window, QPoint(0, 0)) + viewportOffset;
        const QRect indexRect = QRect(labelTopLeft, indexLabel->size()).adjusted(-2, -2, 2, 2);
        m_clipRegion -= indexRect;
        labelObstacles.append(indexRect);
    }
    // The overlay is a sibling of the scrolling viewport, so translate MDI
    // window coordinates before using their title labels as endpoints.
    auto anchorFor = [this, &viewportOffset](QMdiSubWindow *window, int side) -> QPointF {
        QWidget *indexLabel = m_indexLabels.value(window).data();
        const QPoint insideWindow = indexLabel && indexLabel->isVisible() ?
            indexLabel->mapTo(window, QPoint(side < 0 ? -10 : indexLabel->width() + 10,
                indexLabel->height() / 2)) :
            QPoint(side < 0 ? -10 : 48, 12);
        return QPointF(window->geometry().topLeft() + insideWindow + viewportOffset);
    };

    for (const Connection& connection : m_connections)
    {
        if (!connection.source || !connection.target) {
            continue;
        }
        const bool sourceLocal = localWindows.contains(connection.source.data());
        const bool targetLocal = localWindows.contains(connection.target.data());
        if (!sourceLocal && !targetLocal) {
            continue;
        }
        if (sourceLocal && (connection.source->parentWidget() != m_mdi->viewport() ||
                            !connection.source->isVisible() || connection.source->isMinimized())) {
            continue;
        }
        if (targetLocal && (connection.target->parentWidget() != m_mdi->viewport() ||
                            !connection.target->isVisible() || connection.target->isMinimized())) {
            continue;
        }
        const int sourceSide = 1;  // Device ID, or the producer of a pipe.
        const int targetSide = -1;  // Inputs meet the left side of channel, spectrum, and feature IDs.
        const QPointF sourceAnchor = sourceLocal ? anchorFor(connection.source.data(), sourceSide) : QPointF();
        const QPointF targetAnchor = targetLocal ? anchorFor(connection.target.data(), targetSide) : QPointF();
        const bool sourceVisible = sourceLocal && bounds.contains(sourceAnchor);
        const bool targetVisible = targetLocal && bounds.contains(targetAnchor);
        // This workspace draws a link only when at least one endpoint is on screen.
        if (!sourceVisible && !targetVisible) {
            continue;
        }

        Route route;
        QPointF markerEdge;
        bool crossWorkspace = false;
        int stubDirection = 0;  // Which way a cross-workspace stub goes from its window
        // Scrolled endpoints point to a viewport edge. An endpoint in another
        // workspace instead gets a short labeled stub beside the local window.
        if (sourceVisible && targetVisible)
        {
            route.from = sourceAnchor;
            route.to = targetAnchor;
        }
        else if (sourceVisible && targetLocal)
        {
            route.from = sourceAnchor;
            route.to = viewportEdge(bounds, targetAnchor);
            markerEdge = route.to;
            route.marker = scrollArrow(bounds, targetAnchor) + " " + connection.targetLabel;
        }
        else if (targetVisible && sourceLocal)
        {
            route.from = viewportEdge(bounds, sourceAnchor);
            route.to = targetAnchor;
            markerEdge = route.from;
            route.marker = scrollArrow(bounds, sourceAnchor) + " " + connection.sourceLabel;
        }
        else if (sourceVisible)
        {
            route.from = sourceAnchor;
            route.marker = QString("W%1 · %2").arg(connection.targetWorkspace).arg(connection.targetLabel);
            if (!connection.flowLabel.isEmpty()) {
                route.marker += " · " + connection.flowLabel; // One label per stub, at its tip
            }
            const qreal markerWidth = metrics.horizontalAdvance(route.marker) + 8;
            const qreal stubLength = 90;
            const qreal stubY = qBound(bounds.top() + 4, route.from.y() + fanOffset(connection.source.data(), sourceSide),
                bounds.bottom() - 4);
            // Stubs leave to the right. Near the right edge there is no room for the
            // stub and its label, so go left instead, past the window's ID label.
            stubDirection = route.from.x() + stubLength + markerWidth + 10 <= bounds.right() ? 1 : -1;
            const qreal tipX = stubDirection > 0 ? route.from.x() + stubLength :
                qMax(bounds.left() + markerWidth + 10, route.from.x() - stubLength - 60);
            route.to = QPointF(tipX, stubY);
            route.stubTip = 1;
            crossWorkspace = true;
        }
        else
        {
            route.to = targetAnchor;
            route.marker = QString("W%1 · %2").arg(connection.sourceWorkspace).arg(connection.sourceLabel);
            if (!connection.flowLabel.isEmpty()) {
                route.marker += " · " + connection.flowLabel; // One label per stub, at its tip
            }
            const qreal markerWidth = metrics.horizontalAdvance(route.marker) + 8;
            const qreal stubLength = 90;
            // Stubs arrive from the left. Near the left edge there is no room for the
            // stub and its label, so come from the right instead, past the ID label.
            stubDirection = route.to.x() - stubLength - markerWidth - 10 >= bounds.left() ? -1 : 1;
            const qreal tipX = stubDirection < 0 ? route.to.x() - stubLength :
                qMin(bounds.right() - markerWidth - 10, route.to.x() + stubLength + 60);
            const qreal stubY = qBound(bounds.top() + 4, route.to.y() + fanOffset(connection.target.data(), targetSide),
                bounds.bottom() - 4);
            route.from = QPointF(tipX, stubY);
            route.stubTip = -1;
            crossWorkspace = true;
        }
        const QPointF& from = route.from;
        const QPointF& to = route.to;

        route.color = connection.color.isValid() ? connection.color.lighter(150) :
            (connection.kind == PipeLink ? QColor(238, 183, 77) : QColor(56, 204, 228));
        route.kind = connection.kind;
        route.direction = connection.direction;
        route.sourceBox = sourceVisible ?
            QRectF(connection.source->geometry().translated(viewportOffset)) : QRectF();
        route.targetBox = targetVisible ?
            QRectF(connection.target->geometry().translated(viewportOffset)) : QRectF();
        // A stub flipped to the other side goes straight out, behind its ID label.
        route.fromSide = sourceVisible && stubDirection >= 0 ? sourceSide : 0;
        route.toSide = targetVisible && stubDirection <= 0 ? targetSide : 0;
        route.path = connectionPath(from, to, route.fromSide, route.toSide,
            route.sourceBox, route.targetBox, bounds);
        route.length = route.path.length();

        if (!connection.flowLabel.isEmpty() && !crossWorkspace) // Stubs include it in their marker
        {
            const qreal labelWidth = metrics.horizontalAdvance(connection.flowLabel) + 10;
            const qreal labelHeight = metrics.height() + 4;
            // Beside each route's own midpoint, so labels of fanned stubs separate too.
            const QPointF midpoint = route.path.pointAtPercent(0.5);
            const qreal labelX = midpoint.x() - labelWidth / 2;
            const qreal labelY = midpoint.y() - labelHeight - 6;
            route.flowLabel = connection.flowLabel;
            route.flowLabelRect = QRectF(qBound(bounds.left() + 4, labelX,
                                             qMax(bounds.left() + 4, bounds.right() - labelWidth - 4)),
                                         qBound(bounds.top() + 4, labelY,
                                             qMax(bounds.top() + 4, bounds.bottom() - labelHeight - 4)),
                                         labelWidth, labelHeight);
        }

        if (!route.marker.isEmpty())
        {
            const qreal labelWidth = metrics.horizontalAdvance(route.marker) + 8;
            const qreal labelHeight = metrics.height() + 4;
            const QPointF remoteAnchor = sourceVisible ? to : from;
            // A stub's label continues from its tip, in the direction the stub goes.
            const qreal labelX = crossWorkspace ?
                (stubDirection > 0 ? remoteAnchor.x() + 6 : remoteAnchor.x() - labelWidth - 6) :
                (markerEdge.x() >= bounds.right() - 1 ?
                    markerEdge.x() - labelWidth - 8 : markerEdge.x() + 8);
            const qreal labelY = crossWorkspace ?
                remoteAnchor.y() - labelHeight / 2 :
                (markerEdge.y() >= bounds.bottom() - 1 ?
                    markerEdge.y() - labelHeight - 8 : markerEdge.y() + 8);
            route.markerRect = QRectF(qBound(bounds.left() + 4, labelX,
                                          qMax(bounds.left() + 4, bounds.right() - labelWidth - 4)),
                                      qBound(bounds.top() + 4, labelY,
                                          qMax(bounds.top() + 4, bounds.bottom() - labelHeight - 4)),
                                      labelWidth, labelHeight);
        }

        m_routes.append(route);
    }

    // Resolve remaining label overlaps. Place workspace and scroll markers first, as
    // they sit at route ends, then flow labels. Each label moves up or down by whole
    // label heights to the nearest free position inside the viewport.
    auto placeLabel = [&](QRectF& label) {
        if (label.isEmpty()) {
            return;
        }
        const qreal step = label.height() + 4;
        for (int attempt = 0; attempt < 16; ++attempt)
        {
            const int n = (attempt + 1) / 2;
            const QRectF candidate = label.translated(0, (attempt % 2 ? n : -n) * step);
            if (candidate.top() < bounds.top() + 4 || candidate.bottom() > bounds.bottom() - 4) {
                continue;
            }
            bool clear = true;
            for (const QRectF& obstacle : labelObstacles)
            {
                if (candidate.adjusted(-2, -2, 2, 2).intersects(obstacle))
                {
                    clear = false;
                    break;
                }
            }
            if (clear)
            {
                label = candidate;
                break;
            }
        }
        labelObstacles.append(label);
    };
    for (Route& route : m_routes)
    {
        const qreal before = route.markerRect.center().y();
        placeLabel(route.markerRect);
        const qreal dy = route.markerRect.center().y() - before;
        if (route.stubTip != 0 && !qFuzzyIsNull(dy))
        {
            // Keep a moved stub label on the end of its own line.
            QPointF& tip = route.stubTip > 0 ? route.to : route.from;
            tip.ry() += dy;
            route.path = connectionPath(route.from, route.to, route.fromSide, route.toSide,
                route.sourceBox, route.targetBox, bounds);
            route.length = route.path.length();
        }
    }
    for (Route& route : m_routes) {
        placeLabel(route.flowLabelRect);
    }
    // Only now are the routes final, so build the painted region.
    for (Route& route : m_routes)
    {
        QRegion routeRegion(maskStroker.createStroke(route.path).toFillPolygon().toPolygon(), Qt::WindingFill);
        routeRegion += QRectF(route.from.x() - 9, route.from.y() - 9, 18, 18).toAlignedRect();
        routeRegion += QRectF(route.to.x() - 9, route.to.y() - 9, 18, 18).toAlignedRect();
        for (const QRectF& label : {route.markerRect, route.flowLabelRect})
        {
            if (!label.isEmpty()) {
                routeRegion += label.toAlignedRect().adjusted(-2, -2, 2, 2);
            }
        }
        route.bounds = routeRegion.boundingRect();
        paintedRegion += routeRegion;
    }

    paintedRegion &= m_clipRegion;
    // A full-size widget above QOpenGLWidget spectra can occlude their entire
    // viewport. Give the overlay only the pixels needed by its drawing.
    if (paintedRegion.isEmpty()) {
        paintedRegion = QRegion(0, 0, 1, 1);
    }
    if (mask() != paintedRegion)
    {
        // An empty mask means the whole widget was painted.
        const QRegion oldMask = mask().isEmpty() ? QRegion(rect()) : mask();
        setMask(paintedRegion);
        // setMask() asks the QMdiArea to repaint what the overlay no longer covers,
        // but the opaque viewport above it is not repainted, leaving trails of old
        // routes. Repaint the viewport (and its sub windows) there explicitly.
        const QRegion exposed = oldMask - paintedRegion;
        if (!exposed.isEmpty()) {
            m_mdi->viewport()->update(exposed.translated(pos() - m_mdi->viewport()->pos()));
        }
        return true;
    }
    return false;
}

QVector<ConnectionOverlay::Pulse> ConnectionOverlay::pulses() const
{
    QVector<Pulse> result;
    for (const Route& route : m_routes)
    {
        if (route.direction == NoDirection || route.length <= 32) {
            continue;
        }
        const int flowCount = route.direction == BothDirections ? 2 : 1;
        const int pulsesPerFlow = route.length > 360 ? 2 : 1;
        for (int flow = 0; flow < flowCount; ++flow)
        {
            const bool reverse = route.direction == Reverse || (route.direction == BothDirections && flow == 1);
            for (int pulse = 0; pulse < pulsesPerFlow; ++pulse)
            {
                qreal progress = m_phase + qreal(pulse) / pulsesPerFlow;
                if (progress >= 1.0) {
                    progress -= 1.0;
                }
                if (reverse) {
                    progress = 1.0 - progress;
                }
                const qreal tailProgress = qBound<qreal>(0, progress + (reverse ? 0.025 : -0.025), 1);
                result.append({route.path.pointAtPercent(tailProgress), route.path.pointAtPercent(progress), route.color});
            }
        }
    }
    return result;
}

QRegion ConnectionOverlay::pulsesRegion(const QVector<Pulse>& pulses) const
{
    QRegion region;
    for (const Pulse& pulse : pulses) {
        region += QRectF(pulse.tail, pulse.head).normalized().adjusted(-9, -9, 9, 9).toAlignedRect();
    }
    return region;
}

void ConnectionOverlay::animate()
{
    if (!isVisible()) {
        return; // Workspace is a background tab, or minimized.
    }
    // Only the pulses move, so repaint just where they were and where they are now.
    m_phase = (m_clock.elapsed() % 2400) / 2400.0;
    const QRegion region = pulsesRegion(pulses());
    const QRegion dirty = m_pulseRegion | region;
    m_pulseRegion = region;
    if (!dirty.isEmpty()) {
        update(dirty);
    }
}

void ConnectionOverlay::paintEvent(QPaintEvent *event)
{
    // Paint current geometry rather than a stale cache. This paint is clipped to the
    // old mask, so repaint again only if the mask changed.
    if (m_layoutDirty && computeLayout()) {
        update();
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setClipRegion(m_clipRegion & event->region());
    const QRegion& dirty = event->region();

    for (const Route& route : m_routes)
    {
        if (!dirty.intersects(route.bounds)) {
            continue;
        }
        const QColor& color = route.color;
        // drawPath also fills with the current brush. The preceding edge draws
        // its endpoint dots with a solid brush, so clear it before every path.
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 35), 6,
            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(route.path);
        painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 180), 2.2,
            route.kind == PipeLink ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(route.path);
        painter.setBrush(QColor(color.red(), color.green(), color.blue(), 220));
        painter.setPen(QPen(palette().window().color(), 1.5));
        painter.drawEllipse(route.from, 4, 4);
        painter.drawEllipse(route.to, 4, 4);
    }

    for (const Pulse& pulse : pulses())
    {
        const QColor& color = pulse.color;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 210), 4,
            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(pulse.tail, pulse.head);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(color.red(), color.green(), color.blue(), 130));
        painter.drawEllipse(pulse.head, 7, 7);
        painter.setBrush(QColor(255, 255, 255, 245));
        painter.drawEllipse(pulse.head, 3, 3);
    }

    for (const Route& route : m_routes)
    {
        if (!route.flowLabel.isEmpty() && dirty.intersects(route.flowLabelRect.toAlignedRect()))
        {
            painter.setPen(QPen(route.color, 1));
            painter.setBrush(QColor(24, 28, 32, 235));
            painter.drawRoundedRect(route.flowLabelRect, 3, 3);
            painter.setPen(Qt::white);
            painter.drawText(route.flowLabelRect, Qt::AlignCenter, route.flowLabel);
        }
        if (!route.marker.isEmpty() && dirty.intersects(route.markerRect.toAlignedRect()))
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(24, 28, 32, 235));
            painter.drawRoundedRect(route.markerRect, 3, 3);
            painter.setPen(Qt::white);
            painter.drawText(route.markerRect, Qt::AlignCenter, route.marker);
        }
    }
}
