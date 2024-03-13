///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef SDRGUI_GUI_CWMOUSEKEYERENABLER_H_
#define SDRGUI_GUI_CWMOUSEKEYERENABLER_H_

#include <QWidget>

#include "export.h"

class SDRGUI_API CWMouseKeyerEnabler : public QObject {
    Q_OBJECT
public:
    CWMouseKeyerEnabler(QWidget *widget);

signals:
    void leftButtonPress(const QPoint&);  // Emitted for left mouse press
    void leftButtonRelease(const QPoint&);  // Emitted for left mouse release
    void rightButtonPress(const QPoint&);  // Emitted for right mouse press
    void rightButtonRelease(const QPoint&);  // Emitted for right mouse release

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget* m_widget;
};

#endif /* SDRGUI_GUI_CRIGHTCLICKENABLER_H_ */
