///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef SDRGUI_GUI_DIALOGPOSITIONER_H
#define SDRGUI_GUI_DIALOGPOSITIONER_H

#include <QWidget>
#include "export.h"

// DialogPositioner monitors screen orientation and repositions dialog
class SDRGUI_API DialogPositioner : public QObject {

    Q_OBJECT

public:
    DialogPositioner(QWidget *dialog, bool center=false);

    // Center dialog on screen
    static void centerDialog(QWidget *dialog);

    // Make sure dialog is on screen
    static void positionDialog(QWidget *dialog);

    // Restrict size of widget to size of desktop (but not smaller than minimumSize)
    static void sizeToDesktop(QWidget *widget);

private slots:
    void orientationChanged(Qt::ScreenOrientation orientation);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *m_dialog;
    bool m_center;

};

#endif
