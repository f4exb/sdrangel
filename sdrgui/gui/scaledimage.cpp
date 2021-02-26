///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include "scaledimage.h"

ScaledImage::ScaledImage(QWidget *parent) :
    QLabel(parent)
{
}

void ScaledImage::setPixmap(const QPixmap& pixmap)
{
    setPixmap(pixmap, size());
}

void ScaledImage::setPixmap(const QPixmap& pixmap, const QSize& size)
{
    m_pixmap = pixmap;
    m_pixmapScaled = pixmap.scaled(size, Qt::KeepAspectRatio);
    QLabel::setPixmap(m_pixmapScaled);
}

void ScaledImage::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    setPixmap(m_pixmap, event->size());
}
