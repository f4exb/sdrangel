///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRGUI_SOAPYGUI_INTERVALSLIDERGUI_H_
#define SDRGUI_SOAPYGUI_INTERVALSLIDERGUI_H_

#include <QWidget>
#include <QString>

namespace Ui {
    class IntervalSliderGUI;
}

class IntervalSliderGUI : public QWidget
{
    Q_OBJECT
public:
    explicit IntervalSliderGUI(QWidget* parent = 0);
    virtual ~IntervalSliderGUI();

    void setLabel(const QString& text);
    void setUnits(const QString& units);
    void setInterval(double minimum, double maximum);
    virtual double getCurrentValue();
    virtual void setValue(double value);

signals:
    void valueChanged(double value);

private slots:
    void on_intervalSlider_valueChanged(int value);

private:
    Ui::IntervalSliderGUI* ui;
    double m_minimum;
    double m_maximum;
};

#endif /* SDRGUI_SOAPYGUI_INTERVALSLIDERGUI_H_ */
