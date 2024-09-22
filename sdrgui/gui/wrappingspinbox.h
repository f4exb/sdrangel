///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef SDRGUI_GUI_WRAPPINGDSPINBOX_H
#define SDRGUI_GUI_WRAPPINGDSPINBOX_H

#include <QSpinBox>

#include "export.h"

// Extends QSpinBox to generate a signal when spinbox wraps
class SDRGUI_API WrappingSpinBox : public QSpinBox {
    Q_OBJECT

public:
    explicit WrappingSpinBox(QWidget *parent = nullptr);

    void stepBy(int steps) override;

protected:
    void wheelEvent(QWheelEvent *e) override;

private:
    bool m_wheelEvent;
    bool m_wheelUp;

signals:
    void wrapUp();
    void wrapDown();

};

#endif // SDRGUI_GUI_WRAPPINGDSPINBOX_H
