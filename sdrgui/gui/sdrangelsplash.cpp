///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
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

#include <QScreen>

#include "sdrangelsplash.h"

SDRangelSplash::SDRangelSplash(const QPixmap& pixmap)
{
    int screenWidth = screen()->availableGeometry().width();
    QPixmap pm;
    if (pixmap.width() > screenWidth) {
        pm = pixmap.scaledToWidth(screenWidth, Qt::SmoothTransformation);
    } else {
        pm = pixmap;
    }
    QSplashScreen::setPixmap(pm);
};

SDRangelSplash::~SDRangelSplash()
{
};

void SDRangelSplash::drawContents(QPainter *painter)
{
    QPixmap textPix = QSplashScreen::pixmap();
    painter->setPen(this->color);
    painter->drawText(this->rect, this->alignement, this->message);
};

void SDRangelSplash::showStatusMessage(const QString &message, const QColor &color)
{
    this->message = message;
    this->color = color;
    this->showMessage(this->message, this->alignement, this->color);
};

void SDRangelSplash::setMessageRect(QRect rect, int alignement)
{
    this->rect = rect;
    this->alignement = alignement;
};