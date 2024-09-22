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

#ifndef SDRGUI_GUI_PERIODDIAL_H
#define SDRGUI_GUI_PERIODDIAL_H

#include <QWidget>

#include "export.h"

class QHBoxLayout;
class WrappingDial;
class WrappingSpinBox;
class QComboBox;

// Combines QDial, QSpinBox and QComboBox to allow user to enter time period in s, ms or us
class SDRGUI_API PeriodDial : public QWidget {
    Q_OBJECT

public:
    explicit PeriodDial(QWidget *parent = nullptr);

    void setValue(double value);
    double value();

private:

    QHBoxLayout *m_layout;
    WrappingDial *m_dial;
    WrappingSpinBox *m_spinBox;
    QComboBox *m_units;

private slots:

    void on_dial_valueChanged(int value);
    void on_spinBox_valueChanged(int value);
    void on_units_currentIndexChanged(int index);
    void on_wrapUp();
    void on_wrapDown();

signals:
    void valueChanged(double value);

};

#endif // SDRGUI_GUI_PERIODDIAL_H
