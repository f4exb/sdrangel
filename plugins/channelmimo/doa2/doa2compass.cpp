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

    setMinimumSize(m_sizeMin, m_sizeMin);
    setMaximumSize(m_sizeMax, m_sizeMax);
    resize(m_sizeMin, m_sizeMin);

    setFocusPolicy(Qt::NoFocus);

    m_yaw  = 0.0;
    m_alt  = 0.0;
    m_h    = 0.0;
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
}

void DOA2Compass::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    QBrush bgGround(QColor(48,172,220));

    QPen   whitePen(Qt::white);
    QPen   blackPen(Qt::black);
    QPen   redPen(Qt::red);
    QPen   bluePen(Qt::blue);
    QPen   greenPen(Qt::green);

    whitePen.setWidth(1);
    blackPen.setWidth(2);
    redPen.setWidth(2);
    bluePen.setWidth(2);
    greenPen.setWidth(2);

    painter.setRenderHint(QPainter::Antialiasing);

    painter.translate(width() / 2, height() / 2);


    // draw background
    {
        painter.setPen(blackPen);
        painter.setBrush(bgGround);

        painter.drawEllipse(-m_size/2, -m_size/2, m_size, m_size);
    }


    // draw yaw lines
    {
        int     nyawLines = 36;
        float   rotAng = 360.0 / nyawLines;
        int     yawLineLeng = m_size/25;
        double  fx1, fy1, fx2, fy2;
        int     fontSize = 8;
        QString s;

        blackPen.setWidth(1);
        painter.setPen(blackPen);

        for(int i=0; i<nyawLines; i++) {

            if( i == 0 ) {
                s = "N";
                painter.setPen(bluePen);

                painter.setFont(QFont("", fontSize*1.3));
            } else if ( i == 9 ) {
                s = "W";
                painter.setPen(blackPen);

                painter.setFont(QFont("", fontSize*1.3));
            } else if ( i == 18 ) {
                s = "S";
                painter.setPen(redPen);

                painter.setFont(QFont("", fontSize*1.3));
            } else if ( i == 27 ) {
                s = "E";
                painter.setPen(blackPen);

                painter.setFont(QFont("", fontSize*1.3));
            } else {
                s = QString("%1").arg(i*rotAng);
                painter.setPen(blackPen);

                painter.setFont(QFont("", fontSize));
            }

            fx1 = 0;
            fy1 = -m_size/2 + m_offset;
            fx2 = 0;

            if( i % 3 == 0 ) {
                fy2 = fy1 + yawLineLeng;
                painter.drawLine(QPointF(fx1, fy1), QPointF(fx2, fy2));

                fy2 = fy1 + yawLineLeng+4;
                painter.drawText(QRectF(-50, fy2, 100, fontSize+2),
                                 Qt::AlignCenter, s);
            } else {
                fy2 = fy1 + yawLineLeng/2;
                painter.drawLine(QPointF(fx1, fy1), QPointF(fx2, fy2));
            }

            painter.rotate(-rotAng);
        }
    }

    // draw S/N arrow
    {
        int     arrowWidth = m_size/5;
        double  fx1, fy1, fx2, fy2, fx3, fy3;

        fx1 = 0;
        fy1 = -m_size/2 + m_offset + m_size/25 + 15;
        fx2 = -arrowWidth/2;
        fy2 = 0;
        fx3 = arrowWidth/2;
        fy3 = 0;

        painter.setPen(Qt::NoPen);

        painter.setBrush(QBrush(Qt::blue));
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

        painter.setBrush(QBrush(Qt::red));
        QPointF pointsS[3] = {
            QPointF(fx1, fy1),
            QPointF(fx2, fy2),
            QPointF(fx3, fy3)
        };
        painter.drawPolygon(pointsS, 3);
    }


    // draw yaw marker
    {
        int     yawMarkerSize = m_size/12;
        double  fx1, fy1, fx2, fy2, fx3, fy3;

        painter.rotate(-m_yaw);
        painter.setBrush(QBrush(QColor(0xFF, 0x00, 0x00, 0xE0)));

        fx1 = 0;
        fy1 = -m_size/2 + m_offset;
        fx2 = fx1 - yawMarkerSize/2;
        fy2 = fy1 + yawMarkerSize;
        fx3 = fx1 + yawMarkerSize/2;
        fy3 = fy1 + yawMarkerSize;

        QPointF points[3] = {
            QPointF(fx1, fy1),
            QPointF(fx2, fy2),
            QPointF(fx3, fy3)
        };
        painter.drawPolygon(points, 3);

        painter.rotate(m_yaw);
    }

    // draw altitude
    {
        int     altFontSize = 13;
        int     fx, fy, w, h;
        QString s;
        char    buf[200];

        w  = 130;
        h  = 2*(altFontSize + 8);
        fx = -w/2;
        fy = -h/2;

        blackPen.setWidth(2);
        painter.setPen(blackPen);
        painter.setBrush(QBrush(Qt::white));
        painter.setFont(QFont("", altFontSize));

        painter.drawRoundedRect(fx, fy, w, h, 6, 6);

        painter.setPen(bluePen);
        sprintf(buf, "ALT: %6.1f m", m_alt);
        s = buf;
        painter.drawText(QRectF(fx, fy+2, w, h/2), Qt::AlignCenter, s);

        sprintf(buf, "H: %6.1f m", m_h);
        s = buf;
        painter.drawText(QRectF(fx, fy+h/2, w, h/2), Qt::AlignCenter, s);
    }
}

void DOA2Compass::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        m_yaw -= 1.0;
        break;
    case Qt::Key_Right:
        m_yaw += 1.0;
        break;
    case Qt::Key_Down:
        m_alt -= 1.0;
        break;
    case Qt::Key_Up:
        m_alt += 1.0;
        break;
    case Qt::Key_W:
        m_h += 1.0;
        break;
    case Qt::Key_S:
        m_h -= 1.0;
        break;

    default:
        QWidget::keyPressEvent(event);
        break;
    }

    update();
}
