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

#ifndef PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_DISCRETERANGEGUI_H_
#define PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_DISCRETERANGEGUI_H_

#include <QWidget>
#include <QString>

#include "export.h"
#include "itemsettinggui.h"

namespace Ui {
    class DiscreteRangeGUI;
}

class SDRGUI_API DiscreteRangeGUI : public ItemSettingGUI
{
    Q_OBJECT
public:
    explicit DiscreteRangeGUI(QWidget* parent = 0);
    virtual ~DiscreteRangeGUI();

    void setLabel(const QString& text);
    void setUnits(const QString& units);
    void addItem(const QString& itemStr, double itemValue);
    virtual double getCurrentValue();
    virtual void setValue(double value);

private slots:
    void on_rangeCombo_currentIndexChanged(int index);

private:
    Ui::DiscreteRangeGUI* ui;
    std::vector<double> itemValues;
};



#endif /* PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_DISCRETERANGEGUI_H_ */
