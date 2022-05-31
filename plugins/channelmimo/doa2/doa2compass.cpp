///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#include <QtCore>
#include <QtGui>
#include <QDebug>

#include "doa2compass.h"

DOA2Compass::DOA2Compass(QWidget *parent)
    : QWidget(parent)
{
    connect(this, SIGNAL(canvasReplot(void)), this, SLOT(canvasReplot_slot(void)));

    m_sizeMin = 200;
    m_sizeMax = 600;
    m_offset = 2;
    m_size = m_sizeMin - 2*m_offset;
    m_drawLegend = false;

    setMinimumSize(m_sizeMin, m_sizeMin);
    setMaximumSize(m_sizeMax, m_sizeMax);
    resize(m_sizeMin, m_sizeMin);

    setFocusPolicy(Qt::NoFocus);

    m_azPos  = 0.0;
    m_azNeg  = 0.0;
    m_azAnt  = 0.0;
    m_blindAngle = 0.0;
    m_blindColor = QColor(32, 32, 32);
}

DOA2Compass::~DOA2Compass()
{
}

void DOA2Compass::canvasReplot_slot(void)
{
    update();
}

void DOA2Compass::resizeEvent(QResizeEvent *event)
{
    m_size = qMin(width(),height()) - 2*m_offset;
    QWidget::resizeEvent(event);
}

void DOA2Compass::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    QBrush bgGround(palette().button().color());

    QPen   whitePen(Qt::white);
    QPen   blackPen(Qt::black);
    QPen   redPen(Qt::red);
    QPen   bluePen(Qt::blue);
    QPen   greenPen(Qt::green);
    QPen   borderPen(palette().button().color().lighter(200));

    whitePen.setWidth(1);
    blackPen.setWidth(2);
    redPen.setWidth(2);
    bluePen.setWidth(2);
    greenPen.setWidth(2);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(width() / 2, height() / 2);

    // draw background
    {
        painter.setPen(whitePen);
        painter.setBrush(bgGround);
        painter.drawEllipse(-m_size/2, -m_size/2, m_size, m_size);
    }

    // draw blind angle
    if (m_blindAngle != 0)
    {
        painter.setBrush(m_blindColor);
        painter.setPen(m_blindAngleBorder ? borderPen : Qt::NoPen);
        painter.rotate(m_azAnt - 90);
        painter.drawPie(-m_size/2, -m_size/2, m_size, m_size, -m_blindAngle*16, m_blindAngle*32);
        painter.rotate(180);
        painter.drawPie(-m_size/2, -m_size/2, m_size, m_size, -m_blindAngle*16, m_blindAngle*32);
        painter.rotate(-m_azAnt - 90);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(whitePen);
        painter.drawEllipse(-m_size/2, -m_size/2, m_size, m_size);
    }

    // draw compass lines
    {
        int     nyawLines = 36;
        float   rotAng = 360.0 / nyawLines;
        int     yawLineLeng = m_size/25;
        double  fx1, fy1, fx2, fy2;
        int     fontSize = 8;
        QString s;

        blackPen.setWidth(1);
        painter.setPen(whitePen);
        painter.setFont(font());

        for(int i=0; i<nyawLines; i++) {

            if( i == 0 ) {
                s = "N";
            } else if ( i == 9 ) {
                s = "E";
            } else if ( i == 18 ) {
                s = "S";
            } else if ( i == 27 ) {
                s = "W";
            } else {
                s = QString("%1").arg(i*rotAng);
            }

            fx1 = 0;
            fy1 = -m_size/2 + m_offset;
            fx2 = 0;

            if (i % 3 == 0)
            {
                fy2 = fy1 + yawLineLeng;
                painter.drawLine(QPointF(fx1, fy1), QPointF(fx2, fy2));
                fy2 = fy1 + yawLineLeng+4;
                painter.drawText(QRectF(-50, fy2, 100, fontSize+2), Qt::AlignCenter, s);
            }
            else
            {
                fy2 = fy1 + yawLineLeng/2;
                painter.drawLine(QPointF(fx1, fy1), QPointF(fx2, fy2));
            }

            painter.rotate(rotAng);
        }
    }

    // draw antennas arrow
    {
        int     arrowWidth = m_size/20;
        double  fx1, fy1, fx2, fy2, fx3, fy3;

        fx1 = 0;
        fy1 = -m_size/2 + m_offset + m_size/25 + 15;
        fx2 = -arrowWidth/2;
        fy2 = 0;
        fx3 = arrowWidth/2;
        fy3 = 0;

        painter.rotate(m_azAnt);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(QColor(192, 192, 192)));
        QPointF pointsN[3] = {
            QPointF(fx1, fy1),
            QPointF(fx2, fy2),
            QPointF(fx3, fy3)
        };
        painter.drawPolygon(pointsN, 3);

        fx1 = 0;
        fy1 = m_size/2 - m_offset - m_size/25 - 15;
        fx2 = -arrowWidth/2;
        fy2 = 0;
        fx3 = arrowWidth/2;
        fy3 = 0;

        painter.setBrush(QBrush(palette().button().color().lighter(150)));
        QPointF pointsS[3] = {
            QPointF(fx1, fy1),
            QPointF(fx2, fy2),
            QPointF(fx3, fy3)
        };
        painter.drawPolygon(pointsS, 3);

        painter.rotate(-m_azAnt);
    }

    // draw azPos marker
    {
        int     azMarkerSize = m_size/15;
        double  fx1, fy1, fx2, fy2, fx3, fy3, fyl;

        fx1 = 0;
        fy1 = -m_size/2 + m_offset;
        fx2 = fx1 - azMarkerSize/2;
        fy2 = fy1 + azMarkerSize;
        fx3 = fx1 + azMarkerSize/2;
        fy3 = fy1 + azMarkerSize;
        fyl = -m_size/2; //  -m_size/2 + m_offset + m_size/25 + 15;

        painter.rotate(m_azPos);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(QColor(0xFF, 0x60, 0x60, 0xA0)));
        QPointF points[3] = {
            QPointF(fx1, fy1),
            QPointF(fx2, fy2),
            QPointF(fx3, fy3)
        };
        painter.drawPolygon(points, 3);

        painter.setPen(QColor(0xFF, 0x60, 0x60, 0xE0));
        painter.drawLine(0, 0, 0, fyl);

        painter.rotate(-m_azPos);
    }

    // draw azNeg marker
    {
        int     yawMarkerSize = m_size/15;
        double  fx1, fy1, fx2, fy2, fx3, fy3, fyl;

        fx1 = 0;
        fy1 = -m_size/2 + m_offset;
        fx2 = fx1 - yawMarkerSize/2;
        fy2 = fy1 + yawMarkerSize;
        fx3 = fx1 + yawMarkerSize/2;
        fy3 = fy1 + yawMarkerSize;
        fyl = -m_size/2;

        painter.rotate(m_azNeg);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(QColor(0x80, 0xFF, 0x80, 0xA0)));
        QPointF points[3] = {
            QPointF(fx1, fy1),
            QPointF(fx2, fy2),
            QPointF(fx3, fy3)
        };
        painter.drawPolygon(points, 3);

        painter.setPen(QColor(0x80, 0xFF, 0x80, 0xE0));
        painter.drawLine(0, 0, 0, fyl);

        painter.rotate(-m_azNeg);
    }

    // draw legend
    if (m_drawLegend)
    {
        int altFontSize = 8;
        int fx, fy, w, h;

        w  = 10*altFontSize;
        h  = 3*(altFontSize + 8);
        fx = -w/2;
        fy = -h/2;

        painter.setPen(whitePen);
        painter.setBrush(QBrush(Qt::black));
        QFont f = font();
        f.setPointSize(altFontSize);
        painter.setFont(f);

        painter.drawRoundedRect(fx, fy, w, h, 6, 6);

        painter.drawText(QRectF(fx, fy+2, w, h/3), Qt::AlignCenter, QString("POS: %1%2").arg((int) m_azPos, 3, 10, QLatin1Char('0')).arg(QChar(0260)));
        painter.drawText(QRectF(fx, fy+h/3, w, h/3), Qt::AlignCenter, QString("ANT: %1%2").arg((int) m_azAnt, 3, 10, QLatin1Char('0')).arg(QChar(0260)));
        painter.drawText(QRectF(fx, fy+2*(h/3), w, h/3), Qt::AlignCenter, QString("NEG: %1%2").arg((int) m_azNeg, 3, 10, QLatin1Char('0')).arg(QChar(0260)));
    }
}
