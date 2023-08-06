///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef SDRGUI_GUI_LOGSLIDER_H
#define SDRGUI_GUI_LOGSLIDER_H

#include <QSlider>

#include "export.h"

// Logarithmic Slider (hozizontal only)
class SDRGUI_API LogSlider : public QSlider {
    Q_OBJECT

public:

    explicit LogSlider(QWidget *parent = nullptr);

    void setRange(double min, double max);
    void setValue(double value);


signals:

    void logValueChanged(double value);

private slots:
    void handleValueChanged(int value);

private:

    double m_start;
    double m_stop;
    double m_steps;

    static const int m_stepsPerPower = 100;

};

#endif // SDRGUI_GUI_LOGSLIDER_H
