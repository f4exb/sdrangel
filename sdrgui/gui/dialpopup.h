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

#ifndef INCLUDE_GUI_DIALPOPUP_H
#define INCLUDE_GUI_DIALPOPUP_H

#include <QDialog>

#include "export.h"

class QDial;
class QSlider;
class QLabel;
class QPushButton;
class DialogPositioner;

// A popup dialog for QDials that uses a slider instead, which is easier to use
// on a touch screen. Actived with tap and hold or right mouse click
class SDRGUI_API DialPopup : public QDialog {
    Q_OBJECT

public:
    explicit DialPopup(QDial *parent = nullptr);
    QDial *dial() const { return m_dial; }

    // Add popups to all child QDials in a widget
    static void addPopupsToChildDials(QWidget *parent);

public slots:
    virtual void accept() override;
    virtual void reject() override;
    void on_value_valueChanged(int value);
    void display(const QPoint& p);

private:

    QDial *m_dial;
    QSlider *m_value;
    QLabel *m_valueText;
    QLabel *m_label;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    int m_originalValue;
    DialogPositioner *m_positioner;

};

#endif // INCLUDE_GUI_DIALPOPUP_H
