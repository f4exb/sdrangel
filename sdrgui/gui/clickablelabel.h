///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_GUI_CLICKABLELABEL_H_
#define SDRBASE_GUI_CLICKABLELABEL_H_

#include <QObject>
#include <QLabel>

#include "export.h"

class SDRGUI_API ClickableLabel : public QLabel
{
Q_OBJECT
public:
    explicit ClickableLabel();
    explicit ClickableLabel(QWidget* parent);
    explicit ClickableLabel(const QString& text, QWidget* parent=nullptr);
    ~ClickableLabel();
signals:
    void clicked();
protected:
    void mousePressEvent(QMouseEvent* event);
};


#endif /* SDRBASE_GUI_CLICKABLELABEL_H_ */
