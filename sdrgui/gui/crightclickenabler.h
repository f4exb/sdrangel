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

#include <QAbstractButton>
#include <QMouseEvent>

#include "export.h"

class SDRGUI_API CRightClickEnabler : public QObject {
    Q_OBJECT
public:
    CRightClickEnabler(QAbstractButton *button);

signals:
    void rightClick(const QPoint&);

protected:
    inline bool eventFilter(QObject *watched, QEvent *event) override
    {
        (void) watched;

        if (event->type() == QEvent::MouseButtonPress)
        {
            auto mouseEvent = (QMouseEvent*) event;

            if (mouseEvent->button() == Qt::RightButton) {
                emit rightClick(mouseEvent->globalPos());
            }
        }

        return false;
    }

private:
    QAbstractButton* _button;
};

#endif /* SDRGUI_GUI_CRIGHTCLICKENABLER_H_ */
