///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
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
