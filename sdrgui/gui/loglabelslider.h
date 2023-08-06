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

#ifndef SDRGUI_GUI_LOGLABELSLIDER_H
#define SDRGUI_GUI_LOGLABELSLIDER_H

#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "logslider.h"

#include "export.h"

// Logarithmic Slider with labels above
class SDRGUI_API LogLabelSlider : public QWidget {
    Q_OBJECT

public:

    explicit LogLabelSlider(QWidget *parent = nullptr);
    void setRange(double min, double max);
    void setValue(double value) { m_slider->setValue(value); }

signals:

    void logValueChanged(double value);

private slots:
    void handleLogValueChanged(double value);

private:

    QList<QLabel *> m_labels;
    LogSlider *m_slider;
    QVBoxLayout *m_vLayout;
    QHBoxLayout *m_hLayout;

};

#endif // SDRGUI_GUI_LOGLABELSLIDER_H
