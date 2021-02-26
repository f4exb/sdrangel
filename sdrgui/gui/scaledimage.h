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

#ifndef SDRGUI_GUI_SCALEDIMAGE_H
#define SDRGUI_GUI_SCALEDIMAGE_H

#include <QLabel>
#include <QPixmap>
#include <QSize>
#include <QResizeEvent>

#include "export.h"

// Similar to displaying a pixmap with QLabel, except we preserve the aspect ratio
class SDRGUI_API ScaledImage : public QLabel {

public:
    explicit ScaledImage(QWidget *parent = nullptr);

    void setPixmap(const QPixmap& pixmap);
    void setPixmap(const QPixmap& pixmap, const QSize& size);

protected:

    virtual void resizeEvent(QResizeEvent *event);

private:
    QPixmap m_pixmap;
    QPixmap m_pixmapScaled;

};

#endif // SDRGUI_GUI_SCALEDIMAGE_H
