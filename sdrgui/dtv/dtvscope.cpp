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
#include <cstring>

#include <QContextMenuEvent>
#include <QDateTime>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>

#include "dtv/dtvscope.h"

const QColor DTVScope::m_gridColour(255, 255, 255, 64);
const QColor DTVScope::m_zeroColour(255, 255, 255, 128);
const QColor DTVScope::m_labelColour(200, 200, 200);

const QColor DTVScope::m_merColour(0, 220, 255);
const QColor DTVScope::m_iterationsColour(255, 200, 0);
const QColor DTVScope::m_errorsColour(255, 60, 60);

DTVScope::DTVScope(QWidget *parent) :
    QWidget(parent),
    m_view(Response),
    m_constellationSize(0),
    m_alpha(1),
    m_waterfallRows(0),
    m_rowInterval(0.1),
    m_impulseResolution(0.0),
    m_impulseGuard(0),
    m_sampleRate(64e6 / 7.0),
    m_historyInterval(0.2),
    m_lastRowTime(0)
{
    setMinimumSize(300, 150);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setView(Response);
}

void DTVScope::setView(View view)
{
    static const char *const tips[] = {
        "the channel's frequency response across the carriers, relative to its mean: echoes give ripples, and interference or LO leakage dips",
        "the channel's frequency response over the last minute, newest at the top: notches that move are a channel that changes, "
            "as with reflections from aircraft",
        "the channel's impulse response: the paths against delay from the FFT window's start (microseconds: 1 is 300 m further), "
            "with the guard interval shaded. Paths outside it interfere with the next symbol",
        "over the last minute: MER (blue, dB, left scale), LDPC iterations (yellow, right scale, DVB-T2), and packets or FEC blocks "
            "that couldn't be corrected (red bars)"
    };
    m_view = view;
    setToolTip(QString("Constellation of the equalised data cells, and %1. Right-click to choose what's plotted at the right")
        .arg(tips[view]));
    update();
}

void DTVScope::setData(const std::vector<std::complex<float>>& cells, const std::vector<float>& responseDb, int constellationSize, int alpha)
{
    if (!responseDb.empty() && (responseDb != m_responseDb)) {
        addWaterfallRow(responseDb);
    }

    m_cells = cells;
    m_responseDb = responseDb;
    m_constellationSize = constellationSize;
    m_alpha = alpha;
    update();
}

void DTVScope::setPoints(const std::vector<std::complex<float>>& points)
{
    m_points = points;
}

void DTVScope::setSpectrum(bool spectrum)
{
    if (spectrum != m_spectrum)
    {
        m_spectrum = spectrum;
        m_waterfallRows = 0;
        update();
    }
}

void DTVScope::setImpulseResponse(const std::vector<float>& powerDb, double resolution, int guard, double sampleRate)
{
    m_impulseDb = powerDb;
    m_impulseResolution = resolution;
    m_impulseGuard = guard;
    m_sampleRate = sampleRate;

    if (m_view == ImpulseResponse) {
        update();
    }
}

void DTVScope::addHistory(double mer, double iterations, int errors, double interval)
{
    m_history.push_back({(float) mer, (float) iterations, errors});
    m_historyInterval = interval;

    while (m_history.size() > (size_t) m_historyLength) {
        m_history.pop_front();
    }

    if (m_view == History) {
        update();
    }
}

void DTVScope::clearHistory()
{
    m_history.clear();
    m_waterfallRows = 0;
    update();
}

void DTVScope::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    const char *const names[] = {"Frequency response", "Response waterfall", "Impulse response", "History"};

    for (int v = Response; v <= History; v++)
    {
        QAction *action = menu.addAction(names[v]);
        action->setCheckable(true);
        action->setChecked(v == m_view);
        action->setData(v);
    }

    QAction *chosen = menu.exec(event->globalPos());

    if (chosen) {
        setView((View) chosen->data().toInt());
    }
}

void DTVScope::paintEvent(QPaintEvent *event)
{
    (void) event;
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    int side = std::min(height(), width() / 2);
    paintConstellation(painter, QRect(0, 0, side, side));
    const QRect right(side + 4, 0, width() - side - 4, height());

    switch (m_view)
    {
    case Response:
        paintResponse(painter, right);
        paintTitle(painter, right, m_spectrum ? "Spectrum" : "Frequency response");
        break;
    case Waterfall:
        paintWaterfall(painter, right);
        paintTitle(painter, right, m_spectrum ? "Spectrum waterfall" : "Response waterfall");
        break;
    case ImpulseResponse:
        paintImpulseResponse(painter, right);
        paintTitle(painter, right, "Impulse response");
        break;
    case History:
        paintHistory(painter, right);
        paintTitle(painter, right, "History");
        break;
    }
}

// Which view, at the top right, as a reminder that right-clicking chooses another
void DTVScope::paintTitle(QPainter& painter, const QRect& rect, const QString& title)
{
    const QString text = title + " " + QChar(0x25BE);
    const QFontMetrics metrics(painter.font());
    const QRect box(rect.right() - metrics.horizontalAdvance(text) - 8, rect.top() + 1, metrics.horizontalAdvance(text) + 6, metrics.height());
    painter.fillRect(box, QColor(0, 0, 0, 160));
    painter.setPen(m_labelColour);
    painter.drawText(box, Qt::AlignCenter, text);
}

void DTVScope::paintConstellation(QPainter& painter, const QRect& rect)
{
    const float range = 1.5f; // Of the unit power constellation, edge to centre
    const float scale = rect.width() / (2.0f * range);
    const QPointF centre = QRectF(rect).center();
    painter.save();
    painter.setClipRect(rect);

    painter.setPen(m_gridColour);
    painter.drawLine(QPointF(rect.left(), centre.y()), QPointF(rect.right(), centre.y()));
    painter.drawLine(QPointF(centre.x(), rect.top()), QPointF(centre.x(), rect.bottom()));

    // Ideal points
    if (!m_points.empty())
    {
        painter.setPen(QColor(120, 120, 0));

        for (const auto& point : m_points)
        {
            QPointF p(centre.x() + point.real() * scale, centre.y() - point.imag() * scale);
            painter.drawLine(p + QPointF(-3, 0), p + QPointF(3, 0));
            painter.drawLine(p + QPointF(0, -3), p + QPointF(0, 3));
        }
    }
    else if (m_constellationSize > 0)
    {
        // Levels on each axis: odd, moved out by alpha - 1, and their RMS over both axes
        int perAxis = (int) std::round(std::sqrt((double) m_constellationSize));
        std::vector<float> levels(perAxis);
        float power = 0.0f;

        for (int i = 0; i < perAxis; i++)
        {
            const int odd = 2 * i - perAxis + 1;
            levels[i] = (float) (odd + (odd > 0 ? m_alpha - 1 : 1 - m_alpha));
            power += 2.0f * levels[i] * levels[i] / perAxis;
        }

        const float norm = std::sqrt(power);
        painter.setPen(QColor(120, 120, 0));

        for (int i = 0; i < perAxis; i++)
        {
            for (int q = 0; q < perAxis; q++)
            {
                float x = levels[i] / norm;
                float y = levels[q] / norm;
                QPointF p(centre.x() + x * scale, centre.y() - y * scale);
                painter.drawLine(p + QPointF(-3, 0), p + QPointF(3, 0));
                painter.drawLine(p + QPointF(0, -3), p + QPointF(0, 3));
            }
        }
    }

    painter.setPen(QColor(0, 220, 255));
    // All at once, rather than a call (and state check) for each
    std::vector<QPointF> cells;
    cells.reserve(m_cells.size());

    for (const auto& c : m_cells)
    {
        float x = std::max(-range, std::min(range, c.real()));
        float y = std::max(-range, std::min(range, c.imag()));
        cells.emplace_back(centre.x() + x * scale, centre.y() - y * scale);
    }

    if (!cells.empty()) {
        painter.drawPoints(cells.data(), (int) cells.size());
    }

    painter.restore();
}

void DTVScope::paintResponse(QPainter& painter, const QRect& rect)
{
    const float top = 10.0f, bottom = -30.0f; // dB, relative to the mean
    auto y = [&](float db) {
        db = std::max(bottom, std::min(top, db));
        return rect.top() + (top - db) / (top - bottom) * rect.height();
    };

    // Lines every 10 dB, brighter for 0 dB (the mean), and at the quarters of the channel
    for (int i = 1; i < 4; i++)
    {
        painter.setPen(m_gridColour);
        float x = rect.left() + i * rect.width() / 4.0f;
        painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }

    for (float db = top; db >= bottom; db -= 10.0f)
    {
        painter.setPen(db == 0.0f ? m_zeroColour : m_gridColour);
        painter.drawLine(QPointF(rect.left(), y(db)), QPointF(rect.right(), y(db)));
        painter.setPen(m_labelColour);
        painter.drawText(QPointF(rect.left() + 3, y(db) + (db == top ? painter.fontMetrics().ascent() + 1 : -3)), QString("%1 dB").arg(db, 0, 'f', 0));
    }

    if (m_responseDb.size() < 2) {
        return;
    }

    QPainterPath path;

    for (size_t i = 0; i < m_responseDb.size(); i++)
    {
        float x = rect.left() + (float) i / (m_responseDb.size() - 1) * rect.width();

        if (i == 0) {
            path.moveTo(x, y(m_responseDb[i]));
        } else {
            path.lineTo(x, y(m_responseDb[i]));
        }
    }

    painter.setPen(QColor(255, 200, 0));
    painter.drawPath(path);
}

// dB relative to the mean, -30 to +10, from black through blue, cyan and yellow to red
QRgb DTVScope::waterfallColour(float db)
{
    static const int stops[5][3] = {{0, 0, 0}, {0, 0, 255}, {0, 255, 255}, {255, 255, 0}, {255, 0, 0}};
    const float t = std::max(0.0f, std::min(1.0f, (db + 30.0f) / 40.0f)) * 4.0f;
    const int i = std::min(3, (int) t);
    const float f = t - i;
    return qRgb((int) (stops[i][0] + f * (stops[i + 1][0] - stops[i][0])), (int) (stops[i][1] + f * (stops[i + 1][1] - stops[i][1])),
        (int) (stops[i][2] + f * (stops[i + 1][2] - stops[i][2])));
}

void DTVScope::addWaterfallRow(const std::vector<float>& responseDb)
{
    const int width = (int) responseDb.size();

    if (m_waterfall.width() != width)
    {
        m_waterfall = QImage(width, m_waterfallLength, QImage::Format_RGB32);
        m_waterfall.fill(Qt::black);
        m_waterfallRows = 0;
    }

    // How often rows come, for the time scale
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if ((m_lastRowTime > 0) && (now > m_lastRowTime) && (now - m_lastRowTime < 2000)) {
        m_rowInterval = 0.9 * m_rowInterval + 0.1 * (now - m_lastRowTime) / 1000.0;
    }

    m_lastRowTime = now;

    // Older rows down one
    for (int y = m_waterfall.height() - 1; y > 0; y--) {
        memcpy(m_waterfall.scanLine(y), m_waterfall.scanLine(y - 1), m_waterfall.bytesPerLine());
    }

    QRgb *row = reinterpret_cast<QRgb *>(m_waterfall.scanLine(0));

    for (int x = 0; x < width; x++) {
        row[x] = waterfallColour(responseDb[x]);
    }

    m_waterfallRows = std::min(m_waterfallRows + 1, m_waterfall.height());
}

void DTVScope::paintWaterfall(QPainter& painter, const QRect& rect)
{
    if (m_waterfallRows > 0)
    {
        // A row a pixel or more, from the top, as far as there are rows
        const double rowHeight = (double) rect.height() / m_waterfallLength;
        const QRectF target(rect.left(), rect.top(), rect.width(), rowHeight * m_waterfallRows);
        painter.drawImage(target, m_waterfall, QRectF(0, 0, m_waterfall.width(), m_waterfallRows));
    }

    painter.setPen(m_gridColour);

    for (int i = 1; i < 4; i++)
    {
        const float x = rect.left() + i * rect.width() / 4.0f;
        painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }

    // Seconds ago, every 10 s
    const double secondsHigh = m_waterfallLength * m_rowInterval;
    const double pixelsPerSecond = rect.height() / std::max(1e-3, secondsHigh);

    for (int t = 10; t < secondsHigh; t += 10)
    {
        const float y = rect.top() + t * pixelsPerSecond;
        painter.setPen(m_gridColour);
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        const QString label = QString("-%1 s").arg(t);
        const QFontMetrics metrics(painter.font());
        painter.fillRect(QRectF(rect.left() + 1, y - metrics.height(), metrics.horizontalAdvance(label) + 4, metrics.height()), QColor(0, 0, 0, 160));
        painter.setPen(m_labelColour);
        painter.drawText(QPointF(rect.left() + 3, y - 3), label);
    }
}

void DTVScope::paintImpulseResponse(QPainter& painter, const QRect& rect)
{
    const int bins = (int) m_impulseDb.size();

    if ((bins < 8) || (m_impulseResolution <= 0.0) || (m_sampleRate <= 0.0))
    {
        painter.setPen(m_labelColour);
        painter.drawText(rect, Qt::AlignCenter, "No impulse response yet");
        return;
    }

    // From an eighth of the response's span before the window start, to seven eighths after it
    const double period = bins * m_impulseResolution;        // Samples
    const double start = -period / 8.0;
    const double usPerSample = 1e6 / m_sampleRate;
    const float top = 0.0f, bottom = -40.0f;
    auto x = [&](double samples) { return rect.left() + (samples - start) / period * rect.width(); };
    auto y = [&](float db) { return rect.top() + (top - std::max(bottom, std::min(top, db))) / (top - bottom) * rect.height(); };

    // The guard interval: paths from the window start to its end are free of interference between symbols
    painter.fillRect(QRectF(QPointF(x(0.0), rect.top()), QPointF(x(m_impulseGuard), rect.bottom())), QColor(0, 160, 0, 60));
    painter.setPen(m_zeroColour);
    painter.drawLine(QPointF(x(0.0), rect.top()), QPointF(x(0.0), rect.bottom()));

    for (float db = top; db >= bottom; db -= 10.0f)
    {
        painter.setPen(m_gridColour);
        painter.drawLine(QPointF(rect.left(), y(db)), QPointF(rect.right(), y(db)));
        painter.setPen(m_labelColour);
        painter.drawText(QPointF(rect.left() + 3, y(db) + (db == top ? painter.fontMetrics().ascent() + 1 : -3)), QString("%1 dB").arg(db, 0, 'f', 0));
    }

    // Delays, in microseconds, at a step giving 4 to 8 labels
    const double spanUs = period * usPerSample;
    double step = 1.0;

    for (double s : {1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0})
    {
        step = s;

        if (spanUs / s <= 8.0) {
            break;
        }
    }

    for (double us = std::ceil(start * usPerSample / step) * step; us < (start + period) * usPerSample; us += step)
    {
        const float px = x(us / usPerSample);
        painter.setPen(m_gridColour);
        painter.drawLine(QPointF(px, rect.top()), QPointF(px, rect.bottom()));
        painter.setPen(m_labelColour);
        painter.drawText(QPointF(px + 2, rect.bottom() - 3), QString("%1 %2s").arg(us, 0, 'f', 0).arg(QChar(0x00B5)));
    }

    QPainterPath path;
    const int first = bins - bins / 8; // The bin at the start delay, as the response wraps round

    for (int i = 0; i < bins; i++)
    {
        const int b = (first + i) % bins;
        const double delay = start + i * m_impulseResolution;
        const QPointF point(x(delay), y(m_impulseDb[b]));

        if (i == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }

    painter.setPen(QColor(255, 200, 0));
    painter.drawPath(path);
}

void DTVScope::paintHistory(QPainter& painter, const QRect& rect)
{
    const float merTop = 40.0f, iterationsTop = 50.0f;
    const double dx = (double) rect.width() / (m_historyLength - 1);
    auto x = [&](int i) { return rect.right() - (double) ((int) m_history.size() - 1 - i) * dx; };
    bool iterations = false;

    for (const auto& p : m_history) {
        iterations = iterations || (p.m_iterations >= 0.0f);
    }

    // Every 10 dB of MER, and every 10 s
    for (int db = 0; db <= (int) merTop; db += 10)
    {
        const float y = rect.bottom() - db / merTop * rect.height();
        painter.setPen(m_gridColour);
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        painter.setPen(m_merColour);
        painter.drawText(QPointF(rect.left() + 3, y + (db == (int) merTop ? painter.fontMetrics().ascent() + 1 : -3)), QString("%1 dB").arg(db));

        if (iterations && (db < (int) merTop)) // The top's under the title
        {
            const QString label = QString::number((int) std::round(db / merTop * iterationsTop));
            painter.setPen(m_iterationsColour);
            painter.drawText(QPointF(rect.right() - painter.fontMetrics().horizontalAdvance(label) - 3,
                y + (db == (int) merTop ? painter.fontMetrics().ascent() + 1 : -3)), label);
        }
    }

    const int secondsWide = (int) (m_historyLength * m_historyInterval);

    for (int t = 10; t < secondsWide; t += 10)
    {
        const double px = rect.right() - t / m_historyInterval * dx;
        painter.setPen(m_gridColour);
        painter.drawLine(QPointF(px, rect.top()), QPointF(px, rect.bottom()));
    }

    // Uncorrected errors: a bar each, its height logarithmic, a third of the height for 1000
    for (int i = 0; i < (int) m_history.size(); i++)
    {
        if (m_history[i].m_errors > 0)
        {
            const double h = std::min(1.0, std::log10(1.0 + m_history[i].m_errors) / 3.0) * rect.height() / 3.0;
            painter.fillRect(QRectF(x(i) - std::max(1.0, dx / 2), rect.bottom() - h, std::max(2.0, dx), h), m_errorsColour);
        }
    }

    // MER and iterations, with gaps where they aren't known
    auto plot = [&](float HistoryPoint::*value, float scaleTop, const QColor& colour) {
        QPainterPath path;
        bool drawing = false;

        for (int i = 0; i < (int) m_history.size(); i++)
        {
            const float v = m_history[i].*value;

            if (!std::isfinite(v) || (v < 0.0f))
            {
                drawing = false;
                continue;
            }

            const QPointF point(x(i), rect.bottom() - std::min(1.0f, v / scaleTop) * rect.height());

            if (drawing) {
                path.lineTo(point);
            } else {
                path.moveTo(point);
            }

            drawing = true;
        }

        painter.setPen(QPen(colour, 1.5));
        painter.drawPath(path);
    };

    plot(&HistoryPoint::m_mer, merTop, m_merColour);

    if (iterations) {
        plot(&HistoryPoint::m_iterations, iterationsTop, m_iterationsColour);
    }
}
