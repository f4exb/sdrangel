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

#include <QPainter>
#include <QPainterPath>

#include "dabcarrierplot.h"

DABCarrierPlot::DABCarrierPlot(QWidget *parent) :
    QWidget(parent),
    m_median(0.0f),
    m_top(30.0f)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(110);
    setToolTip(tr("Carrier to noise ratio of each carrier, from the null symbol.\n"
        "Notches are multipath; dips over a few carriers are interference.\n"
        "The dashed line is the median (the C/N statistic)."));
}

void DABCarrierPlot::setCarrierCN(const std::vector<float>& cn, float median)
{
    m_cn = cn;
    m_median = median;
    // A scale in 10 dB steps that fits the highest carriers, with some hysteresis
    float highest = 0.0f;
    for (float v : m_cn) {
        highest = std::max(highest, v);
    }
    const float top = std::max(20.0f, 10.0f * std::ceil((highest + 2.0f) / 10.0f));
    if ((top > m_top) || (top < m_top - 10.0f)) {
        m_top = top;
    }
    update();
}

void DABCarrierPlot::clear()
{
    m_cn.clear();
    update();
}

void DABCarrierPlot::paintEvent(QPaintEvent *event)
{
    (void) event;
    QPainter painter(this);
    const QPalette& pal = palette();
    painter.fillRect(rect(), pal.color(QPalette::Base));

    const QFontMetrics metrics(font());
    const int left = metrics.horizontalAdvance("40 dB") + 6;
    const int right = 6;
    const int top = metrics.height() / 2 + 2;
    const int bottom = metrics.height() + 4;
    const QRect plot(left, top, std::max(1, width() - left - right), std::max(1, height() - top - bottom));
    const float bottomDB = -5.0f;

    auto yFor = [&](float db) {
        const float clamped = std::max(bottomDB, std::min(m_top, db));
        return plot.bottom() - (clamped - bottomDB) / (m_top - bottomDB) * plot.height();
    };

    // Grid and scales
    QColor gridColor = pal.color(QPalette::Text);
    gridColor.setAlpha(60);
    painter.setPen(QPen(gridColor, 1));
    for (int db = 0; db <= static_cast<int>(m_top); db += 10)
    {
        const int y = static_cast<int>(std::lround(yFor(static_cast<float>(db))));
        painter.drawLine(plot.left(), y, plot.right(), y);
        painter.setPen(pal.color(QPalette::Text));
        painter.drawText(QRect(0, y - metrics.height() / 2, left - 4, metrics.height()), Qt::AlignRight | Qt::AlignVCenter,
            QString("%1 dB").arg(db));
        painter.setPen(QPen(gridColor, 1));
    }
    const int middle = plot.left() + plot.width() / 2;
    painter.drawLine(middle, plot.top(), middle, plot.bottom());
    painter.setPen(pal.color(QPalette::Text));
    const int labelY = plot.bottom() + 2;
    painter.drawText(QRect(plot.left(), labelY, plot.width(), metrics.height()), Qt::AlignLeft, "-768 kHz");
    painter.drawText(QRect(plot.left(), labelY, plot.width(), metrics.height()), Qt::AlignHCenter, "0");
    painter.drawText(QRect(plot.left(), labelY, plot.width(), metrics.height()), Qt::AlignRight, "+768 kHz");

    if (m_cn.empty())
    {
        painter.drawText(plot, Qt::AlignCenter, tr("No signal"));
        return;
    }

    // Each column shows the range of its carriers, filled from the bottom to their minimum
    const int n = static_cast<int>(m_cn.size());
    const int columns = plot.width();
    QPainterPath area;
    QPainterPath upper;
    area.moveTo(plot.left(), plot.bottom());
    for (int x = 0; x < columns; x++)
    {
        const int first = x * n / columns;
        const int last = std::max(first + 1, (x + 1) * n / columns);
        float lowest = m_cn[first];
        float highest = m_cn[first];
        for (int c = first + 1; c < std::min(last, n); c++)
        {
            lowest = std::min(lowest, m_cn[c]);
            highest = std::max(highest, m_cn[c]);
        }
        area.lineTo(plot.left() + x, yFor(lowest));
        if (x == 0) {
            upper.moveTo(plot.left() + x, yFor(highest));
        } else {
            upper.lineTo(plot.left() + x, yFor(highest));
        }
    }
    area.lineTo(plot.left() + columns - 1, plot.bottom());
    area.closeSubpath();

    QColor fill(255, 165, 0);
    fill.setAlpha(110);
    painter.fillPath(area, fill);
    painter.setPen(QPen(QColor(255, 165, 0), 1));
    painter.drawPath(upper);

    // The median
    const int y = static_cast<int>(std::lround(yFor(m_median)));
    QPen medianPen(pal.color(QPalette::Text), 1, Qt::DashLine);
    painter.setPen(medianPen);
    painter.drawLine(plot.left(), y, plot.right(), y);
}
