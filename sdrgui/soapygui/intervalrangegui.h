///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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


#ifndef PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_INTERVALRANGEGUI_H_
#define PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_INTERVALRANGEGUI_H_

#include <QWidget>
#include <QString>

#include "export.h"
#include "itemsettinggui.h"

namespace Ui {
    class IntervalRangeGUI;
}

class SDRGUI_API IntervalRangeGUI : public ItemSettingGUI
{
    Q_OBJECT
public:
    explicit IntervalRangeGUI(QWidget* parent = 0);
    virtual ~IntervalRangeGUI();

    void setLabel(const QString& text);
    void setUnits(const QString& units);
    void addInterval(double minimum, double maximum);
    void reset();
    virtual double getCurrentValue();
    virtual void setValue(double value);

private slots:
    void on_value_changed(qint64 value);
    void on_rangeInterval_currentIndexChanged(int index);

private:
    Ui::IntervalRangeGUI* ui;
    std::vector<double> m_minima;
    std::vector<double> m_maxima;
    int m_nbDigits;
};

#endif /* PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_INTERVALRANGEGUI_H_ */
