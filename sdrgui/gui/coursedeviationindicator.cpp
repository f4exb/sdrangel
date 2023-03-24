///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include <QPainter>

#include "coursedeviationindicator.h"

CourseDeviationIndicator::CourseDeviationIndicator(QWidget *parent) :
    QWidget(parent),
    m_localizerDDM(0.0f),
    m_glideSlopeDDM(0.0f)
{
}

void CourseDeviationIndicator::setMode(Mode mode)
{
    m_mode = mode;
    update();
}

void CourseDeviationIndicator::setLocalizerDDM(float ddm)
{
    m_localizerDDM = ddm;
    update();
}

void CourseDeviationIndicator::setGlideSlopeDDM(float ddm)
{
    m_glideSlopeDDM = ddm;
    update();
}

void CourseDeviationIndicator::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    QRect r = rect();
    int midW = r.width() / 2;
    int midH = r.height() / 2;
    int spacing;

    // A320 like CDI

    // Black background
    int bgw, bgh;
    if (m_mode == LOC)
    {
        bgw = r.width();
        bgh = 20;
        painter.fillRect(0, midH - bgh, bgw, bgh*2, QColor(0, 0, 0));
    }
    else
    {
        bgw = 20;
        bgh = r.height();
        painter.fillRect(midW - bgw, 0, bgw*2, bgh, QColor(0, 0, 0));
    }

    const int dots = 5;

    // Circles
    painter.setPen(QColor(255, 255, 255));
    const int radius = 4;
    int x, y;

    if (m_mode == LOC)
    {
        spacing = r.width() / 5;
        x = spacing / 2;
        y = midH;
    }
    else
    {
        spacing = r.height() / 5;
        x = midW;
        y = spacing / 2;
    }
    for (int i = 0; i < dots; i++)
    {
        if (i != 2) {
            painter.drawEllipse(QPointF(x, y), radius, radius);
        }
        if (m_mode == LOC) {
            x += spacing;
        } else {
            y += spacing;
        }
    }

    // Diamond (index) - only draw half of symbol if out of range
    // Shouldn't draw the symbol if signal not vaiid
    // Typically, LOC full scale deflection 0.155 DDM (Which is ~2.5deg, so 1 deg per dot, but can be 3 degrees. A320 is 0.8 deg per dot)
    // For GS, full deflection is 0.0875 DDM (0.7deg, so 0.14 deg per dot)
    painter.setPen(QColor(255, 150, 250));
    float dev;
    if (m_mode == LOC) {
        dev = m_localizerDDM / 0.155;
    } else {
        dev = m_glideSlopeDDM / 0.0875;
    }
    dev = std::min(dev, 1.0f);
    dev = std::max(dev, -1.0f);
    if (m_mode == LOC)
    {
        x = midW + dev * r.width() / 2;  // Positive DDM means we're to left of course line
        y = midH;
    }
    else
    {
        x = midW;
        y = midH + dev * r.height() / 2; // Positive DDM means we're above glide path
    }
    int dw = 10;
    int dh = 8;
    painter.drawLine(x, y + dh, x - dw, y);
    painter.drawLine(x - dw, y, x, y - dh);
    painter.drawLine(x + dw, y, x, y - dh);
    painter.drawLine(x, y + dh, x + dw, y);

    // Centre line
    painter.setPen(QColor(255, 255, 70));
    if (m_mode == LOC)
    {
        int lh = 14;
        painter.drawLine(midW, midH + lh, midW, midH - lh);
        painter.drawLine(midW-1, midH + lh, midW-1, midH - lh);
        painter.drawLine(midW+1, midH + lh, midW+1, midH - lh);
    }
    else
    {
        int lw = 14;
        painter.drawLine(midW + lw, midH, midW - lw, midH);
        painter.drawLine(midW + lw, midH - 1, midW - lw, midH - 1);
        painter.drawLine(midW + lw, midH + 1, midW - lw, midH + 1);
    }

    if (m_mode == LOC)
    {
        // Indicate localizer capture
        if (std::abs(m_localizerDDM) < 0.175) // See 3.1.3.7.4
        {
            QFontMetrics fm(painter.font());
            QString text = "LOC";
            int tw = fm.horizontalAdvance(text);
            int th = fm.descent();
            painter.setPen(QColor(0, 255, 0));
            painter.drawText(midW - tw/2, midH - bgh - th, text);
        }
    }
    else
    {
        // Indicate glideslope capture
        if (std::abs(m_glideSlopeDDM) < 0.175) // Can't see a spec for this
        {
            QFontMetrics fm(painter.font());
            QString text = "G/S";
            int tw = fm.horizontalAdvance(text);
            int th = fm.ascent() / 2;
            painter.setPen(QColor(0, 255, 0));
            painter.drawText(midW + bgw + 2, midH + th, text);
        }
    }

}

