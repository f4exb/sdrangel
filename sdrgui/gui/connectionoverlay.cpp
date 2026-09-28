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
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLPaintDevice>
#include <QOpenGLShaderProgram>
#include <QSurfaceFormat>
#include <QPolygonF>
#include <QRegion>
#include <algorithm>
#include <QVector>
#include <QtMath>

// Opacity of links not touching the hovered window.
static constexpr qreal dimmedOpacity = 0.5;
// Channel, device, spectrum and feature title bars: a 2 px border, then a 20 px row.
static constexpr int titleBarMiddle = 12;
static constexpr int titleBarHeight = 24;
static constexpr int anchorOutset = 6;  // Routes are planned from just outside a window's frame

ConnectionOverlay::ConnectionOverlay(QMdiArea *mdi) :
    QOpenGLWidget(mdi),
    m_mdi(mdi)
{
    // QMdiArea scrolls its viewport children; keep the overlay fixed above the viewport.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    // Composited last, with its alpha, over everything including OpenGL spectra.
    setAttribute(Qt::WA_AlwaysStackOnTop);
    QSurfaceFormat surfaceFormat = format();
    surfaceFormat.setAlphaBufferSize(8);
    setFormat(surfaceFormat);
    setGeometry(mdi->viewport()->geometry());
    mdi->viewport()->installEventFilter(this);
    m_animationTimer.setInterval(33);
    connect(&m_animationTimer, &QTimer::timeout, this, &ConnectionOverlay::animate);
    m_clock.start();
    hide();
}

ConnectionOverlay::~ConnectionOverlay()
{
    releaseGL();
}

void ConnectionOverlay::initializeGL()
{
    // The context is recreated when the window changes, e.g. a workspace is floated.
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &ConnectionOverlay::releaseGL, Qt::UniqueConnection);
    m_unpremultiply = new QOpenGLShaderProgram();
    m_unpremultiply->addShaderFromSourceCode(QOpenGLShader::Vertex,
        "attribute highp vec2 position;\n"
        "varying highp vec2 coordinate;\n"
        "void main() {\n"
        "    coordinate = position * 0.5 + 0.5;\n"
        "    gl_Position = vec4(position, 0.0, 1.0);\n"
        "}\n");
    m_unpremultiply->addShaderFromSourceCode(QOpenGLShader::Fragment,
        "varying highp vec2 coordinate;\n"
        "uniform sampler2D source;\n"
        "void main() {\n"
        "    highp vec4 colour = texture2D(source, coordinate);\n"
        "    gl_FragColor = colour.a > 0.0 ? vec4(colour.rgb / colour.a, colour.a) : vec4(0.0);\n"
        "}\n");
    m_unpremultiply->link();
}

void ConnectionOverlay::releaseGL()
{
    if (!m_paintBuffer && !m_resolveBuffer && !m_unpremultiply) {
        return;
    }
    makeCurrent();
    if (m_resolveBuffer != m_paintBuffer) {
        delete m_resolveBuffer;
    }
    delete m_paintBuffer;
    delete m_unpremultiply;
    m_paintBuffer = nullptr;
    m_resolveBuffer = nullptr;
    m_unpremultiply = nullptr;
    doneCurrent();
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
        if (x.m_source.data() != y.m_source.data() || x.m_target.data() != y.m_target.data() ||
            x.m_sourceLabel != y.m_sourceLabel || x.m_targetLabel != y.m_targetLabel ||
            x.m_flowLabel != y.m_flowLabel || x.m_sourceWorkspace != y.m_sourceWorkspace ||
            x.m_targetWorkspace != y.m_targetWorkspace || x.m_kind != y.m_kind ||
            x.m_direction != y.m_direction || x.m_color != y.m_color) {
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
    m_windows.clear();
    for (const Connection& connection : m_connections)
    {
        for (QMdiSubWindow *window : {connection.m_source.data(), connection.m_target.data()})
        {
            if (window && !m_windows.contains(window)) {
                m_windows.append(window);
            }
        }
    }
    watchWindows();
    invalidateLayout();
}

void ConnectionOverlay::watchWindows()
{
    // Cached routes depend on the connected windows' geometry.
    for (const QPointer<QObject>& object : m_watched)
    {
        if (object) {
            object->removeEventFilter(this);
        }
    }
    m_watched.clear();
    for (const QPointer<QMdiSubWindow>& window : m_windows)
    {
        if (window)
        {
            window->installEventFilter(this);
            m_watched.append(window.data());
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
    else if (type == QEvent::Enter || type == QEvent::Leave)
    {
        // Qt sends Enter to a window and all its ancestors, and does not send Leave
        // when the mouse moves on to a child, so this tracks the whole window.
        if (QMdiSubWindow *window = qobject_cast<QMdiSubWindow*>(object))
        {
            if (type == QEvent::Enter) {
                setHovered(window);
            } else if (m_hovered == window) {
                setHovered(nullptr);
            }
        }
    }
    else if (type == QEvent::Move || type == QEvent::Resize || type == QEvent::Show ||
             type == QEvent::Hide || type == QEvent::WindowStateChange || type == QEvent::ParentChange)
    {
        if (type == QEvent::Hide && object == m_hovered) {
            setHovered(nullptr); // A hidden window gets no Leave event.
        }
        invalidateLayout(); // A connected window moved, e.g. when scrolling.
    }
    return QOpenGLWidget::eventFilter(object, event);
}

void ConnectionOverlay::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange || event->type() == QEvent::PaletteChange) {
        invalidateLayout();
    }
    QOpenGLWidget::changeEvent(event);
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
    QOpenGLWidget::showEvent(event);
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

// Evenly spaced points along a path. Flattening once is much cheaper than calling
// QPainterPath::pointAtPercent() per sample, which matters as every candidate route
// of every link is scored on each relayout, e.g. while a window is dragged.
static QVector<QPointF> samplePath(const QPainterPath& path, int samples, qreal& total)
{
    total = 0;
    QPolygonF line;
    for (const QPolygonF& subpath : path.toSubpathPolygons()) {
        line += subpath;
    }
    QVector<QPointF> points;
    if (line.isEmpty()) {
        return points;
    }
    QVector<qreal> cumulative(line.size(), 0.0);
    for (int i = 1; i < line.size(); ++i) {
        cumulative[i] = cumulative[i - 1] + QLineF(line[i - 1], line[i]).length();
    }
    total = cumulative.last();
    points.reserve(samples + 1);
    int segment = 1;
    for (int i = 0; i <= samples; ++i)
    {
        if (line.size() == 1 || total <= 0)
        {
            points.append(line.first());
            continue;
        }
        const qreal distance = total * i / samples;
        while (segment < line.size() - 1 && cumulative[segment] < distance) {
            ++segment;
        }
        const qreal segmentLength = cumulative[segment] - cumulative[segment - 1];
        const qreal t = segmentLength > 0 ? (distance - cumulative[segment - 1]) / segmentLength : 0.0;
        points.append(line[segment - 1] + (line[segment] - line[segment - 1]) * t);
    }
    return points;
}

// Cells of a grid over the overlay covered by routes already placed, and which way those
// routes run through them, so later routes take their own lane rather than drawing on
// top of an earlier one. Crossing another route at right angles costs a little.
// Counts per cell let one route be removed and added again cheaply while links are
// rerouted, and a flat grid makes each lookup an array access.
constexpr quint8 occupancyHorizontal = 1;
constexpr quint8 occupancyVertical = 2;
constexpr qreal occupancyCell = 3; // Fine enough to tell lanes 4 px apart, as tiled windows leave narrow gaps

// Which way a sampled route runs at a point, from its neighbouring samples.
static quint8 occupancyDirection(const QVector<QPointF>& points, int i)
{
    const QPointF delta = points[qMin(i + 1, points.size() - 1)] - points[qMax(i - 1, 0)];
    return qAbs(delta.x()) >= qAbs(delta.y()) ? occupancyHorizontal : occupancyVertical;
}

class Occupancy
{
public:
    explicit Occupancy(const QSize& size) :
        m_columns(qMax(1, qCeil(size.width() / occupancyCell))),
        m_rows(qMax(1, qCeil(size.height() / occupancyCell))),
        m_horizontal(m_columns * m_rows, 0),
        m_vertical(m_columns * m_rows, 0)
    {}

    // The cells a route reserves, each as (cell index << 1) | vertical. Computed once
    // per route, and passed to add() to add or remove the route.
    QVector<quint32> cells(const QPainterPath& path) const
    {
        // Reserve a band either side of the route, so a parallel route keeps about
        // 16 px away rather than running right beside it. A little under 16 px, so the
        // bracket lanes, 16 px apart, are clear of each other's bands. A line of cells
        // across the route at every cell along it fills the band.
        constexpr int band = 4;
        // Only the middle of a route reserves its lane. Near its ends it turns into
        // its window, where other routes need to pass too.
        constexpr qreal freeEnds = 40;
        qreal length = path.length();
        const int samples = qMax(2, qCeil(length / occupancyCell));
        const QVector<QPointF> points = samplePath(path, samples, length);
        const qreal step = length / qMax(1, points.size() - 1);
        QVector<quint32> result;
        for (int i = 0; i < points.size(); ++i)
        {
            if (i * step < freeEnds || (points.size() - 1 - i) * step < freeEnds) {
                continue;
            }
            const quint8 direction = occupancyDirection(points, i);
            const int column = qFloor(points[i].x() / occupancyCell);
            const int row = qFloor(points[i].y() / occupancyCell);
            for (int k = -band; k <= band; ++k)
            {
                const int c = direction == occupancyVertical ? column + k : column;
                const int r = direction == occupancyVertical ? row : row + k;
                if (c >= 0 && c < m_columns && r >= 0 && r < m_rows) {
                    result.append((quint32(r * m_columns + c) << 1) | (direction == occupancyVertical ? 1 : 0));
                }
            }
        }
        return result;
    }

    // Add (delta 1) or remove (delta -1) a route's cells.
    void add(const QVector<quint32>& cells, int delta)
    {
        for (quint32 cell : cells)
        {
            QVector<quint16>& counts = (cell & 1) ? m_vertical : m_horizontal;
            counts[cell >> 1] += delta;
        }
        m_routes += delta;
    }

    bool isEmpty() const { return m_routes <= 0; }

    // The directions of the routes whose bands cover a point.
    quint8 directions(const QPointF& point) const
    {
        const int column = qFloor(point.x() / occupancyCell);
        const int row = qFloor(point.y() / occupancyCell);
        if (column < 0 || column >= m_columns || row < 0 || row >= m_rows) {
            return 0;
        }
        const int index = row * m_columns + column;
        return (m_horizontal[index] ? occupancyHorizontal : 0) | (m_vertical[index] ? occupancyVertical : 0);
    }

private:
    int m_columns;
    int m_rows;
    QVector<quint16> m_horizontal;
    QVector<quint16> m_vertical;
    int m_routes = 0;
};

// How good a route is: its length, and how much of it is drawn over windows or
// outside the viewport. Measured in pixels, so a longer detour does not look
// better just because its crossing is a smaller fraction of it.
struct RouteScore
{
    qreal crossing = 0;
    qreal length = 0;
    int bends = 0;
    // A detour is worth taking when it saves a third of its extra length in crossing.
    // Each bend costs a little, so simpler shapes win when all else is equal.
    qreal value() const { return 3 * crossing + length + 24 * bends; }
};

// A route sampled for scoring, with the part of its score that does not depend on
// other routes worked out once, so it can be scored cheaply as they move.
struct ScoredPath
{
    QPainterPath path;
    QVector<QPointF> points;
    qreal step = 0;
    int fixedCount = 0;     // Half samples over windows or outside the viewport
    RouteScore score;       // Ignoring other routes
};

static ScoredPath scorePath(const QPainterPath& path, const QRectF& sourceBox, const QRectF& targetBox,
                            const QVector<QRectF>& obstacles, const QRectF& bounds)
{
    // Crossing covers the two linked windows, and every other window in the
    // workspace, so routes prefer gaps and open space. An endpoint can be inside a
    // window, e.g. where another window overlaps the edge it is on, or when it was
    // pulled onto its own title bar. The route may then leave that window at the start, or
    // arrive in it at the end, for up to a short distance. Running along a window's
    // border costs fully, as a line on a seam between tiled windows is drawn on top
    // of their frames; running just inside an edge costs half.
    constexpr int samples = 80;
    constexpr qreal freeRun = 30;   // Pixels a route may run inside its start or end window
    ScoredPath scored;
    scored.path = path;
    scored.points = samplePath(path, samples, scored.score.length);
    const QVector<QPointF>& points = scored.points;
    if (points.isEmpty() || scored.score.length <= 0) {
        return scored;
    }
    const qreal step = scored.score.length / samples;
    scored.step = step;
    const int last = points.size() - 1;
    const int maxFree = qCeil(freeRun / step);
    qreal left = points.first().x(), right = left, top = points.first().y(), bottom = top;
    for (const QPointF& point : points)
    {
        left = qMin(left, point.x());
        right = qMax(right, point.x());
        top = qMin(top, point.y());
        bottom = qMax(bottom, point.y());
    }
    const QRectF pathBounds(QPointF(left, top), QPointF(right, bottom));
    auto boxSamples = [&](const QRectF& box, bool edgeDiscount) -> int {
        if (box.isEmpty()) {
            return 0;
        }
        // The window, plus a thin strip either side of its border.
        constexpr qreal border = 4;
        const QRectF body = box.adjusted(-border, -border, border, border);
        if (!body.intersects(pathBounds.adjusted(-1, -1, 1, 1))) {
            return 0; // Most windows are nowhere near a given route
        }
        int first = 0;
        while (first <= last && first < maxFree && body.contains(points[first])) {
            ++first;    // Leading run inside, leaving the start window
        }
        int end = last;
        while (end >= first && last - end < maxFree && body.contains(points[end])) {
            --end;      // Trailing run inside, arriving in the end window
        }
        // Counted in half samples. On the border itself counts fully, so routes do
        // not follow seams between windows. Running vertically just inside a left or
        // right edge, where the lanes beside a column run, is much less intrusive
        // than cutting across the middle, so it counts half. Along the top edge it
        // would cover the title bar, so that counts fully.
        constexpr qreal edgeBand = 64; // Room for four lanes 16 px apart
        const QRectF inner = box.adjusted(border, border, -border, -border);
        const QRectF deep = inner.adjusted(edgeBand, 0, -edgeBand, 0);
        int count = 0;
        for (int i = first; i <= end; ++i)
        {
            if (!body.contains(points[i])) {
                continue;
            }
            const bool besideEdge = edgeDiscount && inner.contains(points[i]) && !deep.contains(points[i]) &&
                occupancyDirection(points, i) == occupancyVertical;
            count += besideEdge ? 1 : 2;
        }
        return count;
    };
    // Crossing the linked windows counts double, with no discount near their
    // edges: a line over a linked window looks like it connects there.
    int count = 2 * (boxSamples(sourceBox, false) + boxSamples(targetBox, false));
    for (const QRectF& obstacle : obstacles) {
        count += boxSamples(obstacle, true);
    }
    // Parts outside the viewport cannot be seen, so they count as crossing too.
    const QRectF visible = bounds.adjusted(-4, -4, 4, 4);
    for (const QPointF& point : points)
    {
        if (!visible.contains(point)) {
            count += 2;
        }
    }
    scored.fixedCount = count;
    scored.score.crossing = count * step / 2; // Half samples to pixels
    // Corners of rounded routes, and the direct curve, are cubic segments.
    for (int e = 0; e < path.elementCount(); ++e)
    {
        if (path.elementAt(e).type == QPainterPath::CurveToElement) {
            ++scored.score.bends;
        }
    }
    return scored;
}

// The score of a sampled route among the routes already placed.
static RouteScore scoreAmong(const ScoredPath& scored, const Occupancy *occupied)
{
    RouteScore score = scored.score;
    if (!occupied || occupied->isEmpty() || scored.points.isEmpty() || score.length <= 0) {
        return score;
    }
    // Running alongside an earlier route, in the same direction, counts as crossing
    // too; crossing it at right angles costs a little, so crossings are avoided when
    // that is cheap, e.g. by nesting two links the other way round. Sharing its line costs a lot: more than running
    // into the edge of a neighbouring window, so parallel links keep their spacing
    // even where windows are tiled with narrow gaps. Ends are exempt, as several
    // links can leave the same anchor.
    constexpr qreal exemptEnds = 20;
    constexpr int overlapWeight = 4;
    const QVector<QPointF>& points = scored.points;
    const qreal step = scored.step;
    const int last = points.size() - 1;
    int count = scored.fixedCount;
    for (int i = 0; i <= last; ++i)
    {
        if (i * step < exemptEnds || (last - i) * step < exemptEnds) {
            continue;
        }
        const quint8 occupiedDirections = occupied->directions(points[i]);
        if (occupiedDirections & occupancyDirection(points, i)) {
            count += 2 * overlapWeight;
        } else if (occupiedDirections) {
            count += 4; // Across the other route's band: about a 150 px detour per crossing
        }
    }
    score.crossing = count * step / 2; // Half samples to pixels
    return score;
}

static RouteScore scoreRoute(const QPainterPath& path, const QRectF& sourceBox, const QRectF& targetBox,
                             const QVector<QRectF>& obstacles, const QRectF& bounds, const Occupancy *occupied)
{
    return scoreAmong(scorePath(path, sourceBox, targetBox, obstacles, bounds), occupied);
}

// The simplest route: a curve, or for both ends on the same side a bracket.
static QPainterPath directPath(const QPointF& from, const QPointF& to, int fromSide, int toSide,
                               const QRectF& sourceBox, const QRectF& targetBox)
{
    constexpr qreal labelLead = 20; // Straight run out from the window before any turn
    const QPointF curveFrom = from + QPointF(fromSide * labelLead, 0);
    const QPointF curveTo = to + QPointF(toSide * labelLead, 0);
    const QPointF delta = curveTo - curveFrom;
    // With both ends on the same side, e.g. windows stacked in a column, a large
    // handle bulges far into neighbouring windows, so keep the curve tight.
    const bool sameSide = fromSide != 0 && fromSide == toSide;
    const qreal handle = qBound<qreal>(32, qAbs(delta.x()) * 0.35 + qAbs(delta.y()) * 0.3, sameSide ? 40 : 180);
    const QPointF firstControl = fromSide == 0 ? curveFrom + delta * 0.3 :
        curveFrom + QPointF(fromSide * handle, 0);
    const QPointF lastControl = toSide == 0 ? curveTo - delta * 0.3 :
        curveTo + QPointF(toSide * handle, 0);
    QPainterPath direct(from);
    if (sameSide)
    {
        // Both ends on the same side, e.g. windows stacked in a column: a straight
        // bracket 16 px out from the windows. A curve here bulges across several
        // lanes, leaving no room for the links beside it.
        const qreal edge = fromSide > 0 ? qMax(qMax(from.x(), to.x()), qMax(sourceBox.right(), targetBox.right())) :
            qMin(qMin(from.x(), to.x()), qMin(sourceBox.left(), targetBox.left()));
        const qreal laneX = edge + fromSide * 16;
        direct = roundedRoute({from, QPointF(laneX, from.y()), QPointF(laneX, to.y()), to});
    }
    else
    {
        if (fromSide != 0) {
            direct.lineTo(curveFrom);
        }
        direct.cubicTo(firstControl, lastControl, curveTo);
        if (toSide != 0) {
            direct.lineTo(to);
        }
    }
    return direct;
}

// Routes around windows, for when the direct route crosses them.
static QVector<QPainterPath> detourPaths(const QPointF& from, const QPointF& to, int fromSide, int toSide,
                                         const QRectF& sourceBox, const QRectF& targetBox, const QRectF& bounds)
{
    const bool sameSide = fromSide != 0 && fromSide == toSide;
    QVector<QPainterPath> detours;
    auto consider = [&](const QPainterPath& candidate) {
        detours.append(candidate);
    };

    // Both ends on the same side: a bracket that steps out, runs along the gap
    // beside the windows, and steps back in.
    if (sameSide)
    {
        // Measured from the outermost window edge, so both windows are cleared. At
        // least 16 px out, so the link visibly steps away from the window first; then
        // every 16 px for parallel lanes.
        const qreal edge = fromSide > 0 ? qMax(qMax(from.x(), to.x()), qMax(sourceBox.right(), targetBox.right())) :
            qMin(qMin(from.x(), to.x()), qMin(sourceBox.left(), targetBox.left()));
        for (qreal out : {16.0, 32.0, 48.0, 64.0, 80.0})
        {
            const qreal laneX = edge + fromSide * out;
            if (laneX >= bounds.left() + 2 && laneX <= bounds.right() - 2) {
                consider(roundedRoute({from, QPointF(laneX, from.y()), QPointF(laneX, to.y()), to}));
            }
        }
    }

    // Elbow routes: out from the source, one vertical run, and into the target. The
    // simplest shape after the direct curve, tried with the vertical run just past
    // the source, just before the target, midway, and along either window's frame.
    {
        constexpr qreal elbowLead = 24;
        QVector<qreal> elbowXs;
        // Midway, except for same-side links: their anchors share an edge, so the
        // midpoint runs straight down beside the windows without stepping out.
        if (!sameSide) {
            elbowXs.append((from.x() + to.x()) / 2);
        }
        // Along the windows' frames, with room for parallel lanes 16 px apart.
        for (qreal lane : {16.0, 24.0, 32.0, 40.0, 48.0, 56.0, 64.0}) {
            elbowXs << sourceBox.left() - lane << sourceBox.right() + lane
                    << targetBox.left() - lane << targetBox.right() + lane;
        }
        if (fromSide != 0) {
            elbowXs.append(from.x() + fromSide * elbowLead);
        }
        if (toSide != 0) {
            elbowXs.append(to.x() + toSide * elbowLead);
        }
        for (qreal elbowX : elbowXs)
        {
            if (elbowX >= bounds.left() + 2 && elbowX <= bounds.right() - 2) {
                consider(roundedRoute({from, QPointF(elbowX, from.y()), QPointF(elbowX, to.y()), to}));
            }
        }
    }

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
    return detours;
}

// Every route for a link, direct first, sampled for scoring.
static QVector<ScoredPath> routeCandidates(const QPointF& from, const QPointF& to, int fromSide, int toSide,
                                           const QRectF& sourceBox, const QRectF& targetBox, const QRectF& bounds,
                                           const QVector<QRectF>& obstacles)
{
    QVector<ScoredPath> candidates;
    candidates.append(scorePath(directPath(from, to, fromSide, toSide, sourceBox, targetBox),
        sourceBox, targetBox, obstacles, bounds));
    if (!sourceBox.isEmpty() && !targetBox.isEmpty())
    {
        for (const QPainterPath& detour : detourPaths(from, to, fromSide, toSide, sourceBox, targetBox, bounds)) {
            candidates.append(scorePath(detour, sourceBox, targetBox, obstacles, bounds));
        }
    }
    return candidates;
}

// The best of a link's candidate routes among the routes already placed. Take the
// best scoring route: short, and clear of windows. Crossing can be unavoidable (e.g.
// an endpoint covered by the other window), and then the direct route is usually
// clearer than a long detour around both windows. Returns its index, and its score.
static int bestCandidate(const QVector<ScoredPath>& candidates, bool sameSide, const Occupancy *occupied,
                         qreal& bestScore)
{
    const RouteScore directScore = scoreAmong(candidates.first(), occupied);
    bestScore = directScore.value();
    if (directScore.crossing <= 0 && !sameSide) {
        return 0;   // Clear, and nothing is shorter. A same-side bracket clears the
                    // outermost window, so a lane nearer the other may be shorter.
    }
    int best = 0;
    for (int i = 1; i < candidates.size(); ++i)
    {
        const qreal score = scoreAmong(candidates[i], occupied).value();
        if (score < bestScore)
        {
            best = i;
            bestScore = score;
        }
    }
    return best;
}

static QPainterPath connectionPath(const QPointF& from, const QPointF& to,
                                   int fromSide, int toSide, const QRectF& sourceBox,
                                   const QRectF& targetBox, const QRectF& bounds,
                                   const QVector<QRectF>& obstacles, const Occupancy *occupied = nullptr)
{
    const bool sameSide = fromSide != 0 && fromSide == toSide;
    const QPainterPath direct = directPath(from, to, fromSide, toSide, sourceBox, targetBox);
    if (sourceBox.isEmpty() || targetBox.isEmpty()) {
        return direct;
    }
    // The direct route alone first, as it is often clear, sparing the detours.
    const RouteScore directScore = scoreRoute(direct, sourceBox, targetBox, obstacles, bounds, occupied);
    if (directScore.crossing <= 0 && !sameSide) {
        return direct;
    }
    QPainterPath bestRoute = direct;
    qreal bestScore = directScore.value();
    for (const QPainterPath& detour : detourPaths(from, to, fromSide, toSide, sourceBox, targetBox, bounds))
    {
        const qreal detourScore = scoreRoute(detour, sourceBox, targetBox, obstacles, bounds, occupied).value();
        if (detourScore < bestScore)
        {
            bestRoute = detour;
            bestScore = detourScore;
        }
    }
    return bestRoute;
}

void ConnectionOverlay::computeLayout()
{
    m_layoutDirty = false;
    m_routes.clear();
    const QList<QMdiSubWindow*> localWindows = m_mdi->subWindowList();
    const QRectF bounds = rect().adjusted(8, 8, -8, -8);
    const QPoint viewportOffset = m_mdi->viewport()->pos() - pos();
    const QFontMetrics metrics(font());
    QVector<QRectF> labelObstacles; // Each label as it is placed
    // Stubs to other workspaces that leave the same anchor fan out vertically, so
    // their lines and labels do not sit on top of each other.
    QHash<QPair<QMdiSubWindow*, int>, int> stubCounts;
    const qreal stubSpacing = metrics.height() + 14;
    auto fanOffset = [&](QMdiSubWindow *window, int side) -> qreal {
        const int n = stubCounts[qMakePair(window, side)]++;
        const int step = (n + 1) / 2;       // 0, 1, 1, 2, 2, ...
        return (n % 2 ? step : -step) * stubSpacing;   // 0, +1, -1, +2, -2, ...
    };
    // Stubs placed above (vertical < 0) or below (vertical > 0) a window stack away
    // from it. Returns false, claiming nothing, if the next slot and its label would
    // not fit inside the viewport, or if the space it needs is covered by another window.
    auto claimVertical = [&](QMdiSubWindow *window, int side, int vertical, qreal anchorX, qreal markerWidth, qreal& tipY) -> bool {
        const QPair<QMdiSubWindow*, int> key(window, side * (vertical < 0 ? 2 : 3)); // Separate from the level fan
        const QRectF box = QRectF(window->geometry().translated(viewportOffset));
        const qreal offset = 16 + stubCounts.value(key) * stubSpacing;
        const qreal y = vertical < 0 ? box.top() - offset : box.bottom() + offset;
        const qreal labelHeight = metrics.height() + 4;
        if (y - labelHeight / 2 < bounds.top() + 4 || y + labelHeight / 2 > bounds.bottom() - 4) {
            return false;
        }
        // Stub and label, whichever side of the tip the label ends up on.
        const qreal bandTop = vertical < 0 ? y - labelHeight / 2 : box.bottom();
        const qreal bandBottom = vertical < 0 ? box.top() : y + labelHeight / 2;
        const QRectF band(anchorX - markerWidth - 40, bandTop, 2 * markerWidth + 80, bandBottom - bandTop);
        for (QMdiSubWindow *other : localWindows)
        {
            if (other != window && other->isVisible() && !other->isMinimized() &&
                band.intersects(QRectF(other->geometry().translated(viewportOffset)))) {
                return false;
            }
        }
        stubCounts[key]++;
        tipY = y;
        return true;
    };
    // A window can be connected on either side, just outside its left edge (side -1)
    // or its right edge (side 1), level with the middle of its title bar. Each link
    // uses whichever sides give it the best route. The overlay is a sibling of the
    // scrolling viewport, so MDI window coordinates are translated.
    auto anchorFor = [&viewportOffset](QMdiSubWindow *window, int side) -> QPointF {
        const QPoint inWindow(side < 0 ? -anchorOutset : window->width() + anchorOutset, titleBarMiddle);
        return QPointF(window->geometry().topLeft() + inWindow + viewportOffset);
    };

    // A flow label sits beside its route's midpoint, so labels of fanned stubs separate too.
    auto placeFlowLabel = [&](Route& route) {
        const qreal labelWidth = metrics.horizontalAdvance(route.m_flowLabel) + 10;
        const qreal labelHeight = metrics.height() + 4;
        const QPointF midpoint = route.m_path.pointAtPercent(0.5);
        const qreal labelX = midpoint.x() - labelWidth / 2;
        const qreal labelY = midpoint.y() - labelHeight - 6;
        route.m_flowLabelRect = QRectF(qBound(bounds.left() + 4, labelX,
                                         qMax(bounds.left() + 4, bounds.right() - labelWidth - 4)),
                                     qBound(bounds.top() + 4, labelY,
                                         qMax(bounds.top() + 4, bounds.bottom() - labelHeight - 4)),
                                     labelWidth, labelHeight);
    };

    // Route links so they can keep out of each other's way: stubs to other workspaces
    // first, as their place is fixed, then links within this workspace from the
    // smallest vertical span up. Short links take the lanes nearest the windows,
    // and longer ones nest around them without crossing.
    Occupancy occupancy(size());
    // Candidate routes of each link within this workspace. They do not change while
    // other links move, so they are made and sampled once, and only their scores
    // among the other links are worked out again when rerouting.
    struct Reroute
    {
        ScoredPath current;
        QPointF anchors[2][2];                      // [source/target][left/right]
        QVector<ScoredPath> candidates[2][2];       // [source side][target side], empty where an anchor is off screen
        QVector<QVector<quint32>> cells[2][2];      // Occupancy cells of each candidate, made when needed
    };
    struct Choice
    {
        int si = -1;
        int ti = -1;
        int index = -1;
        qreal score = 0;
    };
    QVector<Reroute> reroutes; // Parallel to m_routes
    QVector<int> order;
    QVector<qreal> orderKey(m_connections.size(), 0);
    for (int i = 0; i < m_connections.size(); ++i)
    {
        const Connection& connection = m_connections[i];
        order.append(i);
        if (connection.m_source && connection.m_target && localWindows.contains(connection.m_source.data()) &&
            localWindows.contains(connection.m_target.data())) {
            orderKey[i] = 1 + qAbs(connection.m_source->geometry().top() - connection.m_target->geometry().top());
        }
    }
    std::stable_sort(order.begin(), order.end(), [&orderKey](int a, int b) { return orderKey[a] < orderKey[b]; });

    for (int connectionIndex : order)
    {
        const Connection& connection = m_connections[connectionIndex];
        if (!connection.m_source || !connection.m_target) {
            continue;
        }
        const bool sourceLocal = localWindows.contains(connection.m_source.data());
        const bool targetLocal = localWindows.contains(connection.m_target.data());
        if (!sourceLocal && !targetLocal) {
            continue;
        }
        if (sourceLocal && (connection.m_source->parentWidget() != m_mdi->viewport() ||
                            !connection.m_source->isVisible() || connection.m_source->isMinimized())) {
            continue;
        }
        if (targetLocal && (connection.m_target->parentWidget() != m_mdi->viewport() ||
                            !connection.m_target->isVisible() || connection.m_target->isMinimized())) {
            continue;
        }
        // Anchors on both sides of each local endpoint; index 0 is the left, 1 the right.
        // A side can be used when its anchor is on screen. If neither is, e.g. a window
        // wider than the viewport, but its title bar is visible, pull the left anchor
        // inside, onto the ID label end of the title bar, rather than dropping the
        // window's links.
        auto placeAnchors = [&](QMdiSubWindow *window, QPointF anchors[2], bool on[2]) {
            for (int i = 0; i < 2; ++i)
            {
                anchors[i] = anchorFor(window, i ? 1 : -1);
                on[i] = bounds.contains(anchors[i]);
            }
            const QRectF titleBar(QPointF(window->geometry().topLeft() + viewportOffset),
                                  QSizeF(window->width(), titleBarHeight));
            if (!on[0] && !on[1] && titleBar.intersects(bounds))
            {
                anchors[0] = viewportEdge(bounds, anchors[0]);
                on[0] = true;
            }
        };
        QPointF sourceAnchors[2], targetAnchors[2];
        bool sourceOn[2] = {false, false}, targetOn[2] = {false, false};
        if (sourceLocal) {
            placeAnchors(connection.m_source.data(), sourceAnchors, sourceOn);
        }
        if (targetLocal) {
            placeAnchors(connection.m_target.data(), targetAnchors, targetOn);
        }
        const bool sourceVisible = sourceOn[0] || sourceOn[1];
        const bool targetVisible = targetOn[0] || targetOn[1];
        // This workspace draws a link only when at least one endpoint is on screen.
        if (!sourceVisible && !targetVisible) {
            continue;
        }

        Route route;
        Reroute reroute;
        QPointF markerEdge;
        bool crossWorkspace = false;
        int labelDirection = 0; // Which side of a stub's tip its label goes
        route.m_sourceBox = sourceVisible ?
            QRectF(connection.m_source->geometry().translated(viewportOffset)) : QRectF();
        route.m_targetBox = targetVisible ?
            QRectF(connection.m_target->geometry().translated(viewportOffset)) : QRectF();
        for (QMdiSubWindow *window : localWindows)
        {
            if (window != connection.m_source && window != connection.m_target && window->isVisible() &&
                !window->isMinimized() && window->parentWidget() == m_mdi->viewport()) {
                route.m_obstacles.append(QRectF(window->geometry().translated(viewportOffset)));
            }
        }

        // Nearest on-screen anchor of an endpoint to a point.
        auto nearestSide = [](const QPointF anchors[2], const bool on[2], const QPointF& point) -> int {
            if (!on[0] || !on[1]) {
                return on[1] ? 1 : -1;
            }
            return QLineF(anchors[1], point).length() < QLineF(anchors[0], point).length() ? 1 : -1;
        };

        // A stub to another workspace beside a local window: from the side with more
        // room, level with the title bar; else above or below the window; else as far
        // out as the viewport allows on the roomier side.
        auto placeStub = [&](QMdiSubWindow *window, const QPointF anchors[2], const bool on[2],
                             qreal markerWidth, QPointF& local, QPointF& tip, int& side) {
            const qreal stubLength = 90;
            const qreal roomLeft = on[0] ? anchors[0].x() - bounds.left() : -1;
            const qreal roomRight = on[1] ? bounds.right() - anchors[1].x() : -1;
            const int roomier = roomRight >= roomLeft ? 1 : -1;
            for (int s : {roomier, -roomier})
            {
                if ((s > 0 ? roomRight : roomLeft) >= stubLength + markerWidth + 10)
                {
                    side = labelDirection = s;
                    local = anchors[s > 0 ? 1 : 0];
                    const qreal y = qBound(bounds.top() + 4, local.y() + fanOffset(window, s), bounds.bottom() - 4);
                    tip = QPointF(local.x() + s * stubLength, y);
                    return;
                }
            }
            side = roomier;
            local = anchors[side > 0 ? 1 : 0];
            qreal y = 0;
            if (claimVertical(window, side, -1, local.x(), markerWidth, y) ||
                claimVertical(window, side, 1, local.x(), markerWidth, y))
            {
                tip = QPointF(side > 0 ? qMin(bounds.right() - 4, local.x() + 30) : qMax(bounds.left() + 4, local.x() - 30), y);
                const bool fitsOutwards = side > 0 ? tip.x() + 6 + markerWidth <= bounds.right() - 4 :
                    tip.x() - 6 - markerWidth >= bounds.left() + 4;
                labelDirection = fitsOutwards ? side : -side;
                return;
            }
            labelDirection = side;
            y = qBound(bounds.top() + 4, local.y() + fanOffset(window, side), bounds.bottom() - 4);
            tip = QPointF(side > 0 ? qMin(bounds.right() - markerWidth - 10, local.x() + stubLength) :
                qMax(bounds.left() + markerWidth + 10, local.x() - stubLength), y);
        };

        // Scrolled endpoints point to a viewport edge. An endpoint in another
        // workspace instead gets a short labeled stub beside the local window.
        if (sourceVisible && targetVisible)
        {
            route.m_local = true;
            for (int i = 0; i < 2; ++i)
            {
                reroute.anchors[0][i] = sourceAnchors[i];
                reroute.anchors[1][i] = targetAnchors[i];
            }
            // Try each pair of sides; keep the best scoring route.
            Choice best;
            for (int si = 0; si < 2; ++si)
            {
                for (int ti = 0; ti < 2; ++ti)
                {
                    if (!sourceOn[si] || !targetOn[ti]) {
                        continue;
                    }
                    QVector<ScoredPath>& candidates = reroute.candidates[si][ti];
                    candidates = routeCandidates(sourceAnchors[si], targetAnchors[ti], si ? 1 : -1, ti ? 1 : -1,
                        route.m_sourceBox, route.m_targetBox, bounds, route.m_obstacles);
                    reroute.cells[si][ti].resize(candidates.size());
                    qreal score = 0;
                    const int index = bestCandidate(candidates, si == ti, &occupancy, score);
                    if (best.index < 0 || score < best.score) {
                        best = Choice{si, ti, index, score};
                    }
                }
            }
            reroute.current = reroute.candidates[best.si][best.ti][best.index];
            route.m_from = sourceAnchors[best.si];
            route.m_to = targetAnchors[best.ti];
            route.m_fromSide = best.si ? 1 : -1;
            route.m_toSide = best.ti ? 1 : -1;
            route.m_path = reroute.current.path;
        }
        else if (sourceVisible && targetLocal)
        {
            route.m_to = viewportEdge(bounds, targetAnchors[0]);
            route.m_fromSide = nearestSide(sourceAnchors, sourceOn, route.m_to);
            route.m_from = sourceAnchors[route.m_fromSide > 0 ? 1 : 0];
            markerEdge = route.m_to;
            route.m_marker = scrollArrow(bounds, targetAnchors[0]) + " " + connection.m_targetLabel;
        }
        else if (targetVisible && sourceLocal)
        {
            route.m_from = viewportEdge(bounds, sourceAnchors[0]);
            route.m_toSide = nearestSide(targetAnchors, targetOn, route.m_from);
            route.m_to = targetAnchors[route.m_toSide > 0 ? 1 : 0];
            markerEdge = route.m_from;
            route.m_marker = scrollArrow(bounds, sourceAnchors[0]) + " " + connection.m_sourceLabel;
        }
        else if (sourceVisible)
        {
            route.m_marker = QString("W%1 · %2").arg(connection.m_targetWorkspace).arg(connection.m_targetLabel);
            if (!connection.m_flowLabel.isEmpty()) {
                route.m_marker += " · " + connection.m_flowLabel; // One label per stub, at its tip
            }
            placeStub(connection.m_source.data(), sourceAnchors, sourceOn, metrics.horizontalAdvance(route.m_marker) + 8,
                route.m_from, route.m_to, route.m_fromSide);
            route.m_stubTip = 1;
            crossWorkspace = true;
        }
        else
        {
            route.m_marker = QString("W%1 · %2").arg(connection.m_sourceWorkspace).arg(connection.m_sourceLabel);
            if (!connection.m_flowLabel.isEmpty()) {
                route.m_marker += " · " + connection.m_flowLabel; // One label per stub, at its tip
            }
            placeStub(connection.m_target.data(), targetAnchors, targetOn, metrics.horizontalAdvance(route.m_marker) + 8,
                route.m_to, route.m_from, route.m_toSide);
            route.m_stubTip = -1;
            crossWorkspace = true;
        }
        const QPointF& from = route.m_from;
        const QPointF& to = route.m_to;

        route.m_source = connection.m_source.data();
        route.m_target = connection.m_target.data();
        route.m_color = connection.m_color.isValid() ? connection.m_color.lighter(150) :
            (connection.m_kind == PipeLink ? QColor(238, 183, 77) : QColor(56, 204, 228));
        route.m_kind = connection.m_kind;
        route.m_direction = connection.m_direction;
        if (route.m_path.isEmpty()) {
            route.m_path = connectionPath(from, to, route.m_fromSide, route.m_toSide,
                route.m_sourceBox, route.m_targetBox, bounds, route.m_obstacles, &occupancy);
        }
        route.m_length = route.m_path.length();

        if (!connection.m_flowLabel.isEmpty() && !crossWorkspace) // Stubs include it in their marker
        {
            route.m_flowLabel = connection.m_flowLabel;
            placeFlowLabel(route);
        }

        if (!route.m_marker.isEmpty())
        {
            const qreal labelWidth = metrics.horizontalAdvance(route.m_marker) + 8;
            const qreal labelHeight = metrics.height() + 4;
            const QPointF remoteAnchor = sourceVisible ? to : from;
            // A stub's label continues from its tip, usually in the direction the stub goes.
            const qreal labelX = crossWorkspace ?
                (labelDirection > 0 ? remoteAnchor.x() + 6 : remoteAnchor.x() - labelWidth - 6) :
                (markerEdge.x() >= bounds.right() - 1 ?
                    markerEdge.x() - labelWidth - 8 : markerEdge.x() + 8);
            const qreal labelY = crossWorkspace ?
                remoteAnchor.y() - labelHeight / 2 :
                (markerEdge.y() >= bounds.bottom() - 1 ?
                    markerEdge.y() - labelHeight - 8 : markerEdge.y() + 8);
            route.m_markerRect = QRectF(qBound(bounds.left() + 4, labelX,
                                          qMax(bounds.left() + 4, bounds.right() - labelWidth - 4)),
                                      qBound(bounds.top() + 4, labelY,
                                          qMax(bounds.top() + 4, bounds.bottom() - labelHeight - 4)),
                                      labelWidth, labelHeight);
        }

        route.m_cells = occupancy.cells(route.m_path);
        occupancy.add(route.m_cells, 1);
        m_routes.append(route);
        reroutes.append(reroute);
    }

    // Reroute links within this workspace with every other link in place, so the order
    // they were first placed in does not decide which gets the better lane, e.g.
    // whether two links from one device nest without crossing.
    // Links are taken out of the occupancy while they are rerouted, and put back after.
    auto scoreCurrent = [&](int r) -> qreal {
        return scoreAmong(reroutes[r].current, &occupancy).value();
    };
    // Best route for a link around the others in the occupancy, trying each pair of sides.
    auto routeAmong = [&](int r) -> Choice {
        Choice best;
        for (int si = 0; si < 2; ++si)
        {
            for (int ti = 0; ti < 2; ++ti)
            {
                const QVector<ScoredPath>& candidates = reroutes[r].candidates[si][ti];
                if (candidates.isEmpty()) {
                    continue;
                }
                qreal score = 0;
                const int index = bestCandidate(candidates, si == ti, &occupancy, score);
                if (best.index < 0 || score < best.score) {
                    best = Choice{si, ti, index, score};
                }
            }
        }
        return best;
    };
    auto cellsOf = [&](int r, const Choice& choice) -> const QVector<quint32>& {
        QVector<quint32>& cells = reroutes[r].cells[choice.si][choice.ti][choice.index];
        if (cells.isEmpty()) {
            cells = occupancy.cells(reroutes[r].candidates[choice.si][choice.ti][choice.index].path);
        }
        return cells;
    };
    auto scoreChoice = [&](int r, const Choice& choice) -> qreal {
        return scoreAmong(reroutes[r].candidates[choice.si][choice.ti][choice.index], &occupancy).value();
    };
    auto apply = [&](int r, const Choice& choice) {
        Route& route = m_routes[r];
        Reroute& reroute = reroutes[r];
        reroute.current = reroute.candidates[choice.si][choice.ti][choice.index];
        route.m_from = reroute.anchors[0][choice.si];
        route.m_to = reroute.anchors[1][choice.ti];
        route.m_fromSide = choice.si ? 1 : -1;
        route.m_toSide = choice.ti ? 1 : -1;
        route.m_path = reroute.current.path;
        route.m_length = route.m_path.length();
        route.m_cells = cellsOf(r, choice);
    };
    for (int pass = 0; pass < 2; ++pass)
    {
        bool changed = false;
        // Each link on its own.
        for (int r = 0; r < m_routes.size(); ++r)
        {
            if (!m_routes[r].m_local) {
                continue;
            }
            occupancy.add(m_routes[r].m_cells, -1);
            const Choice choice = routeAmong(r);
            if (choice.index >= 0 && choice.score < scoreCurrent(r) - 1)
            {
                apply(r, choice);
                changed = true;
            }
            occupancy.add(m_routes[r].m_cells, 1);
        }
        // Nearby pairs that get in each other's way, placed again in the opposite order.
        // Rerouting one link at a time cannot swap two links' lanes, as each would first
        // have to overlap the other.
        for (int a = 0; a < m_routes.size(); ++a)
        {
            for (int b = 0; b < m_routes.size(); ++b)
            {
                const Route& routeA = m_routes[a];
                const Route& routeB = m_routes[b];
                if (a == b || !routeA.m_local || !routeB.m_local ||
                    !routeA.m_path.boundingRect().adjusted(-32, -32, 32, 32).intersects(routeB.m_path.boundingRect())) {
                    continue;
                }
                occupancy.add(routeA.m_cells, -1);
                occupancy.add(routeB.m_cells, -1);
                const qreal aAlone = scoreCurrent(a);
                const qreal bAlone = scoreCurrent(b);
                occupancy.add(routeB.m_cells, 1);
                const qreal aWithB = scoreCurrent(a);
                occupancy.add(routeB.m_cells, -1);
                occupancy.add(routeA.m_cells, 1);
                const qreal bWithA = scoreCurrent(b);
                occupancy.add(routeA.m_cells, -1);
                if (aWithB > aAlone + 1 || bWithA > bAlone + 1)
                {
                    const Choice newB = routeAmong(b);
                    occupancy.add(cellsOf(b, newB), 1);
                    const Choice newA = routeAmong(a);
                    occupancy.add(cellsOf(b, newB), -1);
                    occupancy.add(cellsOf(a, newA), 1);
                    const qreal newBScore = scoreChoice(b, newB);
                    occupancy.add(cellsOf(a, newA), -1);
                    if (newA.score + newBScore < aWithB + bWithA - 1)
                    {
                        apply(a, newA);
                        apply(b, newB);
                        changed = true;
                    }
                }
                occupancy.add(m_routes[a].m_cells, 1);
                occupancy.add(m_routes[b].m_cells, 1);
            }
        }
        if (!changed) {
            break;
        }
    }
    for (Route& route : m_routes)
    {
        if (route.m_local && !route.m_flowLabel.isEmpty()) {
            placeFlowLabel(route);
        }
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
        const qreal before = route.m_markerRect.center().y();
        placeLabel(route.m_markerRect);
        const qreal dy = route.m_markerRect.center().y() - before;
        if (route.m_stubTip != 0 && !qFuzzyIsNull(dy))
        {
            // Keep a moved stub label on the end of its own line.
            QPointF& tip = route.m_stubTip > 0 ? route.m_to : route.m_from;
            tip.ry() += dy;
            route.m_path = connectionPath(route.m_from, route.m_to, route.m_fromSide, route.m_toSide,
                route.m_sourceBox, route.m_targetBox, bounds, route.m_obstacles);
            route.m_length = route.m_path.length();
        }
    }
    for (Route& route : m_routes) {
        placeLabel(route.m_flowLabelRect);
    }

    // End each link on its windows' borders, so it is clear which of two windows
    // side by side it belongs to. The anchors it was routed from are just outside,
    // so routes keep clear of the frames; they are extended straight in from there.
    auto onBorder = [](const QPointF& anchor, int side, const QRectF& box) -> QPointF {
        const qreal edge = side < 0 ? box.left() : box.right();
        if (side == 0 || box.isEmpty() || qAbs(anchor.x() - (edge + side * anchorOutset)) > 0.5 ||
            qAbs(anchor.y() - (box.top() + titleBarMiddle)) > 0.5) {
            return anchor; // Not at a window, e.g. a stub's tip, or pulled onto the viewport edge
        }
        return QPointF(edge, anchor.y());
    };
    for (Route& route : m_routes)
    {
        const QPointF from = onBorder(route.m_from, route.m_fromSide, route.m_sourceBox);
        const QPointF to = onBorder(route.m_to, route.m_toSide, route.m_targetBox);
        if (from != route.m_from)
        {
            QPainterPath path(from);
            path.connectPath(route.m_path);
            route.m_path = path;
            route.m_from = from;
        }
        if (to != route.m_to)
        {
            route.m_path.lineTo(to);
            route.m_to = to;
        }
        route.m_length = route.m_path.length();
    }
    updateHighlight();
}

QVector<ConnectionOverlay::Pulse> ConnectionOverlay::pulses() const
{
    QVector<Pulse> result;
    for (const Route& route : m_routes)
    {
        if (route.m_direction == NoDirection || route.m_length <= 32 || isDimmed(route)) {
            continue;
        }
        const int flowCount = route.m_direction == BothDirections ? 2 : 1;
        const int pulsesPerFlow = route.m_length > 360 ? 2 : 1;
        for (int flow = 0; flow < flowCount; ++flow)
        {
            const bool reverse = route.m_direction == Reverse || (route.m_direction == BothDirections && flow == 1);
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
                result.append({route.m_path.pointAtPercent(tailProgress), route.m_path.pointAtPercent(progress), route.m_color});
            }
        }
    }
    return result;
}

void ConnectionOverlay::animate()
{
    if (!isVisible()) {
        return; // Workspace is a background tab, or minimized.
    }
    // An OpenGL widget repaints in full, so only repaint when something is moving.
    m_phase = (m_clock.elapsed() % 2400) / 2400.0;
    if (!pulses().isEmpty()) {
        update();
    }
}

void ConnectionOverlay::paintGL()
{
    // Paint current geometry rather than a stale cache.
    if (m_layoutDirty) {
        computeLayout();
    }
    QOpenGLFunctions *gl = context()->functions();
    const qreal ratio = devicePixelRatioF();
    const QSize pixels = size() * ratio;
    if (!m_paintBuffer || m_paintBuffer->size() != pixels)
    {
        if (m_resolveBuffer != m_paintBuffer) {
            delete m_resolveBuffer;
        }
        delete m_paintBuffer;
        // QPainter's OpenGL engine needs multisampling for smooth lines, and a
        // stencil buffer for paths. Without framebuffer blits, paint unsampled.
        QOpenGLFramebufferObjectFormat bufferFormat;
        bufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        const bool blit = QOpenGLFramebufferObject::hasOpenGLFramebufferBlit();
        bufferFormat.setSamples(blit ? 4 : 0);
        m_paintBuffer = new QOpenGLFramebufferObject(pixels, bufferFormat);
        m_resolveBuffer = blit ? new QOpenGLFramebufferObject(pixels) : m_paintBuffer;
    }

    m_paintBuffer->bind();
    gl->glViewport(0, 0, pixels.width(), pixels.height());
    gl->glClearColor(0, 0, 0, 0);
    gl->glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    {
        QOpenGLPaintDevice device(pixels);
        device.setDevicePixelRatio(ratio);
        QPainter painter(&device);
        paintRoutes(painter);
    }
    if (m_resolveBuffer != m_paintBuffer) {
        QOpenGLFramebufferObject::blitFramebuffer(m_resolveBuffer, m_paintBuffer);
    }

    // Copy into the widget, converting to straight alpha.
    gl->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    gl->glViewport(0, 0, pixels.width(), pixels.height());
    gl->glDisable(GL_BLEND);
    gl->glDisable(GL_SCISSOR_TEST);
    gl->glDisable(GL_DEPTH_TEST);
    gl->glDisable(GL_STENCIL_TEST);
    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(GL_TEXTURE_2D, m_resolveBuffer->texture());
    m_unpremultiply->bind();
    m_unpremultiply->setUniformValue("source", 0);
    static const GLfloat quad[] = {-1, -1, 1, -1, -1, 1, 1, 1};
    const int position = m_unpremultiply->attributeLocation("position");
    m_unpremultiply->enableAttributeArray(position);
    m_unpremultiply->setAttributeArray(position, quad, 2);
    gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_unpremultiply->disableAttributeArray(position);
    m_unpremultiply->release();
}

void ConnectionOverlay::paintRoutes(QPainter& painter)
{
    painter.setRenderHint(QPainter::Antialiasing);

    for (const Route& route : m_routes)
    {
        const QColor& color = route.m_color;
        const bool highlighted = m_highlightActive && !isDimmed(route);
        painter.setOpacity(isDimmed(route) ? dimmedOpacity : 1.0);
        // drawPath also fills with the current brush. The preceding edge draws
        // its endpoint dots with a solid brush, so clear it before every path.
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), highlighted ? 90 : 35), highlighted ? 8 : 6,
            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(route.m_path);
        painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 180), 2.2,
            route.m_kind == PipeLink ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(route.m_path);
        painter.setBrush(QColor(color.red(), color.green(), color.blue(), 220));
        painter.setPen(QPen(palette().window().color(), 1.5));
        painter.drawEllipse(route.m_from, 4, 4);
        painter.drawEllipse(route.m_to, 4, 4);
    }
    painter.setOpacity(1.0);

    for (const Pulse& pulse : pulses())
    {
        const QColor& color = pulse.m_color;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 210), 4,
            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(pulse.m_tail, pulse.m_head);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(color.red(), color.green(), color.blue(), 130));
        painter.drawEllipse(pulse.m_head, 7, 7);
        painter.setBrush(QColor(255, 255, 255, 245));
        painter.drawEllipse(pulse.m_head, 3, 3);
    }

    for (const Route& route : m_routes)
    {
        painter.setOpacity(isDimmed(route) ? dimmedOpacity : 1.0);
        if (!route.m_flowLabel.isEmpty())
        {
            painter.setPen(QPen(route.m_color, 1));
            painter.setBrush(QColor(24, 28, 32, 235));
            painter.drawRoundedRect(route.m_flowLabelRect, 3, 3);
            painter.setPen(Qt::white);
            painter.drawText(route.m_flowLabelRect, Qt::AlignCenter, route.m_flowLabel);
        }
        if (!route.m_marker.isEmpty())
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(24, 28, 32, 235));
            painter.drawRoundedRect(route.m_markerRect, 3, 3);
            painter.setPen(Qt::white);
            painter.drawText(route.m_markerRect, Qt::AlignCenter, route.m_marker);
        }
    }
}

void ConnectionOverlay::setHovered(QMdiSubWindow *window)
{
    if (m_hovered == window) {
        return;
    }
    m_hovered = window;
    const bool wasActive = m_highlightActive;
    updateHighlight();
    if (wasActive || m_highlightActive) {
        update(); // Opacity changes affect every route
    }
}

void ConnectionOverlay::updateHighlight()
{
    // Only highlight when the hovered window has links drawn in this workspace;
    // hovering an unconnected window leaves every link at full strength.
    bool active = false;
    if (m_hovered)
    {
        for (const Route& route : m_routes)
        {
            if (route.m_source == m_hovered || route.m_target == m_hovered)
            {
                active = true;
                break;
            }
        }
    }
    m_highlightActive = active;
}

bool ConnectionOverlay::isDimmed(const Route& route) const
{
    return m_highlightActive && route.m_source != m_hovered && route.m_target != m_hovered;
}
