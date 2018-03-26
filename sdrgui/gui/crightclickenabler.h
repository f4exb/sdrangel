///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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

class CRightClickEnabler : public QObject {
    Q_OBJECT
public:
    CRightClickEnabler(QAbstractButton *button);

signals:
    void rightClick();

protected:
    inline bool eventFilter(QObject *watched __attribute__((unused)), QEvent *event) override {
        if (event->type() == QEvent::MouseButtonPress)
        {
            auto mouseEvent = (QMouseEvent*)event;
            if (mouseEvent->button() == Qt::RightButton) {
                //_button->click();
                emit rightClick();
            }
        }
        return false;
    }

private:
    QAbstractButton* _button;
};

#endif /* SDRGUI_GUI_CRIGHTCLICKENABLER_H_ */
