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

#include <algorithm>
#include <cmath>

#include <QContextMenuEvent>
#include <QDateTime>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QToolTip>

#include "dtv/dtvepggrid.h"

DTVEPGGrid::DTVEPGGrid(QWidget *parent) :
    QAbstractScrollArea(parent),
    m_start(0),
    m_end(0),
    m_now(0),
    m_selectedRow(-1),
    m_selectedEvent(-1)
{
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setMouseTracking(true);
    horizontalScrollBar()->setSingleStep(20);
    verticalScrollBar()->setSingleStep(20);
}

int DTVEPGGrid::nameWidth() const
{
    return fontMetrics().horizontalAdvance("0000  Wwwwwwwwwwwwwwwwww");
}

int DTVEPGGrid::headerHeight() const
{
    return fontMetrics().height() + 8;
}

int DTVEPGGrid::rowHeight() const
{
    return 2 * fontMetrics().height() + 8;
}

double DTVEPGGrid::x(qint64 utc) const
{
    return nameWidth() + (utc - m_start) * m_pixelsPerHour / 3600.0 - horizontalScrollBar()->value();
}

qint64 DTVEPGGrid::timeAt(double px) const
{
    return m_start + (qint64) std::floor((px - nameWidth() + horizontalScrollBar()->value()) * 3600.0 / m_pixelsPerHour);
}

void DTVEPGGrid::setRows(const std::vector<Row>& rows, qint64 start, qint64 now)
{
    // Keep the selection, by programme
    int networkId = -1, serviceId = -1, eventId = -1;

    if ((m_selectedRow >= 0) && (m_selectedEvent >= 0))
    {
        networkId = m_rows[m_selectedRow].m_networkId;
        serviceId = m_rows[m_selectedRow].m_serviceId;
        eventId = m_rows[m_selectedRow].m_events[m_selectedEvent].m_eventId;
    }

    // Keep the time at the left of the view, if the start moves
    const qint64 left = m_start != 0 ? timeAt(nameWidth()) : start;
    m_rows = rows;
    m_start = start;
    m_now = now;
    m_end = start + 8 * 86400;
    m_selectedRow = -1;
    m_selectedEvent = -1;
    m_longest.assign(m_rows.size(), 0);

    for (int r = 0; r < (int) m_rows.size(); r++)
    {
        const Row& row = m_rows[r];

        // Events can overlap (one within another), so the last to start may not be the last to end
        for (const DTVGuide::Event& e : row.m_events)
        {
            m_end = std::max(m_end, e.end());
            m_longest[r] = std::max(m_longest[r], (qint64) e.m_duration);
        }

        if ((row.m_networkId == networkId) && (row.m_serviceId == serviceId))
        {
            for (int e = 0; e < (int) row.m_events.size(); e++)
            {
                if (row.m_events[e].m_eventId == eventId)
                {
                    m_selectedRow = r;
                    m_selectedEvent = e;
                }
            }
        }
    }

    updateScrollBars();
    scrollToTime(left);
    viewport()->update();
}

void DTVEPGGrid::setNow(qint64 now)
{
    if (std::abs((double) (now - m_now)) * m_pixelsPerHour >= 3600.0)
    {
        m_now = now;
        viewport()->update();
    }
}

void DTVEPGGrid::scrollToTime(qint64 utc)
{
    horizontalScrollBar()->setValue((int) std::lround((utc - m_start) * m_pixelsPerHour / 3600.0));
}

void DTVEPGGrid::scrollToService(int networkId, int serviceId)
{
    for (int r = 0; r < (int) m_rows.size(); r++)
    {
        if ((m_rows[r].m_networkId == networkId) && (m_rows[r].m_serviceId == serviceId))
        {
            const int top = r * rowHeight();
            const int height = viewport()->height() - headerHeight();

            if ((top < verticalScrollBar()->value()) || (top + rowHeight() > verticalScrollBar()->value() + height)) {
                verticalScrollBar()->setValue(top - height / 3);
            }

            return;
        }
    }
}

bool DTVEPGGrid::selection(int& row, int& event) const
{
    row = m_selectedRow;
    event = m_selectedEvent;
    return (row >= 0) && (event >= 0);
}

void DTVEPGGrid::updateScrollBars()
{
    const int width = (int) std::ceil((m_end - m_start) * m_pixelsPerHour / 3600.0);
    const int height = (int) m_rows.size() * rowHeight();
    const int viewWidth = std::max(0, viewport()->width() - nameWidth());
    const int viewHeight = std::max(0, viewport()->height() - headerHeight());
    horizontalScrollBar()->setRange(0, std::max(0, width - viewWidth));
    horizontalScrollBar()->setPageStep(viewWidth);
    verticalScrollBar()->setRange(0, std::max(0, height - viewHeight));
    verticalScrollBar()->setPageStep(viewHeight);
}

void DTVEPGGrid::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBars();
}

// The row, and programme in it (-1 if none), at a point in the viewport. False if not in the programmes' area
bool DTVEPGGrid::hit(const QPoint& position, int& row, int& event) const
{
    row = -1;
    event = -1;

    if ((position.x() < nameWidth()) || (position.y() < headerHeight())) {
        return false;
    }

    row = (position.y() - headerHeight() + verticalScrollBar()->value()) / rowHeight();

    if ((row < 0) || (row >= (int) m_rows.size()))
    {
        row = -1;
        return false;
    }

    const qint64 t = timeAt(position.x());
    const std::vector<DTVGuide::Event>& events = m_rows[row].m_events;

    for (int e = 0; e < (int) events.size(); e++)
    {
        if ((events[e].m_start <= t) && (t < events[e].end()))
        {
            event = e;
            break;
        }
    }

    return true;
}

void DTVEPGGrid::select(int row, int event)
{
    if ((row == m_selectedRow) && (event == m_selectedEvent)) {
        return;
    }

    m_selectedRow = row;
    m_selectedEvent = event;
    viewport()->update();

    if ((row >= 0) && (event >= 0)) {
        emit eventSelected(row, event);
    }
}

void DTVEPGGrid::mousePressEvent(QMouseEvent *event)
{
    int row, e;

    if (hit(event->pos(), row, e) && (e >= 0)) {
        select(row, e);
    }

    QAbstractScrollArea::mousePressEvent(event);
}

void DTVEPGGrid::mouseDoubleClickEvent(QMouseEvent *event)
{
    int row, e;

    if (hit(event->pos(), row, e) && (e >= 0))
    {
        select(row, e);
        emit eventActivated(row, e);
    }
}

void DTVEPGGrid::contextMenuEvent(QContextMenuEvent *event)
{
    int row, e;

    if (hit(event->pos(), row, e) && (e >= 0))
    {
        select(row, e);
        emit eventMenuRequested(row, e, event->globalPos());
    }
}

// Arrows move between programmes and services, Enter plays
void DTVEPGGrid::keyPressEvent(QKeyEvent *event)
{
    if ((m_selectedRow < 0) || (m_selectedEvent < 0))
    {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }

    const std::vector<DTVGuide::Event>& events = m_rows[m_selectedRow].m_events;
    const DTVGuide::Event& current = events[m_selectedEvent];
    int row = m_selectedRow, e = m_selectedEvent;

    switch (event->key())
    {
    case Qt::Key_Left:
        e = std::max(0, e - 1);
        break;
    case Qt::Key_Right:
        e = std::min((int) events.size() - 1, e + 1);
        break;
    case Qt::Key_Up:
    case Qt::Key_Down:
    {
        // The programme on at the same time in the next row with programmes
        const int step = event->key() == Qt::Key_Up ? -1 : 1;
        const qint64 t = std::max(current.m_start, timeAt(nameWidth()));

        for (int r = row + step; (r >= 0) && (r < (int) m_rows.size()); r += step)
        {
            const std::vector<DTVGuide::Event>& other = m_rows[r].m_events;

            if (other.empty()) {
                continue;
            }

            auto it = std::find_if(other.begin(), other.end(), [t](const DTVGuide::Event& o) { return o.end() > t; });
            row = r;
            e = it != other.end() ? (int) (it - other.begin()) : (int) other.size() - 1;
            break;
        }
        break;
    }
    case Qt::Key_Return:
    case Qt::Key_Enter:
        emit eventActivated(row, e);
        return;
    default:
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }

    select(row, e);

    // Keep it in view
    const DTVGuide::Event& selected = m_rows[row].m_events[e];
    const double left = x(selected.m_start), right = x(selected.end());

    if ((left < nameWidth()) || (right > viewport()->width())) {
        scrollToTime(selected.m_start - 1800);
    }

    scrollToService(m_rows[row].m_networkId, m_rows[row].m_serviceId);
}

bool DTVEPGGrid::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent *help = static_cast<QHelpEvent *>(event);
        int row, e;

        if (hit(help->pos(), row, e) && (e >= 0))
        {
            const DTVGuide::Event& ev = m_rows[row].m_events[e];
            QToolTip::showText(help->globalPos(), QString("%1 - %2 %3")
                .arg(QDateTime::fromSecsSinceEpoch(ev.m_start).toLocalTime().toString("hh:mm"))
                .arg(QDateTime::fromSecsSinceEpoch(ev.end()).toLocalTime().toString("hh:mm"))
                .arg(ev.m_title), viewport());
        }
        else
        {
            QToolTip::hideText();
        }

        return true;
    }

    return QAbstractScrollArea::viewportEvent(event);
}

void DTVEPGGrid::paintEvent(QPaintEvent *)
{
    QPainter painter(viewport());
    const QPalette& pal = palette();
    const int width = viewport()->width();
    const int height = viewport()->height();
    const int names = nameWidth();
    const int header = headerHeight();
    const int rh = rowHeight();
    const int top = verticalScrollBar()->value();
    const qint64 from = timeAt(names);
    const qint64 to = timeAt(width) + 1;
    const QColor disabled = pal.color(QPalette::Disabled, QPalette::Text);
    painter.fillRect(viewport()->rect(), pal.color(QPalette::Base));

    // Programmes
    painter.save();
    painter.setClipRect(names, header, width - names, height - header);
    const int firstRow = top / rh;

    for (int r = firstRow; (r < (int) m_rows.size()) && (header + r * rh - top < height); r++)
    {
        const Row& row = m_rows[r];
        const int y = header + r * rh - top;
        // In start order, ends needn't be (an event within another): from the first that could reach the view, by the
        // row's longest, those that do
        const auto first = std::lower_bound(row.m_events.begin(), row.m_events.end(), from - m_longest[r],
            [](const DTVGuide::Event& e, qint64 t) { return e.m_start < t; });

        for (auto it = first; (it != row.m_events.end()) && (it->m_start < to); ++it)
        {
            const DTVGuide::Event& e = *it;

            if (e.end() <= from) {
                continue;
            }
            const int index = (int) (it - row.m_events.begin());
            const QRectF box(x(e.m_start), y, x(e.end()) - x(e.m_start), rh);
            const bool selected = (r == m_selectedRow) && (index == m_selectedEvent);
            const bool onNow = (e.m_start <= m_now) && (m_now < e.end());
            const bool scheduled = m_isScheduled && m_isScheduled(row.m_networkId, row.m_serviceId, e.m_eventId);
            QColor fill = selected ? pal.color(QPalette::Highlight) : onNow ? pal.color(QPalette::AlternateBase) : pal.color(QPalette::Base);
            painter.fillRect(box, fill);
            painter.setPen(pal.color(QPalette::Mid));
            painter.drawRect(box.adjusted(0, 0, -1, -1));

            if (scheduled)
            {
                painter.setPen(QPen(Qt::red, 2));
                painter.drawRect(box.adjusted(1, 1, -2, -2));
            }

            // The title and start, from the left of the view if the programme started before it
            const double textLeft = std::max(box.left(), (double) names) + 4;
            const QRectF text(textLeft, box.top() + 2, box.right() - textLeft - 4, box.height() - 4);

            if (text.width() > 8)
            {
                QColor color = selected ? pal.color(QPalette::HighlightedText) : !row.m_tunable || (e.end() <= m_now) ? disabled : pal.color(QPalette::Text);
                painter.setPen(color);
                QFont font = painter.font();
                font.setBold(onNow);
                painter.setFont(font);
                const QString title = painter.fontMetrics().elidedText(e.m_title, Qt::ElideRight, (int) text.width());
                painter.drawText(text, Qt::AlignLeft | Qt::AlignTop, title);
                font.setBold(false);
                painter.setFont(font);
                const QString time = QDateTime::fromSecsSinceEpoch(e.m_start).toLocalTime().toString("hh:mm")
                    + (scheduled ? "  REC" : "");
                painter.drawText(text, Qt::AlignLeft | Qt::AlignBottom, painter.fontMetrics().elidedText(time, Qt::ElideRight, (int) text.width()));
            }
        }

        painter.setPen(pal.color(QPalette::Mid));
        painter.drawLine(names, y + rh - 1, width, y + rh - 1);
    }

    // Now
    const double nowX = x(m_now);

    if ((nowX >= names) && (nowX < width))
    {
        painter.setPen(QPen(Qt::red, 2));
        painter.drawLine(QPointF(nowX, header), QPointF(nowX, height));
    }

    painter.restore();

    // Times: half hours, and the day at midnight (local time)
    painter.fillRect(0, 0, width, header, pal.color(QPalette::Button));
    painter.setPen(pal.color(QPalette::ButtonText));
    painter.save();
    painter.setClipRect(names, 0, width - names, header);

    for (qint64 t = from - (from % 1800) - 1800; t < to; t += 1800)
    {
        const double tx = x(t);
        const QDateTime local = QDateTime::fromSecsSinceEpoch(t).toLocalTime();
        const bool midnight = (local.time().hour() == 0) && (local.time().minute() == 0);
        QFont font = painter.font();
        font.setBold(midnight);
        painter.setFont(font);
        painter.drawLine(QPointF(tx, header - 4), QPointF(tx, header));
        painter.drawText(QPointF(tx + 3, header - 6), midnight ? local.toString("ddd d MMM") : local.toString("hh:mm"));
    }

    painter.restore();

    // The day of the left of the view, above the services
    painter.fillRect(0, 0, names, header, pal.color(QPalette::Button));
    painter.drawText(QRect(4, 0, names - 8, header), Qt::AlignVCenter | Qt::AlignLeft,
        QDateTime::fromSecsSinceEpoch(from).toLocalTime().toString("dddd d MMMM"));

    // Services
    painter.setClipRect(0, header, names, height - header);

    for (int r = firstRow; (r < (int) m_rows.size()) && (header + r * rh - top < height); r++)
    {
        const Row& row = m_rows[r];
        const int y = header + r * rh - top;
        painter.fillRect(0, y, names, rh, pal.color(QPalette::Window));
        painter.setPen(row.m_tunable ? pal.color(QPalette::WindowText) : disabled);
        QFont font = painter.font();
        font.setItalic(!row.m_actual);
        painter.setFont(font);
        const QString text = !row.m_channel.isEmpty() ? QString("%1  %2").arg(row.m_channel).arg(row.m_name) : row.m_name;
        painter.drawText(QRect(4, y, names - 8, rh), Qt::AlignVCenter | Qt::AlignLeft,
            painter.fontMetrics().elidedText(text, Qt::ElideRight, names - 8));
        painter.setPen(pal.color(QPalette::Mid));
        painter.drawLine(0, y + rh - 1, names, y + rh - 1);
    }

    painter.setClipping(false);
    painter.setPen(pal.color(QPalette::Mid));
    painter.drawLine(names - 1, 0, names - 1, height);
}
