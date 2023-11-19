///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef SDRGUI_GUI_CRIGHTCLICKENABLER_H_
#define SDRGUI_GUI_CRIGHTCLICKENABLER_H_

#include <QWidget>

#include "export.h"

class SDRGUI_API CRightClickEnabler : public QObject {
    Q_OBJECT
public:
    CRightClickEnabler(QWidget *widget);

signals:
    void rightClick(const QPoint&);  // Emitted for right mouse click and touch tap and hold

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget* m_widget;
    bool m_mousePressed;
};

#endif /* SDRGUI_GUI_CRIGHTCLICKENABLER_H_ */
